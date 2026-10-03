#include "net/udpsocket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace xvorin::evix {

/**
 * @brief UdpSocket构造函数
 * @param eventLoop[in] EventLoop 引用
 */
UdpSocket::UdpSocket(net::EventLoop& eventLoop)
    : eventLoop(eventLoop)
{
    createSocket();
}

/**
 * @brief 创建socket描述符,此函数为虚函数,需要被重写
 * @return true
 * @return false
 */
bool UdpSocket::createSocket(void)
{
    if (!isValid()) {
        set_fd(socket(AF_INET, SOCK_DGRAM, 0));
        if (!isValid()) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 此函数为虚析构函数,为了释放资源
 */
UdpSocket::~UdpSocket(void)
{
    this->close();
}

/**
 * @brief 关闭socket函数
 */
void UdpSocket::close(void)
{
    if (isValid()) {
        if (-1 == ::close(this->fd_)) {
            return;
        }
    }
    this->fd_ = -1;
    return;
}

/**
 * @brief 查询分配的socket描述符是否有效
 * @return true
 * @return false
 */
bool UdpSocket::isValid(void)
{
    return this->fd_ != -1;
}

/**
 * @brief 返回分配的socket描述符
 * @return int32_t socket描述符
 */
int32_t UdpSocket::get_fd(void)
{
    return this->fd_;
}

/**
 * @brief Set the Fd object
 * @param fd
 */
void UdpSocket::set_fd(int32_t fd)
{
    this->fd_ = fd;
}

/**
 * @brief 将socket描述符设置为阻塞或者非阻塞状态
 * @param flag[in] 为true则设置为非阻塞,为false设置为阻塞
 * @return true
 * @return false
 */
bool UdpSocket::set_non_blocking(bool flag)
{
    if (!isValid()) {
        return false;
    }

    int opt = fcntl(this->fd_, F_GETFL, 0);
    if (opt == -1) {
        return false;
    }

    if (flag) {
        opt |= O_NONBLOCK;
    } else {
        opt &= ~O_NONBLOCK;
    }

    if (fcntl(this->fd_, F_SETFL, opt) == -1) {
        return false;
    }
    return true;
}

/**
 * @brief bind一个端口,且设置读取回调函数
 * @param ip[in] 绑定的ip地址
 * @param port[in] 绑定的端口号
 * @param readCallBack[in] 读取回调函数
 * @return true
 * @return false
 */
bool UdpSocket::bind(const std::string& ip, uint16_t port, ReadCallBack readCallBack)
{
    if (!isValid()) {
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (ip.empty()) {
        return false;
    }

    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (::bind(get_fd(), (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return false;
    }

    this->readCallBack = std::move(readCallBack);
    channelPtr.reset(new net::Channel(&eventLoop, get_fd()), [](net::Channel* channel) {
        channel->remove();
        delete channel;
    });
    channelPtr->set_read_callback(std::bind(&UdpSocket::recv, this, std::placeholders::_1));
    channelPtr->enable_reading();
    return true;
}

/**
 * @brief 发送数据的函数
 * @param buf[in] 需要发送出去数据的缓冲区首地址
 * @param size[in] 需要发送出去数据的字节个数
 * @param peerIp[in] 对端绑定的ip地址
 * @param peerPort[in] 对端绑定的端口号
 * @return ssize_t 返回-1表示失败,其它代表发送出去的字节个数
 */
ssize_t UdpSocket::send(const void* buf, size_t size, const std::string& peerIp, uint16_t peerPort)
{
    if (!isValid()) {
        return 0;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peerPort);
    addr.sin_addr.s_addr = inet_addr(peerIp.c_str());
    return ::sendto(get_fd(), buf, size, 0, (struct sockaddr*)&addr, sizeof(addr));
}

/**
 * @brief 接收数据函数,用来做回调函数
 * @param timeStamp[in] 接收到数据时刻的时间戳
 */
void UdpSocket::recv(Timestamp)
{
    char data[70000] { 0 };
    size_t size = ::recvfrom(get_fd(), data, sizeof(data), 0, nullptr, nullptr);
    if (size > 0) {
        readCallBack(data, size);
    }
}

} // namespace xvorin::evix
