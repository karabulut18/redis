#pragma once

#include "../lib/redis/RespParser.h"
#include <string>
#include <vector>

class Client;

/**
 * A self-contained command ready for server-side execution.
 * The request is a RespValue (usually an Array) that anchors the
 * memory from the network buffer.
 */
struct Command
{
    RespValue request;
    Client* client = nullptr;
};
