#pragma once

#include "net/channel.h"
#include "net/event_loop.h"

#include <functional>
#include <thread>

namespace xvorin::evix {

class UdpSocket {
    typedef std::function<void(const char*, size_t)> ReadCallBack;

    typedef std::function<void(void)> TimerCallBack;

public:
    UdpSocket(net::EventLoop& eventLoop);

    virtual ~UdpSocket(void);

    bool isValid(void);

    void close(void);

    bool set_non_blocking(bool flag);

    bool bind(const std::string& ip, uint16_t port, ReadCallBack readCallBack);

    ssize_t send(const void*, size_t, const std::string&, uint16_t);

private:
    int32_t get_fd(void);

    void set_fd(int32_t);

    bool createSocket(void);

private:
    int32_t fd_ { -1 };

    ReadCallBack readCallBack;

    net::EventLoop& eventLoop;

    std::shared_ptr<net::Channel> channelPtr { nullptr };

    void recv(Timestamp timeStamp);
};

} // namespace xvorin::evix
