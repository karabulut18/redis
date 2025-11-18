#include "TcpServer.h"
#include <errno.h>

#include <thread>
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> // read(), write(), close()
#include <netdb.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include "constants.h"
#include "ITcpServer.h"
#include "TcpConnection.h"



bool TcpServer::IsRunning()
{
    return _state == ServerState::Running;
};

bool TcpServer::Init()
{
    if(_state != ServerState::Uninitialized)
    {
        return false;
    }

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket == -1)
    {
        _error.Set(errno, "socket creation");
        return false;
    };
    int val = 1;
    setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    _serverAddress.sin_family = AF_INET; 
    _serverAddress.sin_addr.s_addr = htonl(0); 
    _serverAddress.sin_port = htons(_port); 

    if (bind(_socket, (struct sockaddr*)&_serverAddress, sizeof(_serverAddress)) == -1)
    {
        _error.Set(errno, "bind");
        return false;
    };

    if (listen(_socket, SOMAXCONN) == -1)
    {
        _error.Set(errno, "listen");
        return false;
    };

    _state = ServerState::Initialized;

    std::thread serverThread(&TcpServer::RunThread, this);
    serverThread.detach();

    {
        std::unique_lock<std::mutex> lock(_cv_mutex);
       if(!_cv.wait_for(lock, std::chrono::seconds(THREAD_START_TIMEOUT_SECONDS), [this]{return _state  == ServerState::Running;}))
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

    while(_state == ServerState::Running)
    {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int clientSocket = accept(_socket, (struct sockaddr *)&client_addr, &addrlen);
        if(clientSocket == -1)
        {
            _error.Set(errno, "accept connection");
        }
        else
        {
            getpeername(clientSocket, (struct sockaddr *)&client_addr, &addrlen);

            TcpConnection* connection = TcpConnection::CreateFromSocket(clientSocket);

            // fix the logical mistake here
            ITcpConnection* connectionOwner = _owner->AcceptConnection(_clientIndex, connection);
            connection->SetOwner(connectionOwner);

            if(connection->Init())
                _clientsById.insert({_clientIndex++, connection});
            else
                delete connection;
        }
    }

    CleanUp();
};

void TcpServer::CleanUp()
{
    close(_socket);
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
    std::map<int, TcpConnection*>::iterator it = _clientsById.find(id);
    if(it != _clientsById.end())
    {
        delete it->second;
        _clientsById.erase(it);
    }
}