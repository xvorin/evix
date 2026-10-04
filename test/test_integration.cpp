#include "test_framework.h"

#include "evix/base/count_down_latch.h"
#include "evix/net/buffer.h"
#include "evix/net/event_loop.h"
#include "evix/net/event_loop_thread_pool.h"
#include "evix/net/inet_address.h"
#include "evix/net/tcp_client.h"
#include "evix/net/tcp_connection.h"
#include "evix/net/tcp_server.h"
#include "evix/net/udp_transceiver.h"

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

namespace {

// Run a full server+client echo exchange over loopback on its own EventLoop
// thread, and return the bytes the client got back (empty on timeout).
// With io_threads > 0 the server dispatches the connection to an IO thread
// from its EventLoopThreadPool instead of the acceptor loop.
std::string run_echo(const std::string& payload, uint16_t port, int io_threads = 0,
    size_t* out_pool_loops = nullptr)
{
    std::atomic<EventLoop*> loop_ptr { nullptr };
    CountDownLatch loop_ready(1);
    std::promise<std::string> result;
    std::future<std::string> future = result.get_future();
    std::atomic<bool> delivered { false };
    std::atomic<size_t> pool_loops { 0 };

    std::thread th([&]() {
        EventLoop loop;
        loop_ptr.store(&loop);

        InetAddress listen_addr(port);
        TcpServer server(&loop, listen_addr, "echo", TcpServer::kReusePort);
        server.set_thread_num(io_threads);
        server.set_message_callback(
            [](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
                conn->send(buf); // echo everything back
            });
        server.start();
        pool_loops.store(server.thread_pool()->get_all_loops().size());

        InetAddress server_addr("127.0.0.1", port);
        TcpClient client(&loop, server_addr, "client");
        client.set_connection_callback(
            [&](const TcpConnectionPtr& conn) {
                if (conn->connected()) {
                    conn->send(std::string_view(payload));
                } else {
                    // Peer closed and the connection is fully torn down:
                    // now it is safe to stop the loop.
                    loop.quit();
                }
            });
        client.set_message_callback(
            [&](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
                if (buf->readable_bytes() >= payload.size() && !delivered.exchange(true)) {
                    result.set_value(buf->retrieve_all_as_string());
                    conn->shutdown(); // triggers the close handshake
                }
            });
        client.connect();

        loop_ready.count_down();
        loop.loop();
        // server/client destroyed here, in the loop thread, before loop.
    });

    loop_ready.wait();

    std::string got;
    bool ready = future.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    if (ready) {
        got = future.get();
    } else {
        // Fallback: unblock the loop so the thread can exit. quit() is thread
        // safe and wakes the loop up.
        while (loop_ptr.load() == nullptr) {
            std::this_thread::yield();
        }
        loop_ptr.load()->quit();
    }

    th.join();
    if (out_pool_loops) {
        *out_pool_loops = pool_loops.load();
    }
    return got;
}

// Send one datagram to an echoing UdpTransceiver and return the echo (empty on
// timeout). Server and client transceivers share one EventLoop thread.
std::string run_udp_echo(const std::string& payload, uint16_t port)
{
    std::atomic<EventLoop*> loop_ptr { nullptr };
    CountDownLatch loop_ready(1);
    std::promise<std::string> result;
    std::future<std::string> future = result.get_future();
    std::atomic<bool> delivered { false };

    std::thread th([&]() {
        EventLoop loop;
        loop_ptr.store(&loop);

        UdpTransceiver server(&loop, "userver");
        server.set_message_callback(
            [](UdpTransceiver* self, Buffer* buf, const InetAddress& peer, Timestamp) {
                std::string msg = buf->retrieve_all_as_string();
                self->send(msg, peer); // echo back to sender
            });
        server.bind(InetAddress(port));

        UdpTransceiver client(&loop, "uclient");
        client.set_message_callback(
            [&](UdpTransceiver*, Buffer* buf, const InetAddress&, Timestamp) {
                if (!delivered.exchange(true)) {
                    result.set_value(buf->retrieve_all_as_string());
                    loop.quit();
                }
            });
        client.bind(InetAddress("127.0.0.1", 0)); // ephemeral local port
        client.send(std::string_view(payload), InetAddress("127.0.0.1", port));

        loop_ready.count_down();
        loop.loop();
    });

    loop_ready.wait();

    std::string got;
    bool ready = future.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    if (ready) {
        got = future.get();
    } else {
        while (loop_ptr.load() == nullptr) {
            std::this_thread::yield();
        }
        loop_ptr.load()->quit();
    }

    th.join();
    return got;
}

} // namespace

TEST_CASE(tcp_echo_small)
{
    std::string got = run_echo("hello", 46123);
    TEST_ASSERT(got == "hello", "Echo of small payload mismatched");
    return true;
}

TEST_CASE(tcp_echo_large)
{
    std::string payload(200 * 1024, 'Z'); // forces output buffering + partial writes
    std::string got = run_echo(payload, 46124);
    TEST_ASSERT(got.size() == payload.size(), "Echo of large payload truncated");
    TEST_ASSERT(got == payload, "Echo of large payload corrupted");
    return true;
}

TEST_CASE(tcp_echo_threadpool)
{
    // Server runs 2 IO threads: the connection is handled off the acceptor
    // loop, exercising EventLoopThreadPool dispatch end to end.
    size_t pool_loops = 0;
    std::string payload(128 * 1024, 'Q');
    std::string got = run_echo(payload, 46125, 2, &pool_loops);
    TEST_ASSERT(pool_loops == 2, "Thread pool should expose 2 IO loops");
    TEST_ASSERT(got == payload, "Echo via thread pool corrupted");
    return true;
}

TEST_CASE(udp_echo)
{
    std::string got = run_udp_echo("ping-udp", 46126);
    TEST_ASSERT(got == "ping-udp", "UDP echo payload mismatched");
    return true;
}

int main()
{
    std::cout << "========== NET INTEGRATION TESTS ==========" << std::endl;

    RUN_TEST(tcp_echo_small);
    RUN_TEST(tcp_echo_large);
    RUN_TEST(tcp_echo_threadpool);
    RUN_TEST(udp_echo);

    TEST_SUMMARY();
    return TEST_RESULT();
}
