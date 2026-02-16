#pragma once

#include <string>
#include <vector>

class Client;

/**
 * A self-contained command ready for server-side execution.
 * Args are owned strings (copied from the client's network buffer)
 * so the command is safe to process after the buffer is freed.
 * The client pointer is used to route the response back.
 */
struct Command
{
    std::vector<std::string> args;
    Client* client = nullptr;
};
