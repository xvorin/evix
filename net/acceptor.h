#pragma once

#include "net/channel.h"
#include "net/socket.h"

#include <functional>

namespace xvorin::evix::net {

class EventLoop;
class InetAddress;

class Acceptor {
public:
    Acceptor(const Acceptor&) = delete;
    void operator=(const Acceptor&) = delete;

    typedef std::function<void(int sockfd, const InetAddress&)> NewConnectionCallback;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void set_new_connection_callback(const NewConnectionCallback& cb)
    {
        new_connection_callback_ = cb;
    }

    void listen();

    bool listening() const
    {
        return listening_;
    }

private:
    void handle_read();

    EventLoop* loop_;
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listening_;
    int idle_fd_;
};

} // namespace xvorin::evix::net
