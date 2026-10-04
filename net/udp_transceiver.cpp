#include "evix/net/udp_transceiver.h"

#include "evix/base/logging.h"
#include "evix/net/buffer.h"
#include "evix/net/channel.h"
#include "evix/net/event_loop.h"
#include "evix/net/sockets_ops.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

UdpTransceiver::UdpTransceiver(EventLoop* loop, const std::string& name)
    : loop_(loop)
    , name_(name)
    , sockfd_(sockets::create_nonblocking_udp_or_die(AF_INET))
    , channel_(new Channel(loop, sockfd_))
    , bound_(false)
    , reading_(false)
{
    channel_->set_read_callback(std::bind(&UdpTransceiver::handle_read, this, _1));
}

UdpTransceiver::~UdpTransceiver()
{
    // The transceiver is loop-affine: the channel is added to / removed from
    // the poller on the loop thread, so it must also be destroyed there.
    loop_->assert_in_loop_thread();
    if (bound_) {
        channel_->disable_all();
        channel_->remove();
    }
    sockets::close(sockfd_);
}

bool UdpTransceiver::bind(const InetAddress& listen_addr)
{
    loop_->assert_in_loop_thread();
    // IPv4 only: the socket is created AF_INET in the ctor.
    assert(listen_addr.family() == AF_INET);
    if (::bind(sockfd_, listen_addr.get_sock_addr(), static_cast<socklen_t>(sizeof(struct sockaddr_in))) < 0) {
        EXLE("UdpTransceiver[%s]::bind(%s) failed: %s", name_.c_str(), listen_addr.to_ip_port().c_str(),
            strerror(errno));
        return false;
    }
    bound_ = true;
    channel_->enable_reading();
    reading_ = true;
    return true;
}

ssize_t UdpTransceiver::send(const void* data, size_t len, const InetAddress& peer)
{
    loop_->assert_in_loop_thread();
    const socklen_t addrlen = (peer.family() == AF_INET6) ? static_cast<socklen_t>(sizeof(struct sockaddr_in6))
                                                          : static_cast<socklen_t>(sizeof(struct sockaddr_in));
    ssize_t n = ::sendto(sockfd_, data, len, 0, peer.get_sock_addr(), addrlen);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        EXLE("UdpTransceiver[%s]::send to %s failed: %s", name_.c_str(), peer.to_ip_port().c_str(), strerror(errno));
    }
    return n;
}

ssize_t UdpTransceiver::send(const std::string_view& data, const InetAddress& peer)
{
    return send(data.data(), data.size(), peer);
}

void UdpTransceiver::start_read()
{
    loop_->run_in_loop(std::bind(&UdpTransceiver::start_read_in_loop, this));
}

void UdpTransceiver::start_read_in_loop()
{
    loop_->assert_in_loop_thread();
    if (bound_ && !reading_) {
        channel_->enable_reading();
        reading_ = true;
    }
}

void UdpTransceiver::stop_read()
{
    loop_->run_in_loop(std::bind(&UdpTransceiver::stop_read_in_loop, this));
}

void UdpTransceiver::stop_read_in_loop()
{
    loop_->assert_in_loop_thread();
    if (reading_) {
        channel_->disable_reading();
        reading_ = false;
    }
}

void UdpTransceiver::handle_read(Timestamp receive_time)
{
    loop_->assert_in_loop_thread();
    // Read exactly one datagram per readable event. The channel is level
    // triggered, so any further queued datagrams re-arm the next loop
    // iteration -- draining in a loop here would starve other channels and
    // timers under a UDP flood.
    char buf[65536];
    struct sockaddr_in raw;
    memset(&raw, 0x00, sizeof raw);
    socklen_t addrlen = static_cast<socklen_t>(sizeof raw);
    ssize_t n = ::recvfrom(sockfd_, buf, sizeof buf, 0, reinterpret_cast<struct sockaddr*>(&raw), &addrlen);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            EXLE("UdpTransceiver[%s]::handle_read recvfrom failed: %s", name_.c_str(), strerror(errno));
        }
        return;
    }
    if (message_callback_) {
        InetAddress peer(raw);
        Buffer datagram;
        datagram.append(buf, static_cast<size_t>(n));
        message_callback_(this, &datagram, peer, receive_time);
    }
}
