#include "../lib/TcpConnection.h"
#include "Client.h"
#include <unistd.h>



void Client::OnMessage(char* buffer, ssize_t length) 
{
    printf("message:\n%s\n", buffer);   
};

void Client::OnDisconnect() 
{
    printf("Client disconnected\n");
};

void Client::Heartbeat()
{
    char heartbeat[] = "Heartbeat from client";
    _connection->Send(heartbeat, sizeof(heartbeat));
}

Client* Client::Get()
{
    static Client* instance = new Client();
    return instance;
}

Client::~Client() 
{
    delete _connection;
}

Client::Client() 
{
    _connection = TcpConnection::CreateFromPortAndIp(DEFAULT_PORT, DEFAULT_IP);
}

bool Client::Init()
{
    _connection->SetOwner(this);
    return _connection->Init();
}

bool Client::IsRunning()
{
    return _connection->IsRunning();
}

void Client::Stop()
{
    _connection->Stop();
}

void Client::Run()
{
    while(_connection->IsRunning())
    {
        sleep(1);
        Heartbeat();
    }
    printf("Client stopped\n");
}

void signal_handler(int signum)
{
    if (Client::Get()->IsRunning())
        Client::Get()->Stop();
}

int main()
{

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Client::Get()->Init();
    Client::Get()->Run();

    return 0;
};