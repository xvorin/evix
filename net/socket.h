#pragma once

struct tcp_info;

namespace xvorin::evix::net {

class InetAddress;

class Socket {
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
    }

    // Socket(Socket&&) // move constructor in C++11
    ~Socket();

    int fd() const
    {
        return sockfd_;
    }

    // return true if success.
    bool get_tcp_info(struct tcp_info*) const;
    bool get_tcp_info_string(char* buf, int len) const;

    /// abort if address in use
    void bind_address(const InetAddress& localaddr);

    /// abort if address in use
    void listen();

    /// On success, returns a non-negative integer that is
    /// a descriptor for the accepted socket, which has been
    /// set to non-blocking and close-on-exec. *peeraddr is assigned.
    /// On error, -1 is returned, and *peeraddr is untouched.
    int accept(InetAddress* peeraddr);

    void shutdown_write();

    ///
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    ///
    void set_tcp_no_delay(bool on);

    ///
    /// Enable/disable SO_REUSEADDR
    ///
    void set_reuse_addr(bool on);

    ///
    /// Enable/disable SO_REUSEPORT
    ///
    void set_reuse_port(bool on);

    ///
    /// Enable/disable SO_KEEPALIVE
    ///
    void set_keep_alive(bool on);

private:
    Socket(const Socket&) = delete;
    void operator=(const Socket&) = delete;

private:
    const int sockfd_;
};

} // namespace xvorin::evix::net
