#pragma once

#define TCP_MAX_MESSAGE_SIZE 8*1024

struct header
{
    unsigned int length;
    unsigned int type;
    header(unsigned int length, unsigned int type): length(length), type(type) {}
} __attribute__((packed, aligned(8)));