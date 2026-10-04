#include "evix/net/connector.h"

#include "evix/net/channel.h"
#include "evix/net/event_loop.h"
#include "evix/net/sockets_ops.h"

#include "evix/base/logging.h"

#include <errno.h>
#include <string.h>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , server_addr_(serverAddr)
    , connect_(false)
    , state_(kDisconnected)
    , retry_delay_ms_(kInitRetryDelayMs)
{
}

Connector::~Connector()
{
    assert(!channel_);
}

void Connector::start()
{
    connect_ = true;
    loop_->run_in_loop(std::bind(&Connector::start_in_loop, this));
}

void Connector::start_in_loop()
{
    loop_->assert_in_loop_thread();
    assert(state_ == kDisconnected);
    if (connect_) {
        connect();
    } else {
    }
}

void Connector::stop()
{
    connect_ = false;
    loop_->queue_in_loop(std::bind(&Connector::stop_in_loop, this));
}

void Connector::stop_in_loop()
{
    loop_->assert_in_loop_thread();
    if (state_ == kConnecting) {
        set_state(kDisconnected);
        int sockfd = remove_and_reset_channel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    int sockfd = sockets::create_nonblocking_or_die(server_addr_.family());
    int ret = sockets::connect(sockfd, server_addr_.get_sock_addr());
    int saved_errno = (ret == 0) ? 0 : errno;

    switch (saved_errno) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        connecting(sockfd);
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        retry(sockfd);
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        EXLE("Connector::connect() fatal connect error to %s: %s",
            server_addr_.to_ip_port().c_str(), strerror(saved_errno));
        sockets::close(sockfd);
        break;

    default:
        EXLE("Connector::connect() unexpected connect error to %s: %s",
            server_addr_.to_ip_port().c_str(), strerror(saved_errno));
        sockets::close(sockfd);
        break;
    }
}

void Connector::restart()
{
    loop_->assert_in_loop_thread();
    set_state(kDisconnected);
    retry_delay_ms_ = kInitRetryDelayMs;
    connect_ = true;
    start_in_loop();
}

void Connector::connecting(int sockfd)
{
    set_state(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->set_write_callback(std::bind(&Connector::handle_write, this));
    channel_->set_error_callback(std::bind(&Connector::handle_error, this));

    channel_->enable_writing();
}

int Connector::remove_and_reset_channel()
{
    channel_->disable_all();
    channel_->remove();
    int sockfd = channel_->fd();
    loop_->queue_in_loop(std::bind(&Connector::reset_channel, this));
    return sockfd;
}

void Connector::reset_channel()
{
    channel_.reset();
}

void Connector::handle_write()
{
    if (state_ == kConnecting) {
        int sockfd = remove_and_reset_channel();
        int err = sockets::get_socket_error(sockfd);

        if (err) {
            EXLW("Connector::handle_write() SO_ERROR = %d %s", err, strerror(err));
            retry(sockfd);
        } else if (sockets::is_self_connect(sockfd)) {
            EXLW("Connector::handle_write() self connect, retrying");
            retry(sockfd);
        } else {
            set_state(kConnected);
            if (connect_) {
                new_connection_callback_(sockfd);
            } else {
                sockets::close(sockfd);
            }
        }
    } else {
        assert(state_ == kDisconnected);
    }
}

void Connector::handle_error()
{
    if (state_ == kConnecting) {
        int sockfd = remove_and_reset_channel();
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    set_state(kDisconnected);
    if (connect_) {
        loop_->run_after(retry_delay_ms_ / 1000.0, std::bind(&Connector::start_in_loop, shared_from_this()));
        retry_delay_ms_ = std::min(retry_delay_ms_ * 2, kMaxRetryDelayMs);
    } else {
    }
}
