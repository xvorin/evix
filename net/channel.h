#pragma once

#include "base/timestamp.h"

#include <functional>
#include <memory>
#include <string>

namespace xvorin::evix::net {

class EventLoop;

class Channel {
public:
    Channel(const Channel&) = delete;
    void operator=(const Channel&) = delete;

    typedef std::function<void()> EventCallback;
    typedef std::function<void(Timestamp)> ReadEventCallback;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handle_event(Timestamp receive_time);

    void set_read_callback(ReadEventCallback cb)
    {
        read_callback_ = std::move(cb);
    }

    void set_write_callback(EventCallback cb)
    {
        write_callback_ = std::move(cb);
    }

    void set_close_callback(EventCallback cb)
    {
        close_callback_ = std::move(cb);
    }

    void set_error_callback(EventCallback cb)
    {
        error_callback_ = std::move(cb);
    }

    void tie(const std::shared_ptr<void>&);

    int fd() const
    {
        return fd_;
    }

    int events() const
    {
        return events_;
    }

    void set_revents(int revt)
    {
        revents_ = revt;
    }

    bool is_none_event() const
    {
        return events_ == kNoneEvent;
    }

    void enable_reading()
    {
        events_ |= kReadEvent;
        update();
    }

    void disable_reading()
    {
        events_ &= ~kReadEvent;
        update();
    }

    void enable_writing()
    {
        events_ |= kWriteEvent;
        update();
    }

    void disable_writing()
    {
        events_ &= ~kWriteEvent;
        update();
    }

    void disable_all()
    {
        events_ = kNoneEvent;
        update();
    }

    bool is_writing() const
    {
        return events_ & kWriteEvent;
    }

    bool is_reading() const
    {
        return events_ & kReadEvent;
    }

    int index()
    {
        return index_;
    }

    void set_index(int idx)
    {
        index_ = idx;
    }

    std::string revents_to_string() const;
    std::string events_to_string() const;

    void do_not_log_hup()
    {
        log_hup_ = false;
    }

    EventLoop* owner_loop()
    {
        return loop_;
    }

    void remove();

private:
    static std::string events_to_string(int fd, int ev);

    void update();
    void handle_event_with_guard(Timestamp receive_time);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;

    int index_;

    bool log_hup_;
    std::weak_ptr<void> tie_;
    bool tied_;
    bool event_handling_;
    bool added_to_loop_;
    ReadEventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;
};

} // namespace xvorin::evix::net
