#pragma once

#include "net/tcp_connection.h"

#include <atomic>
#include <map>
#include <string>

namespace xvorin::evix::net {

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
///
class TcpServer {
public:
    TcpServer(const TcpServer&) = delete;

    void operator=(const TcpServer&) = delete;

    typedef std::function<void(EventLoop*)> ThreadInitCallback;

    enum Option {
        kNoReusePort,
        kReusePort,
    };

    // TcpServer(EventLoop* loop, const InetAddress& listenAddr);
    TcpServer(EventLoop* loop,
        const InetAddress& listen_addr,
        const std::string& nameArg,
        Option option = kNoReusePort);

    ~TcpServer(); // force out-line dtor, for std::unique_ptr members.

    const std::string& ip_port() const
    {
        return ip_port_;
    }

    const std::string& name() const
    {
        return name_;
    }

    EventLoop* get_loop() const
    {
        return loop_;
    }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    void set_thread_num(int numThreads);

    void set_thread_init_callback(const ThreadInitCallback& cb)
    {
        threadInitCallback_ = cb;
    }

    /// valid after calling start()
    /*std::shared_ptr<EventLoopThreadPool> threadPool()
        {
            return threadPool_;
        }*/

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

    /// Set connection callback.
    /// Not thread safe.
    void set_connection_callback(const ConnectionCallback& cb)
    {
        connectionCallback_ = cb;
    }

    /// Set message callback.
    /// Not thread safe.
    void set_message_callback(const MessageCallback& cb)
    {
        messageCallback_ = cb;
    }

    /// Set write complete callback.
    /// Not thread safe.
    void set_write_complete_callback(const WriteCompleteCallback& cb)
    {
        writeCompleteCallback_ = cb;
    }

    void send(const std::string& msg);

    void send_mock(const std::string& msg, int index);

private:
    /// Not thread safe, but in loop
    void new_connection(int sockfd, const InetAddress& peer_addr);

    /// Thread safe.
    void remove_connection(const TcpConnectionPtr& conn);

    /// Not thread safe, but in loop
    void remove_connection_in_loop(const TcpConnectionPtr& conn);

    typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

    EventLoop* loop_; // the acceptor loop

    const std::string ip_port_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; // avoid revealing Acceptor

    // std::shared_ptr<EventLoopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    std::atomic<int32_t> started_;
    // always in loop thread
    int next_conn_id_;
    ConnectionMap connections_;
    // xvorin::evix::net::EventLoopThread eventLoopThread_;
};

} // namespace xvorin::evix::net
