#pragma once

#define TCP_MAX_MESSAGE_SIZE 8*1024
#define SIZE_VARIABLE_LENGTH sizeof(m_size_t)

typedef uint32_t m_size_t;

struct header
{
    uint32_t length;
    uint32_t type;
    header(uint32_t length, uint32_t type): length(length), type(type) {}
} __attribute__((packed, aligned(8)));