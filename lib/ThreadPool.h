#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// not used for now

class ThreadPool;

class Worker
{
    ThreadPool* _pool;

public:
    Worker(ThreadPool* pool) : _pool(pool)
    {
    }
    void operator()();
};

class ThreadPool
{
    friend class Worker;
    bool _stop;
    std::mutex _queueMutex;
    std::condition_variable _cv;

    std::vector<std::thread> _workers;
    std::queue<std::function<void()>> _tasks;

public:
    ThreadPool(size_t n = std::thread::hardware_concurrency());
    void Enqueue(std::function<void()> task);
    ~ThreadPool();

private:
    std::function<void()> Dequeue();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
};
