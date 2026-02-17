#pragma once
#include "SystemUtil.h"
#include <cstddef>
#include <cstdlib>
#include <new>

/**
 * BufferSegment represents a page-aligned hardware-optimized slab of memory.
 * It uses raw allocation (posix_memalign) to ensure it starts at a physical page boundary.
 */
class BufferSegment
{
public:
    explicit BufferSegment(size_t capacity) : _capacity(capacity)
    {
        size_t alignment = SystemUtil::GetPageSize();
        // Ensure capacity is a multiple of alignment for clean page usage
        if (_capacity % alignment != 0)
        {
            _capacity = ((_capacity / alignment) + 1) * alignment;
        }

        if (posix_memalign((void**)&_data, alignment, _capacity) != 0)
        {
            throw std::bad_alloc();
        }
        reset();
    }

    ~BufferSegment()
    {
        free(_data);
    }

    // Disable copy and assignment
    BufferSegment(const BufferSegment&) = delete;
    BufferSegment& operator=(const BufferSegment&) = delete;

    size_t writable() const
    {
        return _capacity - _wpos;
    }
    size_t readable() const
    {
        return _wpos - _rpos;
    }
    size_t capacity() const
    {
        return _capacity;
    }

    char* writePtr()
    {
        return _data + _wpos;
    }
    const char* readPtr() const
    {
        return _data + _rpos;
    }

    void commit(size_t len)
    {
        if (_wpos + len <= _capacity)
        {
            _wpos += len;
        }
    }

    void consume(size_t len)
    {
        if (_rpos + len <= _wpos)
        {
            _rpos += len;
        }
    }

    bool isFull() const
    {
        return _wpos == _capacity;
    }
    bool isEmpty() const
    {
        return _rpos == _wpos;
    }

    void reset()
    {
        _rpos = 0;
        _wpos = 0;
    }

private:
    char* _data;
    size_t _capacity;

    // Use alignas to prevent false sharing between the metadata and data
    // or between different threads accessing metadata.
    alignas(128) size_t _rpos;
    alignas(128) size_t _wpos;
};
