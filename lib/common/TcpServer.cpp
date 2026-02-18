#include "TcpServer.h"
#include <errno.h>

#include "ITcpServer.h"
#include "TcpConnection.h"
#include "constants.h"
#include "fd_util.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h> // read(), write(), close()

bool TcpServer::IsRunning()
{
    return _state == ServerState::Running;
};

bool TcpServer::Init()
{
    if (_state != ServerState::Uninitialized)
    {
        return false;
    }

    _socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_socketfd == -1)
    {
        _error.Set(errno, "socket creation");
        return false;
    };
    int val = 1;
    setsockopt(_socketfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    if (_concurrencyType == ConcurrencyType::EventBased)
    {
        if (!fd_util::fd_set_nonblock(_socketfd))
        {
            _error.Set(errno, "fd_set_nonblock");
            return false;
        }

        // Initialize Wakeup Pipe
        if (pipe(_wakeupPipe) == -1)
        {
            _error.Set(errno, "pipe creation");
            return false;
        }
        fd_util::fd_set_nonblock(_wakeupPipe[0]);
        fd_util::fd_set_nonblock(_wakeupPipe[1]);
    }

    _serverAddress.sin_family = AF_INET;
    _serverAddress.sin_addr.s_addr = htonl(0);
    _serverAddress.sin_port = htons(_port);

    if (bind(_socketfd, (struct sockaddr*)&_serverAddress, sizeof(_serverAddress)) == -1)
    {
        _error.Set(errno, "bind");
        return false;
    };

    if (listen(_socketfd, SOMAXCONN) == -1)
    {
        _error.Set(errno, "listen");
        return false;
    };

    _state = ServerState::Initialized;

    std::thread serverThread(&TcpServer::RunThread, this);
    serverThread.detach();

    {
        std::unique_lock<std::mutex> lock(_cv_mutex);
        if (!_cv.wait_for(lock, std::chrono::seconds(THREAD_START_TIMEOUT_SECONDS),
                          [this] { return _state == ServerState::Running; }))
        {
            _state = ServerState::Stopped;
            _error.Set(ETIMEDOUT, "Server thread failed to start within the timeout period.");
            return false;
        }
    }

    return true;
};

void TcpServer::RunThread()
{

    if (_state != ServerState::Initialized)
        return;

    _state = ServerState::Running;
    _cv.notify_one();

    if (_concurrencyType == ConcurrencyType::EventBased)
        EventBased();
    else if (_concurrencyType == ConcurrencyType::ThreadBased)
        ThreadBased();

    CleanUp();
};

void TcpServer::SetConcurrencyType(ConcurrencyType type)
{
    _concurrencyType = type;
}

void TcpServer::EventBased()
{
    while (_state == ServerState::Running)
    {
        _pollArgs.clear();

        struct pollfd server_pfd = {_socketfd, POLLIN, 0};
        _pollArgs.push_back(server_pfd);

        for (std::pair<int, TcpConnection*> iter : _connectionsBySocketfds)
        {
            TcpConnection* conn = iter.second;

            struct pollfd client_pfd = {conn->_socketfd, POLLIN, 0}; // always poll for reads

            if (conn->_connWrite) // poll for write only if write is set
                client_pfd.events |= POLLOUT;
            _pollArgs.push_back(client_pfd);
        }

        // Add Wakeup Pipe Read End
        struct pollfd wakeup_pfd = {_wakeupPipe[0], POLLIN, 0};
        _pollArgs.push_back(wakeup_pfd);

        int returnValue = poll(_pollArgs.data(), (nfds_t)_pollArgs.size(), -1);

        if (returnValue < 0 && errno == EINTR)
            continue;

        if (returnValue < 0)
        {
            _error.Set(errno, "poll");
            break;
        }

        if (_pollArgs[0].revents & POLLIN) // first one is for listening socket
            handleAccept();

        // Check Wakeup Pipe (last element)
        if (_pollArgs.back().revents & POLLIN)
        {
            // Drain the pipe — connections have already been written to directly
            // by QueueResponse via Enqueue(); draining just lets poll() proceed.
            char buf[128];
            while (read(_wakeupPipe[0], buf, sizeof(buf)) > 0)
                ;
        }

        // connections are indexes 1 to size-2 (if wakeup is last)
        // Adjust loop limit
        size_t limit = _pollArgs.size() - 1;

        for (size_t i = 1; i < limit; i++)
        {
            uint32_t ready = _pollArgs[i].revents;
            TcpConnection* conn = _connectionsBySocketfds[_pollArgs[i].fd];
            if (ready & POLLIN)
                conn->handleRead();
            if (ready & POLLOUT)
                conn->handleWrite();

            if ((ready & POLLERR) || (ready & POLLHUP) || conn->closeRequested())
            {
                conn->Stop();
                delete conn;
                _connectionsBySocketfds.erase(_pollArgs[i].fd);
            }
        }
    }
}

void TcpServer::handleAccept()
{
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int clientSocket = accept(_socketfd, (struct sockaddr*)&client_addr, &addrlen);
    if (clientSocket == -1)
    {
        _error.Set(errno, "accept connection");
    }
    else
    {
        getpeername(clientSocket, (struct sockaddr*)&client_addr, &addrlen);

        TcpConnection* connection = TcpConnection::CreateFromSocket(clientSocket);

        // fix the logical mistake here
        ITcpConnection* connectionOwner = _owner->AcceptConnection(connection->_socketfd, connection);
        connection->SetOwner(connectionOwner);
        if (connection->Init(_concurrencyType))
            _connectionsBySocketfds.insert({connection->_socketfd, connection});
        else
            delete connection;
    }
}

void TcpServer::ThreadBased()
{
    while (_state == ServerState::Running)
    {
        // Poll the listen socket so we can check _state periodically
        // and accept new connections promptly without blocking indefinitely.
        struct pollfd pfd = {_socketfd, POLLIN, 0};
        int ready = poll(&pfd, 1, 100 /*ms*/);
        if (ready > 0 && (pfd.revents & POLLIN))
            handleAccept();
    }
}

void TcpServer::CleanUp()
{
    TcpConnection* conn;
    for (std::pair<int, TcpConnection*> iter : _connectionsBySocketfds)
    {
        conn = iter.second;
        conn->Stop();
        delete conn;
    }
    _connectionsBySocketfds.clear();
    close(_socketfd);
}

void TcpServer::Stop()
{
    if (_state == ServerState::Running)
    {
        _state = ServerState::StopRequested;
    }
};

TcpServer::TcpServer(ITcpServer* owner, int port)
{
    _owner = owner;
    _port = port;
    _state = ServerState::Uninitialized;
};

TcpServer::~TcpServer()
{
    if (_state == ServerState::Running)
        Stop();
};

void TcpServer::RemoveClient(int id)
{
    std::map<int, TcpConnection*>::iterator it = _connectionsBySocketfds.find(id);
    if (it != _connectionsBySocketfds.end())
    {
        delete it->second;
        _connectionsBySocketfds.erase(it);
    }
}

void TcpServer::QueueResponse(int clientSocketFd, const std::string& data)
{
    if (_concurrencyType != ConcurrencyType::EventBased)
    {
        // ThreadBased: send directly on the calling thread via the connection.
        auto it = _connectionsBySocketfds.find(clientSocketFd);
        if (it != _connectionsBySocketfds.end())
            it->second->Send(data.c_str(), data.size());
        return;
    }

    // EventBased: write directly into the connection's outgoing buffer
    // (thread-safe via Enqueue), then wake the I/O thread via the pipe.
    {
        auto it = _connectionsBySocketfds.find(clientSocketFd);
        if (it != _connectionsBySocketfds.end())
            it->second->Enqueue(data.c_str(), data.size());
    }

    char buf = 'x';
    write(_wakeupPipe[1], &buf, 1);
}