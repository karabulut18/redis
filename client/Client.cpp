#include "Client.h"
#include "../lib/common/Output.h"
#include "../lib/common/TcpConnection.h"
#include "../lib/redis/RespParser.h"
#include <string>
#include <unistd.h>

size_t Client::OnMessageReceive(const char* buffer, m_size_t length)
{
    size_t totalConsumed = 0;
    while (totalConsumed < length)
    {
        RespValue val;
        size_t bytesRead = 0;
        RespStatus status = _parser->decode(buffer + totalConsumed, length - totalConsumed, val, bytesRead);

        if (status == RespStatus::Incomplete)
        {
            break;
        }
        else if (status == RespStatus::Invalid)
        {
            // Protocol error, maybe close connection?
            PUTF_LN("Invalid RESP protocol");
            // Consume 1 byte to try to recover/skip? or just close?
            // For now, let's close or just consume everything to avoid loop
            return length; // Consume all to drop invalid packet
        }

        // Successfully parsed a message
        if (val.type == RespType::Array)
        {
            const auto& array = val.getArray();
            if (!array.empty() && array[0].type == RespType::BulkString)
            {
                if (array[0].getString() == "PING")
                {
                    PUTF_LN("Client received PING");
                    std::string reply = "+PONG\r\n";
                    Send(reply.c_str(), reply.length());
                }
                else
                {
                    std::string reply = "-ERR unknown command\r\n";
                    Send(reply.c_str(), reply.length());
                }
            }
        }
        else if (val.type == RespType::SimpleString)
        {
            if (val.getString() == "PING")
            {
                std::string reply = "+PONG\r\n";
                Send(reply.c_str(), reply.length());
            }
        }

        totalConsumed += bytesRead;
    }
    return totalConsumed;
};

void Client::OnDisconnect()
{
    PUTFC_LN("Client disconnected");
};

void Client::Send(const char* c, ssize_t size)
{
    _connection->Send(c, size);
}

Client* Client::Get()
{
    static Client* instance = new Client();
    return instance;
}

Client::~Client()
{
    delete _connection;
    delete _parser;
}

Client::Client()
{
    _connection = TcpConnection::CreateFromPortAndIp(DEFAULT_PORT, DEFAULT_IP);
}

bool Client::Init()
{
    _parser = new RespParser();
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
        // SendHeartbeat();
        Ping();
    }
    PUTFC_LN("Client stopped");
}

void Client::Ping()
{
    static RespValue val;
    val.type = RespType::SimpleString;
    val.value = "PING";
    std::string encoded = RespParser::encode(val);
    _connection->Send(encoded.c_str(), encoded.length());
}

void signal_handler(int signum)
{
    if (Client::Get()->IsRunning())
        Client::Get()->Stop();
}

int main(int argc, char* argv[])
{
    Output::GetInstance()->Init("redis_client");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Client::Get()->Init();
    Client::Get()->Run();

    return 0;
};