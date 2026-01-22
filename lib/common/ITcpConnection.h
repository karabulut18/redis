#pragma once
#include "frame_header.h"
#include <string>

class ITcpConnection
{
public:
    virtual size_t OnMessageReceive(const char *buffer, m_size_t length) = 0;
    virtual void OnDisconnect() = 0;
    virtual ~ITcpConnection() = default;
};