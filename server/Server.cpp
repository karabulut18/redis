#include <signal.h>

#include "../lib/common/Output.h"
#include "../lib/common/TcpConnection.h"
#include "../lib/common/TcpServer.h"
#include "../lib/redis/RespParser.h"
#include "Client.h"
#include "Server.h"
#include <iostream>
#include <unistd.h>

Server* Server::Get()
{
    static Server instance;
    return &instance;
}

Server::Server()
{
    _tcpServer = new TcpServer(this, 6379);
    _persistence = new Persistence("appendonly.aof");
    _tcpServer->SetConcurrencyType(ConcurrencyType::EventBased);
}

Server::~Server()
{
    delete _tcpServer;
    delete _persistence;
}

bool Server::Init()
{
    if (!_tcpServer->Init())
        return false;

    // Load AOF
    std::cout << "Loading AOF..." << std::endl;
    _persistence->Load(
        [this](const std::vector<std::string>& args)
        {
            // Use a fake client with ID -1 for replay
            // We reuse the existing Client class but pass nullptr as connection.
            // Ensure Client constructor/methods don't crash with nullptr connection.
            Client fakeClient(-1, nullptr);

            // We need to temporarily disable persistence appending during replay
            // otherwise we might double-write (though logic is "HandleCommand" calls Append)
            // Check if we need to disable _persistence temporarily or handle it.

            // Actually HandleCommand calls _persistence->Append(args).
            // We MUST disable persistence during replay.
            // A simple way is to set _persistence to nullptr temporarily or add a flag.
            // But _persistence is used by other threads? No, Init is single threaded.

            Persistence* p = _persistence;
            _persistence = nullptr; // Disable appending

            RespValue request;
            request.setArray({});
            auto& arr = request.getArray();
            for (const auto& arg : args)
            {
                RespValue val;
                val.type = RespType::BulkString;
                val.value = arg; // This copies, which is fine for replay
                arr.push_back(std::move(val));
            }

            ProcessCommand(&fakeClient, request);

            _persistence = p; // Restore
        });

    return true;
}

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

bool Server::IsRunning()
{
    if (_tcpServer == nullptr)
        return false;

    return _tcpServer->IsRunning();
}

void Server::Stop()
{
    if (_tcpServer == nullptr)
        return;

    _tcpServer->Stop();
}

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
        if (_persistence)
            _persistence->Tick();
        {
            std::unique_lock<std::mutex> lock(_wakeupMutex);
            _wakeupCv.wait_for(lock, std::chrono::milliseconds(1));
        }
    }
    PUTF_LN("Server stopped\n");
}

void Server::OnClientDisconnect(int id)
{
    PUTF_LN("Client disconnected: " + std::to_string(id));
    // Don't touch _clients here — we're on the I/O thread.
    // Push the ID to the deferred queue; the main thread will clean up.
    _pendingDisconnects.push(id);
}

void Server::ProcessCommands()
{
    // Drain deferred disconnects first — main thread is the only writer to _clients.
    int disconnectedId;
    while (_pendingDisconnects.pop(disconnectedId))
    {
        auto it = _clients.find(disconnectedId);
        if (it != _clients.end())
        {
            delete it->second;
            _clients.erase(it);
        }
    }

    // Iterate _clients — no concurrent writers, no lock needed.
    for (auto& [id, client] : _clients)
    {
        Command cmd;
        while (client->DequeueCommand(cmd))
            ProcessCommand(client, cmd.request);
    }
}

void Server::QueueResponse(Client* client, const RespValue& response)
{
    std::string data = client->PrepareResponse(response);
    _tcpServer->QueueResponse(client->GetId(), data);
}

void Server::WakeUp()
{
    _wakeupCv.notify_one();
}

void Server::ProcessCommand(Client* client, const RespValue& request)
{
    if (request.type != RespType::Array)
        return;

    const auto& arr = request.getArray();
    if (arr.empty())
        return;

    std::string cmdName = arr[0].toString();
    CommandId id = GetCommandId(cmdName);

    RespValue response;

    // Dispatch
    switch (id)
    {
    case CommandId::Ping:
        PC_Ping(arr, response);
        break;
    case CommandId::Echo:
        PC_Echo(arr, response);
        break;
    case CommandId::Set:
        PC_Set(arr, response);
        break;
    case CommandId::Get:
        PC_Get(arr, response);
        break;
    case CommandId::Del:
        PC_Del(arr, response);
        break;
    case CommandId::Config:
        PC_Config(arr, response);
        break;
    case CommandId::Expire:
        PC_Expire(arr, response);
        break;
    case CommandId::PExpire:
        PC_PExpire(arr, response);
        break;
    case CommandId::Ttl:
        PC_Ttl(arr, response);
        break;
    case CommandId::PTtl:
        PC_PTtl(arr, response);
        break;
    case CommandId::Persist:
        PC_Persist(arr, response);
        break;
    case CommandId::Incr:
        PC_Incr(arr, response);
        break;
    case CommandId::IncrBy:
        PC_IncrBy(arr, response);
        break;
    case CommandId::Decr:
        PC_Decr(arr, response);
        break;
    case CommandId::DecrBy:
        PC_DecrBy(arr, response);
        break;
    case CommandId::Type:
        PC_Type(arr, response);
        break;
    case CommandId::ZAdd:
        PC_ZAdd(arr, response);
        break;
    case CommandId::ZRem:
        PC_ZRem(arr, response);
        break;
    case CommandId::ZScore:
        PC_ZScore(arr, response);
        break;
    case CommandId::ZRank:
        PC_ZRank(arr, response);
        break;
    case CommandId::ZRange:
        PC_ZRange(arr, response);
        break;
    case CommandId::ZRangeByScore:
        PC_ZRangeByScore(arr, response);
        break;
    case CommandId::ZCard:
        PC_ZCard(arr, response);
        break;
    case CommandId::HSet:
        PC_HSet(arr, response);
        break;
    case CommandId::HGet:
        PC_HGet(arr, response);
        break;
    case CommandId::HDel:
        PC_HDel(arr, response);
        break;
    case CommandId::HGetAll:
        PC_HGetAll(arr, response);
        break;
    case CommandId::HLen:
        PC_HLen(arr, response);
        break;
    case CommandId::HMSet:
        PC_HMSet(arr, response);
        break;
    case CommandId::HMGet:
        PC_HMGet(arr, response);
        break;
    case CommandId::LPush:
        PC_LPush(arr, response);
        break;
    case CommandId::RPush:
        PC_RPush(arr, response);
        break;
    case CommandId::LPop:
        PC_LPop(arr, response);
        break;
    case CommandId::RPop:
        PC_RPop(arr, response);
        break;
    case CommandId::LLen:
        PC_LLen(arr, response);
        break;
    case CommandId::LRange:
        PC_LRange(arr, response);
        break;
    case CommandId::SAdd:
        PC_SAdd(arr, response);
        break;
    case CommandId::SRem:
        PC_SRem(arr, response);
        break;
    case CommandId::SIsMember:
        PC_SIsMember(arr, response);
        break;
    case CommandId::SMembers:
        PC_SMembers(arr, response);
        break;
    case CommandId::SCard:
        PC_SCard(arr, response);
        break;
    case CommandId::Client:
        PC_Client(arr, response);
        break;
    case CommandId::FlushAll:
        PC_FlushAll(arr, response);
        break;
    case CommandId::BgRewriteAof:
        PC_BgRewriteAof(arr, response);
        break;
    case CommandId::Keys:
        PC_Keys(arr, response);
        break;
    case CommandId::Exists:
        PC_Exists(arr, response);
        break;
    case CommandId::Rename:
        PC_Rename(arr, response);
        break;
    case CommandId::Unknown:
        response.type = RespType::Error;
        response.value = std::string_view("ERR unknown subcommand");
        break;
    default:
        response.type = RespType::Error;
        response.value = std::string("ERR unknown command '") + cmdName + "'";
        break;
    }

    // Persistence
    if (_persistence && IsWriteCommand(id) && response.type != RespType::Error)
    {
        _persistence->Append(arr);
    }

    QueueResponse(client, response);
}
void Server::PC_Del(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'del' command");
        return;
    }
    int64_t deleted = 0;
    for (size_t i = 1; i < args.size(); i++)
    {
        if (_db.del(args[i].toString()))
            deleted++;
    }
    response.type = RespType::Integer;
    response.value = deleted;
}

// ...

void Server::PC_Incr(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'incr' command");
        return;
    }
    std::string key = args[1].toString();
    try
    {
        response.type = RespType::Integer;
        response.value = _db.incr(key);
    }
    catch (const std::exception& e)
    {
        response.type = RespType::Error;
        response.value = std::string(e.what());
    }
}

void Server::PC_IncrBy(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'incrby' command");
        return;
    }
    int64_t increment = 0;
    try
    {
        increment = std::stoll(args[2].toString());
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR value is not an integer or out of range");
        return;
    }
    std::string key = args[1].toString();
    try
    {
        response.type = RespType::Integer;
        response.value = _db.incrby(key, increment);
    }
    catch (const std::exception& e)
    {
        response.type = RespType::Error;
        response.value = std::string(e.what());
    }
}

void Server::PC_Decr(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'decr' command");
        return;
    }
    std::string key = args[1].toString();
    try
    {
        response.type = RespType::Integer;
        response.value = _db.decr(key);
    }
    catch (const std::exception& e)
    {
        response.type = RespType::Error;
        response.value = std::string(e.what());
    }
}

void Server::PC_DecrBy(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'decrby' command");
        return;
    }
    int64_t decrement = 0;
    try
    {
        decrement = std::stoll(args[2].toString());
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR value is not an integer or out of range");
        return;
    }
    std::string key = args[1].toString();
    try
    {
        response.type = RespType::Integer;
        response.value = _db.decrby(key, decrement);
    }
    catch (const std::exception& e)
    {
        response.type = RespType::Error;
        response.value = std::string(e.what());
    }
}

void Server::PC_Type(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'type' command");
        return;
    }
    std::string key = args[1].toString();
    EntryType type = _db.getType(key);
    response.type = RespType::SimpleString;
    switch (type)
    {
    case EntryType::NONE:
        response.value = std::string_view("none");
        break;
    case EntryType::STRING:
        response.value = std::string_view("string");
        break;
    case EntryType::LIST:
        response.value = std::string_view("list");
        break;
    case EntryType::SET:
        response.value = std::string_view("set");
        break;
    case EntryType::ZSET:
        response.value = std::string_view("zset");
        break;
    case EntryType::HASH:
        response.value = std::string_view("hash");
        break;
    }
}

void Server::PC_Expire(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'expire' command");
        return;
    }
    try
    {
        std::string key = args[1].toString();
        int64_t seconds = std::stoll(args[2].toString());
        bool ok = _db.expire(key, seconds * 1000);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR value is not an integer or out of range");
    }
}

void Server::PC_PExpire(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'pexpire' command");
        return;
    }
    try
    {
        std::string key = args[1].toString();
        int64_t ms = std::stoll(args[2].toString());
        bool ok = _db.expire(key, ms);
        response.type = RespType::Integer;
        response.value = ok ? int64_t(1) : int64_t(0);
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR value is not an integer or out of range");
    }
}

void Server::PC_Ttl(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'ttl' command");
        return;
    }
    int64_t pttl = _db.pttl(args[1].toString());
    response.type = RespType::Integer;
    response.value = (pttl >= 0) ? (pttl / 1000) : pttl;
}

void Server::PC_PTtl(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'pttl' command");
        return;
    }
    response.type = RespType::Integer;
    response.value = _db.pttl(args[1].toString());
}

void Server::PC_Persist(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'persist' command");
        return;
    }
    bool ok = _db.persist(args[1].toString());
    response.type = RespType::Integer;
    response.value = ok ? int64_t(1) : int64_t(0);
}

void Server::PC_Ping(const std::vector<RespValue>& args, RespValue& response)
{
    response.type = RespType::SimpleString;
    if (args.size() > 1)
        response.value = args[1].toString();
    else
        response.value = std::string_view("PONG");
}

void Server::PC_Echo(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'echo' command");
        return;
    }
    response.type = RespType::BulkString;
    response.value = args[1].toString();
}

void Server::PC_Set(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'set' command");
        return;
    }
    std::string key = args[1].toString();
    std::string value = args[2].toString();

    int64_t ttlMs = -1;
    for (size_t i = 3; i < args.size(); i++)
    {
        std::string opt = args[i].toString();
        for (char& c : opt)
            c = toupper(c);

        if (opt == "EX" && i + 1 < args.size())
        {
            try
            {
                ttlMs = std::stoll(args[++i].toString()) * 1000;
            }
            catch (...)
            {
            }
        }
        else if (opt == "PX" && i + 1 < args.size())
        {
            try
            {
                ttlMs = std::stoll(args[++i].toString());
            }
            catch (...)
            {
            }
        }
    }

    _db.set(key, value, ttlMs);
    response.type = RespType::SimpleString;
    response.value = std::string_view("OK");
}

void Server::PC_Get(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'get' command");
        return;
    }
    std::string key = args[1].toString();
    const std::string* val = _db.get(key);

    if (val)
    {
        response.type = RespType::BulkString;
        response.value = std::string_view(val->data(), val->size());
    }
    else
    {
        response.type = RespType::Null;
    }
}

void Server::PC_ZAdd(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 4 || (args.size() - 2) % 2 != 0)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zadd' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t addedCount = 0;

    for (size_t i = 2; i + 1 < args.size(); i += 2)
    {
        try
        {
            double score = std::stod(args[i].toString());
            std::string member = args[i + 1].toString();
            if (_db.zadd(key, score, member))
                addedCount++;
        }
        catch (...)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR value is not a valid float");
            return;
        }
    }
    response.type = RespType::Integer;
    response.value = (int64_t)addedCount;
}

void Server::PC_ZRem(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zrem' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t removedCount = 0;
    for (size_t i = 2; i < args.size(); i++)
    {
        if (_db.zrem(key, args[i].toString()))
            removedCount++;
    }
    response.type = RespType::Integer;
    response.value = (int64_t)removedCount;
}

void Server::PC_ZCard(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zcard' command");
        return;
    }
    response.type = RespType::Integer;
    response.value = _db.zcard(args[1].toString());
}

void Server::PC_ZScore(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zscore' command");
        return;
    }
    auto score = _db.zscore(args[1].toString(), args[2].toString());
    if (score)
    {
        response.type = RespType::BulkString;
        std::string s = std::to_string(*score);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (s.back() == '.')
            s.pop_back();
        response.value = s;
    }
    else
    {
        response.type = RespType::Null;
    }
}

void Server::PC_ZRange(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 4)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zrange' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t start = 0, stop = 0;
    try
    {
        start = std::stoll(args[2].toString());
        stop = std::stoll(args[3].toString());
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR value is not an integer or out of range");
        return;
    }

    bool withScores = false;
    if (args.size() > 4)
    {
        std::string opt = args[4].toString();
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
        m.value = std::string_view(item.member);
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
    response.setArray(std::move(arr));
}

void Server::PC_ZRangeByScore(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 4)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zrangebyscore' command");
        return;
    }
    std::string key = args[1].toString();
    double min, max;
    try
    {
        min = std::stod(args[2].toString());
        max = std::stod(args[3].toString());
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR min or max is not a float");
        return;
    }

    auto result = _db.zrangebyscore(key, min, max);
    response.type = RespType::Array;
    std::vector<RespValue> arr;
    for (const auto& item : result)
    {
        RespValue m;
        m.type = RespType::BulkString;
        m.value = std::string_view(item.member);
        arr.push_back(m);
    }
    response.setArray(std::move(arr));
}

void Server::PC_ZRank(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'zrank' command");
        return;
    }
    std::string key = args[1].toString();
    std::string member = args[2].toString();
    auto rank = _db.zrank(key, member);
    if (rank)
    {
        response.type = RespType::Integer;
        response.value = (int64_t)*rank;
    }
    else
    {
        response.type = RespType::Null;
    }
}

// ...

void Server::PC_HSet(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 4)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hset' command");
        return;
    }
    std::string key = args[1].toString();
    std::string field = args[2].toString();
    std::string value = args[3].toString();
    int result = _db.hset(key, field, value);
    if (result < 0)
    {
        response.type = RespType::Error;
        response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    response.type = RespType::Integer;
    response.value = result ? int64_t(1) : int64_t(0);
}

void Server::PC_HGet(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hget' command");
        return;
    }
    std::string key = args[1].toString();
    std::string field = args[2].toString();
    auto val = _db.hget(key, field);
    if (val)
    {
        response.type = RespType::BulkString;
        response.value = *val;
    }
    else
    {
        response.type = RespType::Null;
    }
}

void Server::PC_HDel(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hdel' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t deleted = 0;
    for (size_t i = 2; i < args.size(); i++)
    {
        if (_db.hdel(key, args[i].toString()))
            deleted++;
    }
    response.type = RespType::Integer;
    response.value = deleted;
}

void Server::PC_HGetAll(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hgetall' command");
        return;
    }
    std::string key = args[1].toString();
    auto result = _db.hgetall(key);
    response.type = RespType::Array;
    std::vector<RespValue> arr;
    for (const auto& [field, value] : result)
    {
        RespValue f, v;
        f.type = RespType::BulkString;
        f.value = std::string_view(field);
        v.type = RespType::BulkString;
        v.value = std::string_view(value);
        arr.push_back(f);
        arr.push_back(v);
    }
    response.setArray(std::move(arr));
}

void Server::PC_HLen(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hlen' command");
        return;
    }
    response.type = RespType::Integer;
    response.value = (int64_t)_db.hlen(args[1].toString());
}

void Server::PC_HMSet(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 4 || (args.size() - 2) % 2 != 0)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hmset' command");
        return;
    }
    std::string key = args[1].toString();
    for (size_t i = 2; i + 1 < args.size(); i += 2)
    {
        _db.hset(key, args[i].toString(), args[i + 1].toString());
    }
    response.type = RespType::SimpleString;
    response.value = std::string_view("OK");
}

void Server::PC_HMGet(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'hmget' command");
        return;
    }
    std::string key = args[1].toString();
    response.type = RespType::Array;
    std::vector<RespValue> arr;
    for (size_t i = 2; i < args.size(); i++)
    {
        auto val = _db.hget(key, args[i].toString());
        RespValue r;
        if (val)
        {
            r.type = RespType::BulkString;
            r.value = *val;
        }
        else
            r.type = RespType::Null;
        arr.push_back(r);
    }
    response.setArray(std::move(arr));
}

void Server::PC_LPush(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'lpush' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t len = 0;
    for (size_t i = 2; i < args.size(); i++)
    {
        len = _db.lpush(key, args[i].toString());
        if (len < 0)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
    }
    response.type = RespType::Integer;
    response.value = len;
}

void Server::PC_RPush(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'rpush' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t len = 0;
    for (size_t i = 2; i < args.size(); i++)
    {
        len = _db.rpush(key, args[i].toString());
        if (len < 0)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
    }
    response.type = RespType::Integer;
    response.value = len;
}

void Server::PC_LPop(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'lpop' command");
        return;
    }
    auto val = _db.lpop(args[1].toString());
    if (val)
    {
        response.type = RespType::BulkString;
        response.value = *val;
    }
    else
    {
        response.type = RespType::Null;
    }
}

void Server::PC_RPop(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'rpop' command");
        return;
    }
    auto val = _db.rpop(args[1].toString());
    if (val)
    {
        response.type = RespType::BulkString;
        response.value = *val;
    }
    else
    {
        response.type = RespType::Null;
    }
}

void Server::PC_LLen(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'llen' command");
        return;
    }
    response.type = RespType::Integer;
    response.value = _db.llen(args[1].toString());
}

void Server::PC_LRange(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 4)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'lrange' command");
        return;
    }
    int64_t start = 0, stop = 0;
    try
    {
        start = std::stoll(args[2].toString());
        stop = std::stoll(args[3].toString());
    }
    catch (...)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR value is not an integer or out of range");
        return;
    }

    auto items = _db.lrange(args[1].toString(), start, stop);
    response.type = RespType::Array;
    std::vector<RespValue> arr;
    for (const auto& item : items)
    {
        RespValue v;
        v.type = RespType::BulkString;
        v.value = item;
        arr.push_back(v);
    }
    response.setArray(std::move(arr));
}

void Server::PC_SAdd(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'sadd' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t added = 0;
    for (size_t i = 2; i < args.size(); i++)
    {
        int r = _db.sadd(key, args[i].toString());
        if (r < 0)
        {
            response.type = RespType::Error;
            response.value = std::string_view("WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        added += r;
    }
    response.type = RespType::Integer;
    response.value = added;
}

void Server::PC_SRem(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'srem' command");
        return;
    }
    std::string key = args[1].toString();
    int64_t removed = 0;
    for (size_t i = 2; i < args.size(); i++)
    {
        removed += _db.srem(key, args[i].toString());
    }
    response.type = RespType::Integer;
    response.value = removed;
}

void Server::PC_SIsMember(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'sismember' command");
        return;
    }
    int result = _db.sismember(args[1].toString(), args[2].toString());
    response.type = RespType::Integer;
    response.value = (int64_t)result;
}

void Server::PC_SMembers(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'smembers' command");
        return;
    }
    auto members = _db.smembers(args[1].toString());
    response.type = RespType::Array;
    std::vector<RespValue> arr;
    for (const auto& member : members)
    {
        RespValue v;
        v.type = RespType::BulkString;
        v.value = member;
        arr.push_back(v);
    }
    response.setArray(std::move(arr));
}

void Server::PC_SCard(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'scard' command");
        return;
    }
    response.type = RespType::Integer;
    response.value = _db.scard(args[1].toString());
}

// ...

void Server::PC_Client(const std::vector<RespValue>& args, RespValue& response)
{
    response.type = RespType::SimpleString;
    response.value = std::string_view("OK");
}

void Server::PC_FlushAll(const std::vector<RespValue>& args, RespValue& response)
{
    _db.clear();
    response.type = RespType::SimpleString;
    response.value = std::string_view("OK");
}

void Server::PC_Config(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'config' command");
        return;
    }

    std::string subcmd = args[1].toString();
    for (char& c : subcmd)
        c = toupper(c);

    if (subcmd == "GET")
    {
        if (args.size() < 3)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'config get' command");
            return;
        }
        std::string param = args[2].toString();
        if (param == "appendfsync-interval")
        {
            response.type = RespType::Array;
            std::vector<RespValue> respArr;
            RespValue k, v;
            k.type = RespType::BulkString;
            k.value = std::string_view("appendfsync-interval");
            v.type = RespType::BulkString;
            v.value = std::to_string(_persistence ? _persistence->GetFlushInterval() : 0);
            respArr.push_back(k);
            respArr.push_back(v);
            response.setArray(std::move(respArr));
        }
        else
        {
            response.type = RespType::Array;
            response.setArray({});
        }
    }
    else if (subcmd == "SET")
    {
        if (args.size() < 4)
        {
            response.type = RespType::Error;
            response.value = std::string_view("ERR wrong number of arguments for 'config set' command");
            return;
        }
        std::string param = args[2].toString();
        std::string value = args[3].toString();
        if (param == "appendfsync-interval")
        {
            try
            {
                int interval = std::stoi(value);
                if (_persistence)
                    _persistence->SetFlushInterval(interval);
                response.type = RespType::SimpleString;
                response.value = std::string_view("OK");
            }
            catch (...)
            {
                response.type = RespType::Error;
                response.value = std::string_view("ERR invalid value for 'appendfsync-interval'");
            }
        }
        else
        {
            response.type = RespType::SimpleString;
            response.value = std::string_view("OK");
        }
    }
    else
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR unknown sub-command for 'config'");
    }
}

void Server::PC_BgRewriteAof(const std::vector<RespValue>& args, RespValue& response)
{
    if (!_persistence)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR persistence is disabled");
        return;
    }

    if (_persistence->IsRewriting())
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR background rewrite already in progress");
        return;
    }

    if (_persistence->StartRewrite(_db))
    {
        response.type = RespType::SimpleString;
        response.value = std::string_view("Background append only file rewriting started");
    }
    else
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR failed to start background rewrite");
    }
}

void Server::PC_Keys(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'keys' command");
        return;
    }
    auto keys = _db.keys(args[1].toString());
    std::vector<RespValue> arr;
    arr.reserve(keys.size());
    for (auto& k : keys)
    {
        RespValue v;
        v.type = RespType::BulkString;
        v.value = std::move(k);
        arr.push_back(std::move(v));
    }
    response.type = RespType::Array;
    response.setArray(std::move(arr));
}

void Server::PC_Exists(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() < 2)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'exists' command");
        return;
    }
    int64_t count = 0;
    for (size_t i = 1; i < args.size(); i++)
    {
        if (_db.exists(args[i].toString()))
            count++;
    }
    response.type = RespType::Integer;
    response.value = count;
}

void Server::PC_Rename(const std::vector<RespValue>& args, RespValue& response)
{
    if (args.size() != 3)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR wrong number of arguments for 'rename' command");
        return;
    }
    bool ok = _db.rename(args[1].toString(), args[2].toString());
    if (!ok)
    {
        response.type = RespType::Error;
        response.value = std::string_view("ERR no such key");
        return;
    }
    response.type = RespType::SimpleString;
    response.value = std::string_view("OK");
}

int main(int argc, char** argv)
{
    Output::GetInstance()->Init("redis_server");
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    if (!Server::Get()->Init())
        return 1;

    Server::Get()->Run();

    return 0;
}