#pragma once

#include "constants.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "header.h"

class ITcpConnection;



enum class OwnerType
{
    Server,
    Client
};

enum class ClientState
{
    Uninitialized,
    Initialized,
    OwnerSet,
    Running,
    StopRequested,
    Stopped
};

class TcpConnection
{
    ITcpConnection* _owner;
    OwnerType _ownerType;

    std::condition_variable _cv;
    std::mutex              _cv_mutex;

    std::atomic<ClientState> _state;

public:
    int _socket;
    int _port;
    char _ip[IP_NAME_LENGTH];
    char _buffer[TCP_MAX_MESSAGE_SIZE];

    ~TcpConnection();

    static TcpConnection* CreateFromPortAndIp(int port, const char* ip);
    static TcpConnection* CreateFromSocket(int socket);

    void SetOwner(ITcpConnection* owner);

    bool Init();
    void Stop();
    void Send(const char* buffer, ssize_t length);
    bool IsRunning();

private:
    TcpConnection();
    void RunThread();
};