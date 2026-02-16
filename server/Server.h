#pragma once

#include "../lib/common/ITcpServer.h"
#include "../lib/redis/Database.h"
#include "Command.h"
#include <map>

class TcpServer;
class TcpConnection;
class Client;
struct RespValue;

class Server : public ITcpServer
{
    TcpServer* _tcpServer = nullptr;
    Server();
    ~Server() override;

    std::map<int, Client*> _clients;
    Database _db;

public:
    ITcpConnection* AcceptConnection(int id, TcpConnection* connection) override;
    static Server* Get();
    void OnClientDisconnect(int id);
    void Run();

    // Command Processing
    void ProcessCommands();
    void HandleCommand(Client* client, const std::vector<std::string>& args);
    void QueueResponse(Client* client, const RespValue& response);
    void SendResponse(Client* client, const RespValue& response);

    bool IsRunning();
    void Stop();
    bool Init();
};