#pragma once

#include "../lib/common/ITcpServer.h"
#include "../lib/redis/CommandIds.h"
#include "../lib/redis/Database.h"
#include "../lib/redis/Persistence.h"
#include "../lib/redis/RespParser.h"
#include "Command.h"
#include <map>
#include <mutex>
#include <vector>

class TcpServer;
class TcpConnection;
class Client;
struct RespValue;

class Server : public ITcpServer
{
    TcpServer* _tcpServer;
    Server();
    ~Server() override;

    std::map<int, Client*> _clients;
    std::mutex _clientsMutex;
    Database _db;
    Persistence* _persistence = nullptr;

public:
    ITcpConnection* AcceptConnection(int id, TcpConnection* connection) override;
    static Server* Get();
    void OnClientDisconnect(int id);
    void Run();

    // Command Processing
    void ProcessCommands();
    void HandleCommand(Client* client, const RespValue& request);
    void QueueResponse(Client* client, const RespValue& response);

private:
    // Command Handlers
    void HandlePing(const std::vector<RespValue>& args, RespValue& response);
    void HandleEcho(const std::vector<RespValue>& args, RespValue& response);
    void HandleSet(const std::vector<RespValue>& args, RespValue& response);
    void HandleGet(const std::vector<RespValue>& args, RespValue& response);
    void HandleDel(const std::vector<RespValue>& args, RespValue& response);
    void HandleExpire(const std::vector<RespValue>& args, RespValue& response);
    void HandlePExpire(const std::vector<RespValue>& args, RespValue& response);
    void HandleTtl(const std::vector<RespValue>& args, RespValue& response);
    void HandlePTtl(const std::vector<RespValue>& args, RespValue& response);
    void HandlePersist(const std::vector<RespValue>& args, RespValue& response);
    void HandleIncr(const std::vector<RespValue>& args, RespValue& response);
    void HandleIncrBy(const std::vector<RespValue>& args, RespValue& response);
    void HandleDecr(const std::vector<RespValue>& args, RespValue& response);
    void HandleDecrBy(const std::vector<RespValue>& args, RespValue& response);
    void HandleType(const std::vector<RespValue>& args, RespValue& response);
    void HandleZAdd(const std::vector<RespValue>& args, RespValue& response);
    void HandleZRem(const std::vector<RespValue>& args, RespValue& response);
    void HandleZScore(const std::vector<RespValue>& args, RespValue& response);
    void HandleZRank(const std::vector<RespValue>& args, RespValue& response);
    void HandleZRange(const std::vector<RespValue>& args, RespValue& response);
    void HandleZRangeByScore(const std::vector<RespValue>& args, RespValue& response);
    void HandleZCard(const std::vector<RespValue>& args, RespValue& response);
    void HandleHSet(const std::vector<RespValue>& args, RespValue& response);
    void HandleHGet(const std::vector<RespValue>& args, RespValue& response);
    void HandleHDel(const std::vector<RespValue>& args, RespValue& response);
    void HandleHGetAll(const std::vector<RespValue>& args, RespValue& response);
    void HandleHLen(const std::vector<RespValue>& args, RespValue& response);
    void HandleHMSet(const std::vector<RespValue>& args, RespValue& response);
    void HandleHMGet(const std::vector<RespValue>& args, RespValue& response);
    void HandleLPush(const std::vector<RespValue>& args, RespValue& response);
    void HandleRPush(const std::vector<RespValue>& args, RespValue& response);
    void HandleLPop(const std::vector<RespValue>& args, RespValue& response);
    void HandleRPop(const std::vector<RespValue>& args, RespValue& response);
    void HandleLLen(const std::vector<RespValue>& args, RespValue& response);
    void HandleLRange(const std::vector<RespValue>& args, RespValue& response);
    void HandleSAdd(const std::vector<RespValue>& args, RespValue& response);
    void HandleSRem(const std::vector<RespValue>& args, RespValue& response);
    void HandleSIsMember(const std::vector<RespValue>& args, RespValue& response);
    void HandleSMembers(const std::vector<RespValue>& args, RespValue& response);
    void HandleSCard(const std::vector<RespValue>& args, RespValue& response);
    void HandleClient(const std::vector<RespValue>& args, RespValue& response);
    void HandleFlushAll(const std::vector<RespValue>& args, RespValue& response);
    void HandleConfig(const std::vector<RespValue>& args, RespValue& response);
    void HandleBgRewriteAof(const std::vector<RespValue>& args, RespValue& response);

public:
    bool IsRunning();
    void Stop();
    bool Init();
};