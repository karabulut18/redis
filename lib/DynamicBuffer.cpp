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

bool DynamicBuffer::canConsumeFrame(m_size_t& frameSize) const
{
    frameSize = 0;
    if(_buffer.size() < SIZE_VARIABLE_LENGTH)
        return false;
    memcpy(&frameSize, _buffer.data(), SIZE_VARIABLE_LENGTH);

    if(_buffer.size() >= SIZE_VARIABLE_LENGTH + frameSize)
        return true;
 
    return false;
}

bool DynamicBuffer::canConsumeFrame() const
{
    m_size_t size = 0;
    if(_buffer.size() < SIZE_VARIABLE_LENGTH)
        return false;
    memcpy(&size, _buffer.data(), SIZE_VARIABLE_LENGTH);

    if(_buffer.size() >= SIZE_VARIABLE_LENGTH + size)
        return true;
 
    return false;
}

const char* DynamicBuffer::peekFramePtr() const
{
    m_size_t frameSize = 0;
    if(!canConsumeFrame(frameSize))
        return nullptr;

    if(frameSize == 0)
        return nullptr;
    
    return _buffer.data();
}

void DynamicBuffer::consume(m_size_t size)
{
    _buffer.erase(_buffer.begin(), _buffer.begin() + size);
}