#include "../lib/TcpConnection.h"
#include "Client.h"
#include <unistd.h>
#include <signal.h>
#include "../msg/types.h"
#include "../msg/shortString.h"


void Client::OnMessage(const char* buffer, m_size_t length) 
{
    const header* hdr = reinterpret_cast<const header*>(buffer);
    switch(hdr->type)
    {
        case msg::SHORT_STRING:
        {
            const msg::shortString* msg = reinterpret_cast<const msg::shortString*>(buffer);
            printf("message:\n%s\n", msg->_buffer);
        }
        default:
            break;
    };
};

void Client::OnDisconnect() 
{
    printf("Client disconnected\n");
};

void Client::SendHeartbeat()
{
    msg::shortString msg;
    static int heartbeatCount = 0;
    snprintf(msg._buffer, sizeof(msg._buffer),"Heartbeat %d from client", heartbeatCount++);
    heartbeatCount = heartbeatCount%10000;
    _connection->Send(reinterpret_cast<char*>(&msg), sizeof(msg));
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
    return _connection->Init(ConcurrencyType::ThreadBased);
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
        SendHeartbeat();
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