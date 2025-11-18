#include "../lib/ITcpConnection.h"

class TcpConnection;


class Client : public ITcpConnection
{
    TcpConnection* _connection = nullptr;
    Client();
public:
    void OnMessage(char* buffer, ssize_t length) override;
    void OnDisconnect() override;
    ~Client() override;

    static Client* Get();

    bool Init();
    bool IsRunning();
    void Stop();
    void Run();

    void Heartbeat();
};