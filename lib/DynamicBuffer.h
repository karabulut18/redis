#pragma once

#include <vector>
#include "frame_header.h"

// data is serialized as blocks: [size_t payload_size] [char* payload_data]
// This struct helps in managing a byte stream to extract these frames.

struct DynamicBuffer
{
    std::vector<char> _buffer;
    void append(const char* data, m_size_t size);

    bool canConsumeFrame() const;
    bool canConsumeFrame(m_size_t& frameSize) const;
    m_size_t peekFrameSize() const;
    const char* peekFramePtr() const;
    void consume(m_size_t size); // consume per frame for avoding undefined behivour
};