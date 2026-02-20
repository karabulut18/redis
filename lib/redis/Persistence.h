#pragma once

#include "../common/ProcessUtil.h"
#include "RespParser.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class Database;

class Persistence
{
public:
    Persistence(const std::string& filepath);
    ~Persistence();

    // Appends a command to the internal buffer.
    // Thread-safe.
    void Append(const std::vector<RespValue>& args);

    // Flushes the internal buffer to disk.
    // Returns true if successful.
    bool Flush();

    // --- AOF Persistence ---

    // Loads the AOF file and replays commands.
    // Callback is called for each parsed command args.
    // Returns true if successful.
    bool Load(std::function<void(const std::vector<std::string>&)> replayCallback);

    // Initialises the background AOF rewrite process.
    // Returns true if the child process was started successfully.
    bool StartRewrite(Database& db);

    // Returns true if a rewrite is currently in progress.
    bool IsRewriting() const;

    // Buffers commands for the rewrite completion.
    void BufferForRewrite(const std::vector<RespValue>& args);

    // --- RDB Persistence ---
    // Synchronous save to RDB format
    bool SaveRdb(Database& db);

    // Asynchronous save to RDB format
    bool BgSaveRdb(Database& db);

    // Loads from RDB format. Should be called before AOF Load.
    bool LoadRdb(Database& db);

    // Configuration
    void SetFlushInterval(int64_t seconds);
    int64_t GetFlushInterval() const;

    // Checks if it's time to flush and flushes if necessary.
    // Should be called periodically.
    void Tick();

private:
    std::string _filepath;    // AOF filepath
    std::string _rdbFilepath; // RDB filepath
    std::ofstream _file;
    std::vector<char> _buffer; // In-memory buffer for AOF
    std::mutex _mutex;

    std::atomic<int64_t> _flushIntervalSeconds;
    std::chrono::steady_clock::time_point _lastFlushTime;

    // Helper to encode command to RESP
    std::string EncodeCommand(const std::vector<RespValue>& args);

    // State for AOF Rewrite
    ProcessUtil _rewriteProcess;
    std::string _tmpFilepath;
    std::vector<std::string> _rewriteBuffer;
    // _rewriteBuffer is protected by _mutex (consistent lock ordering, no deadlock risk)
    std::atomic<bool> _isRewriting{false};
    std::atomic<bool> _isBgSavingRdb{false};

    void HandleRewriteCompletion();
    void CleanupRewrite(bool success);
};
