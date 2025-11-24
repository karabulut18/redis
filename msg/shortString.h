#pragma once

#include "header.h" 
#include "types.h"

#define SHORT_STR_BUFFER_SIZE 250

namespace msg
{

struct shortString
{
    header          _header;
    char            _buffer[SHORT_STR_BUFFER_SIZE];
    shortString() : _header(sizeof(shortString), msg::SHORT_STRING, MSG_VERSION)
    {
    }
} __attribute__((packed, aligned(8)));

}
