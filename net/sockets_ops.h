#pragma once

#include <arpa/inet.h>

namespace xvorin::evix::net::sockets {

int create_nonblocking_or_die(sa_family_t family);

int create_nonblocking_udp_or_die(sa_family_t family);

int connect(int sockfd, const struct sockaddr* addr);

void bind_or_die(int sockfd, const struct sockaddr* addr);

void listen_or_die(int sockfd);

int accept(int sockfd, struct sockaddr_in6* addr);

ssize_t read(int sockfd, void* buf, size_t count);

ssize_t readv(int sockfd, const struct iovec* iov, int iovcnt);

ssize_t write(int sockfd, const void* buf, size_t count);

void close(int sockfd);

void shutdown_write(int sockfd);

void to_ip_port(char* buf, size_t size, const struct sockaddr* addr);

void to_ip(char* buf, size_t size, const struct sockaddr* addr);

void from_ip_port(const char* ip, uint16_t port, struct sockaddr_in* addr);

void from_ip_port(const char* ip, uint16_t port, struct sockaddr_in6* addr);

int get_socket_error(int sockfd);

const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);

const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);

struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr);

const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);

const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

struct sockaddr_in6 get_local_addr(int sockfd);

struct sockaddr_in6 get_peer_addr(int sockfd);

bool is_self_connect(int sockfd);

} // namespace xvorin::evix::net::sockets
