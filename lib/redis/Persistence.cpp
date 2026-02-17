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
    {
        BufferForRewrite(args);
    }

    std::string encoded = EncodeCommand(args);
    _buffer.insert(_buffer.end(), encoded.begin(), encoded.end());

    // Optimisation: If interval is 0 (Always), flush immediately
    if (_flushIntervalSeconds == 0)
    {
        if (_file.is_open())
        {
            _file.write(encoded.data(), encoded.size());
            _file.flush();
        }
        else
        {
            _file.open(_filepath, std::ios::app | std::ios::binary);
            _file.write(encoded.data(), encoded.size());
            _file.flush();
        }
        // Remove from buffer since we just wrote declaration
        // Logic fix: The buffer contains previous stuff too?
        // If interval is 0, we can assume buffer is empty usually,
        // or we flush everything.
        // Let's just defer to Flush() logic but call it specially.
        // But Flush() takes lock. We have lock.
        // So we need internal flush or just write explicitly.

        // Since we already appended to buffer, let's clear buffer.
        // Actually, if we wrote to file, we should clear the specific part of buffer?
        // No, simplest is: buffer grows, if interval==0, write buffer to file and clear buffer.
        // But we just appended `encoded`.
        // Let's correct this logic:

        // Re-open if needed (though it should be open)
        if (!_file.is_open())
            _file.open(_filepath, std::ios::app | std::ios::binary);

        // Write EVERYTHING in buffer (in case user switched from non-0 to 0)
        _file.write(_buffer.data(), _buffer.size());
        _file.flush();
        _buffer.clear();
        _lastFlushTime = std::chrono::steady_clock::now();
    }
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
    std::lock_guard<std::mutex> lock(_rewriteMutex);
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
        std::lock_guard<std::mutex> lock(_rewriteMutex);
        for (const auto& cmd : _rewriteBuffer)
        {
            tmpFile.write(cmd.data(), cmd.size());
        }
        _rewriteBuffer.clear();
    }
    tmpFile.flush();
    tmpFile.close();

    // 2. Atomically replace the old AOF with the new one
    // Close the current file first
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_file.is_open())
            _file.close();

        if (rename(_tmpFilepath.c_str(), _filepath.c_str()) == 0)
        {
            // Re-open the new file
            _file.open(_filepath, std::ios::app | std::ios::binary);
        }
        else
        {
            // If rename failed, try to re-open old file at least
            _file.open(_filepath, std::ios::app | std::ios::binary);
        }
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
