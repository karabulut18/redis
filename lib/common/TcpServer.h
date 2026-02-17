#pragma once

#include "ConcurrencyType.h"
#include "Error.h"
#include "LockFreeRingBuffer.h"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <string>

class ITcpServer;
class TcpConnection;

enum class ServerState
{
    Uninitialized,
    Initialized,
    Running,
    StopRequested,
    Stopped
};

class TcpServer
{
    struct sockaddr_in _serverAddress = {};

    ITcpServer* _owner;

    std::atomic<ServerState> _state;

    std::condition_variable _cv;
    std::mutex _cv_mutex;

protected:
    ConcurrencyType _concurrencyType = ConcurrencyType::ThreadBased;
    std::map<int, TcpConnection*> _connectionsBySocketfds;

    Error _error;

    std::vector<struct pollfd> _pollArgs;

public:
    int _socketfd;
    struct PendingResponse
    {
        int clientSocketFd;
        std::string data;
    };

    LockFreeRingBuffer<PendingResponse>* _responseQueue;
    int _wakeupPipe[2]; // [0] = read, [1] = write

public:
    int _port;
    TcpServer(ITcpServer* owner, int port);
    ~TcpServer();

    bool Init();
    void RunThread();
    void Stop();
    bool IsRunning(); // This was removed in the instruction's public section, but not explicitly removed. Keeping it.

    void SetConcurrencyType(ConcurrencyType type);
    void RemoveClient(int id);

    // Thread-safe method to queue response from logic thread
    void QueueResponse(int clientSocketFd, const std::string& data);

protected: // Changed from private to protected as per instruction
    // void RunThread(); // Removed duplicate declaration to maintain syntactic correctness
    void EventBased();
    void ThreadBased();
    void CleanUp();

    void handleAccept();
};