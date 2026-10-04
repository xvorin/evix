#include "evix/net/inet_address.h"

#include "evix/net/endian.h"
#include "evix/net/sockets_ops.h"

#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>

#pragma GCC diagnostic ignored "-Wold-style-cast"

static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;

#pragma GCC diagnostic error "-Wold-style-cast"

using namespace xvorin::evix;
using namespace xvorin::evix::net;

static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6), "InetAddress is same size as sockaddr_in6");

InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6)
{
    if (ipv6) {
        memset(&addr6_, 0x00, sizeof addr6_);
        addr6_.sin6_family = AF_INET6;
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = sockets::hostToNetwork16(port);
    } else {
        memset(&addr_, 0x00, sizeof addr_);
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
        addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
        addr_.sin_port = sockets::hostToNetwork16(port);
    }
}

InetAddress::InetAddress(const std::string& ip, uint16_t port, bool ipv6)
{
    if (ipv6) {
        memset(&addr6_, 0x00, sizeof addr6_);
        sockets::from_ip_port(ip.c_str(), port, &addr6_);
    } else {
        memset(&addr_, 0x00, sizeof addr_);
        sockets::from_ip_port(ip.c_str(), port, &addr_);
    }
}

std::string InetAddress::to_ip_port() const
{
    char buf[64] = "";
    sockets::to_ip_port(buf, sizeof buf, get_sock_addr());
    return buf;
}

std::string InetAddress::to_ip() const
{
    char buf[64] = "";
    sockets::to_ip(buf, sizeof buf, get_sock_addr());
    return buf;
}

uint32_t InetAddress::ipNetEndian() const
{
    assert(family() == AF_INET);
    return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::to_port() const
{
    return sockets::networkToHost16(portNetEndian());
}

static thread_local char t_resolve_buffer[64 * 1024];

bool InetAddress::resolve(const std::string& hostname, InetAddress* out)
{
    assert(out != nullptr);
    struct hostent hent = {};
    struct hostent* he = nullptr;
    int herrno = 0;
    memset(&hent, 0x00, sizeof(hent));
    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolve_buffer, sizeof t_resolve_buffer, &he, &herrno);
    if (ret == 0 && he != nullptr) {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        return true;
    } else {
        if (ret) { }
        return false;
    }
}

void InetAddress::set_scope_id(uint32_t scope_id)
{
    if (family() == AF_INET6) {
        addr6_.sin6_scope_id = scope_id;
    }
}
