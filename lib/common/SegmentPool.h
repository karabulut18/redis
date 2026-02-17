#pragma once
#include "BufferSegment.h"
#include "SystemUtil.h"
#include <memory>
#include <mutex>
#include <vector>

/**
 * SegmentPool implements a system-aware multi-tiered object pool.
 */
class SegmentPool
{
public:
    static SegmentPool* GetInstance()
    {
        static SegmentPool instance;
        return &instance;
    }

    std::shared_ptr<BufferSegment> acquire(size_t minSize = 0)
    {
        size_t pageSize = SystemUtil::GetPageSize();
        // Tier 1: Small (1 Page)
        // Tier 2: Jumbo (8 Pages)
        size_t targetSize = (minSize <= pageSize) ? pageSize : (pageSize * 8);

        std::lock_guard<std::mutex> lock(_mutex);
        auto& pool = (targetSize == pageSize) ? _smallPool : _largePool;

        if (pool.empty())
        {
            return wrap(new BufferSegment(targetSize));
        }

        BufferSegment* segment = pool.back();
        pool.pop_back();
        segment->reset();
        return wrap(segment);
    }

    ~SegmentPool()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto s : _smallPool)
            delete s;
        for (auto s : _largePool)
            delete s;
        _smallPool.clear();
        _largePool.clear();
    }

private:
    SegmentPool() = default;

    std::shared_ptr<BufferSegment> wrap(BufferSegment* raw)
    {
        return std::shared_ptr<BufferSegment>(raw, [this](BufferSegment* s) { this->release(s); });
    }

    void release(BufferSegment* segment)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (segment->capacity() == SystemUtil::GetPageSize())
        {
            _smallPool.push_back(segment);
        }
        else
        {
            _largePool.push_back(segment);
        }
    }

    std::mutex _mutex;
    std::vector<BufferSegment*> _smallPool;
    std::vector<BufferSegment*> _largePool;
};
