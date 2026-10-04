#include "evix/net/tcp_client.h"

#include "evix/net/connector.h"
#include "evix/net/event_loop.h"
#include "evix/net/sockets_ops.h"

#include <stdio.h>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

namespace detail {

void remove_connection(EventLoop* loop, const TcpConnectionPtr& conn)
{
    loop->queue_in_loop(std::bind(&TcpConnection::connect_destroyed, conn));
}

void remove_connector(const ConnectorPtr&)
{
}

} // namespace detail

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& nameArg)
    : loop_(loop)
    , connector_(new Connector(loop, serverAddr))
    , name_(nameArg)
    , connection_callback_(defaultConnectionCallback)
    , message_callback_(defaultMessageCallback)
    , retry_(false)
    , connect_(true)
    , next_conn_id_(1)
{
    connector_->set_new_connection_callback(std::bind(&TcpClient::new_connection, this, _1));
}

TcpClient::~TcpClient()
{
    TcpConnectionPtr conn;
    bool unique = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }

    if (conn) {
        assert(loop_ == conn->get_loop());
        CloseCallback cb = std::bind(&detail::remove_connection, loop_, _1);
        loop_->run_in_loop(std::bind(&TcpConnection::set_close_callback, conn, cb));
        if (unique) {
            conn->force_close();
        }
    } else {
        connector_->stop();
        loop_->run_after(1, std::bind(&detail::remove_connector, connector_));
    }
}

void TcpClient::connect()
{
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect()
{
    connect_ = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connection_) {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

void TcpClient::new_connection(int sockfd)
{
    loop_->assert_in_loop_thread();
    InetAddress peerAddr(sockets::get_peer_addr(sockfd));
    char buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr.to_ip_port().c_str(), next_conn_id_);
    ++next_conn_id_;
    std::string connName = name_ + buf;
    InetAddress localAddr(sockets::get_local_addr(sockfd));

    TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));
    conn->set_connection_callback(connection_callback_);
    conn->set_message_callback(message_callback_);
    conn->set_write_complete_callback(write_complete_callback_);
    conn->set_close_callback(std::bind(&TcpClient::remove_connection, this, _1));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_ = conn;
    }
    conn->connect_established();
}

void TcpClient::remove_connection(const TcpConnectionPtr& conn)
{
    loop_->assert_in_loop_thread();
    assert(loop_ == conn->get_loop());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        assert(connection_ == conn);
        connection_.reset();
    }
    loop_->queue_in_loop(std::bind(&TcpConnection::connect_destroyed, conn));
    if (retry_ && connect_) {
        connector_->restart();
    }
}
