#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <utility>

/**
 * Lock-free Single-Producer-Single-Consumer (SPSC) Ring Buffer
 *
 * Simple and efficient implementation using atomic operations.
 * Works with POD types (int, char, structs, etc.)
 *
 * Key features:
 * - Lock-free for SPSC scenarios
 * - Cache-line aligned to prevent false sharing
 * - Memory ordering optimizations for performance
 */
template <typename T> class LockFreeRingBuffer
{
    T* _buffer;
    const size_t _capacity;

    // Cache-line aligned to prevent false sharing
    alignas(64) std::atomic<size_t> _head;
    alignas(64) std::atomic<size_t> _tail;

public:
    explicit LockFreeRingBuffer(size_t capacity)
        : _capacity(capacity + 1) // +1 to distinguish full from empty
          ,
          _head(0), _tail(0)
    {
        _buffer = new T[_capacity];
    }

    ~LockFreeRingBuffer()
    {
        delete[] _buffer;
    }

    /**
     * Push an element to the buffer (producer side)
     * Returns true if successful, false if buffer is full
     */
    bool push(const T& item)
    {
        const size_t current_tail = _tail.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % _capacity;

        // Check if buffer is full
        if (next_tail == _head.load(std::memory_order_acquire))
            return false;

        _buffer[current_tail] = item;

        // Release ensures the write is visible before updating tail
        _tail.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * Push an element to the buffer (producer side) - Move semantics
     * Returns true if successful, false if buffer is full
     */
    bool push(T&& item)
    {
        const size_t current_tail = _tail.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % _capacity;

        // Check if buffer is full
        if (next_tail == _head.load(std::memory_order_acquire))
            return false;

        _buffer[current_tail] = std::move(item);

        // Release ensures the write is visible before updating tail
        _tail.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * Pop an element from the buffer (consumer side)
     * Returns true if successful, false if buffer is empty
     */
    bool pop(T& item)
    {
        const size_t current_head = _head.load(std::memory_order_relaxed);

        // Check if buffer is empty
        if (current_head == _tail.load(std::memory_order_acquire))
            return false;

        item = std::move(_buffer[current_head]);

        // Release ensures the read completes before updating head
        _head.store((current_head + 1) % _capacity, std::memory_order_release);
        return true;
    }

    bool isEmpty() const
    {
        return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
    }

    bool isFull() const
    {
        const size_t next_tail = (_tail.load(std::memory_order_acquire) + 1) % _capacity;
        return next_tail == _head.load(std::memory_order_acquire);
    }

    size_t size() const
    {
        const size_t head = _head.load(std::memory_order_acquire);
        const size_t tail = _tail.load(std::memory_order_acquire);
        return (tail - head + _capacity) % _capacity;
    }

    size_t capacity() const
    {
        return _capacity - 1;
    }

private:
    // Delete copy constructor and assignment operator
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
};