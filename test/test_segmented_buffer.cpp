#include "../lib/common/SegmentedBuffer.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

void test_basic_append_consume()
{
    SegmentedBuffer buf;
    buf.append("hello", 5);
    buf.append(" world", 6);

    assert(buf.size() == 11);
    // peek() only shows the first segment, so this works if data is small
    assert(buf.peek() == "hello world");

    buf.consume(6);
    assert(buf.size() == 5);
    assert(buf.peek() == "world");

    buf.consume(5);
    assert(buf.size() == 0);
    assert(buf.empty());
}

void test_segmented_write()
{
    SegmentedBuffer buf;
    size_t actual = 0;
    char* ptr = buf.getWritePtr(100, actual);
    assert(actual >= 100);
    std::memcpy(ptr, "zero-copy", 9);
    buf.commitWrite(9);

    assert(buf.size() == 9);
    assert(buf.peek() == "zero-copy");
}

void test_spill_over()
{
    size_t pageSize = SystemUtil::GetPageSize();
    SegmentedBuffer buf;

    // Fill the first segment exactly
    std::string large(pageSize, 'A');
    buf.append(large.c_str(), pageSize);

    // Add 1 more byte to force a spill to a new segment
    buf.append("B", 1);

    assert(buf.size() == pageSize + 1);
    // peek() should only show the first segment
    assert(buf.peek().size() == pageSize);

    // peekContiguous should handle the spill via the thread_local overflow buffer
    std::string_view view = buf.peekContiguous(pageSize + 1);
    assert(view.size() == pageSize + 1);
    assert(view[pageSize - 1] == 'A');
    assert(view[pageSize] == 'B');
}

void test_alignment_and_pooling()
{
    size_t pageSize = SystemUtil::GetPageSize();
    SegmentPool* pool = SegmentPool::GetInstance();

    // Acquire small segment
    auto seg = pool->acquire(100);
    assert(seg->capacity() == pageSize);
    // Verify page alignment
    assert((uintptr_t)seg->readPtr() % pageSize == 0);

    // Acquire jumbo segment
    auto jumbo = pool->acquire(pageSize + 1);
    assert(jumbo->capacity() == pageSize * 8);
    assert((uintptr_t)jumbo->readPtr() % pageSize == 0);

    // Verify pooling
    {
        auto temp = pool->acquire(1);
        // segment is released to pool at end of block
    }
    auto reused = pool->acquire(1);
    assert(reused->capacity() == pageSize);
}

int main()
{
    std::cout << "=== SegmentedBuffer System-Aware Tests ===" << std::endl;

    std::cout << "Detected Page Size: " << SystemUtil::GetPageSize() << " bytes" << std::endl;
    std::cout << "Detected Cache Line: " << SystemUtil::GetCacheLineSize() << " bytes" << std::endl;

    std::cout << "Running basic_append_consume... ";
    test_basic_append_consume();
    std::cout << "PASS" << std::endl;

    std::cout << "Running segmented_write... ";
    test_segmented_write();
    std::cout << "PASS" << std::endl;

    std::cout << "Running spill_over... ";
    test_spill_over();
    std::cout << "PASS" << std::endl;

    std::cout << "Running alignment_and_pooling... ";
    test_alignment_and_pooling();
    std::cout << "PASS" << std::endl;

    std::cout << "All SegmentedBuffer tests passed!" << std::endl;
    return 0;
}
