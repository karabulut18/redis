#include "LockFreeByteRingBuffer.h"

LockFreeByteRingBuffer::LockFreeByteRingBuffer(size_t capacity) : _capacity(capacity + 1), _head(0), _tail(0)
{
    _buffer = new char[_capacity];
}

LockFreeByteRingBuffer::~LockFreeByteRingBuffer()
{
    delete[] _buffer;
}

size_t LockFreeByteRingBuffer::write(const char* data, size_t len)
{
    const size_t current_tail = _tail.load(std::memory_order_relaxed);
    const size_t current_head = _head.load(std::memory_order_acquire);

    // Calculate available space
    size_t available;
    if (current_tail >= current_head)
        available = _capacity - current_tail + current_head - 1;
    else
        available = current_head - current_tail - 1;

    if (available == 0)
        return 0;

    const size_t to_write = (len < available) ? len : available;

    // Handle wrap-around
    if (current_tail + to_write <= _capacity)
        memcpy(_buffer + current_tail, data, to_write);
    else
    {
        const size_t first_chunk = _capacity - current_tail;
        memcpy(_buffer + current_tail, data, first_chunk);
        memcpy(_buffer, data + first_chunk, to_write - first_chunk);
    }

    _tail.store((current_tail + to_write) % _capacity, std::memory_order_release);
    return to_write;
}

size_t LockFreeByteRingBuffer::read(char* data, size_t len)
{
    const size_t current_head = _head.load(std::memory_order_relaxed);
    const size_t current_tail = _tail.load(std::memory_order_acquire);

    // Calculate available data
    size_t available;
    if (current_tail >= current_head)
        available = current_tail - current_head;
    else
        available = _capacity - current_head + current_tail;

    if (available == 0)
        return 0;

    const size_t to_read = (len < available) ? len : available;

    // Handle wrap-around
    if (current_head + to_read <= _capacity)
        memcpy(data, _buffer + current_head, to_read);
    else
    {
        const size_t first_chunk = _capacity - current_head;
        memcpy(data, _buffer + current_head, first_chunk);
        memcpy(data + first_chunk, _buffer, to_read - first_chunk);
    }

    _head.store((current_head + to_read) % _capacity, std::memory_order_release);
    return to_read;
}

bool LockFreeByteRingBuffer::isEmpty() const
{
    return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
}

bool LockFreeByteRingBuffer::isFull() const
{
    const size_t next_tail = (_tail.load(std::memory_order_acquire) + 1) % _capacity;
    return next_tail == _head.load(std::memory_order_acquire);
}

size_t LockFreeByteRingBuffer::size() const
{
    const size_t head = _head.load(std::memory_order_acquire);
    const size_t tail = _tail.load(std::memory_order_acquire);
    return (tail - head + _capacity) % _capacity;
}

size_t LockFreeByteRingBuffer::capacity() const
{
    return _capacity - 1;
}