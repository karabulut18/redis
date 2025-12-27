#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>

/*
LockFreeByteRingBuffer
SPSC Lock-free ring buffer for byte data.
for bulk read and write
*/

class LockFreeByteRingBuffer
{
    char* _buffer;
    const size_t _capacity;

    alignas(64) std::atomic<size_t> _head;
    alignas(64) std::atomic<size_t> _tail;

public:
    explicit LockFreeByteRingBuffer(size_t capacity);
    ~LockFreeByteRingBuffer();
    size_t write(const char* data, size_t len);
    size_t read(char* data, size_t len);
    bool isEmpty() const;
    bool isFull() const;
    size_t size() const;
    size_t capacity() const;

private:
    LockFreeByteRingBuffer(const LockFreeByteRingBuffer&) = delete;
    LockFreeByteRingBuffer& operator=(const LockFreeByteRingBuffer&) = delete;
};