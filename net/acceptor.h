#pragma once

#include "evix/net/channel.h"
#include "evix/net/socket.h"

#include <functional>

namespace xvorin::evix::net {

class EventLoop;
class InetAddress;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

public:
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
    Acceptor(const Acceptor&) = delete;
    void operator=(const Acceptor&) = delete;

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
