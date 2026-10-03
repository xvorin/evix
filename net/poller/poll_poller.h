#pragma once

#include "net/poller.h"

#include <vector>

struct pollfd;

namespace xvorin::evix {

namespace net {

class PollPoller : public Poller {
public:
    PollPoller(EventLoop* loop);

    ~PollPoller() override;

    Timestamp poll(int timeout_ms, ChannelList* active_channels) override;

    void update_channel(Channel* channel) override;

    void remove_channel(Channel* channel) override;

private:
    void fill_active_channels(int num_events, ChannelList* active_channels) const;

    typedef std::vector<struct pollfd> PollFdList;

    PollFdList pollfds_;
};

} // namespace xvorin::evix::net
} // namespace xvorin::evix
