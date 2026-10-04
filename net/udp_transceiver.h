#pragma once

#include "evix/base/timestamp.h"
#include "evix/net/inet_address.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace xvorin::evix::net {

class Buffer;
class Channel;
class EventLoop;

///
/// A UDP endpoint bound to an EventLoop.
///
/// Follows the same style as TcpServer/TcpConnection: it owns a Channel on the
/// loop, delivers each received datagram to a message callback, and sends
/// datagrams to an InetAddress peer. Unlike TCP there is no connection object,
/// so the peer address travels with every message.
///
/// IPv4 only: the underlying socket is created AF_INET; bind() asserts the
/// listen address family is AF_INET.
///
class UdpTransceiver {
public:
    /// (self, datagram payload, peer address, receive time)
    using MessageCallback = std::function<void(UdpTransceiver*, Buffer*, const InetAddress&, Timestamp)>;

public:
    UdpTransceiver(EventLoop* loop, const std::string& name);
    ~UdpTransceiver();

    EventLoop* get_loop() const
    {
        return loop_;
    }

    const std::string& name() const
    {
        return name_;
    }

    /// Not thread safe.
    void set_message_callback(const MessageCallback& cb)
    {
        message_callback_ = cb;
    }

    /// Bind to a local address and start receiving. Returns false on failure.
    /// Must be called in the loop thread.
    bool bind(const InetAddress& listen_addr);

    /// Send one datagram to @p peer. Returns bytes sent, or -1 on error.
    /// Must be called in the loop thread.
    ssize_t send(const void* data, size_t len, const InetAddress& peer);
    ssize_t send(const std::string_view& data, const InetAddress& peer);

    void start_read();
    void stop_read();

    bool is_reading() const
    {
        return reading_;
    }

private:
    UdpTransceiver(const UdpTransceiver&) = delete;
    UdpTransceiver& operator=(const UdpTransceiver&) = delete;

private:
    void handle_read(Timestamp receive_time);
    void start_read_in_loop();
    void stop_read_in_loop();

    EventLoop* loop_;
    const std::string name_;
    int sockfd_;
    std::unique_ptr<Channel> channel_;
    bool bound_;
    bool reading_;
    MessageCallback message_callback_;
};

} // namespace xvorin::evix::net
