#include "TcpConnection.h"

#include <arpa/inet.h> // inet_addr()
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h> // read(), write(), close()

#include "ITcpConnection.h"
#include "constants.h"
#include "fd_util.h"
#include "frame_header.h"

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

    if (_concurencyType == ConcurrencyType::EventBased)
        return PrepareEventBased();

    std::thread t(&TcpConnection::RunThread, this);
    t.detach();

    {
        std::unique_lock<std::mutex> lock(_cv_mutex);
        if (!_cv.wait_for(lock, std::chrono::seconds(THREAD_START_TIMEOUT_SECONDS),
                          [this] { return _state == ClientState::Running; }))
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
    std::lock_guard<std::mutex> lock(_outgoingMutex);
    if (_outgoing.empty())
    {
        _connWrite = false;
        return;
    }

    // Get a contiguous view of the outgoing data.
    // If it spans segments, we'll send it in chunks across multiple calls or use writev.
    // Simplifying for now: send what's contiguous.
    std::string_view outgoing = _outgoing.peek();
    ssize_t bytesWritten = write(_socketfd, outgoing.data(), outgoing.size());

    if (bytesWritten < 0)
    {
        printf("TcpConnection handleWrite: write failed, errno=%d (EAGAIN=%d)\n", errno, EAGAIN);
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            _connClose = true;
    }
    else if (bytesWritten > 0)
    {
        printf("TcpConnection handleWrite: wrote %zd bytes to fd %d\n", bytesWritten, _socketfd);
        _outgoing.consume(bytesWritten);
    }

    if (_outgoing.empty())
        _connWrite = false;
}

void TcpConnection::handleRead()
{
    // ZERO-COPY HOOK: Get a direct pointer to the memory segment
    // We hint with TCP_MAX_MESSAGE_SIZE to suggest a good segment size
    size_t writableSize = 0;
    void* writePtr = _incoming.getWritePtr(TCP_MAX_MESSAGE_SIZE, writableSize);

    ssize_t rv = read(_socketfd, writePtr, writableSize);
    if (rv <= 0)
    {
        if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        _connClose = true;
        return;
    }

    // Commit the data back to the segmented buffer
    _incoming.commitWrite(rv);

    while (!_incoming.empty())
    {
        // Try to process as much as we can.
        // We peek as far as we can to avoid unnecessary cross-segment copies in the parser.
        std::string_view data = _incoming.peekContiguous(_incoming.size());
        size_t consumed = _owner->OnMessageReceive(data.data(), data.size());

        if (consumed > 0)
        {
            _incoming.consume(consumed);
        }
        else
        {
            break;
        }
    }
}

bool TcpConnection::PrepareEventBased()
{
    if (_state != ClientState::OwnerSet)
        return false;

    if (fd_util::fd_set_nonblock(_socketfd) == false)
        return false;

    _connWrite = false;
    _connClose = false;

    _state = ClientState::Running;
    return true;
}

void TcpConnection::SetOwner(ITcpConnection* owner)
{
    if (_state != ClientState::Initialized)
        return;

    _owner = owner;
    _state = ClientState::OwnerSet;
}

void TcpConnection::RunThread() // thread based
{
    if (_state != ClientState::OwnerSet)
        return;

    _state = ClientState::Running;
    _cv.notify_one();

    while (_state == ClientState::Running)
    {
        handleRead();
    };

    _owner->OnDisconnect();
    close(_socketfd);
    _state = ClientState::Stopped;
}

void TcpConnection::Stop()
{
    if (_state == ClientState::Running)
    {
        if (_concurencyType == ConcurrencyType::EventBased)
        {
            if (_socketfd != -1)
                close(_socketfd);
            _state = ClientState::Stopped;
            _connClose = true;
            if (_owner)
                _owner->OnDisconnect();
        }
        else
            _state = ClientState::StopRequested;
    }
}

void TcpConnection::DetachSocket()
{
    _socketfd = -1;
    _state = ClientState::Stopped;
};

void TcpConnection::Send(const char* buffer, ssize_t length)
{
    if (_state != ClientState::Running)
        return;

    _outgoing.append(buffer, length);

    if (_concurencyType == ConcurrencyType::EventBased)
    {
        _connWrite = true;
    }
    else
    {
        // Thread-based: try to flush until outgoing is empty or socket blocks
        while (!_outgoing.empty())
        {
            std::string_view out = _outgoing.peek();
            ssize_t bytesWritten = write(_socketfd, out.data(), out.size());
            if (bytesWritten <= 0)
            {
                if (bytesWritten < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    break;
                _connClose = true;
                break;
            }
            _outgoing.consume(bytesWritten);
        }
    }
};

void TcpConnection::Enqueue(const char* buffer, ssize_t length)
{
    if (_state != ClientState::Running)
        return;
    std::lock_guard<std::mutex> lock(_outgoingMutex);
    _outgoing.append(buffer, length);
    _connWrite = true;
}

TcpConnection::~TcpConnection()
{
    if (_state == ClientState::Running) // make sure the socket is closed
        Stop();
}