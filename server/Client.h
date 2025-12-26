#pragma once

#include "../lib/ITcpConnection.h"

class TcpConnection;
class RespParser;

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
    TcpConnection* _connection = nullptr;
    RespParser* _parser = nullptr;
};