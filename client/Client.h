#include "../lib/ITcpConnection.h"

class TcpConnection;
class RespParser;

class Client : public ITcpConnection
{
    TcpConnection* _connection = nullptr;
    RespParser* _parser = nullptr;
    Client();

public:
    size_t OnMessageReceive(const char* buffer, m_size_t length) override;
    void OnDisconnect() override;
    ~Client() override;

    static Client* Get();

    bool Init();
    bool IsRunning();
    void Stop();
    void Run();
    void Send(const char* c, ssize_t size);
    void Ping();
};