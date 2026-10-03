#include "net/timer_queue.h"

#include "net/event_loop.h"
#include "net/timer.h"
#include "net/timer_id.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstring>

namespace xvorin::evix::net {
namespace detail {

int create_timerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
        TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
    }
    return timerfd;
}

struct timespec how_much_time_from_now(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100) {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void read_timerfd(int timerfd, Timestamp)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    if (n != sizeof howmany) {
    }
}

void reset_timerfd(int timerfd, Timestamp expiration)
{
    struct itimerspec new_value;
    struct itimerspec old_value;
    memset(&new_value, 0x00, sizeof new_value);
    memset(&old_value, 0x00, sizeof old_value);
    new_value.it_value = how_much_time_from_now(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &new_value, &old_value);
    if (ret) {
    }
}

} // namespace detail

using namespace xvorin::evix;

using namespace xvorin::evix::net;

using namespace xvorin::evix::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerfd_(create_timerfd())
    , timerfd_channel_(loop, timerfd_)
    , timers_()
    , calling_expired_timers_(false)
{
    timerfd_channel_.set_read_callback(std::bind(&TimerQueue::handle_read, this));
    timerfd_channel_.enable_reading();
}

TimerQueue::~TimerQueue()
{
    timerfd_channel_.disable_all();
    timerfd_channel_.remove();
    ::close(timerfd_);
    for (const Entry& timer : timers_) {
        delete timer.second;
    }
}

TimerId TimerQueue::add_timer(TimerCallback cb, Timestamp when, double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->run_in_loop(std::bind(&TimerQueue::add_timer_in_loop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->run_in_loop(std::bind(&TimerQueue::cancel_in_loop, this, timerId));
}

void TimerQueue::add_timer_in_loop(Timer* timer)
{
    loop_->assert_in_loop_thread();
    bool earliest_changed = insert(timer);
    if (earliest_changed) {
        reset_timerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::cancel_in_loop(TimerId timerId)
{
    loop_->assert_in_loop_thread();
    assert(timers_.size() == active_timers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = active_timers_.find(timer);

    if (it != active_timers_.end()) {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        (void)n;
        delete it->first;
        active_timers_.erase(it);
    } else if (calling_expired_timers_) {
        canceling_timers_.insert(timer);
    }
    assert(timers_.size() == active_timers_.size());
}

void TimerQueue::handle_read()
{
    loop_->assert_in_loop_thread();
    Timestamp now(Timestamp::now());
    read_timerfd(timerfd_, now);
    std::vector<Entry> expired = get_expired(now);
    calling_expired_timers_ = true;
    canceling_timers_.clear();
    for (const Entry& it : expired) {
        it.second->run();
    }
    calling_expired_timers_ = false;
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::get_expired(Timestamp now)
{
    assert(timers_.size() == active_timers_.size());
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);
    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = active_timers_.erase(timer);
        assert(n == 1);
        (void)n;
    }
    assert(timers_.size() == active_timers_.size());
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp next_expire;
    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat() && canceling_timers_.find(timer) == canceling_timers_.end()) {
            it.second->restart(now);
            insert(it.second);
        } else {
            delete it.second;
        }
    }
    if (!timers_.empty()) {
        next_expire = timers_.begin()->second->expiration();
    }
    if (next_expire.valid()) {
        reset_timerfd(timerfd_, next_expire);
    }
}

bool TimerQueue::insert(Timer* timer)
{
    loop_->assert_in_loop_thread();
    assert(timers_.size() == active_timers_.size());
    bool earliest_changed = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();

    if (it == timers_.end() || when < it->first) {
        earliest_changed = true;
    }
    {
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }
    {
        std::pair<ActiveTimerSet::iterator, bool> result = active_timers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }
    assert(timers_.size() == active_timers_.size());
    return earliest_changed;
}

} // namespace xvorin::evix::net
