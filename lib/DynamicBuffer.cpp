#include "DynamicBuffer.h"
#include <cstring>

void DynamicBuffer::append(const char* data, m_size_t size)
{
    _buffer.insert(_buffer.end(), data, data + size);
}

m_size_t DynamicBuffer::peekFrameSize() const
{
    if(_buffer.size() < SIZE_VARIABLE_LENGTH)
        return 0;

    m_size_t size = 0;
    memcpy(&size, _buffer.data(), SIZE_VARIABLE_LENGTH);
    return size;
}

bool DynamicBuffer::canConsume() const
{
    m_size_t size = 0;
    if(_buffer.size() < SIZE_VARIABLE_LENGTH)
        return false;
    memcpy(&size, _buffer.data(), SIZE_VARIABLE_LENGTH);

    if(_buffer.size() >= SIZE_VARIABLE_LENGTH + size)
        return true;
 
    return false;
}

m_size_t DynamicBuffer::peekFrame(char* data) const
{
    m_size_t size = canConsume();
    if(size == 0)
        return 0;
    
    memcpy(data, _buffer.data() + sizeof(m_size_t), size);
    return size;
}

const char* DynamicBuffer::peekFramePtr() const
{
    m_size_t size = canConsume();
    if(size == 0)
        return nullptr;
    
    return _buffer.data();
}


m_size_t DynamicBuffer::consumeframe(char* data)
{
    m_size_t size = canConsume();
    if(size == 0)
        return 0;
    
    peekFrame(data);
    consume(size);
    return size;
}

void DynamicBuffer::consume(m_size_t size)
{
    _buffer.erase(_buffer.begin(), _buffer.begin() + size);
}