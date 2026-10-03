#include "net/tcp_server.h"

#include "net/acceptor.h"
#include "net/event_loop.h"
#include "net/sockets_ops.h"

#include <stdio.h> // snprintf
#include <string>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

using namespace std;

TcpServer::TcpServer(EventLoop* loop,
    const InetAddress& listen_addr,
    const string& nameArg,
    Option option)
    : loop_(loop)
    , ip_port_(listen_addr.to_ip_port())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listen_addr, option == kReusePort))
    //, threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_(defaultConnectionCallback)
    , messageCallback_(defaultMessageCallback)
    , next_conn_id_(1)
{
    acceptor_->set_new_connection_callback(
        std::bind(&TcpServer::new_connection, this, _1, _2));
}

TcpServer::~TcpServer()
{
    loop_->assert_in_loop_thread();
    // LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";
    for (auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->get_loop()->run_in_loop(
            std::bind(&TcpConnection::connect_destroyed, conn));
    }
}

void TcpServer::send(const std::string& msg)
{
    for (auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        conn->send(msg);
    }
}

void TcpServer::send_mock(const std::string& msg, int index)
{
    int cnt = 0;
    for (auto& item : connections_) {
        if (cnt < index) {
            continue;
        }
        if (cnt > index) {
            break;
        }
        TcpConnectionPtr conn(item.second);
        conn->send(msg);
        cnt++;
    }
}

void TcpServer::set_thread_num(int numThreads)
{
    assert(0 <= numThreads);
    // threadPool_->set_thread_num(numThreads);
}

void TcpServer::start()
{
    if (started_.exchange(1) == 0) {
        // threadPool_->start(threadInitCallback_);
        assert(!acceptor_->listening());
        loop_->run_in_loop(
            std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

void TcpServer::new_connection(int sockfd, const InetAddress& peer_addr)
{
    loop_->assert_in_loop_thread();
    // EventLoop* ioLoop = threadPool_->getNextLoop();
    EventLoop* ioLoop = loop_;
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ip_port_.c_str(), next_conn_id_);
    ++next_conn_id_;
    string connName = name_ + buf;

    /*LOG_INFO << "TcpServer::new_connection [" << name_
             << "] - new connection [" << connName
             << "] from " << peer_addr.to_ip_port();*/
    InetAddress local_addr(sockets::get_local_addr(sockfd));

    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
        connName,
        sockfd,
        local_addr,
        peer_addr));

    connections_[connName] = conn;
    conn->set_connection_callback(connectionCallback_);
    conn->set_message_callback(messageCallback_);
    conn->set_write_complete_callback(writeCompleteCallback_);
    conn->set_close_callback(
        std::bind(&TcpServer::remove_connection, this, _1)); // FIXME: unsafe
    ioLoop->run_in_loop(std::bind(&TcpConnection::connect_established, conn));
}

void TcpServer::remove_connection(const TcpConnectionPtr& conn)
{
    // FIXME: unsafe
    loop_->run_in_loop(std::bind(&TcpServer::remove_connection_in_loop, this, conn));
}

void TcpServer::remove_connection_in_loop(const TcpConnectionPtr& conn)
{
    loop_->assert_in_loop_thread();
    // LOG_INFO << "TcpServer::remove_connection_in_loop [" << name_ << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);
    EventLoop* ioLoop = conn->get_loop();
    ioLoop->queue_in_loop(
        std::bind(&TcpConnection::connect_destroyed, conn));
}
