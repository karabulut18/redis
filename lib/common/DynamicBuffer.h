#pragma once

#include <cstddef>
#include <vector>

struct DynamicBuffer
{
    std::vector<char> _buffer;

    void append(const char* data, size_t size);
    void consume(size_t size);

    const char* data() const;
    size_t size() const;
    void clear();
};