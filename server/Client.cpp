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
    // Do not access _connection here—it is owned and deleted by TcpServer.
    // By the time Client is deleted, its connection has already been destroyed.
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
    // Append incoming raw bytes to our segmented buffer
    _inBuffer.append(buffer, size);

    while (!_inBuffer.empty())
    {
        RespValue val;
        size_t bytesRead = 0;
        // Parse directly from the segmented buffer
        RespStatus status = _parser->decode(_inBuffer, val, bytesRead);

        if (status == RespStatus::Incomplete)
        {
            break;
        }
        else if (status == RespStatus::Invalid)
        {
            PUTF_LN("Invalid RESP protocol on client " + std::to_string(_id));
            _inBuffer.consume(_inBuffer.size()); // Clear problematic buffer
            return size;
        }

        // Dispatch command
        if (val.type == RespType::Array)
        {
            Command cmd;
            cmd.client = this;
            cmd.request = std::move(val);

            if (!EnqueueCommand(std::move(cmd)))
            {
                PUTF_LN("WARN: command queue full for client " + std::to_string(_id) + ", dropping command");
                RespValue err;
                err.type = RespType::Error;
                err.value = std::string_view("ERR server command queue full, please retry");
                SendResponse(err);
            }
        }
        else if (val.type == RespType::SimpleString)
        {
            // Handle inline PING/COMMANDS
            if (val.toString() == "PING")
            {
                Command cmd;
                cmd.client = this;
                // Wrap simple string in an array to maintain internal consistency
                std::vector<RespValue> args;
                RespValue arg;
                arg.type = RespType::BulkString;
                arg.value = std::string_view("PING");
                args.push_back(std::move(arg));
                cmd.request.setArray(std::move(args));

                if (!EnqueueCommand(std::move(cmd)))
                {
                    PUTF_LN("WARN: command queue full for client " + std::to_string(_id) + ", dropping PING");
                    RespValue err;
                    err.type = RespType::Error;
                    err.value = std::string_view("ERR server command queue full, please retry");
                    SendResponse(err);
                }
            }
        }

        _inBuffer.consume(bytesRead);
    }

    // We always report how much we accepted from the network,
    // which is the full 'size' because we appended it to _inBuffer.
    return size;
}

bool Client::EnqueueCommand(Command cmd)
{
    bool ok = _commandQueue->push(std::move(cmd));
    if (ok)
        Server::Get()->WakeUp(); // Wake main thread immediately
    return ok;
}

bool Client::DequeueCommand(Command& cmd)
{
    return _commandQueue->pop(cmd);
}

void Client::OnDisconnect()
{
    PUTF_LN("Client disconnected: " + std::to_string(_id));
    Server::Get()->OnClientDisconnect(this);
}

void Client::Ping()
{
    RespValue val;
    val.type = RespType::SimpleString;
    val.value = std::string_view("PING");
    std::string encoded = _parser->encode(val);
    _connection->Send(encoded.c_str(), encoded.length());
}
