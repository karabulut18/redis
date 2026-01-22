#pragma once

#include <condition_variable>
#include <list>
#include <mutex>

template <typename T> class ThreadSafeQueue
{
private:
    std::list<T> _queue;
    std::mutex _mutex;
    std::condition_variable _condition;

public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue& other) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue& other) = delete;

    void Push(const T& item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(item);
        _condition.notify_one();
    }

    bool TryPop(T& item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty())
            return false;
        item = _queue.front();
        _queue.pop_front();
        return true;
    }

    void Pop(T& item)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this] { return !_queue.empty(); });
        item = _queue.front();
        _queue.pop_front();
    }

    bool Empty() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }
};