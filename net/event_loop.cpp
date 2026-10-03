#include "net/event_loop.h"

#include "net/channel.h"
#include "net/poller.h"
#include "net/sockets_ops.h"
#include "net/timer_queue.h"

#include <algorithm>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

namespace {

__thread EventLoop* t_loop_in_this_thread = nullptr;

const int k_poll_time_ms = 10000;

int create_eventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        abort();
    }
    return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"

class IgnoreSigPipe {
public:
    IgnoreSigPipe()
    {
        ::signal(SIGPIPE, SIG_IGN);
    }
};

#pragma GCC diagnostic error "-Wold-style-cast"

} // namespace

IgnoreSigPipe initObj;

EventLoop* EventLoop::get_event_loop_of_current_thread()
{
    return t_loop_in_this_thread;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , event_handling_(false)
    , calling_pending_functors_(false)
    , iteration_(0)
    , thread_id_(CurrentThread::tid())
    , poller_(Poller::new_default_poller(this))
    , timer_queue_(new TimerQueue(this))
    , wakeup_fd_(create_eventfd())
    , wakeup_channel_(new Channel(this, wakeup_fd_))
    , current_active_channel_(nullptr)
{
    if (t_loop_in_this_thread) {
    } else {
        t_loop_in_this_thread = this;
    }

    wakeup_channel_->set_read_callback(std::bind(&EventLoop::handle_read, this));
    wakeup_channel_->enable_reading();
}

EventLoop::~EventLoop()
{
    wakeup_channel_->disable_all();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
    t_loop_in_this_thread = nullptr;
}

void EventLoop::loop()
{
    assert(!looping_);
    assert_in_loop_thread();
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        active_channels_.clear();
        poll_return_time_ = poller_->poll(k_poll_time_ms, &active_channels_);
        ++iteration_;
        event_handling_ = true;
        for (Channel* channel : active_channels_) {
            current_active_channel_ = channel;
            current_active_channel_->handle_event(poll_return_time_);
        }
        current_active_channel_ = nullptr;
        event_handling_ = false;
        do_pending_functors();
    }

    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;

    if (!is_in_loop_thread()) {
        wakeup();
    }
}

void EventLoop::run_in_loop(Functor cb)
{
    if (is_in_loop_thread()) {
        cb();
    } else {
        queue_in_loop(std::move(cb));
    }
}

void EventLoop::queue_in_loop(Functor cb)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if (!is_in_loop_thread() || calling_pending_functors_) {
        wakeup();
    }
}

size_t EventLoop::queue_size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_functors_.size();
}

TimerId EventLoop::run_at(Timestamp time, TimerCallback cb)
{
    return timer_queue_->add_timer(std::move(cb), time, 0.0);
}

TimerId EventLoop::run_after(double delay, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return run_at(time, std::move(cb));
}

TimerId EventLoop::run_every(double interval, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timer_queue_->add_timer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId)
{
    return timer_queue_->cancel(timerId);
}

void EventLoop::update_channel(Channel* channel)
{
    assert(channel->owner_loop() == this);
    assert_in_loop_thread();
    poller_->update_channel(channel);
}

void EventLoop::remove_channel(Channel* channel)
{
    assert(channel->owner_loop() == this);
    assert_in_loop_thread();
    if (event_handling_) {
        assert(
            current_active_channel_ == channel
            || std::find(active_channels_.begin(), active_channels_.end(), channel) == active_channels_.end());
    }
    poller_->remove_channel(channel);
}

bool EventLoop::has_channel(Channel* channel)
{
    assert(channel->owner_loop() == this);
    assert_in_loop_thread();
    return poller_->has_channel(channel);
}

void EventLoop::abort_not_in_loop_thread()
{
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeup_fd_, &one, sizeof one);
    if (n != sizeof one) {
    }
}

void EventLoop::handle_read()
{
    uint64_t one = 1;
    ssize_t n = sockets::read(wakeup_fd_, &one, sizeof one);
    if (n != sizeof one) {
    }
}

void EventLoop::do_pending_functors()
{
    std::vector<Functor> functors;
    calling_pending_functors_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }
    for (const Functor& functor : functors) {
        functor();
    }
    calling_pending_functors_ = false;
}
