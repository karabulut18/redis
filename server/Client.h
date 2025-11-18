#pragma once

#include "../lib/ITcpConnection.h"

class TcpConnection;

class Client : public ITcpConnection
{
    int _id = 0;
public:
    Client(int id, TcpConnection* connection);
    ~Client() override;

    void OnMessage(char* buffer, ssize_t length) override;
    void OnDisconnect() override;
    void Send(const char* c, ssize_t size);
private:
    TcpConnection* _connection = nullptr;
};