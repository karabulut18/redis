#pragma once

#include <cstdint>

#define TCP_MAX_MESSAGE_SIZE 8*1024
#define SIZE_VARIABLE_LENGTH sizeof(m_size_t)

typedef uint32_t m_size_t;

struct frame_header
{
    m_size_t length;
    frame_header(m_size_t length): length(length) {};
} __attribute__((packed, aligned(8)));