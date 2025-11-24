#pragma once

#include "../lib/ITcpConnection.h"

class TcpConnection;

class Client : public ITcpConnection
{
    int _id = 0;
public:
    Client(int id, TcpConnection* connection);
    ~Client() override;

    void OnMessageReceive(const char* buffer, m_size_t size) override;
    void OnDisconnect() override;
    void Send(const char* c, ssize_t size);
private:
    TcpConnection* _connection = nullptr;
};