#include "Persistence.h"
#include "AofRewriteVisitor.h"
#include "Database.h"
#include "RdbVisitor.h"
#include "RespParser.h"
#include <cstdio>
#include <iostream>
#include <unistd.h>

Persistence::Persistence(const std::string& filepath)
    : _filepath(filepath), _rdbFilepath("dump.rdb"), _flushIntervalSeconds(1) // Default 1s
{
    _lastFlushTime = std::chrono::steady_clock::now();
    // Open file in append mode immediately to create if not exists
    _file.open(_filepath, std::ios::app | std::ios::binary);
}

Persistence::~Persistence()
{
    Flush();
    if (_file.is_open())
        _file.close();
}

void Persistence::SetFlushInterval(int64_t seconds)
{
    _flushIntervalSeconds = seconds;
}

int64_t Persistence::GetFlushInterval() const
{
    return _flushIntervalSeconds;
}

std::string Persistence::EncodeCommand(const std::vector<RespValue>& args)
{
    // RESP Array encoding using RespParser::encode
    RespValue arr;
    arr.type = RespType::Array;
    arr.setArray(args); // This involves a copy of the vector, which is fine for AOF encoding
    return RespParser::encode(arr);
}

void Persistence::Append(const std::vector<RespValue>& args)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_isRewriting)
        BufferForRewrite(args);

    std::string encoded = EncodeCommand(args);

    if (_flushIntervalSeconds == 0)
    {
        // Always-flush mode: write directly to file, skip the buffer entirely.
        // Do NOT add to _buffer first — that would cause a double-write.
        if (!_file.is_open())
            _file.open(_filepath, std::ios::app | std::ios::binary);
        _file.write(encoded.data(), encoded.size());
        _file.flush();
        return;
    }

    // Periodic-flush mode: accumulate in buffer; Tick() will flush on schedule.
    _buffer.insert(_buffer.end(), encoded.begin(), encoded.end());
}

bool Persistence::Flush()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_buffer.empty())
        return true;

    if (!_file.is_open())
        _file.open(_filepath, std::ios::app | std::ios::binary);

    _file.write(_buffer.data(), _buffer.size());
    _file.flush();

    if (_file.fail())
    {
        std::cerr << "Failed to write to AOF file" << std::endl;
        return false;
    }

    _buffer.clear();
    _lastFlushTime = std::chrono::steady_clock::now();
    return true;
}

void Persistence::Tick()
{
    if (_isRewriting || _isBgSavingRdb)
    {
        int exitCode = 0;
        ProcessUtil::Status s = _rewriteProcess.checkStatus(&exitCode);
        if (s == ProcessUtil::Status::EXITED || s == ProcessUtil::Status::SIGNALED || s == ProcessUtil::Status::ERROR)
        {
            if (_isRewriting)
            {
                CleanupRewrite(s == ProcessUtil::Status::EXITED && exitCode == 0);
            }
            else if (_isBgSavingRdb)
            {
                _isBgSavingRdb = false;
            }
        }
    }

    if (_flushIntervalSeconds == 0)
        return; // Handled in Append

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - _lastFlushTime).count();

    if (elapsed >= _flushIntervalSeconds)
    {
        Flush();
    }
}

bool Persistence::Load(std::function<void(const std::vector<std::string>&)> replayCallback)
{
    // Need to read the file
    std::ifstream infile(_filepath, std::ios::binary);
    if (!infile.is_open())
        return true; // File doesn't exist yet

    // Get size
    infile.seekg(0, std::ios::end);
    size_t size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    if (size == 0)
        return true;

    // Read full content
    std::vector<char> content(size);
    infile.read(content.data(), size);

    RespParser parser;
    size_t totalConsumed = 0;

    while (totalConsumed < size)
    {
        RespValue val;
        size_t bytesRead = 0;
        RespStatus status = parser.decode(content.data() + totalConsumed, size - totalConsumed, val, bytesRead);

        if (status == RespStatus::Incomplete)
            break;
        if (status == RespStatus::Invalid)
        {
            std::cerr << "Corrupt AOF file found or parse error." << std::endl;
            // Might want to continue or stop? standard redis stops.
            return false;
        }

        if (val.type == RespType::Array)
        {
            std::vector<std::string> args;
            std::vector<RespValue> respArgs = val.getArray();
            for (const auto& item : respArgs)
            {
                // Arguments are usually bulk strings or simple strings
                // Convert array to args
                args.push_back(item.toString());
            }
            if (!args.empty())
            {
                replayCallback(args);
            }
        }

        totalConsumed += bytesRead;
    }

    return true;
}

void Persistence::BufferForRewrite(const std::vector<RespValue>& args)
{
    // Called only from Append(), which already holds _mutex.
    // _rewriteBuffer is exclusively accessed under _mutex — no extra lock needed.
    if (!_isRewriting)
        return;
    std::string encoded = EncodeCommand(args);
    _rewriteBuffer.push_back(encoded);
}

bool Persistence::StartRewrite(Database& db)
{
    if (_isRewriting || _isBgSavingRdb)
        return false;

    _tmpFilepath = _filepath + ".tmp";
    _isRewriting = true;
    _rewriteBuffer.clear();

    pid_t pid = _rewriteProcess.forkAndRun(
        [&]()
        {
            // Child process
            std::ofstream tmpFile(_tmpFilepath, std::ios::binary);
            if (!tmpFile.is_open())
            {
                _exit(1);
            }

            AofRewriteVisitor visitor(tmpFile);
            db.accept(visitor);
            tmpFile.flush();
            tmpFile.close();
            _exit(0);
        });

    if (pid < 0)
    {
        _isRewriting = false;
        return false;
    }

    return true;
}

bool Persistence::IsRewriting() const
{
    return _isRewriting;
}

void Persistence::HandleRewriteCompletion()
{
    // 1. Append the rewrite buffer to the temp file
    std::ofstream tmpFile(_tmpFilepath, std::ios::app | std::ios::binary);
    if (!tmpFile.is_open())
        return;

    {
        // Use _mutex consistently — _rewriteBuffer is always accessed under _mutex.
        // This eliminates the lock-order inversion that previously risked deadlock.
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& cmd : _rewriteBuffer)
            tmpFile.write(cmd.data(), cmd.size());
        _rewriteBuffer.clear();
    }
    tmpFile.flush();
    tmpFile.close();

    // 2. Atomically replace the old AOF with the new one
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_file.is_open())
            _file.close();

        if (rename(_tmpFilepath.c_str(), _filepath.c_str()) == 0)
            _file.open(_filepath, std::ios::app | std::ios::binary);
        else
            _file.open(_filepath, std::ios::app | std::ios::binary);
    }
}

void Persistence::CleanupRewrite(bool success)
{
    if (success)
    {
        HandleRewriteCompletion();
    }
    else
    {
        // Just remove temp file
        remove(_tmpFilepath.c_str());
    }
    _isRewriting = false;
    _rewriteBuffer.clear();
}

bool Persistence::SaveRdb(Database& db)
{
    std::ofstream rdbFile(_rdbFilepath, std::ios::binary);
    if (!rdbFile.is_open())
        return false;

    RdbVisitor visitor(rdbFile);
    db.accept(visitor);
    visitor.writeEOF();

    rdbFile.flush();
    rdbFile.close();
    return true;
}

bool Persistence::BgSaveRdb(Database& db)
{
    if (_isRewriting || _isBgSavingRdb)
        return false;

    std::string tmpRdb = _rdbFilepath + ".tmp";
    _isBgSavingRdb = true;

    pid_t pid = _rewriteProcess.forkAndRun(
        [&]()
        {
            std::ofstream rdbFile(tmpRdb, std::ios::binary);
            if (!rdbFile.is_open())
                _exit(1);

            RdbVisitor visitor(rdbFile);
            db.accept(visitor);
            visitor.writeEOF();

            rdbFile.flush();
            rdbFile.close();

            rename(tmpRdb.c_str(), _rdbFilepath.c_str());
            _exit(0);
        });

    if (pid < 0)
    {
        _isRewriting = false;
        return false;
    }

    return true;
}

bool Persistence::LoadRdb(Database& db)
{
    std::ifstream inFile(_rdbFilepath, std::ios::binary);
    if (!inFile.is_open())
        return true; // Normal if it doesn't exist

    char magic[6];
    if (!inFile.read(magic, 6) || std::strncmp(magic, "RDB001", 6) != 0)
    {
        std::cerr << "Invalid RDB file signature" << std::endl;
        return false;
    }

    auto readLength = [&](uint64_t& out) -> bool
    {
        char buf[8];
        if (!inFile.read(buf, 8))
            return false;
        out = 0;
        for (int i = 0; i < 8; ++i)
        {
            out |= (static_cast<uint64_t>(static_cast<unsigned char>(buf[i])) << (i * 8));
        }
        return true;
    };

    auto readString = [&](std::string& out) -> bool
    {
        uint64_t len = 0;
        if (!readLength(len))
            return false;
        out.resize(len);
        if (len > 0)
        {
            if (!inFile.read(out.data(), len))
                return false;
        }
        return true;
    };

    while (true)
    {
        uint8_t type;
        if (!inFile.read(reinterpret_cast<char*>(&type), 1))
            break;

        if (type == 0 /* TYPE_EOF */)
            break;

        int64_t expiresAt = -1;
        char expBuf[8];
        if (!inFile.read(expBuf, 8))
            break;

        uint64_t exp_u64 = 0;
        for (int i = 0; i < 8; ++i)
        {
            exp_u64 |= (static_cast<uint64_t>(static_cast<unsigned char>(expBuf[i])) << (i * 8));
        }
        expiresAt = static_cast<int64_t>(exp_u64);

        std::string key;
        if (!readString(key))
            break;

        int64_t ttlMs = -1;
        if (expiresAt > 0)
        {
            ttlMs = expiresAt - currentTimeMs();
            if (ttlMs <= 0)
                ttlMs = 1; // Expire almost immediately if already past
        }

        if (type == 1 /* TYPE_STRING */)
        {
            std::string val;
            if (!readString(val))
                break;
            db.set(key, val, ttlMs);
        }
        else if (type == 2 /* TYPE_LIST */)
        {
            uint64_t len = 0;
            if (!readLength(len))
                break;
            for (uint64_t i = 0; i < len; ++i)
            {
                std::string item;
                if (!readString(item))
                    break;
                db.rpush(key, item);
            }
            if (ttlMs > 0)
                db.expire(key, ttlMs);
        }
        else if (type == 3 /* TYPE_SET */)
        {
            uint64_t len = 0;
            if (!readLength(len))
                break;
            for (uint64_t i = 0; i < len; ++i)
            {
                std::string item;
                if (!readString(item))
                    break;
                db.sadd(key, item);
            }
            if (ttlMs > 0)
                db.expire(key, ttlMs);
        }
        else if (type == 4 /* TYPE_HASH */)
        {
            uint64_t len = 0;
            if (!readLength(len))
                break;
            for (uint64_t i = 0; i < len; ++i)
            {
                std::string field, val;
                if (!readString(field) || !readString(val))
                    break;
                db.hset(key, field, val);
            }
            if (ttlMs > 0)
                db.expire(key, ttlMs);
        }
        else if (type == 5 /* TYPE_ZSET */)
        {
            uint64_t len = 0;
            if (!readLength(len))
                break;
            for (uint64_t i = 0; i < len; ++i)
            {
                std::string member;
                if (!readString(member))
                    break;

                uint64_t scoreRaw = 0;
                if (!readLength(scoreRaw))
                    break;
                double score = 0;
                std::memcpy(&score, &scoreRaw, 8);

                db.zadd(key, score, member);
            }
            if (ttlMs > 0)
                db.expire(key, ttlMs);
        }
    }

    return true;
}
