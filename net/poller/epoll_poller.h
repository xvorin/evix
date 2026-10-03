#pragma once

#include "net/poller.h"

#include <vector>

struct epoll_event;

namespace xvorin::evix {

namespace net {

class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop* loop);

    ~EPollPoller() override;

    Timestamp poll(int timeout_ms, ChannelList* active_channels) override;

    void update_channel(Channel* channel) override;

    void remove_channel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    static const char* operation_to_string(int op);

    void fill_active_channels(int num_events, ChannelList* active_channels) const;

    void update(int operation, Channel* channel);

    typedef std::vector<struct epoll_event> EventList;

    int epollfd_;
    EventList events_;
};

} // namespace xvorin::evix::net
} // namespace xvorin::evix
