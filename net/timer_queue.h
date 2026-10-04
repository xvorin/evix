#pragma once

#include "evix/base/timestamp.h"
#include "evix/net/callbacks.h"
#include "evix/net/channel.h"

#include <mutex>
#include <set>
#include <vector>

namespace xvorin::evix::net {

class EventLoop;
class Timer;
class TimerId;

class TimerQueue {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId add_timer(TimerCallback cb, Timestamp when, double interval);
    void cancel(TimerId timerId);

private:
    TimerQueue(const TimerQueue&) = delete;
    void operator=(const TimerQueue&) = delete;

private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;
    void add_timer_in_loop(Timer* timer);
    void cancel_in_loop(TimerId timerId);
    void handle_read();
    std::vector<Entry> get_expired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);
    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfd_channel_;
    TimerList timers_;
    ActiveTimerSet active_timers_;
    bool calling_expired_timers_;

    ActiveTimerSet canceling_timers_;
};

} // namespace xvorin::evix::net
