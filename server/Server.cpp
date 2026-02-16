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
    std::lock_guard<std::mutex> lock(_clientsMutex);
    auto it = _clients.find(id);
    if (it != _clients.end())
    {
        PUTF_LN("Cleaning up stale client on FD " + std::to_string(id));
        delete it->second;
        _clients.erase(it);
    }
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
    std::lock_guard<std::mutex> lock(_clientsMutex);
    auto it = _clients.find(id);
    if (it != _clients.end())
    {
        delete it->second;
        _clients.erase(it);
    }
}

void Server::ProcessCommands()
{
    std::lock_guard<std::mutex> lock(_clientsMutex);
    for (auto& [id, client] : _clients)
    {
        Command cmd;
        while (client->DequeueCommand(cmd))
        {
            PUTF_LN("Dequeue command for client " + std::to_string(id));
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
        QueueResponse(client, response);
    }
    else if (cmd == "SET")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'set' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "GET")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'get' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        const std::string* val = _db.get(key);

        if (val)
        {
            response.type = RespType::BulkString;
            response.value = std::string_view(val->data(), val->size());
            QueueResponse(client, response);
        }
        else
        {
            response.type = RespType::Null;
            QueueResponse(client, response);
        }
    }
    else if (cmd == "DEL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'del' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "EXPIRE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'expire' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int64_t seconds = std::stoll(args[2]);
        bool ok = _db.expire(key, seconds * 1000);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        QueueResponse(client, response);
    }
    else if (cmd == "PEXPIRE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'pexpire' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int64_t ms = std::stoll(args[2]);
        bool ok = _db.expire(key, ms);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        QueueResponse(client, response);
    }
    else if (cmd == "TTL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'ttl' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int64_t pttl = _db.pttl(key);

        response.type = RespType::Integer;
        if (pttl >= 0)
            response.value = pttl / 1000;
        else
            response.value = pttl;
        QueueResponse(client, response);
    }
    else if (cmd == "PTTL")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'pttl' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int64_t pttl = _db.pttl(key);
        response.type = RespType::Integer;
        response.value = pttl;
        QueueResponse(client, response);
    }
    else if (cmd == "PERSIST")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'persist' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        bool ok = _db.persist(key);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
        QueueResponse(client, response);
    }
    else if (cmd == "INCR")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'incr' command");
            QueueResponse(client, response);
            return;
        }
        std::pair<int64_t, bool> res = _db.incrby(args[1], 1);
        if (!res.second)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR value is not an integer or out of range, or wrong type");
            QueueResponse(client, response);
            return;
        }
        response.type = RespType::Integer;
        response.value = res.first;
        QueueResponse(client, response);
    }
    else if (cmd == "INCRBY")
    {
        if (args.size() != 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'incrby' command");
            QueueResponse(client, response);
            return;
        }
        int64_t incr = 0;
        try
        {
            incr = std::stoll(args[2]);
        }
        catch (...)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR value is not an integer or out of range");
            QueueResponse(client, response);
            return;
        }

        std::pair<int64_t, bool> res = _db.incrby(args[1], incr);
        if (!res.second)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR value is not an integer or out of range, or wrong type");
            QueueResponse(client, response);
            return;
        }
        response.type = RespType::Integer;
        response.value = res.first;
        QueueResponse(client, response);
    }
    else if (cmd == "EXISTS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'exists' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "KEYS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'keys' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "DBSIZE")
    {
        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(_db.size());
        QueueResponse(client, response);
    }
    else if (cmd == "RENAME")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rename' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "ZADD")
    {
        if (args.size() < 4 || (args.size() - 2) % 2 != 0)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zadd' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int64_t addedCount = 0;

        for (size_t i = 2; i + 1 < args.size(); i += 2)
        {
            try
            {
                double score = std::stod(args[i]);
                std::string member = args[i + 1];
                if (_db.zadd(key, score, member))
                    addedCount++;
            }
            catch (...)
            {
                response.type = RespType::Error;
                response.value = std::string_view("ERR value is not a valid float");
                QueueResponse(client, response);
                return;
            }
        }

        response.type = RespType::Integer;
        response.value = addedCount;
        QueueResponse(client, response);
    }
    else if (cmd == "ZREM")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrem' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "ZCARD")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zcard' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        response.type = RespType::Integer;
        response.value = _db.zcard(key);
        QueueResponse(client, response);
    }
    else if (cmd == "ZSCORE")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zscore' command");
            QueueResponse(client, response);
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
            QueueResponse(client, response);
        }
        else
        {
            response.type = RespType::Null;
            QueueResponse(client, response);
        }
    }
    else if (cmd == "ZRANGE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrange' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "ZRANGEBYSCORE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'zrangebyscore' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "HSET")
    {
        if (args.size() != 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hset' command");
            QueueResponse(client, response);
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
            QueueResponse(client, response);
            return;
        }

        response.type = RespType::Integer;
        response.value = (int64_t)result;
        QueueResponse(client, response);
    }
    else if (cmd == "HGET")
    {
        if (args.size() != 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hget' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        std::string field = args[2];

        auto val = _db.hget(key, field);
        if (val)
        {
            response.type = RespType::BulkString;
            response.value = *val;
            QueueResponse(client, response);
        }
        else
        {
            response.type = RespType::Null;
            QueueResponse(client, response);
        }
    }
    else if (cmd == "HDEL")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hdel' command");
            QueueResponse(client, response);
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
                QueueResponse(client, response);
                return;
            }
            if (res == 1)
                count++;
        }

        response.type = RespType::Integer;
        response.value = (int64_t)count;
        QueueResponse(client, response);
    }
    else if (cmd == "HLEN")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hlen' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int64_t len = _db.hlen(key);
        if (len == -1)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
            return;
        }
        response.type = RespType::Integer;
        response.value = len;
        QueueResponse(client, response);
    }
    else if (cmd == "HGETALL")
    {
        if (args.size() != 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'hgetall' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "TYPE")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'type' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "LPUSH")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'lpush' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "RPUSH")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rpush' command");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "LPOP")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'lpop' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "RPOP")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'rpop' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "LLEN")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'llen' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
            return;
        }

        int64_t len = _db.llen(key);
        response.type = RespType::Integer;
        response.value = len;
        QueueResponse(client, response);
    }
    else if (cmd == "LRANGE")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'lrange' command");
            QueueResponse(client, response);
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
            QueueResponse(client, response);
            return;
        }

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::LIST)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "SADD")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'sadd' command");
            QueueResponse(client, response);
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
                QueueResponse(client, response);
                return;
            }
            added += res;
        }

        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(added);
        QueueResponse(client, response);
    }
    else if (cmd == "SREM")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'srem' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        int removed = 0;

        EntryType type = _db.getType(key);
        if (_db.exists(key) && type != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
            return;
        }

        for (size_t i = 2; i < args.size(); ++i)
        {
            removed += _db.srem(key, args[i]);
        }

        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(removed);
        QueueResponse(client, response);
    }
    else if (cmd == "SISMEMBER")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'sismember' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];
        std::string member = args[2];

        if (_db.exists(key) && _db.getType(key) != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
            return;
        }

        int res = _db.sismember(key, member);
        response.type = RespType::Integer;
        response.value = static_cast<int64_t>(res);
        QueueResponse(client, response);
    }
    else if (cmd == "SMEMBERS")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'smembers' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];

        if (_db.exists(key) && _db.getType(key) != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
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
        QueueResponse(client, response);
    }
    else if (cmd == "SCARD")
    {
        if (args.size() < 2)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'scard' command");
            QueueResponse(client, response);
            return;
        }
        std::string key = args[1];

        if (_db.exists(key) && _db.getType(key) != EntryType::SET)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            QueueResponse(client, response);
            return;
        }

        int64_t card = _db.scard(key);
        response.type = RespType::Integer;
        response.value = card;
        QueueResponse(client, response);
    }
    else if (cmd == "CLIENT")
    {
        // Ignore CLIENT commands for compatibility
        response.type = RespType::SimpleString;
        response.value = std::string_view("OK");
        QueueResponse(client, response);
    }
    else if (cmd == "FLUSHALL")
    {
        _db.clear();
        response.type = RespType::SimpleString;
        response.value = std::string_view("OK");
        QueueResponse(client, response);
    }
    else
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR unknown command");
        QueueResponse(client, response);
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