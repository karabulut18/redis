#pragma once

#include "Error.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <map>
#include <netinet/in.h>

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

    int _clientIndex = 0;
    std::map<int, TcpConnection*> _clientsById;

    Error   _error;
public:
    int     _socket;
    int     _port;

    TcpServer(ITcpServer* owner, int port);
    ~TcpServer();

    bool Init();
    void Stop();
    bool IsRunning();

private:
    void RunThread();
    void CleanUp();
};