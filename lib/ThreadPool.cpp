#include "ThreadPool.h"

#include "Output.h"

void Worker::operator()() {
  while (true) {
    std::function<void()> selected_task = _pool->Dequeue();
    if (selected_task == nullptr)
      return;

    try {
      selected_task();
    } catch (std::exception &e) {
      PUTF_LN("Worker thread caught an exception:" + std::string(e.what()));
    }
  }
}

std::function<void()> ThreadPool::Dequeue() {
  std::unique_lock<std::mutex> lock(_queueMutex);
  std::function<void()> selected_task = nullptr;
  _cv.wait(lock, [this] { return _stop || !_tasks.empty(); });
  if (_stop && _tasks.empty())
    return nullptr;
  selected_task = std::move(_tasks.front());
  _tasks.pop();
  return selected_task;
}

ThreadPool::ThreadPool(int n) : _stop(false) {
  _workers.reserve(n);
  for (size_t i = 0; i < n; ++i)
    _workers.emplace_back(Worker(this));
};

void ThreadPool::Enqueue(std::function<void()> task) {
  {
    std::unique_lock<std::mutex> lock(_queueMutex);
    if (_stop)
      return;
    _tasks.emplace(std::move(task));
  }
  _cv.notify_one();
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(_queueMutex);
    _stop = true;
  }
  _cv.notify_all();

  for (std::thread &worker : _workers) {
    if (worker.joinable())
      worker.join();
  }
}