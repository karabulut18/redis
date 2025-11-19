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
        header hdr(0,0);
        ssize_t bytesRead = read(_socket, &hdr, sizeof(hdr));

        if(bytesRead < sizeof(header))
            break;

        if(hdr.length > TCP_MAX_MESSAGE_SIZE)
            break;

        memcpy(_buffer, &hdr, sizeof(header));
        ssize_t bodyLength = hdr.length - sizeof(header);
        ssize_t bodyBytesToRead = 0;

        if(bodyLength < 0)
            break;

        while(bytesRead < bodyLength)
        {
            bytesRead = read(_socket, _buffer + sizeof(header) + bodyBytesToRead, bodyLength - bodyBytesToRead);
            if(bytesRead < 0)
                break;
            bodyBytesToRead += bytesRead;
        }
 
        if(bytesRead < 0)
            break;

        _owner->OnMessage(_buffer, hdr.length);
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

    ssize_t bytesSent = 0;

    while(bytesSent < length)
    {
        ssize_t bytesToWrite = length - bytesSent;
        ssize_t bytesWritten = write(_socket, buffer + bytesSent, bytesToWrite);
        if(bytesWritten < 0)
            break;
        bytesSent += bytesWritten;
    }
};

TcpConnection::~TcpConnection()
{
    if(_state == ClientState::Running)
        Stop();
}