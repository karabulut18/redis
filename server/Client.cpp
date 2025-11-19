#include "Client.h"
#include "../lib/TcpConnection.h"
#include "Server.h"
#include "../msg/types.h"
#include "../msg/shortString.h"


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


void Client::OnMessage(char* buffer, ssize_t length)
{
    const header* hdr = reinterpret_cast<const header*>(buffer);
    switch(hdr->type)
    {
        case msg::SHORT_STRING:
        {
            msg::shortString* msg = reinterpret_cast<msg::shortString*>(buffer);
            printf("message:\n%s\n", msg->_buffer);
        }
        default:
            break;
    };
}

void Client::OnDisconnect()
{
    // this might be a if the connection is closed by deleting the client
    // or the thread has to wait in Stop call
    printf("Client disconnected\n");
    Server::Get()->OnClientDisconnect(_id);
}