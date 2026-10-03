#include "net/poller.h"

#include "net/channel.h"

using namespace xvorin::evix;
using namespace xvorin::evix::net;

Poller::Poller(EventLoop* loop)
    : owner_loop_(loop)
{
}

Poller::~Poller() = default;

bool Poller::has_channel(Channel* channel) const
{
    assert_in_loop_thread();
    ChannelMap::const_iterator it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}
