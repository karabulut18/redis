#include "Client.h"
#include "../lib/common/Output.h"
#include "../lib/common/TcpConnection.h"
#include "../lib/redis/Database.h"
#include "../lib/redis/RespParser.h"
#include "Server.h"
#include <string>

// Global database instance
static Database& getDatabase()
{
    static Database db;
    return db;
}

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

void Client::SendResponse(const RespValue& response)
{
    std::string encoded = RespParser::encode(response);
    Send(encoded.c_str(), encoded.length());
}

void Client::HandleCommand(const std::vector<RespValue>& args)
{
    if (args.empty())
        return;

    // First argument is always the command name
    std::string cmd(args[0].getString());

    // Convert to uppercase for case-insensitive matching
    for (char& c : cmd)
        c = toupper(c);

    RespValue response;

    if (cmd == "PING")
    {
        response.type = RespType::SimpleString;
        if (args.size() > 1)
            response.value = args[1].getString(); // PING with message
        else
            response.value = std::string_view("PONG");
        SendResponse(response);
    }
    else if (cmd == "SET")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'set' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        std::string value(args[2].getString());
        getDatabase().set(key, value);

        response.type = RespType::SimpleString;
        response.value = std::string_view("OK");
        SendResponse(response);
    }
    else if (cmd == "GET")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'get' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        const std::string* val = getDatabase().get(key);

        if (val)
        {
            response.type = RespType::BulkString;
            response.value = std::string_view(val->data(), val->size());
            SendResponse(response);
        }
        else
        {
            response.type = RespType::Null;
            SendResponse(response);
        }
    }
    else if (cmd == "DEL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'del' command");
            SendResponse(response);
            return;
        }
        int64_t deleted = 0;
        for (size_t i = 1; i < args.size(); i++)
        {
            std::string key(args[i].getString());
            if (getDatabase().del(key))
                deleted++;
        }
        response.type = RespType::Integer;
        response.value = deleted;
        SendResponse(response);
    }
    else
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR unknown command");
        SendResponse(response);
    }
}

size_t Client::OnMessageReceive(const char* buffer, m_size_t size)
{
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
            HandleCommand(val.getArray());
        }
        else if (val.type == RespType::SimpleString)
        {
            // Handle inline PING (sent as simple string by some clients)
            if (val.getString() == "PING")
            {
                RespValue response;
                response.type = RespType::SimpleString;
                response.value = std::string_view("PONG");
                SendResponse(response);
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
    RespValue val;
    val.type = RespType::SimpleString;
    val.value = std::string_view("PING");
    std::string encoded = RespParser::encode(val);
    _connection->Send(encoded.c_str(), encoded.length());
}