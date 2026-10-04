#pragma once

#include "evix/net/tcp_connection.h"

#include <atomic>
#include <mutex>

namespace xvorin::evix::net {

class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient {
public:
    TcpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& nameArg);
    ~TcpClient();

    void connect();
    void disconnect();

    void stop();

    TcpConnectionPtr connection() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    EventLoop* get_loop() const
    {
        return loop_;
    }

    bool retry() const
    {
        return retry_;
    }

    void enable_retry()
    {
        retry_ = true;
    }

    const std::string& name() const
    {
        return name_;
    }

    void set_connection_callback(ConnectionCallback cb)
    {
        connection_callback_ = std::move(cb);
    }

    void set_message_callback(MessageCallback cb)
    {
        message_callback_ = std::move(cb);
    }

    void set_write_complete_callback(WriteCompleteCallback cb)
    {
        write_complete_callback_ = std::move(cb);
    }

private:
    TcpClient(const TcpClient&) = delete;
    void operator=(const TcpClient&) = delete;

private:
    void new_connection(int sockfd);

    void remove_connection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    ConnectorPtr connector_;

    const std::string name_;
    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    bool retry_;

    bool connect_;

    int next_conn_id_;
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;
};

} // namespace xvorin::evix::net
