#pragma once

#include <vector>
#include "header.h"

// data is serialized as blocks: [size_t payload_size] [char* payload_data]
// This struct helps in managing a byte stream to extract these frames.

struct DynamicBuffer
{
    std::vector<char> _buffer;
    void append(const char* data, m_size_t size);

    bool canConsume() const;
    m_size_t peekFrameSize() const;
    m_size_t peekFrame(char* data) const;
    const char* peekFramePtr() const;
    m_size_t consumeframe(char* data);
    void consume(m_size_t size);
};