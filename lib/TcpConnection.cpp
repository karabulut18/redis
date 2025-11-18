#include "TcpConnection.h"

#include <string.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h> // read(), write(), close()
#include <arpa/inet.h> // inet_addr()
#include <thread>

#include "ITcpConnection.h"
#include "constants.h"
#include "header.h"


bool TcpConnection::IsRunning()
{
    return _state == ClientState::Running;
};

TcpConnection* TcpConnection::CreateFromPortAndIp(int port, const char* ip)
{
    TcpConnection* instance = new TcpConnection();
    instance->_port = port;
    memcpy(instance->_ip, ip, IP_NAME_LENGTH);
    instance->_ownerType = OwnerType::Client;
    instance->_state = ClientState::Initialized;
    return instance;
}

TcpConnection* TcpConnection::CreateFromSocket(int socket)
{
    TcpConnection* instance = new TcpConnection();
    instance->_socket = socket;
    instance->_ownerType = OwnerType::Server;
    instance->_state = ClientState::Initialized;
    return instance;
}

TcpConnection::TcpConnection()
{
    _socket = -1;
    _port = -1;
    _owner = nullptr;
    memset(_ip, 0, IP_NAME_LENGTH);
    memset(_buffer, 0, TCP_MAX_MESSAGE_SIZE);
    _state = ClientState::Uninitialized;
}

bool TcpConnection::Init()
{
    if (_state != ClientState::OwnerSet)
        return false;

    if (_ownerType == OwnerType::Client)
    {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        if (_socket == -1)
            return false;

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(_port);
        address.sin_addr.s_addr = inet_addr(_ip);

        if (connect(_socket, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            close(_socket);
            return false;
        }
    }

    std::thread t(&TcpConnection::RunThread, this);
    t.detach();

    {
        std::unique_lock<std::mutex> lock(_cv_mutex);
        if(!_cv.wait_for(lock, std::chrono::seconds(THREAD_START_TIMEOUT_SECONDS), [this]{return _state  == ClientState::Running;}))
            return false;
    }

    return true;
}

void TcpConnection::SetOwner(ITcpConnection* owner)
{
    if(_state != ClientState::Initialized)
        return;

    _owner = owner;
    _state = ClientState::OwnerSet;
}

void TcpConnection::RunThread()
{
    if (_state != ClientState::OwnerSet)
        return;

    _state = ClientState::Running;
    _cv.notify_one();

    while (_state == ClientState::Running)
    {
        memset(_buffer, 0, TCP_MAX_MESSAGE_SIZE);
        ssize_t messageLength = read(_socket, _buffer, TCP_MAX_MESSAGE_SIZE);
        if (messageLength > 0)
            _owner->OnMessage(_buffer, messageLength);
        else if (messageLength <= 0)
            break;
    };

    _owner->OnDisconnect();
    close(_socket);
    _state = ClientState::Stopped;
}

void TcpConnection::Stop()
{
    if (_state == ClientState::Running)
    {
        _state = ClientState::StopRequested;
    }
};

void TcpConnection::Send(const char* buffer, ssize_t length)
{
    if (_state != ClientState::Running)
        return;

    write(_socket, buffer, length);
};

TcpConnection::~TcpConnection()
{
    if(_state == ClientState::Running)
        Stop();
}