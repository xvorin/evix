#pragma once

#include "net/buffer.h"
#include "net/callbacks.h"
#include "net/inet_address.h"

#include <any>
#include <memory>

struct tcp_info;

namespace xvorin::evix::net {

class Channel;
class EventLoop;
class Socket;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop,
        const std::string& name,
        int sockfd,
        const InetAddress& localAddr,
        const InetAddress& peerAddr);

    ~TcpConnection();

    EventLoop* get_loop() const
    {
        return loop_;
    }

    const std::string& name() const
    {
        return name_;
    }

    const InetAddress& local_address() const
    {
        return local_addr_;
    }

    const InetAddress& peer_address() const
    {
        return peer_addr_;
    }

    bool connected() const
    {
        return state_ == kConnected;
    }

    bool disconnected() const
    {
        return state_ == kDisconnected;
    }

    bool get_tcp_info(struct tcp_info*) const;
    std::string get_tcp_info_string() const;

    void send(const void* message, int len);
    void send(const std::string_view& message);
    void send(Buffer* message);

    void shutdown();
    void force_close();
    void force_close_with_delay(double seconds);

    void set_tcp_no_delay(bool on);

    void start_read();
    void stop_read();

    bool is_reading() const
    {
        return reading_;
    }

    void set_context(const std::any& context)
    {
        context_ = context;
    }

    const std::any& get_context() const
    {
        return context_;
    }

    std::any* get_mutable_context()
    {
        return &context_;
    }

    void set_connection_callback(const ConnectionCallback& cb)
    {
        connection_callback_ = cb;
    }

    void set_message_callback(const MessageCallback& cb)
    {
        message_callback_ = cb;
    }

    void set_write_complete_callback(const WriteCompleteCallback& cb)
    {
        write_complete_callback_ = cb;
    }

    void set_high_water_mark_callback(const HighWaterMarkCallback& cb, size_t high_water_mark)
    {
        high_water_mark_callback_ = cb;
        high_water_mark_ = high_water_mark;
    }

    Buffer* input_buffer()
    {
        return &input_buffer_;
    }

    Buffer* output_buffer()
    {
        return &output_buffer_;
    }

    void set_close_callback(const CloseCallback& cb)
    {
        close_callback_ = cb;
    }

    void connect_established();
    void connect_destroyed();

private:
    enum StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void handle_read(Timestamp receive_time);
    void handle_write();
    void handle_close();
    void handle_error();

    void send_in_loop(const std::string_view& message);
    void send_in_loop(const void* message, size_t len);
    void shutdown_in_loop();
    void force_close_in_loop();

    void set_state(StateE s)
    {
        state_ = s;
    }

    const char* state_to_string() const;
    void start_read_in_loop();
    void stop_read_in_loop();

    EventLoop* loop_;
    const std::string name_;
    StateE state_;
    bool reading_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress local_addr_;
    const InetAddress peer_addr_;
    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    HighWaterMarkCallback high_water_mark_callback_;
    CloseCallback close_callback_;
    size_t high_water_mark_;
    Buffer input_buffer_;
    Buffer output_buffer_;
    std::any context_;
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

} // namespace xvorin::evix::net
