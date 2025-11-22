#pragma once 
#include <string>

class ITcpConnection
{
public:
    virtual void OnMessage(const char* buffer, m_size_t length) = 0;
    virtual void OnDisconnect() = 0;
    virtual ~ITcpConnection() = default;
};