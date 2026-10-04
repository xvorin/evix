#pragma once

#include "evix/base/thread_pool.h"

#include <functional>
#include <string>
#include <vector>

namespace xvorin::evix::net {

class EventLoop;

///
/// A pool of IO threads, each running its own EventLoop.
///
/// New connections are handed out round-robin via get_next_loop(), so each
/// connection stays pinned to one EventLoop and its callbacks always run on
/// that loop's thread (per-connection thread safety).
///
/// Built on top of the generic xvorin::evix::ThreadPool: every worker hosts
/// exactly one EventLoop for its whole lifetime.
///
class EventLoopThreadPool {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

public:
    EventLoopThreadPool(EventLoop* base_loop, std::string name);
    ~EventLoopThreadPool();

    /// Must be called before start(). 0 means run everything on base_loop.
    void set_thread_num(int num_threads)
    {
        num_threads_ = num_threads;
    }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    /// Round-robin next IO loop; returns base_loop when the pool is empty.
    /// Valid after start(), called from the base loop thread.
    EventLoop* get_next_loop();

    std::vector<EventLoop*> get_all_loops();

    bool started() const
    {
        return started_;
    }
    const std::string& name() const
    {
        return name_;
    }

private:
    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

private:
    EventLoop* base_loop_;
    std::string name_;
    bool started_;
    int num_threads_;
    size_t next_;
    xvorin::evix::ThreadPool pool_;
    std::vector<EventLoop*> loops_;
};

} // namespace xvorin::evix::net
