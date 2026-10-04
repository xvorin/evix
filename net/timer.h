#pragma once

#include "evix/base/timestamp.h"
#include "evix/net/callbacks.h"

#include <atomic>

namespace xvorin::evix::net {

class Timer {
public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb))
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval > 0.0)
        , sequence_(++s_num_created_)
    {
    }

    void run() const
    {
        callback_();
    }

    Timestamp expiration() const
    {
        return expiration_;
    }

    bool repeat() const
    {
        return repeat_;
    }

    int64_t sequence() const
    {
        return sequence_;
    }

    void restart(Timestamp now);

    static int64_t num_created()
    {
        return s_num_created_.load();
    }

private:
    Timer(const Timer&) = delete;
    void operator=(const Timer&) = delete;

private:
    const TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const bool repeat_;
    const int64_t sequence_;
    static std::atomic<int64_t> s_num_created_;
};

} // namespace xvorin::evix::net
