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
            response.value = args[1].getString();
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

        // Parse optional flags: EX seconds | PX milliseconds
        int64_t ttlMs = -1;
        for (size_t i = 3; i < args.size(); i++)
        {
            std::string opt(args[i].getString());
            for (char& c : opt)
                c = toupper(c);

            if (opt == "EX" && i + 1 < args.size())
            {
                ttlMs = std::stoll(std::string(args[++i].getString())) * 1000;
            }
            else if (opt == "PX" && i + 1 < args.size())
            {
                ttlMs = std::stoll(std::string(args[++i].getString()));
            }
        }

        getDatabase().set(key, value, ttlMs);
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
    else if (cmd == "EXPIRE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'expire' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t seconds = std::stoll(std::string(args[2].getString()));
        bool ok = getDatabase().expire(key, seconds * 1000);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        SendResponse(response);
    }
    else if (cmd == "PEXPIRE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'pexpire' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t ms = std::stoll(std::string(args[2].getString()));
        bool ok = getDatabase().expire(key, ms);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        SendResponse(response);
    }
    else if (cmd == "TTL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'ttl' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t pttl = getDatabase().pttl(key);

        response.type = RespType::Integer;
        if (pttl >= 0)
            response.value = pttl / 1000; // convert ms → seconds
        else
            response.value = pttl; // -1 or -2
        SendResponse(response);
    }
    else if (cmd == "PTTL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'pttl' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t pttl = getDatabase().pttl(key);
        response.type = RespType::Integer;
        response.value = pttl;
        SendResponse(response);
    }
    else if (cmd == "PERSIST")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'persist' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        bool ok = getDatabase().persist(key);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        SendResponse(response);
    }
    else if (cmd == "EXISTS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'exists' command");
            SendResponse(response);
            return;
        }
        int64_t count = 0;
        for (size_t i = 1; i < args.size(); i++)
        {
            std::string key(args[i].getString());
            if (getDatabase().exists(key))
                count++;
        }
        response.type = RespType::Integer;
        response.value = count;
        SendResponse(response);
    }
    else if (cmd == "KEYS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'keys' command");
            SendResponse(response);
            return;
        }
        std::string pattern(args[1].getString());
        auto matched = getDatabase().keys(pattern);

        response.type = RespType::Array;
        std::vector<RespValue> arr;
        arr.reserve(matched.size());
        for (auto& k : matched)
        {
            RespValue elem;
            elem.type = RespType::BulkString;
            elem.value = std::string_view(k.data(), k.size());
            arr.push_back(elem);
        }
        response.value = std::move(arr);
        SendResponse(response);
    }
    else if (cmd == "DBSIZE")
    {
        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(getDatabase().size());
        SendResponse(response);
    }
    else if (cmd == "RENAME")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rename' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        std::string newkey(args[2].getString());
        bool ok = getDatabase().rename(key, newkey);
        if (ok)
        {
            response.type = RespType::SimpleString;
            response.value = std::string_view("OK");
        }
        else
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR no such key");
        }
        SendResponse(response);
    }
    else if (cmd == "ZADD")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zadd' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        double score = std::stod(std::string(args[2].getString()));
        std::string member(args[3].getString());

        bool added = getDatabase().zadd(key, score, member);
        response.type = RespType::Integer;
        response.value = added ? int64_t(1) : int64_t(0);
        SendResponse(response);
    }
    else if (cmd == "ZREM")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrem' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t removedCount = 0;
        for (size_t i = 2; i < args.size(); i++)
        {
            std::string member(args[i].getString());
            if (getDatabase().zrem(key, member))
                removedCount++;
        }
        response.type = RespType::Integer;
        response.value = removedCount;
        SendResponse(response);
    }
    else if (cmd == "ZCARD")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zcard' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        response.type = RespType::Integer;
        response.value = getDatabase().zcard(key);
        SendResponse(response);
    }
    else if (cmd == "ZSCORE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zscore' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        std::string member(args[2].getString());
        auto score = getDatabase().zscore(key, member);

        if (score)
        {
            response.type = RespType::BulkString;
            // Convert double to string for RESP bulk string
            std::string s = std::to_string(*score);
            // Remove trailing zeros for cleaner output
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.')
                s.pop_back();
            response.value = s;
            SendResponse(response);
        }
        else
        {
            response.type = RespType::Null;
            SendResponse(response);
        }
    }
    else if (cmd == "ZRANGE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrange' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t start = std::stoll(std::string(args[2].getString()));
        int64_t stop = std::stoll(std::string(args[3].getString()));
        bool withScores = false;

        if (args.size() > 4)
        {
            std::string opt(args[4].getString());
            for (char& c : opt)
                c = toupper(c);
            if (opt == "WITHSCORES")
                withScores = true;
        }

        auto result = getDatabase().zrange(key, start, stop);
        response.type = RespType::Array;
        std::vector<RespValue> arr;
        for (const auto& item : result)
        {
            RespValue m;
            m.type = RespType::BulkString;
            m.value = item.member;
            arr.push_back(m);

            if (withScores)
            {
                RespValue s;
                s.type = RespType::BulkString;
                std::string scoreStr = std::to_string(item.score);
                scoreStr.erase(scoreStr.find_last_not_of('0') + 1, std::string::npos);
                if (scoreStr.back() == '.')
                    scoreStr.pop_back();
                s.value = scoreStr;
                arr.push_back(s);
            }
        }
        response.value = std::move(arr);
        SendResponse(response);
    }
    else if (cmd == "ZRANGEBYSCORE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrangebyscore' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        double min = std::stod(std::string(args[2].getString()));
        double max = std::stod(std::string(args[3].getString()));

        // Basic parser for WITHSCORES (optional 4th arg)
        // Ignoring full ZRANGEBYSCORE options syntax for now
        bool withScores = (args.size() > 4 && args[4].getString() == "WITHSCORES");

        auto result = getDatabase().zrangebyscore(key, min, max);

        response.type = RespType::Array;
        std::vector<RespValue> array;
        for (const auto& item : result)
        {
            RespValue val;
            val.type = RespType::BulkString;
            val.value = item.member; // implicit conversion
            array.push_back(val);
            if (withScores)
            {
                RespValue score;
                score.type = RespType::BulkString;
                score.setStringOwned(std::to_string(item.score));
            }
        }
        response.value = array;
        SendResponse(response);
    }
    // --- Hash Commands ---
    else if (cmd == "HSET")
    {
        if (args.size() != 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hset' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        std::string field(args[2].getString());
        std::string value(args[3].getString());

        int result = getDatabase().hset(key, field, value);
        if (result == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            SendResponse(response);
            return;
        }

        response.type = RespType::Integer;
        response.value = (int64_t)result;
        SendResponse(response);
    }
    else if (cmd == "HGET")
    {
        if (args.size() != 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hget' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        std::string field(args[2].getString());

        auto val = getDatabase().hget(key, field);
        if (val)
        {
            response.type = RespType::BulkString;
            response.value = *val; // Points to temporary string from optional?
                                   // Database::hget returns optional<string>.
                                   // `val` holds the string. `val` lifetime is this scope.
                                   // `response.value` points to `val`'s content.
                                   // Safe.
        }
        else
        {
            response.type = RespType::Null;
        }
        SendResponse(response);
    }
    else if (cmd == "HDEL")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hdel' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int count = 0;
        for (size_t i = 2; i < args.size(); ++i)
        {
            std::string field(args[i].getString());
            int res = getDatabase().hdel(key, field);
            if (res == -1)
            {
                response.type = RespType::Error;
                response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
                SendResponse(response);
                return;
            }
            if (res == 1)
                count++;
        }

        response.type = RespType::Integer;
        response.value = (int64_t)count;
        SendResponse(response);
    }
    else if (cmd == "HLEN")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hlen' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        int64_t len = getDatabase().hlen(key);
        if (len == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            SendResponse(response);
            return;
        }
        response.type = RespType::Integer;
        response.value = len;
        SendResponse(response);
    }
    else if (cmd == "HGETALL")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hgetall' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        auto result = getDatabase().hgetall(key);
        // result is vector<HGetAllResult>. Scope is valid.

        // RESP3 map or RESP2 array? Let's use Array for max compatibility with CLI unless Map is requested.
        // Actually RESP3 Map is better if we support it.
        // But `redis-cli` might expect Array if it negotiates RESP2.
        // My server doesn't negotiate yet.
        // Let's use Map since it's cleaner in code, or Array (flat list) which is safer.
        // Array (flat list) is standard Redis behavior for HGETALL in RESP2.
        // Let's use Array for now.

        response.type = RespType::Array;
        std::vector<RespValue> array;
        for (const auto& item : result)
        {
            RespValue field, val;
            field.type = RespType::BulkString;
            field.value = item.field;
            val.type = RespType::BulkString;
            val.value = item.value;
            array.push_back(field);
            array.push_back(val);
        }
        response.value = array;
        SendResponse(response);
    }
    else if (cmd == "TYPE")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'type' command");
            SendResponse(response);
            return;
        }
        std::string key(args[1].getString());
        response.type = RespType::SimpleString;
        if (!getDatabase().exists(key))
        {
            response.value = std::string_view("none");
        }
        else
        {
            EntryType type = getDatabase().getType(key);
            if (type == EntryType::STRING)
                response.value = std::string_view("string");
            else if (type == EntryType::ZSET)
                response.value = std::string_view("zset");
            else if (type == EntryType::HASH)
                response.value = std::string_view("hash");
            else
                response.value = std::string_view("unknown");
        }
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