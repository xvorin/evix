#pragma once

#include "evix/base/current_thread.h"
#include "evix/base/timestamp.h"
#include "evix/net/callbacks.h"
#include "evix/net/timer_id.h"

#include <any>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace xvorin::evix::net {

class Channel;
class EPollPoller;
class TimerQueue;

class EventLoop {
public:
    using Functor = std::function<void()>;

public:
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    Timestamp poll_return_time() const
    {
        return poll_return_time_;
    }

    int64_t iteration() const
    {
        return iteration_;
    }

    void run_in_loop(Functor cb);
    void queue_in_loop(Functor cb);

    size_t queue_size() const;

    TimerId run_at(Timestamp time, TimerCallback cb);
    TimerId run_after(double delay, TimerCallback cb);
    TimerId run_every(double interval, TimerCallback cb);

    void cancel(TimerId timerId);
    void wakeup();

    void update_channel(Channel* channel);
    void remove_channel(Channel* channel);
    bool has_channel(Channel* channel);

    void assert_in_loop_thread()
    {
        if (!is_in_loop_thread()) {
            abort_not_in_loop_thread();
        }
    }

    bool is_in_loop_thread() const
    {
        return thread_id_ == CurrentThread::tid();
    }

    bool event_handling() const
    {
        return event_handling_;
    }

    void set_context(const std::any& context)
    {
        context_ = context;
    }

    const std::any& get_context() const
    {
        return context_;
    }

    std::any* get_mutable_context()
    {
        return &context_;
    }

    static EventLoop* get_event_loop_of_current_thread();

private:
    EventLoop(const EventLoop&) = delete;
    void operator=(const EventLoop&) = delete;

private:
    void abort_not_in_loop_thread();
    void handle_read();
    void do_pending_functors();
    void print_active_channels() const;

    using ChannelList = std::vector<Channel*>;

    bool looping_;
    std::atomic<bool> quit_;
    bool event_handling_;
    bool calling_pending_functors_;
    int64_t iteration_;
    const pid_t thread_id_;
    Timestamp poll_return_time_;
    std::unique_ptr<EPollPoller> poller_;
    std::unique_ptr<TimerQueue> timer_queue_;
    int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;
    std::any context_;
    ChannelList active_channels_;
    Channel* current_active_channel_;
    mutable std::mutex mutex_;
    std::vector<Functor> pending_functors_;
};

} // namespace xvorin::evix::net
