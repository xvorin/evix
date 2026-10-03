#pragma once

#include "base/timestamp.h"
#include "net/event_loop.h"

#include <map>
#include <vector>

namespace xvorin::evix::net {

class Channel;

class Poller {
public:
    Poller(const Poller&) = delete;
    void operator=(const Poller&) = delete;

    typedef std::vector<Channel*> ChannelList;

    Poller(EventLoop* loop);
    virtual ~Poller();

    virtual Timestamp poll(int timeout_ms, ChannelList* active_channels) = 0;

    virtual void update_channel(Channel* channel) = 0;
    virtual void remove_channel(Channel* channel) = 0;
    virtual bool has_channel(Channel* channel) const;

    static Poller* new_default_poller(EventLoop* loop);

    void assert_in_loop_thread() const
    {
        owner_loop_->assert_in_loop_thread();
    }

protected:
    typedef std::map<int, Channel*> ChannelMap;
    ChannelMap channels_;

private:
    EventLoop* owner_loop_;
};

} // namespace xvorin::evix::net
