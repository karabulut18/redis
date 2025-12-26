#pragma once

#include "../lib/ITcpServer.h"
#include "DataBase.h"
#include <map>

#define PORT 8089

class TcpServer;
class TcpConnection;
class Client;

class Server : public ITcpServer
{
    TcpServer* _tcpServer = nullptr;
    Server();
    ~Server() override;

    DataBase _dataBase;
    std::map<int, Client*> _clients;

public:
    ITcpConnection* AcceptConnection(int id, TcpConnection* connection) override;
    static Server* Get();
    void OnClientDisconnect(int id);
    void Run();

    bool IsRunning();
    void Stop();
    bool Init();
};