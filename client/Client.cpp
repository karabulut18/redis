#include "Client.h"
#include "../lib/Output.h"
#include "../lib/TcpConnection.h"
#include "../msg/shortString.h"
#include "../msg/types.h"
#include <signal.h>
#include <string>
#include <unistd.h>

size_t Client::OnMessageReceive(const char *buffer, m_size_t length)
{
    const msg::header *hdr = reinterpret_cast<const msg::header *>(buffer);
    switch (hdr->_type)
    {
    case msg::SHORT_STRING:
    {
        const msg::shortString *msg = reinterpret_cast<const msg::shortString *>(buffer);
        PUTF("message:\n" + std::string(msg->_buffer) + "\n");
    }
    default:
        break;
    };
    return length; // Consume everything (mock)
};

void Client::OnDisconnect()
{
    PUTFC_LN("Client disconnected");
};

void Client::SendHeartbeat()
{
    msg::shortString msg;
    static int heartbeatCount = 0;
    snprintf(msg._buffer, sizeof(msg._buffer), "Heartbeat %d from client", heartbeatCount++);
    heartbeatCount = heartbeatCount % 10000;
    _connection->Send(reinterpret_cast<char *>(&msg), sizeof(msg));
}

Client *Client::Get()
{
    static Client *instance = new Client();
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
    PUTFC_LN("Client Created");
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
    while (_connection->IsRunning())
    {
        sleep(1);
        SendHeartbeat();
    }
    PUTFC_LN("Client stopped");
}

void signal_handler(int signum)
{
    if (Client::Get()->IsRunning())
        Client::Get()->Stop();
}

int main()
{
    Output::GetInstance()->Init("redis_client");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Client::Get()->Init();
    Client::Get()->Run();

    return 0;
};