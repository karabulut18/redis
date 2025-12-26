#include "DynamicBuffer.h"

void DynamicBuffer::append(const char* data, size_t size)
{
    _buffer.insert(_buffer.end(), data, data + size);
}

void DynamicBuffer::consume(size_t size)
{
    if (size > _buffer.size())
    {
        _buffer.clear();
    }
    else
    {
        _buffer.erase(_buffer.begin(), _buffer.begin() + size);
    }
}

const char* DynamicBuffer::data() const
{
    return _buffer.data();
}

size_t DynamicBuffer::size() const
{
    return _buffer.size();
}

void DynamicBuffer::clear()
{
    _buffer.clear();
}