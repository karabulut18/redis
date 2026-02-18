#include "Persistence.h"
#include "AofRewriteVisitor.h"
#include "Database.h"
#include "RespParser.h"
#include <cstdio>
#include <iostream>
#include <unistd.h>

Persistence::Persistence(const std::string& filepath) : _filepath(filepath), _flushIntervalSeconds(1) // Default 1s
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
    if (_isRewriting)
    {
        int exitCode = 0;
        ProcessUtil::Status s = _rewriteProcess.checkStatus(&exitCode);
        if (s == ProcessUtil::Status::EXITED || s == ProcessUtil::Status::SIGNALED || s == ProcessUtil::Status::ERROR)
        {
            CleanupRewrite(s == ProcessUtil::Status::EXITED && exitCode == 0);
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
    if (_isRewriting)
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
