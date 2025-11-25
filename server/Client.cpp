#include "Client.h"
#include "../lib/TcpConnection.h"
#include "Server.h"
#include "../msg/types.h"
#include "../msg/shortString.h"
#include "../lib/Output.h"
#include <string>


Client::Client(int id, TcpConnection* connection)
{
    _id = id;
    _connection = connection;
}

Client::~Client()
{
    if(_connection != nullptr && _connection->IsRunning())
        _connection->Stop(); // connection is not deleted here since it is not created here
};

void Client::Send(const char* c, ssize_t size)
{
    _connection->Send(c, size);
}

void Client::OnMessageReceive(const char* buffer, m_size_t size)
{
    const msg::header* hdr = reinterpret_cast<const msg::header*>(buffer);
    switch(hdr->_type)
    {
        case msg::SHORT_STRING:
        {
            const msg::shortString* msg = reinterpret_cast<const msg::shortString*>(buffer);
            PUTF("message:\n"+ std::string(msg->_buffer) + "\n");
        }
        default:
            break;
    };
}

void Client::OnDisconnect()
{
    // this might be a if the connection is closed by deleting the client
    // or the thread has to wait in Stop call
    PUTF_LN("Client disconnected: " + std::to_string(_id) + std::to_string(__LINE__));
    Server::Get()->OnClientDisconnect(_id);
}