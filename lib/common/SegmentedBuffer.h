#pragma once
#include "SegmentPool.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <string_view>

/**
 * SegmentedBuffer manages a sequence of BufferSegments.
 * It provides a contiguous-like interface over non-contiguous memory slabs.
 * Its design allows TcpConnection to read directly into segments, achieving true zero-copy.
 */
class SegmentedBuffer
{
public:
    SegmentedBuffer() : _totalSize(0)
    {
    }

    /**
     * Standard append logic (performs a copy).
     * Used for small data or when direct writing isn't possible.
     */
    void append(const char* data, size_t len)
    {
        size_t written = 0;
        while (written < len)
        {
            if (_segments.empty() || _segments.back()->writable() == 0)
            {
                _segments.push_back(SegmentPool::GetInstance()->acquire(len - written));
            }

            auto& last = _segments.back();
            size_t toWrite = std::min(last->writable(), len - written);
            std::memcpy(last->writePtr(), data + written, toWrite);
            last->commit(toWrite);
            written += toWrite;
            _totalSize += toWrite;
        }
    }

    /**
     * ZERO-COPY: Provides a pointer where the network layer can write data directly.
     * actualSize returns how much space is available in the current segment.
     */
    char* getWritePtr(size_t hint, size_t& actualSize)
    {
        if (_segments.empty() || _segments.back()->writable() == 0)
        {
            _segments.push_back(SegmentPool::GetInstance()->acquire(hint));
        }
        auto& last = _segments.back();
        actualSize = last->writable();
        return last->writePtr();
    }

    void commitWrite(size_t len)
    {
        if (!_segments.empty())
        {
            _segments.back()->commit(len);
            _totalSize += len;
        }
    }

    /**
     * Returns a view of the contiguous data in the first segment.
     */
    std::string_view peek() const
    {
        if (_segments.empty())
            return {};
        return {_segments.front()->readPtr(), _segments.front()->readable()};
    }

    /**
     * Returns a view of 'len' bytes. If they span segments, it uses a thread-local
     * overflow buffer. Use this sparingly as it involves a small copy.
     */
    std::string_view peekContiguous(size_t len)
    {
        if (_totalSize < len)
            return {};

        auto first = _segments.front();
        if (first->readable() >= len)
        {
            return {first->readPtr(), len};
        }

        // Edge case: Spill-over across segments
        static thread_local std::string overflowBuffer;
        overflowBuffer.clear();
        overflowBuffer.reserve(len);

        size_t collected = 0;
        for (auto& seg : _segments)
        {
            size_t toCollect = std::min(len - collected, seg->readable());
            overflowBuffer.append(seg->readPtr(), toCollect);
            collected += toCollect;
            if (collected == len)
                break;
        }
        return overflowBuffer;
    }

    /**
     * O(1) consumption. Drops segments that are fully read.
     */
    void consume(size_t len)
    {
        size_t toConsume = std::min(len, _totalSize);
        _totalSize -= toConsume;

        while (toConsume > 0 && !_segments.empty())
        {
            auto first = _segments.front();
            size_t canConsume = std::min(toConsume, first->readable());
            first->consume(canConsume);
            toConsume -= canConsume;

            if (first->readable() == 0)
            {
                _segments.pop_front();
            }
        }
    }

    size_t size() const
    {
        return _totalSize;
    }
    bool empty() const
    {
        return _totalSize == 0;
    }

    /**
     * Returns the anchor for the first segment.
     * Useful for RespValue to keep the memory alive.
     */
    std::shared_ptr<BufferSegment> getFrontAnchor()
    {
        if (_segments.empty())
            return nullptr;
        return _segments.front();
    }

private:
    std::deque<std::shared_ptr<BufferSegment>> _segments;
    size_t _totalSize;
};
