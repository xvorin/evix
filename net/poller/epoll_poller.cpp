#include "net/poller/epoll_poller.h"

#include "net/channel.h"

#include <assert.h>
#include <cstddef>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>

using namespace xvorin::evix;

using namespace xvorin::evix::net;

static_assert(EPOLLIN == POLLIN, "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI, "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT, "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP, "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR, "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP, "epoll uses same flag values as poll");

namespace {

const int kNew = -1;

const int kAdded = 1;

const int kDeleted = 2;

}

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if (epollfd_ < 0) {
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeout_ms, ChannelList* active_channels)
{
    int num_events = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeout_ms);
    int saved_errno = errno;
    Timestamp now(Timestamp::now());

    if (num_events > 0) {
        fill_active_channels(num_events, active_channels);
        if (static_cast<size_t>(num_events) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (num_events == 0) {
    } else {
        if (saved_errno != EINTR) {
            errno = saved_errno;
        }
    }
    return now;
}

void EPollPoller::fill_active_channels(int num_events, ChannelList* active_channels) const
{
    assert(static_cast<size_t>(num_events) <= events_.size());

    for (int i = 0; i < num_events; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
#endif
        channel->set_revents(events_[i].events);
        active_channels->push_back(channel);
    }
}

void EPollPoller::update_channel(Channel* channel)
{
    Poller::assert_in_loop_thread();
    const int index = channel->index();

    if (index == kNew || index == kDeleted) {
        int fd = channel->fd();
        if (index == kNew) {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        } else {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        int fd = channel->fd();
        (void)fd;
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);

        if (channel->is_none_event()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::remove_channel(Channel* channel)
{
    Poller::assert_in_loop_thread();
    int fd = channel->fd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->is_none_event());
    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    size_t n = channels_.erase(fd);
    (void)n;
    assert(n == 1);

    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel)
{
    struct epoll_event event;
    memset(&event, 0x00, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
        } else {
        }
    }
}

const char* EPollPoller::operation_to_string(int op)
{
    switch (op) {
    case EPOLL_CTL_ADD:
        return "ADD";

    case EPOLL_CTL_DEL:
        return "DEL";

    case EPOLL_CTL_MOD:
        return "MOD";

    default:
        assert(false && "ERROR op");
        return "Unknown Operation";
    }
}
