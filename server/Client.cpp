#include "Client.h"
#include "../lib/common/Output.h"
#include "../lib/common/TcpConnection.h"
#include "../lib/redis/RespParser.h"
#include "Server.h"
#include <string>

// Global database instance

Client::Client(int id, TcpConnection* connection)
{
    _id = id;
    _connection = connection;
    _parser = new RespParser();
    _commandQueue = new LockFreeRingBuffer<Command>(1024);
}

Client::~Client()
{
    if (_connection != nullptr && _connection->IsRunning())
        _connection->Stop();
    delete _parser;
    delete _commandQueue;
    _commandQueue = nullptr; // Safety
}

void Client::Send(const char* c, ssize_t size)
{
    _connection->Send(c, size);
}

void Client::SendResponse(const RespValue& response)
{
    std::string encoded = RespParser::encode(response);
    Send(encoded.c_str(), encoded.length());
}

std::string Client::PrepareResponse(const RespValue& response)
{
    return RespParser::encode(response);
}

size_t Client::OnMessageReceive(const char* buffer, m_size_t size)
{
    PUTF_LN("Client " + std::to_string(_id) + " received " + std::to_string(size) + " bytes");
    size_t totalConsumed = 0;
    while (totalConsumed < size)
    {
        RespValue val;
        size_t bytesRead = 0;
        RespStatus status = _parser->decode(buffer + totalConsumed, size - totalConsumed, val, bytesRead);

        if (status == RespStatus::Incomplete)
        {
            break;
        }
        else if (status == RespStatus::Invalid)
        {
            PUTF_LN("Invalid RESP protocol");
            return size;
        }

        // Dispatch command
        if (val.type == RespType::Array)
        {
            Command cmd;
            cmd.client = this;

            // Convert RespValue args to string args for our Command struct
            std::vector<RespValue> args = val.getArray();
            for (const auto& arg : args)
            {
                cmd.args.push_back(std::string(arg.getString()));
            }

            EnqueueCommand(cmd);
            PUTF_LN("Enqueued command: " + cmd.args[0]);
        }
        else if (val.type == RespType::SimpleString)
        {
            // Handle inline PING (sent as simple string by some clients)
            if (val.getString() == "PING")
            {
                Command cmd;
                cmd.client = this;
                cmd.args.push_back("PING");
                cmd.args.push_back("PING");
                EnqueueCommand(cmd);
                PUTF_LN("Enqueued inline PING");
            }
        }

        totalConsumed += bytesRead;
    }
    return totalConsumed;
}

bool Client::EnqueueCommand(Command cmd)
{
    return _commandQueue->push(cmd);
}

bool Client::DequeueCommand(Command& cmd)
{
    return _commandQueue->pop(cmd);
}

void Client::OnDisconnect()
{
    PUTF_LN("Client disconnected: " + std::to_string(_id));
    Server::Get()->OnClientDisconnect(_id);
}

void Client::Ping()
{
    RespValue val;
    val.type = RespType::SimpleString;
    val.value = std::string_view("PING");
    std::string encoded = RespParser::encode(val);
    _connection->Send(encoded.c_str(), encoded.length());
}
