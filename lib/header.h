#pragma once

#define TCP_MAX_MESSAGE_SIZE 8*1024

struct header
{
    unsigned int length;
    unsigned int type;
} __attribute__((packed, aligned(8)));