#include "Client.h"
#include "../lib/Output.h"
#include "../lib/RespParser.h"
#include "../lib/TcpConnection.h"
#include "Server.h"
#include <string>

Client::Client(int id, TcpConnection* connection)
{
    _id = id;
    _connection = connection;
    _parser = new RespParser();
}

Client::~Client()
{
    if (_connection != nullptr && _connection->IsRunning())
        _connection->Stop();
    delete _parser;
}

void Client::Send(const char* c, ssize_t size)
{
    _connection->Send(c, size);
}

size_t Client::OnMessageReceive(const char* buffer, m_size_t size)
{
    size_t totalConsumed = 0;
    while (totalConsumed < size)
    {
        RespValue val;
        size_t bytesRead = 0;
        RespStatus status = _parser->decode(buffer + totalConsumed, size - totalConsumed, val, bytesRead);
        RespValue response;

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
        std::string responseString;
        if (val.type == RespType::Array)
        {
            const auto& array = val.getArray();
            if (!array.empty() && array[0].type == RespType::BulkString)
            {
                if (array[0].getString() == "PING")
                {
                    response.type = RespType::SimpleString;
                    response.value = "PONG";
                    responseString = RespParser::encode(response);
                    Send(responseString.c_str(), responseString.length());
                }
                else
                {
                    response.type = RespType::Error;
                    response.value = "ERR unknown command";
                    responseString = RespParser::encode(response);
                    Send(responseString.c_str(), responseString.length());
                }
            }
        }
        else if (val.type == RespType::SimpleString)
        {
            if (val.getString() == "PING")
            {
                PUTF_LN("Server received PING");
                response.type = RespType::SimpleString;
                response.value = "PONG";
                responseString = RespParser::encode(response);
                Send(responseString.c_str(), responseString.length());
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

void Client::Ping()
{
    static RespValue val;
    val.type = RespType::SimpleString;
    val.value = "PING";
    _connection->Send(RespParser::encode(val).c_str(), RespParser::encode(val).length());
}