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
#include "fd_util.h"

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
    instance->_socketfd = socket;
    instance->_ownerType = OwnerType::Server;
    instance->_state = ClientState::Initialized;
    return instance;
}

TcpConnection::TcpConnection()
{
    _socketfd = -1;
    _port = -1;
    _owner = nullptr;
    memset(_ip, 0, IP_NAME_LENGTH);
    _state = ClientState::Uninitialized;
}


bool TcpConnection::Init(ConcurrencyType type)
{
    if (_state != ClientState::OwnerSet)
        return false;

    _concurencyType = type;

    if (_ownerType == OwnerType::Client)
    {
        _socketfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_socketfd == -1)
            return false;

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(_port);
        address.sin_addr.s_addr = inet_addr(_ip);

        if (connect(_socketfd, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            close(_socketfd);
            return false;
        }
    }

    if(_concurencyType == ConcurrencyType::EventBased)
        return PrepareEventBased();

    std::thread t(&TcpConnection::RunThread, this);
    t.detach();

    {
        std::unique_lock<std::mutex> lock(_cv_mutex);
        if(!_cv.wait_for(lock, std::chrono::seconds(THREAD_START_TIMEOUT_SECONDS), [this]{return _state  == ClientState::Running;}))
            return false;
    }

    return true;
}

bool TcpConnection::closeRequested()
{
    return _connClose;
}

void TcpConnection::handleWrite()
{
    if(!_outgoing.canConsume())
        return;

    m_size_t frameSize = _outgoing.peekFrameSize();
    if(frameSize == 0)
        return;

    const char* outgoing = _outgoing.peekFramePtr();
    if(outgoing == nullptr)
        return;

    ssize_t rv = write(_socketfd, outgoing, frameSize);
    if (rv < 0) {
        _connClose= true;    // error handling
        return;
    }
    _outgoing.consume(rv);
}

void TcpConnection::handleRead()
{
    ssize_t rv = read(_socketfd, _buffer, sizeof(_buffer));
    if (rv <= 0) 
    {  // handle IO error (rv < 0) or EOF (rv == 0)
        _connClose = true;
        return;
    }
    // 2. Add new data to the `Conn::incoming` buffer.
    _incoming.append(_buffer, (m_size_t)rv);

    if(_incoming.canConsume())
    {
        m_size_t frameSize = _incoming.peekFrameSize();
        if(frameSize == 0)
            return;

        const char* data = _incoming.peekFramePtr();
        if(data == nullptr)
            return;
        
        _owner->OnMessage(data, frameSize);

        _incoming.consume(frameSize);
    }
}

bool TcpConnection::PrepareEventBased()
{
    if(_state != ClientState::OwnerSet)
        return false;

    if(fd_util::fd_set_nonblock(_socketfd) == false) 
        return false;

    _connRead   = true; // read first messaage
    _connWrite  = false;
    _connClose  = false;
    
    _state = ClientState::Running;
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
        // this part is specialized for the header
        header hdr(0,0);
        ssize_t bytesRead = read(_socketfd, &hdr, sizeof(hdr));

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
            bytesRead = read(_socketfd, _buffer + sizeof(header) + bodyBytesToRead, bodyLength - bodyBytesToRead);
            if(bytesRead < 0)
                break;
            bodyBytesToRead += bytesRead;
        }
 
        if(bytesRead < 0)
            break;

        _owner->OnMessage(_buffer, hdr.length);
    };

    _owner->OnDisconnect();
    close(_socketfd);
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
        ssize_t bytesWritten = write(_socketfd, buffer + bytesSent, bytesToWrite);
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