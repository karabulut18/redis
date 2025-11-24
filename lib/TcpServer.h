#pragma once

#include "Error.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <map>
#include <netinet/in.h>
#include "ConcurrencyType.h"

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
    std::mutex              _cv_mutex;

    ConcurrencyType _concurrencyType = ConcurrencyType::ThreadBased;
    std::map<int, TcpConnection*> _connectionsBySocketfds;

    Error   _error;

    std::vector<struct pollfd> _pollArgs;
public:
    int     _socketfd;
    int     _port;

    TcpServer(ITcpServer* owner, int port);
    ~TcpServer();

    void SetConcurrencyType(ConcurrencyType type);
    bool Init();
    void Stop();
    bool IsRunning();
    void RemoveClient(int id);

private:
    void RunThread();
    void EventBased();
    void ThreadBased();
    void CleanUp();

    void handleAccept();
};