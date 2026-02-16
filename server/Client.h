#pragma once

#include "../lib/common/ITcpConnection.h"
#include <vector>

class TcpConnection;
class RespParser;
struct RespValue;

class Client : public ITcpConnection
{
    int _id = 0;

public:
    Client(int id, TcpConnection* connection);
    ~Client() override;

    size_t OnMessageReceive(const char* buffer, m_size_t size) override;
    void OnDisconnect() override;
    void Send(const char* c, ssize_t size);
    void Ping();

private:
    void SendResponse(const RespValue& response);
    void HandleCommand(const std::vector<RespValue>& args);

    TcpConnection* _connection = nullptr;
    RespParser* _parser = nullptr;
};