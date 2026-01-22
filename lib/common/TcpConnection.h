#pragma once

#include "constants.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "frame_header.h"
#include "DynamicBuffer.h"
#include "ConcurrencyType.h"

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
    OwnerType       _ownerType;
    ConcurrencyType _concurencyType = ConcurrencyType::ThreadBased;

    std::condition_variable _cv;
    std::mutex              _cv_mutex;

    std::atomic<ClientState> _state;

    char _buffer[TCP_MAX_MESSAGE_SIZE];
public:
    int _port;
    char _ip[IP_NAME_LENGTH];

    int _socketfd;
    bool _connWrite = false;
    bool _connClose = false;

    DynamicBuffer _incoming;
    DynamicBuffer _outgoing;

    ~TcpConnection();

    static TcpConnection* CreateFromPortAndIp(int port, const char* ip);
    static TcpConnection* CreateFromSocket(int socket);

    void SetOwner(ITcpConnection* owner);

    bool Init(ConcurrencyType type);
    void Stop();
    void Send(const char* buffer, ssize_t length);
    bool IsRunning();


    void handleWrite();
    void handleRead();
    bool closeRequested();

private:
    TcpConnection();
    void RunThread();
    bool PrepareEventBased();
};