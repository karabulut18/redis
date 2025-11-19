#pragma once

#include "../lib/header.h" 

#define SHORT_STR_BUFFER_SIZE 250

namespace msg
{

struct shortString
{
    header  _header;
    char    _buffer[SHORT_STR_BUFFER_SIZE];
    shortString() : _header(sizeof(shortString), 0)
    {
    }
} __attribute__((packed, aligned(8)));

}
