#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace xvorin::evix {

///
/// A fixed-size pool of worker threads pulling tasks from a shared queue.
///
/// Tasks are plain `std::function<void()>`. The queue can be bounded via
/// set_max_queue_size(): when full, run() blocks the producer instead of
/// dropping work (backpressure). If start() was never called (zero threads),
/// run() executes the task synchronously in the calling thread.
///
/// Lifecycle: construct -> (optional) set_* -> start(n) -> run()... -> stop().
/// stop() drains all queued tasks, then joins the workers; the destructor calls
/// stop() if still running. A worker executing a long-running task must return
/// on its own before it can be joined.
///
class ThreadPool {
public:
    using Task = std::function<void()>;

public:
    explicit ThreadPool(std::string name = "ThreadPool");
    ~ThreadPool();

    /// Must be called before start().
    void set_max_queue_size(size_t max)
    {
        max_queue_size_ = max;
    }
    void set_thread_init_callback(Task cb)
    {
        thread_init_cb_ = std::move(cb);
    }

    void start(int num_threads);
    void stop();

    /// Enqueue a task. Runs inline when the pool has no worker threads.
    void run(Task task);

    size_t queue_size() const;
    size_t thread_count() const
    {
        return threads_.size();
    }
    const std::string& name() const
    {
        return name_;
    }
    bool running() const
    {
        return running_.load();
    }

private:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    void run_in_thread();
    Task take();
    bool is_full() const; // caller holds mutex_

    std::string name_;
    Task thread_init_cb_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<Task> queue_;
    size_t max_queue_size_ { 0 };
    std::vector<std::thread> threads_;
    std::atomic<bool> running_ { false };
};

} // namespace xvorin::evix
