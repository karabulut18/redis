#include <signal.h>

#include "../lib/common/Output.h"
#include "../lib/common/TcpConnection.h"
#include "../lib/common/TcpServer.h"
#include "../lib/redis/RespParser.h"
#include "Client.h"
#include "Server.h"
#include <unistd.h>

Server* Server::Get()
{
    static Server* instance = new Server();
    return instance;
}

Server::Server()
{
    _tcpServer = new TcpServer(this, DEFAULT_PORT);
    _tcpServer->SetConcurrencyType(ConcurrencyType::EventBased);
    PUTF_LN("Server is set");
};

bool Server::Init()
{
    return _tcpServer->Init();
};

ITcpConnection* Server::AcceptConnection(int id, TcpConnection* connection)
{
    Client* client = new Client(id, connection);
    _clients.insert({id, client});
    PUTF_LN("New client connected " + std::to_string(id));
    return client;
}

Server::~Server()
{
    delete _tcpServer;
};

bool Server::IsRunning()
{
    if (_tcpServer == nullptr)
        return false;

    return _tcpServer->IsRunning();
};

void Server::Stop()
{
    if (_tcpServer == nullptr)
        return;

    _tcpServer->Stop();
};

void signal_handler(int signum)
{
    if (Server::Get()->IsRunning())
        Server::Get()->Stop();
}

void Server::Run()
{
    while (_tcpServer->IsRunning())
    {
        ProcessCommands();
        // sleep(1); // Sleep is too slow for main loop, maybe just yield?
        // Or better yet, just run as fast as possible or sleep very briefly?
        // standard redis single thread loop.
        // For now, let's just keep it tight or use a condition variable?
        // Since we are polling queues, we can't block on just one.
        // A short sleep (e.g. 1ms or 100us) prevents 100% CPU when idle.
        usleep(100);
    }
    PUTF_LN("Server stopped\n");
}

void Server::OnClientDisconnect(int id)
{
    PUTF_LN("Client disconnected: " + std::to_string(id));
    _clients.erase(id);
}

void Server::ProcessCommands()
{
    for (auto& [id, client] : _clients)
    {
        Command cmd;
        while (client->DequeueCommand(cmd))
        {
            HandleCommand(client, cmd.args);
        }
    }
}

void Server::QueueResponse(Client* client, const RespValue& response)
{
    std::string data = client->PrepareResponse(response);
    _tcpServer->QueueResponse(client->GetId(), data);
}

void Server::SendResponse(Client* client, const RespValue& response)
{
    // Legacy helper, redirects to Queue
    QueueResponse(client, response);
}

void Server::HandleCommand(Client* client, const std::vector<std::string>& args)
{
    if (args.empty())
        return;

    // First argument is always the command name
    std::string cmd = args[0];

    // Convert to uppercase for case-insensitive matching
    for (char& c : cmd)
        c = toupper(c);

    RespValue response;

    if (cmd == "PING")
    {
        response.type = RespType::SimpleString;
        if (args.size() > 1)
            response.value = args[1];
        else
            response.value = std::string_view("PONG");
        client->SendResponse(response);
    }
    else if (cmd == "SET")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'set' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        std::string value = args[2];

        // Parse optional flags: EX seconds | PX milliseconds
        int64_t ttlMs = -1;
        for (size_t i = 3; i < args.size(); i++)
        {
            std::string opt = args[i];
            for (char& c : opt)
                c = toupper(c);

            if (opt == "EX" && i + 1 < args.size())
            {
                ttlMs = std::stoll(args[++i]) * 1000;
            }
            else if (opt == "PX" && i + 1 < args.size())
            {
                ttlMs = std::stoll(args[++i]);
            }
        }

        _db.set(key, value, ttlMs);
        response.type = RespType::SimpleString;
        response.value = std::string_view("OK");
        client->SendResponse(response);
    }
    else if (cmd == "GET")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'get' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        const std::string* val = _db.get(key);

        if (val)
        {
            response.type = RespType::BulkString;
            response.value = std::string_view(val->data(), val->size());
            client->SendResponse(response);
        }
        else
        {
            response.type = RespType::Null;
            client->SendResponse(response);
        }
    }
    else if (cmd == "DEL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'del' command");
            client->SendResponse(response);
            return;
        }
        int64_t deleted = 0;
        for (size_t i = 1; i < args.size(); i++)
        {
            std::string key = args[i];
            if (_db.del(key))
                deleted++;
        }
        response.type = RespType::Integer;
        response.value = deleted;
        client->SendResponse(response);
    }
    else if (cmd == "EXPIRE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'expire' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t seconds = std::stoll(args[2]);
        bool ok = _db.expire(key, seconds * 1000);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        client->SendResponse(response);
    }
    else if (cmd == "PEXPIRE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'pexpire' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t ms = std::stoll(args[2]);
        bool ok = _db.expire(key, ms);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        client->SendResponse(response);
    }
    else if (cmd == "TTL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'ttl' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t pttl = _db.pttl(key);

        response.type = RespType::Integer;
        if (pttl >= 0)
            response.value = pttl / 1000;
        else
            response.value = pttl;
        client->SendResponse(response);
    }
    else if (cmd == "PTTL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'pttl' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t pttl = _db.pttl(key);
        response.type = RespType::Integer;
        response.value = pttl;
        client->SendResponse(response);
    }
    else if (cmd == "PERSIST")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'persist' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        bool ok = _db.persist(key);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        client->SendResponse(response);
    }
    else if (cmd == "EXISTS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'exists' command");
            client->SendResponse(response);
            return;
        }
        int64_t count = 0;
        for (size_t i = 1; i < args.size(); i++)
        {
            std::string key = args[i];
            if (_db.exists(key))
                count++;
        }
        response.type = RespType::Integer;
        response.value = count;
        client->SendResponse(response);
    }
    else if (cmd == "KEYS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'keys' command");
            client->SendResponse(response);
            return;
        }
        std::string pattern = args[1];
        auto matched = _db.keys(pattern);

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
        client->SendResponse(response);
    }
    else if (cmd == "DBSIZE")
    {
        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(_db.size());
        client->SendResponse(response);
    }
    else if (cmd == "RENAME")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rename' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        std::string newkey = args[2];
        bool ok = _db.rename(key, newkey);
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
        client->SendResponse(response);
    }
    else if (cmd == "ZADD")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zadd' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        double score = std::stod(args[2]);
        std::string member = args[3];

        bool added = _db.zadd(key, score, member);
        response.type = RespType::Integer;
        response.value = added ? int64_t(1) : int64_t(0);
        client->SendResponse(response);
    }
    else if (cmd == "ZREM")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrem' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t removedCount = 0;
        for (size_t i = 2; i < args.size(); i++)
        {
            std::string member = args[i];
            if (_db.zrem(key, member))
                removedCount++;
        }
        response.type = RespType::Integer;
        response.value = removedCount;
        client->SendResponse(response);
    }
    else if (cmd == "ZCARD")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zcard' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        response.type = RespType::Integer;
        response.value = _db.zcard(key);
        client->SendResponse(response);
    }
    else if (cmd == "ZSCORE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zscore' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        std::string member = args[2];
        auto score = _db.zscore(key, member);

        if (score)
        {
            response.type = RespType::BulkString;
            std::string s = std::to_string(*score);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.')
                s.pop_back();
            response.value = s;
            client->SendResponse(response);
        }
        else
        {
            response.type = RespType::Null;
            client->SendResponse(response);
        }
    }
    else if (cmd == "ZRANGE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrange' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t start = std::stoll(args[2]);
        int64_t stop = std::stoll(args[3]);
        bool withScores = false;

        if (args.size() > 4)
        {
            std::string opt = args[4];
            for (char& c : opt)
                c = toupper(c);
            if (opt == "WITHSCORES")
                withScores = true;
        }

        auto result = _db.zrange(key, start, stop);
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
        client->SendResponse(response);
    }
    else if (cmd == "ZRANGEBYSCORE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrangebyscore' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        double min = std::stod(args[2]);
        double max = std::stod(args[3]);

        bool withScores = (args.size() > 4 && args[4] == "WITHSCORES");

        auto result = _db.zrangebyscore(key, min, max);

        response.type = RespType::Array;
        std::vector<RespValue> array;
        for (const auto& item : result)
        {
            RespValue val;
            val.type = RespType::BulkString;
            val.value = item.member;
            array.push_back(val);
            if (withScores)
            {
                RespValue score;
                score.type = RespType::BulkString;
                score.setStringOwned(std::to_string(item.score));
                array.push_back(score);
            }
        }
        response.value = array;
        client->SendResponse(response);
    }
    else if (cmd == "HSET")
    {
        if (args.size() != 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hset' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        std::string field = args[2];
        std::string value = args[3];

        int result = _db.hset(key, field, value);
        if (result == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        response.type = RespType::Integer;
        response.value = (int64_t)result;
        client->SendResponse(response);
    }
    else if (cmd == "HGET")
    {
        if (args.size() != 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hget' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        std::string field = args[2];

        auto val = _db.hget(key, field);
        if (val)
        {
            response.type = RespType::BulkString;
            response.value = *val;
            client->SendResponse(response);
        }
        else
        {
            response.type = RespType::Null;
            client->SendResponse(response);
        }
    }
    else if (cmd == "HDEL")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hdel' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int count = 0;
        for (size_t i = 2; i < args.size(); ++i)
        {
            std::string field = args[i];
            int res = _db.hdel(key, field);
            if (res == -1)
            {
                response.type = RespType::Error;
                response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
                client->SendResponse(response);
                return;
            }
            if (res == 1)
                count++;
        }

        response.type = RespType::Integer;
        response.value = (int64_t)count;
        client->SendResponse(response);
    }
    else if (cmd == "HLEN")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hlen' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t len = _db.hlen(key);
        if (len == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }
        response.type = RespType::Integer;
        response.value = len;
        client->SendResponse(response);
    }
    else if (cmd == "HGETALL")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hgetall' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        auto result = _db.hgetall(key);

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
        client->SendResponse(response);
    }
    else if (cmd == "TYPE")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'type' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        response.type = RespType::SimpleString;
        if (!_db.exists(key))
        {
            response.value = std::string_view("none");
        }
        else
        {
            EntryType type = _db.getType(key);
            if (type == EntryType::STRING)
                response.value = std::string_view("string");
            else if (type == EntryType::ZSET)
                response.value = std::string_view("zset");
            else if (type == EntryType::HASH)
                response.value = std::string_view("hash");
            else if (type == EntryType::LIST)
                response.value = std::string_view("list");
            else if (type == EntryType::SET)
                response.value = std::string_view("set");
            else
                response.value = std::string_view("unknown");
        }
        client->SendResponse(response);
    }
    else if (cmd == "LPUSH")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'lpush' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t len = 0;

        for (size_t i = 2; i < args.size(); ++i)
        {
            len = _db.lpush(key, args[i]);
            if (len == -1)
                break;
        }

        if (len == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
        }
        else
        {
            response.type = RespType::Integer;
            response.value = len;
        }
        client->SendResponse(response);
    }
    else if (cmd == "RPUSH")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rpush' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t len = 0;
        for (size_t i = 2; i < args.size(); ++i)
        {
            len = _db.rpush(key, args[i]);
            if (len == -1)
                break;
        }

        if (len == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
        }
        else
        {
            response.type = RespType::Integer;
            response.value = len;
        }
        client->SendResponse(response);
    }
    else if (cmd == "LPOP")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'lpop' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        auto val = _db.lpop(key);

        if (!val)
        {
            response.type = RespType::Null;
        }
        else
        {
            response.type = RespType::BulkString;
            response.setStringOwned(*val);
        }
        client->SendResponse(response);
    }
    else if (cmd == "RPOP")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rpop' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        auto val = _db.rpop(key);

        if (!val)
        {
            response.type = RespType::Null;
        }
        else
        {
            response.type = RespType::BulkString;
            response.setStringOwned(*val);
        }
        client->SendResponse(response);
    }
    else if (cmd == "LLEN")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'llen' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        int64_t len = _db.llen(key);
        response.type = RespType::Integer;
        response.value = len;
        client->SendResponse(response);
    }
    else if (cmd == "LRANGE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'lrange' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int64_t start = 0;
        int64_t stop = 0;

        try
        {
            start = std::stoll(args[2]);
            stop = std::stoll(args[3]);
        }
        catch (...)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR value is not an integer or out of range");
            client->SendResponse(response);
            return;
        }

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        auto items = _db.lrange(key, start, stop);
        response.type = RespType::Array;
        std::vector<RespValue> arr;
        arr.reserve(items.size());

        for (const auto& item : items)
        {
            RespValue v;
            v.type = RespType::BulkString;
            v.setStringOwned(item);
            arr.push_back(v);
        }
        response.value = std::move(arr);
        client->SendResponse(response);
    }
    else if (cmd == "SADD")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'sadd' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int added = 0;

        for (size_t i = 2; i < args.size(); ++i)
        {
            int res = _db.sadd(key, args[i]);
            if (res == -1)
            {
                response.type = RespType::Error;
                response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
                client->SendResponse(response);
                return;
            }
            added += res;
        }

        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(added);
        client->SendResponse(response);
    }
    else if (cmd == "SREM")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'srem' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        int removed = 0;

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        for (size_t i = 2; i < args.size(); ++i)
        {
            removed += _db.srem(key, args[i]);
        }

        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(removed);
        client->SendResponse(response);
    }
    else if (cmd == "SISMEMBER")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'sismember' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];
        std::string member = args[2];

        if (_db.exists(key) && _db.getType(key) != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        int res = _db.sismember(key, member);
        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(res);
        client->SendResponse(response);
    }
    else if (cmd == "SMEMBERS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'smembers' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];

        if (_db.exists(key) && _db.getType(key) != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        auto members = _db.smembers(key);
        response.type = RespType::Array;

        std::vector<RespValue> arr;
        arr.reserve(members.size());
        for (const auto& m : members)
        {
            RespValue v;
            v.type = RespType::BulkString;
            v.setStringOwned(m);
            arr.push_back(v);
        }
        response.value = std::move(arr);
        client->SendResponse(response);
    }
    else if (cmd == "SCARD")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'scard' command");
            client->SendResponse(response);
            return;
        }
        std::string key = args[1];

        if (_db.exists(key) && _db.getType(key) != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            client->SendResponse(response);
            return;
        }

        int64_t card = _db.scard(key);
        response.type = RespType::Integer;
        response.value = card;
        client->SendResponse(response);
    }
    else
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR unknown command");
        client->SendResponse(response);
    }
}

int main()
{
    Output::GetInstance()->Init("redis_server");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Server::Get()->Init();
    Server::Get()->Run();

    return 0;
};