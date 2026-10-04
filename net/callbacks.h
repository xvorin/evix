#pragma once

#include "evix/base/timestamp.h"

#include <cassert>
#include <functional>
#include <memory>

namespace xvorin::evix {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

template <typename T>
inline T* get_pointer(const std::shared_ptr<T>& ptr)
{
    return ptr.get();
}

template <typename T>
inline T* get_pointer(const std::unique_ptr<T>& ptr)
{
    return ptr.get();
}

template <typename To, typename From>
inline ::std::shared_ptr<To> down_pointer_cast(const ::std::shared_ptr<From>& f)
{
    assert(f == nullptr || dynamic_cast<To*>(get_pointer(f)) != nullptr);
    return ::std::static_pointer_cast<To>(f);
}

namespace net {

class Buffer;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using TimerCallback = std::function<void()>;

using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;

void defaultConnectionCallback(const TcpConnectionPtr& conn);
void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp receive_time);

} // namespace xvorin::evix::net
} // namespace xvorin::evix
