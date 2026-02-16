#include "../lib/common/LockFreeByteRingBuffer.h"
#include "../lib/common/LockFreeRingBuffer.h"
#include <cassert>
#include <iostream>
#include <thread>

void test_basic()
{
    std::cout << "Test 1: Basic Operations\n";

    LockFreeRingBuffer<int> buffer(10);

    assert(buffer.isEmpty());
    assert(!buffer.isFull());

    // Push some items
    for (int i = 0; i < 5; ++i)
        assert(buffer.push(i));

    // Pop them back
    int value;
    for (int i = 0; i < 5; ++i)
    {
        assert(buffer.pop(value));
        assert(value == i);
    }

    assert(buffer.isEmpty());
    std::cout << "✓ Passed\n\n";
}

void test_producer_consumer()
{
    std::cout << "Test 2: Producer-Consumer\n";

    const size_t NUM_ITEMS = 100000;
    LockFreeRingBuffer<int> buffer(1024);

    std::thread producer(
        [&]()
        {
            for (size_t i = 0; i < NUM_ITEMS; ++i)
            {
                while (!buffer.push(static_cast<int>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

    std::thread consumer(
        [&]()
        {
            int value;
            for (size_t i = 0; i < NUM_ITEMS; ++i)
            {
                while (!buffer.pop(value))
                {
                    std::this_thread::yield();
                }
                assert(value == static_cast<int>(i));
            }
        });

    producer.join();
    consumer.join();

    assert(buffer.isEmpty());
    std::cout << "✓ Passed - " << NUM_ITEMS << " items\n\n";
}

void test_byte_buffer()
{
    std::cout << "Test 3: Byte Buffer\n";

    LockFreeByteRingBuffer buffer(100);

    const char* msg = "Hello, Lock-Free!";
    size_t len = strlen(msg);

    size_t written = buffer.write(msg, len);
    assert(written == len);

    char read_buf[100];
    size_t read_count = buffer.read(read_buf, len);
    assert(read_count == len);
    assert(memcmp(read_buf, msg, len) == 0);

    std::cout << "✓ Passed\n\n";
}

int main()
{
    std::cout << "\n=== Lock-Free Ring Buffer Tests ===\n\n";

    test_basic();
    test_producer_consumer();
    test_byte_buffer();

    std::cout << "✓ All tests passed!\n\n";
    return 0;
}
