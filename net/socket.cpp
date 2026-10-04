#include "evix/net/socket.h"

#include "evix/base/logging.h"
#include "evix/net/inet_address.h"
#include "evix/net/sockets_ops.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h> // snprintf

using namespace xvorin::evix;
using namespace xvorin::evix::net;

Socket::~Socket()
{
    sockets::close(sockfd_);
}

bool Socket::get_tcp_info(struct tcp_info* tcpi) const
{
    socklen_t len = sizeof(*tcpi);
    memset(tcpi, 0x00, len);
    return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::get_tcp_info_string(char* buf, int len) const
{
    struct tcp_info tcpi;
    bool ok = get_tcp_info(&tcpi);

    if (ok) {
        snprintf(buf, len,
            "unrecovered=%u "
            "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
            "lost=%u retrans=%u rtt=%u rttvar=%u "
            "sshthresh=%u cwnd=%u total_retrans=%u",
            tcpi.tcpi_retransmits, // Number of unrecovered [RTO] timeouts
            tcpi.tcpi_rto, // Retransmit timeout in usec
            tcpi.tcpi_ato, // Predicted tick of soft clock in usec
            tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss, tcpi.tcpi_lost, // Lost packets
            tcpi.tcpi_retrans, // Retransmitted packets out
            tcpi.tcpi_rtt, // Smoothed round trip time in usec
            tcpi.tcpi_rttvar, // Medium deviation
            tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,
            tcpi.tcpi_total_retrans); // Total retransmits for entire connection
    }

    return ok;
}

void Socket::bind_address(const InetAddress& addr)
{
    sockets::bind_or_die(sockfd_, addr.get_sock_addr());
}

void Socket::listen()
{
    sockets::listen_or_die(sockfd_);
}

int Socket::accept(InetAddress* peeraddr)
{
    struct sockaddr_in6 addr;
    memset(&addr, 0x00, sizeof addr);
    int connfd = sockets::accept(sockfd_, &addr);
    if (connfd >= 0) {
        peeraddr->set_sock_addr_inet6(addr);
    }
    return connfd;
}

void Socket::shutdown_write()
{
    sockets::shutdown_write(sockfd_);
}

void Socket::set_tcp_no_delay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

void Socket::set_reuse_addr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}

void Socket::set_reuse_port(bool on)
{
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        EXLE("Socket::set_reuse_port(%d) SO_REUSEPORT failed", sockfd_);
    }
#else
    if (on) {
        EXLE("SO_REUSEPORT is not supported.");
    }
#endif
}

void Socket::set_keep_alive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
    // FIXME CHECK
}
