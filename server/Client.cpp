#include "Client.h"
#include "../lib/Output.h"
#include "../lib/TcpConnection.h"
#include "Server.h"
#include <string>

Client::Client(int id, TcpConnection *connection)
{
    _id = id;
    _connection = connection;
}

Client::~Client()
{
    if (_connection != nullptr && _connection->IsRunning())
        _connection->Stop();
}

void Client::Send(const char *c, ssize_t size)
{
    _connection->Send(c, size);
}

size_t Client::OnMessageReceive(const char *buffer, m_size_t size)
{
    size_t totalConsumed = 0;
    while (totalConsumed < size)
    {
        RespValue val;
        size_t bytesRead = 0;
        RespStatus status = _parser.decode(buffer + totalConsumed, size - totalConsumed, val, bytesRead);

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
            return size; // Consume all to drop invalid packet
        }

        // Successfully parsed a message
        if (val.type == RespType::Array)
        {
            if (!val.array_val.empty() && val.array_val[0].type == RespType::BulkString)
            {
                std::string cmd = val.array_val[0].str_val;
                if (cmd == "PING")
                {
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
            if (val.str_val == "PING")
            {
                std::string reply = "+PONG\r\n";
                Send(reply.c_str(), reply.length());
            }
        }

        totalConsumed += bytesRead;
    }
    return totalConsumed;
}

void Client::OnDisconnect()
{
    PUTF_LN("Client disconnected: " + std::to_string(_id));
    Server::Get()->OnClientDisconnect(_id);
}