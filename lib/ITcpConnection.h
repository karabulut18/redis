#pragma once 
#include <string>
#include "frame_header.h"


class ITcpConnection
{
public:
    virtual void OnMessageReceive(const char* buffer, m_size_t length) = 0;
    virtual void OnDisconnect() = 0;
    virtual ~ITcpConnection() = default;
};