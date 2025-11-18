#pragma once

class ITcpConnection;
class TcpConnection;


class ITcpServer
{
public:
    virtual ~ITcpServer() = default;
    virtual ITcpConnection* AcceptConnection(int id, TcpConnection* connection) = 0;
};