#include "evix/base/thread_pool.h"

#include <cassert>

using namespace xvorin::evix;

ThreadPool::ThreadPool(std::string name)
    : name_(std::move(name))
{
}

ThreadPool::~ThreadPool()
{
    if (running_.load()) {
        stop();
    }
}

void ThreadPool::start(int num_threads)
{
    assert(threads_.empty());
    assert(num_threads >= 0);
    running_.store(true);
    threads_.reserve(static_cast<size_t>(num_threads));
    for (int i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this] { run_in_thread(); });
    }
    if (num_threads == 0 && thread_init_cb_) {
        thread_init_cb_();
    }
}

void ThreadPool::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_.store(false);
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    for (std::thread& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

void ThreadPool::run(Task task)
{
    if (threads_.empty()) {
        task();
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(lock, [this] { return !is_full() || !running_.load(); });
    if (!running_.load()) {
        return;
    }
    queue_.push_back(std::move(task));
    not_empty_.notify_one();
}

size_t ThreadPool::queue_size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool ThreadPool::is_full() const
{
    return max_queue_size_ > 0 && queue_.size() >= max_queue_size_;
}

ThreadPool::Task ThreadPool::take()
{
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this] { return !queue_.empty() || !running_.load(); });
    Task task;
    // Returns an empty task only when stopping with a drained queue; otherwise
    // always hands back real work, so stop() drains all pending tasks.
    if (!queue_.empty()) {
        task = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
    }
    return task;
}

void ThreadPool::run_in_thread()
{
    if (thread_init_cb_) {
        thread_init_cb_();
    }
    while (true) {
        Task task = take();
        if (!task) {
            break; // stopping and nothing left to drain
        }
        task();
    }
}
