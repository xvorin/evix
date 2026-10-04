#pragma once

#include "evix/net/inet_address.h"

#include <atomic>
#include <functional>
#include <memory>

namespace xvorin::evix::net {

class Channel;
class EventLoop;

class Connector : public std::enable_shared_from_this<Connector> {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

public:
    Connector(EventLoop* loop, const InetAddress& serverAddr);

    ~Connector();

    void set_new_connection_callback(const NewConnectionCallback& cb)
    {
        new_connection_callback_ = cb;
    }

    void start();

    void restart();

    void stop();

    const InetAddress& server_address() const
    {
        return server_addr_;
    }

private:
    Connector(const Connector&) = delete;
    void operator=(const Connector&) = delete;

private:
    enum States { kDisconnected, kConnecting, kConnected };

    static const int kMaxRetryDelayMs = 30 * 1000;

    static const int kInitRetryDelayMs = 500;

    void set_state(States s)
    {
        state_ = s;
    }

    void start_in_loop();
    void stop_in_loop();

    void connect();
    void connecting(int sockfd);

    void handle_write();
    void handle_error();

    void retry(int sockfd);

    int remove_and_reset_channel();
    void reset_channel();

    EventLoop* loop_;
    InetAddress server_addr_;
    bool connect_;

    States state_;

    std::unique_ptr<Channel> channel_;
    NewConnectionCallback new_connection_callback_;
    int retry_delay_ms_;
};

} // namespace xvorin::evix::net
