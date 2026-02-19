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

        // ── Phase 1: build poll list under shared lock ──────────────────────
        {
            std::shared_lock<std::shared_mutex> lock(_connMapMutex);
            struct pollfd server_pfd = {_socketfd, POLLIN, 0};
            _pollArgs.push_back(server_pfd);

            for (const auto& [fd, conn] : _connectionsBySocketfds)
            {
                struct pollfd client_pfd = {conn->_socketfd, POLLIN, 0};
                if (conn->_connWrite)
                    client_pfd.events |= POLLOUT;
                _pollArgs.push_back(client_pfd);
            }
        }

        // ── Phase 2: wakeup pipe at the end ────────────────────────────────
        struct pollfd wakeup_pfd = {_wakeupPipe[0], POLLIN, 0};
        _pollArgs.push_back(wakeup_pfd);

        // ── Phase 3: poll — no lock held ───────────────────────────────────
        int returnValue = poll(_pollArgs.data(), (nfds_t)_pollArgs.size(), -1);

        if (returnValue < 0 && errno == EINTR)
            continue;

        if (returnValue < 0)
        {
            _error.Set(errno, "poll");
            break;
        }

        // handleAccept moved to the end of the loop cycle to avoid contamination of poll results on FD reuse.

        // Drain wakeup pipe
        if (_pollArgs.back().revents & POLLIN)
        {
            char buf[128];
            while (read(_wakeupPipe[0], buf, sizeof(buf)) > 0)
                ;
        }

        // ── Phase 4: handle I/O — no lock held (handleRead can re-enter QueueResponse) ─
        size_t limit = _pollArgs.size() - 1;
        std::vector<int> toClose;

        for (size_t i = 1; i < limit; i++)
        {
            int fd = _pollArgs[i].fd;
            uint32_t ready = _pollArgs[i].revents;

            // Look up the connection under a brief shared lock
            TcpConnection* conn = nullptr;
            {
                std::shared_lock<std::shared_mutex> lock(_connMapMutex);
                auto it = _connectionsBySocketfds.find(fd);
                if (it == _connectionsBySocketfds.end())
                    continue; // already removed
                conn = it->second;
            }

            // I/O without any lock held — handleRead may call QueueResponse
            if (ready & POLLIN)
                conn->handleRead();
            if (ready & POLLOUT)
                conn->handleWrite();

            if ((ready & POLLERR) || (ready & POLLHUP) || conn->closeRequested())
                toClose.push_back(fd);
        }

        // ── Phase 5: erase closed connections under exclusive lock ──────────
        if (!toClose.empty())
        {
            std::unique_lock<std::shared_mutex> lock(_connMapMutex);
            for (int fd : toClose)
            {
                auto it = _connectionsBySocketfds.find(fd);
                if (it != _connectionsBySocketfds.end())
                {
                    it->second->Stop();
                    delete it->second;
                    _connectionsBySocketfds.erase(it);
                }
            }
        }

        // ── Phase 6: handle accept AFTER existing clients have been purged ──────────
        // This avoids applying stale pollution from closed FDs to new connections that
        // might reuse the same FD in the same cycle.
        if (_pollArgs[0].revents & POLLIN)
            handleAccept();
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
        {
            std::unique_lock<std::shared_mutex> lock(_connMapMutex);
            auto it = _connectionsBySocketfds.find(connection->_socketfd);
            if (it != _connectionsBySocketfds.end())
            {
                it->second->DetachSocket();
                delete it->second;
                _connectionsBySocketfds.erase(it);
            }
            _connectionsBySocketfds.insert({connection->_socketfd, connection});
        }
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
    std::unique_lock<std::shared_mutex> lock(_connMapMutex);
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
    std::unique_lock<std::shared_mutex> lock(_connMapMutex);
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
        std::shared_lock<std::shared_mutex> lock(_connMapMutex);
        auto it = _connectionsBySocketfds.find(clientSocketFd);
        if (it != _connectionsBySocketfds.end())
            it->second->Send(data.c_str(), data.size());
        return;
    }

    // EventBased: write directly into the connection's outgoing buffer
    // (thread-safe via Enqueue), then wake the I/O thread via the pipe.
    {
        std::shared_lock<std::shared_mutex> lock(_connMapMutex);
        auto it = _connectionsBySocketfds.find(clientSocketFd);
        if (it != _connectionsBySocketfds.end())
            it->second->Enqueue(data.c_str(), data.size());
    }

    char buf = 'x';
    write(_wakeupPipe[1], &buf, 1);
}