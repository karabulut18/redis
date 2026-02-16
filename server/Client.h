#pragma once

#include "../lib/common/ITcpConnection.h"

#include "../lib/common/LockFreeRingBuffer.h"
#include "Command.h"

class TcpConnection;
class RespParser;
struct RespValue;

class Client : public ITcpConnection
{
    int _id = 0;

public:
    Client(int id, TcpConnection* connection);
    ~Client() override;
    int GetId() const
    {
        return _id;
    }

    size_t OnMessageReceive(const char* buffer, m_size_t size) override;
    void OnDisconnect() override;
    void Send(const char* c, ssize_t size);
    void Ping();

    // Queue operations
    bool EnqueueCommand(Command cmd);
    bool DequeueCommand(Command& cmd);

    void SendResponse(const RespValue& response);
    std::string PrepareResponse(const RespValue& response);

private:
    TcpConnection* _connection = nullptr;
    RespParser* _parser = nullptr;

    // Per-client lock-free queue (capacity 1024 commands)
    LockFreeRingBuffer<Command>* _commandQueue = nullptr;
};