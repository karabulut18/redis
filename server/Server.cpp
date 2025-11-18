#include <signal.h>

#include "Server.h"
#include "../lib/TcpServer.h"
#include "../lib/TcpConnection.h"
#include "Client.h"
#include <unistd.h>


Server* Server::Get()
{
    static Server* instance = new Server();
    return instance;
}

Server::Server()
{
    _tcpServer = new TcpServer(this, DEFAULT_PORT);
};

bool Server::Init()
{
    return _tcpServer->Init();
};

ITcpConnection* Server::AcceptConnection(int id, TcpConnection* connection)
{
    Client* client = new Client(id, connection);
    _clients.insert({id, client});
    return client;
}

void Server::SendHeartbeat()
{
    char heartbeat[] = "Heartbeat from server";
    for(std::pair<int, Client*> iterator : _clients)
    {
        iterator.second->Send(heartbeat, sizeof(heartbeat));
    }
}


Server::~Server()
{
    delete _tcpServer;
};


bool Server::IsRunning()
{
    if (_tcpServer == nullptr)
        return false;

    return _tcpServer->IsRunning();
};

void Server::Stop()
{
    if (_tcpServer == nullptr)
        return;

    _tcpServer->Stop();
};

void signal_handler(int signum)
{
    if (Server::Get()->IsRunning())
        Server::Get()->Stop();
}

void Server::Run()
{
    while(_tcpServer->IsRunning())
    {
        sleep(1);
        SendHeartbeat();
    }
    printf("Server stopped\n");
}

int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Server::Get()->Init();
    Server::Get()->Run();

    return 0;
};