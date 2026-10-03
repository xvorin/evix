#include "net/poller.h"

#include "net/poller/epoll_poller.h"
#include "net/poller/poll_poller.h"

#include <stdlib.h>

using namespace xvorin::evix::net;

Poller* Poller::new_default_poller(EventLoop* loop)
{
    if (::getenv("MUDUO_USE_POLL")) {
        return new PollPoller(loop);
    } else {
        return new EPollPoller(loop);
    }
}
