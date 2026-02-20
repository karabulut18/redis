#pragma once

#include "../lib/common/ITcpServer.h"
#include "../lib/common/LockFreeRingBuffer.h"
#include "../lib/redis/CommandIds.h"
#include "../lib/redis/Config.h"
#include "../lib/redis/Database.h"
#include "../lib/redis/Persistence.h"
#include "../lib/redis/RespParser.h"
#include "Command.h"
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TcpServer;
class TcpConnection;
class Client;
struct RespValue;

class Server : public ITcpServer
{
    TcpServer* _tcpServer;
    ~Server() override;

    std::map<int, Client*> _clients;
    std::mutex _clientsMutex; // Guards AcceptConnection (I/O thread inserts)
    // Deferred disconnect queue: I/O thread pushes Client* pointers here;
    // main thread drains, verifies identity, and deletes them.
    LockFreeRingBuffer<Client*> _pendingDisconnects{256};
    // Stale clients evicted by AcceptConnection during FD reuse: main thread
    // deletes these separate from _pendingDisconnects so _clients stays consistent.
    LockFreeRingBuffer<Client*> _staleClientsToDelete{256};
    // Wakeup condvar: I/O thread signals after enqueuing a command so the main
    // thread wakes immediately instead of waiting up to 1ms.
    std::mutex _wakeupMutex;
    std::condition_variable _wakeupCv;
    Database _db;
    Persistence* _persistence = nullptr;

public:
    ITcpConnection* AcceptConnection(int id, TcpConnection* connection) override;
    static Server* Get();
    // Must be called once before Get() to configure port / AOF path.
    // If not called, defaults (port 6379, appendonly.aof) are used.
    static void InitFromConfig(const ServerConfig& cfg);
    void OnClientDisconnect(Client* client);
    void Run();

    // Command Processing
    void ProcessCommands();
    // Pub/Sub Implementation
    std::unordered_map<std::string, std::unordered_set<Client*>> _pubsubChannels;

    void ProcessCommand(Client* client, const RespValue& request);
    void QueueResponse(Client* client, const RespValue& response);
    void WakeUp(); // Called by I/O thread after enqueuing a command

private:
    // Process Commands
    void PC_Ping(const std::vector<RespValue>& args, RespValue& response);
    void PC_Echo(const std::vector<RespValue>& args, RespValue& response);
    void PC_Set(const std::vector<RespValue>& args, RespValue& response);
    void PC_Get(const std::vector<RespValue>& args, RespValue& response);
    void PC_Del(const std::vector<RespValue>& args, RespValue& response);
    void PC_Expire(const std::vector<RespValue>& args, RespValue& response);
    void PC_PExpire(const std::vector<RespValue>& args, RespValue& response);
    void PC_Ttl(const std::vector<RespValue>& args, RespValue& response);
    void PC_PTtl(const std::vector<RespValue>& args, RespValue& response);
    void PC_Persist(const std::vector<RespValue>& args, RespValue& response);
    void PC_Incr(const std::vector<RespValue>& args, RespValue& response);
    void PC_IncrBy(const std::vector<RespValue>& args, RespValue& response);
    void PC_Decr(const std::vector<RespValue>& args, RespValue& response);
    void PC_DecrBy(const std::vector<RespValue>& args, RespValue& response);
    void PC_Type(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZAdd(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZRem(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZScore(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZRank(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZRange(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZRangeByScore(const std::vector<RespValue>& args, RespValue& response);
    void PC_ZCard(const std::vector<RespValue>& args, RespValue& response);
    void PC_HSet(const std::vector<RespValue>& args, RespValue& response);
    void PC_HGet(const std::vector<RespValue>& args, RespValue& response);
    void PC_HDel(const std::vector<RespValue>& args, RespValue& response);
    void PC_HGetAll(const std::vector<RespValue>& args, RespValue& response);
    void PC_HLen(const std::vector<RespValue>& args, RespValue& response);
    void PC_HMSet(const std::vector<RespValue>& args, RespValue& response);
    void PC_HMGet(const std::vector<RespValue>& args, RespValue& response);
    void PC_LPush(const std::vector<RespValue>& args, RespValue& response);
    void PC_RPush(const std::vector<RespValue>& args, RespValue& response);
    void PC_LPop(const std::vector<RespValue>& args, RespValue& response);
    void PC_RPop(const std::vector<RespValue>& args, RespValue& response);
    void PC_LLen(const std::vector<RespValue>& args, RespValue& response);
    void PC_LRange(const std::vector<RespValue>& args, RespValue& response);
    void PC_SAdd(const std::vector<RespValue>& args, RespValue& response);
    void PC_SRem(const std::vector<RespValue>& args, RespValue& response);
    void PC_SIsMember(const std::vector<RespValue>& args, RespValue& response);
    void PC_SMembers(const std::vector<RespValue>& args, RespValue& response);
    void PC_SCard(const std::vector<RespValue>& args, RespValue& response);
    void PC_Client(const std::vector<RespValue>& args, RespValue& response);
    void PC_FlushAll(const std::vector<RespValue>& args, RespValue& response);
    void PC_Config(const std::vector<RespValue>& args, RespValue& response);
    void PC_BgRewriteAof(const std::vector<RespValue>& args, RespValue& response);
    void PC_Save(const std::vector<RespValue>& args, RespValue& response);
    void PC_BgSave(const std::vector<RespValue>& args, RespValue& response);
    void PC_Keys(const std::vector<RespValue>& args, RespValue& response);
    void PC_Exists(const std::vector<RespValue>& args, RespValue& response);
    void PC_Rename(const std::vector<RespValue>& args, RespValue& response);
    void PC_MGet(const std::vector<RespValue>& args, RespValue& response);
    void PC_MSet(const std::vector<RespValue>& args, RespValue& response);
    void PC_Object(const std::vector<RespValue>& args, RespValue& response);

    // Pub/Sub
    void PC_Subscribe(Client* client, const std::vector<RespValue>& args, RespValue& response);
    void PC_Unsubscribe(Client* client, const std::vector<RespValue>& args, RespValue& response);
    void PC_Publish(Client* client, const std::vector<RespValue>& args, RespValue& response);
    void SendPubSubMessage(Client* client, const std::string& type, const std::string& channel,
                           const RespValue& payload);
    void CleanupClientPubSub(Client* client);

private:
    Server(int port, const std::string& aofFilename, int flushInterval);

public:
    bool Init();
    bool IsRunning();
    void Stop();
};