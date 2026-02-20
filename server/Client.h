#pragma once

#include "../lib/common/ITcpConnection.h"

#include "../lib/common/LockFreeRingBuffer.h"
#include "../lib/common/SegmentedBuffer.h"
#include "Command.h"
#include <string>
#include <unordered_set>

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

    bool isSubscribed() const
    {
        return !subscribedChannels.empty();
    }
    void addSubscription(const std::string& channel)
    {
        subscribedChannels.insert(channel);
    }
    bool removeSubscription(const std::string& channel)
    {
        return subscribedChannels.erase(channel) > 0;
    }
    const std::unordered_set<std::string>& getSubscriptions() const
    {
        return subscribedChannels;
    }

private:
    std::unordered_set<std::string> subscribedChannels;
    TcpConnection* _connection = nullptr;
    RespParser* _parser = nullptr;
    SegmentedBuffer _inBuffer;

    // Per-client lock-free queue (capacity 1024 commands)
    LockFreeRingBuffer<Command>* _commandQueue = nullptr;
};