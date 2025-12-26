#include <signal.h>

#include "../lib/Output.h"
#include "../lib/TcpConnection.h"
#include "../lib/TcpServer.h"
#include "../msg/shortString.h"
#include "../msg/types.h"
#include "Client.h"
#include "Server.h"
#include <unistd.h>

Server* Server::Get()
{
    static Server* instance = new Server();
    return instance;
}

Server::Server()
{
    _tcpServer = new TcpServer(this, DEFAULT_PORT);
    _tcpServer->SetConcurrencyType(ConcurrencyType::EventBased);
    PUTF_LN("Server is set");
};

bool Server::Init()
{
    return _tcpServer->Init();
};

ITcpConnection* Server::AcceptConnection(int id, TcpConnection* connection)
{
    Client* client = new Client(id, connection);
    _clients.insert({id, client});
    PUTF_LN("New client connected " + std::to_string(id));
    return client;
}

void Server::SendHeartbeat()
{
    static int heartbeatCount = 0;
    msg::shortString msg;
    snprintf(msg._buffer, sizeof(msg._buffer), "Heartbeat %d from server", heartbeatCount++);

    for (std::pair<int, Client*> iterator : _clients)
        iterator.second->Send(reinterpret_cast<char*>(&msg), sizeof(msg));

    heartbeatCount = heartbeatCount % 10000;
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
    while (_tcpServer->IsRunning())
    {
        sleep(1);
        // SendHeartbeat();
    }
    PUTF_LN("Server stopped\n");
}

void Server::OnClientDisconnect(int id)
{
    PUTF_LN("Client disconnected: " + std::to_string(id));
    _clients.erase(id);
}

int main()
{
    Output::GetInstance()->Init("redis_server");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Server::Get()->Init();
    Server::Get()->Run();

    return 0;
};