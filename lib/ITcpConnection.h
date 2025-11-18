#pragma once 
#include <string>

class ITcpConnection
{
public:
    virtual void OnMessage(char* buffer, ssize_t length) = 0;
    virtual void OnDisconnect() = 0;
    virtual ~ITcpConnection() = default;
};