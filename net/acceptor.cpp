#include "evix/net/acceptor.h"

#include "evix/base/logging.h"
#include "evix/net/event_loop.h"
#include "evix/net/inet_address.h"
#include "evix/net/sockets_ops.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , accept_socket_(sockets::create_nonblocking_or_die(listenAddr.family()))
    , accept_channel_(loop, accept_socket_.fd())
    , listening_(false)
    , idle_fd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idle_fd_ >= 0);
    accept_socket_.set_reuse_addr(true);
    accept_socket_.set_reuse_port(reuseport);
    accept_socket_.bind_address(listenAddr);
    accept_channel_.set_read_callback(std::bind(&Acceptor::handle_read, this));
}

Acceptor::~Acceptor()
{
    accept_channel_.disable_all();
    accept_channel_.remove();
    ::close(idle_fd_);
}

void Acceptor::listen()
{
    loop_->assert_in_loop_thread();
    listening_ = true;
    accept_socket_.listen();
    accept_channel_.enable_reading();
}

void Acceptor::handle_read()
{
    loop_->assert_in_loop_thread();
    InetAddress peerAddr;

    int connfd = accept_socket_.accept(&peerAddr);

    if (connfd >= 0) {
        if (new_connection_callback_) {
            new_connection_callback_(connfd, peerAddr);
        } else {
            sockets::close(connfd);
        }
    } else {
        EXLE("Acceptor::handle_read() accept failed: %s", strerror(errno));
        // Read the section named "The special problem of
        // accept()ing when you can't" in libev's doc.
        if (errno == EMFILE) {
            ::close(idle_fd_);
            idle_fd_ = ::accept(accept_socket_.fd(), nullptr, nullptr);
            ::close(idle_fd_);
            idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
