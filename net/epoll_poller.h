#pragma once

#include "evix/base/timestamp.h"
#include "evix/net/event_loop.h"

#include <map>
#include <vector>

struct epoll_event;

namespace xvorin::evix::net {

class Channel;

///
/// The epoll(7)-based IO multiplexer and the only poller in evix.
///
/// Owns the epoll fd and the fd -> Channel map, and fills the owning
/// EventLoop's active-channel list on each poll().
///
class EPollPoller {
public:
    using ChannelList = std::vector<Channel*>;

public:
    explicit EPollPoller(EventLoop* loop);
    ~EPollPoller();

    Timestamp poll(int timeout_ms, ChannelList* active_channels);

    void update_channel(Channel* channel);
    void remove_channel(Channel* channel);
    bool has_channel(Channel* channel) const;

    void assert_in_loop_thread() const
    {
        owner_loop_->assert_in_loop_thread();
    }

private:
    EPollPoller(const EPollPoller&) = delete;
    EPollPoller& operator=(const EPollPoller&) = delete;

private:
    static const int kInitEventListSize = 16;

    static const char* operation_to_string(int op);

    void fill_active_channels(int num_events, ChannelList* active_channels) const;
    void update(int operation, Channel* channel);

    using EventList = std::vector<struct epoll_event>;
    using ChannelMap = std::map<int, Channel*>;

    EventLoop* owner_loop_;
    int epollfd_;
    EventList events_;
    ChannelMap channels_;
};

} // namespace xvorin::evix::net
