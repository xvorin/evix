#include "evix/net/tcp_connection.h"

#include "evix/base/logging.h"
#include "evix/base/weak_callback.h"
#include "evix/net/channel.h"
#include "evix/net/event_loop.h"
#include "evix/net/socket.h"
#include "evix/net/sockets_ops.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

void xvorin::evix::net::defaultConnectionCallback(const TcpConnectionPtr&)
{
}

void xvorin::evix::net::defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf, Timestamp)
{
    buf->retrieve_all();
}

TcpConnection::TcpConnection(
    EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr)
    : loop_(loop)
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , local_addr_(localAddr)
    , peer_addr_(peerAddr)
    , high_water_mark_(64 * 1024 * 1024)
{
    channel_->set_read_callback(std::bind(&TcpConnection::handle_read, this, _1));
    channel_->set_write_callback(std::bind(&TcpConnection::handle_write, this));
    channel_->set_close_callback(std::bind(&TcpConnection::handle_close, this));
    channel_->set_error_callback(std::bind(&TcpConnection::handle_error, this));
    socket_->set_keep_alive(true);
}

TcpConnection::~TcpConnection()
{
    assert(state_ == kDisconnected);
}

bool TcpConnection::get_tcp_info(struct tcp_info* tcpi) const
{
    return socket_->get_tcp_info(tcpi);
}

std::string TcpConnection::get_tcp_info_string() const
{
    char buf[1024];
    buf[0] = '\0';
    socket_->get_tcp_info_string(buf, sizeof buf);
    return buf;
}

void TcpConnection::send(const void* data, int len)
{
    send(std::string_view(static_cast<const char*>(data), len));
}

void TcpConnection::send(const std::string_view& message)
{
    if (state_ == kConnected) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(message);
        } else {
            void (TcpConnection::*fp)(const std::string_view& message) = &TcpConnection::send_in_loop;
            loop_->run_in_loop(std::bind(fp, this, std::string(message)));
        }
    }
}

void TcpConnection::send(Buffer* buf)
{
    if (state_ == kConnected) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(buf->peek(), buf->readable_bytes());
            buf->retrieve_all();
        } else {
            void (TcpConnection::*fp)(const std::string_view& message) = &TcpConnection::send_in_loop;
            loop_->run_in_loop(std::bind(fp, this, buf->retrieve_all_as_string()));
        }
    }
}

void TcpConnection::send_in_loop(const std::string_view& message)
{
    send_in_loop(message.data(), message.size());
}

void TcpConnection::send_in_loop(const void* data, size_t len)
{
    loop_->assert_in_loop_thread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool fault_error = false;

    if (state_ == kDisconnected) {
        return;
    }

    if (!channel_->is_writing() && output_buffer_.readable_bytes() == 0) {
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && write_complete_callback_) {
                loop_->queue_in_loop(std::bind(write_complete_callback_, shared_from_this()));
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    fault_error = true;
                }
            }
        }
    }

    assert(remaining <= len);

    if (!fault_error && remaining > 0) {
        size_t old_len = output_buffer_.readable_bytes();
        if (old_len + remaining >= high_water_mark_ && old_len < high_water_mark_ && high_water_mark_callback_) {
            loop_->queue_in_loop(std::bind(high_water_mark_callback_, shared_from_this(), old_len + remaining));
        }
        output_buffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        if (!channel_->is_writing()) {
            channel_->enable_writing();
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected) {
        set_state(kDisconnecting);
        loop_->run_in_loop(std::bind(&TcpConnection::shutdown_in_loop, this));
    }
}

void TcpConnection::shutdown_in_loop()
{
    loop_->assert_in_loop_thread();
    if (!channel_->is_writing()) {
        socket_->shutdown_write();
    }
}

void TcpConnection::force_close()
{
    if (state_ == kConnected || state_ == kDisconnecting) {
        set_state(kDisconnecting);
        loop_->queue_in_loop(std::bind(&TcpConnection::force_close_in_loop, shared_from_this()));
    }
}

void TcpConnection::force_close_with_delay(double seconds)
{
    if (state_ == kConnected || state_ == kDisconnecting) {
        set_state(kDisconnecting);
        loop_->run_after(seconds, makeWeakCallback(shared_from_this(), &TcpConnection::force_close));
    }
}

void TcpConnection::force_close_in_loop()
{
    loop_->assert_in_loop_thread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        handle_close();
    }
}

const char* TcpConnection::state_to_string() const
{
    switch (state_) {
    case kDisconnected:
        return "kDisconnected";

    case kConnecting:
        return "kConnecting";

    case kConnected:
        return "kConnected";

    case kDisconnecting:
        return "kDisconnecting";

    default:
        return "unknown state";
    }
}

void TcpConnection::set_tcp_no_delay(bool on)
{
    socket_->set_tcp_no_delay(on);
}

void TcpConnection::start_read()
{
    loop_->run_in_loop(std::bind(&TcpConnection::start_read_in_loop, this));
}

void TcpConnection::start_read_in_loop()
{
    loop_->assert_in_loop_thread();
    if (!reading_ || !channel_->is_reading()) {
        channel_->enable_reading();
        reading_ = true;
    }
}

void TcpConnection::stop_read()
{
    loop_->run_in_loop(std::bind(&TcpConnection::stop_read_in_loop, this));
}

void TcpConnection::stop_read_in_loop()
{
    loop_->assert_in_loop_thread();
    if (reading_ || channel_->is_reading()) {
        channel_->disable_reading();
        reading_ = false;
    }
}

void TcpConnection::connect_established()
{
    loop_->assert_in_loop_thread();
    assert(state_ == kConnecting);
    set_state(kConnected);
    channel_->tie(shared_from_this());
    channel_->enable_reading();
    connection_callback_(shared_from_this());
}

void TcpConnection::connect_destroyed()
{
    loop_->assert_in_loop_thread();
    if (state_ == kConnected) {
        set_state(kDisconnected);
        channel_->disable_all();
        connection_callback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::handle_read(Timestamp receive_time)
{
    loop_->assert_in_loop_thread();
    int saved_errno = 0;
    ssize_t n = input_buffer_.readFd(channel_->fd(), &saved_errno);

    if (n > 0) {
        message_callback_(shared_from_this(), &input_buffer_, receive_time);
    } else if (n == 0) {
        handle_close();
    } else {
        errno = saved_errno;
        handle_error();
    }
}

void TcpConnection::handle_write()
{
    loop_->assert_in_loop_thread();
    if (channel_->is_writing()) {
        ssize_t n = sockets::write(channel_->fd(), output_buffer_.peek(), output_buffer_.readable_bytes());
        if (n > 0) {
            output_buffer_.retrieve(n);
            if (output_buffer_.readable_bytes() == 0) {
                channel_->disable_writing();
                if (write_complete_callback_) {
                    loop_->queue_in_loop(std::bind(write_complete_callback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) {
                    shutdown_in_loop();
                }
            }
        } else {
            EXLE("TcpConnection::handle_write() write to fd=%d failed: %s", channel_->fd(), strerror(errno));
        }
    } else {
    }
}

void TcpConnection::handle_close()
{
    loop_->assert_in_loop_thread();
    assert(state_ == kConnected || state_ == kDisconnecting);
    set_state(kDisconnected);
    channel_->disable_all();
    TcpConnectionPtr guardThis(shared_from_this());
    connection_callback_(guardThis);
    close_callback_(guardThis);
}

void TcpConnection::handle_error()
{
    int err = sockets::get_socket_error(channel_->fd());
    EXLE("TcpConnection::handle_error() SO_ERROR = %d %s", err, strerror(err));
}
