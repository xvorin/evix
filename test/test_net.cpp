#include "test_framework.h"

#include "evix/net/buffer.h"
#include "evix/net/endian.h"
#include "evix/net/event_loop.h"
#include "evix/net/inet_address.h"
#include "evix/net/socket.h"
#include "evix/net/timer.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

// ============== Endian Tests ==============
TEST_CASE(endian_host_to_network)
{
    uint32_t host = 0x12345678;
    uint32_t net = sockets::hostToNetwork32(host);

#if __BYTE_ORDER == __LITTLE_ENDIAN
    TEST_ASSERT(net == 0x78563412, "hostToNetwork32 failed");
#else
    TEST_ASSERT(net == host, "hostToNetwork32 should be same on big endian");
#endif

    return true;
}

TEST_CASE(endian_network_to_host)
{
    uint32_t net = 0x78563412;
    uint32_t host = sockets::networkToHost32(net);

#if __BYTE_ORDER == __LITTLE_ENDIAN
    TEST_ASSERT(host == 0x12345678, "networkToHost32 failed");
#else
    TEST_ASSERT(host == net, "networkToHost32 should be same on big endian");
#endif

    return true;
}

TEST_CASE(endian_16bit)
{
    uint16_t host = 0x1234;
    uint16_t net = sockets::hostToNetwork16(host);

#if __BYTE_ORDER == __LITTLE_ENDIAN
    TEST_ASSERT(net == 0x3412, "hostToNetwork16 failed");
#else
    TEST_ASSERT(net == host, "hostToNetwork16 should be same on big endian");
#endif

    return true;
}

// ============== Inet Address Tests ==============
TEST_CASE(inet_address_constructor)
{
    InetAddress addr(8080);
    TEST_ASSERT(addr.to_port() == 8080, "Port should be 8080");
    return true;
}

TEST_CASE(inet_address_ip)
{
    InetAddress addr("127.0.0.1", 8080);
    std::string ip = addr.to_ip();
    TEST_ASSERT(ip == "127.0.0.1", "IP should be 127.0.0.1");
    return true;
}

TEST_CASE(inet_address_to_ip_port)
{
    InetAddress addr("192.168.1.1", 8080);
    std::string ip_port = addr.to_ip_port();
    TEST_ASSERT(ip_port.find("192.168.1.1") != std::string::npos, "to_ip_port failed");
    TEST_ASSERT(ip_port.find("8080") != std::string::npos, "to_ip_port port missing");
    return true;
}

// ============== Buffer Tests ==============
TEST_CASE(buffer_basic)
{
    Buffer buf;
    TEST_ASSERT(buf.readable_bytes() == 0, "Empty buffer should have 0 readable bytes");
    TEST_ASSERT(buf.writable_bytes() > 0, "Buffer should have writable space");
    return true;
}

TEST_CASE(buffer_append)
{
    Buffer buf;
    buf.append("hello", 5);
    TEST_ASSERT(buf.readable_bytes() == 5, "Should have 5 readable bytes");

    const char* data = buf.peek();
    TEST_ASSERT(memcmp(data, "hello", 5) == 0, "Peek should return correct data");
    return true;
}

TEST_CASE(buffer_retrieve)
{
    Buffer buf;
    buf.append("hello", 5);
    buf.retrieve(3);
    TEST_ASSERT(buf.readable_bytes() == 2, "Should have 2 readable bytes after retrieve");

    std::string s = buf.retrieve_all_as_string();
    TEST_ASSERT(s == "lo", "retrieve_all_as_string failed");
    return true;
}

TEST_CASE(buffer_retrieve_int)
{
    Buffer buf;
    buf.append_int32(0x12345678);

    int32_t val = buf.read_int32();
    TEST_ASSERT(val == 0x12345678, "read_int32 failed");
    TEST_ASSERT(buf.readable_bytes() == 0, "Buffer should be empty after reading");
    return true;
}

TEST_CASE(buffer_prepend)
{
    Buffer buf;
    buf.append("world", 5);
    buf.prepend_int32(42);

    TEST_ASSERT(buf.readable_bytes() == 9, "Should have 9 bytes");

    int32_t val = buf.read_int32();
    TEST_ASSERT(val == 42, "prepend_int32 failed");
    return true;
}

TEST_CASE(buffer_find_crlf)
{
    Buffer buf;
    buf.append("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", 38);

    const char* crlf = buf.find_crlf();
    TEST_ASSERT(crlf != nullptr, "find_crlf should find CRLF");
    TEST_ASSERT(crlf - buf.peek() == 14, "CRLF should be at position 14");
    return true;
}

TEST_CASE(buffer_grow)
{
    Buffer buf; // kInitialSize = 1024
    std::string big(5000, 'x');
    buf.append(big);
    TEST_ASSERT(buf.readable_bytes() == 5000, "Buffer should grow to hold 5000 bytes");
    TEST_ASSERT(buf.retrieve_all_as_string() == big, "Grown buffer content mismatch");
    return true;
}

TEST_CASE(buffer_internal_move)
{
    // Exercise make_space's "move readable data to front" branch: fill, drain
    // part (opening prependable room), then append again without growing.
    Buffer buf(64);
    buf.append(std::string(60, 'a'));
    buf.retrieve(50); // reader_index advances, 10 readable left
    size_t cap_before = buf.internal_capacity();
    buf.append(std::string(40, 'b')); // should reuse space via internal move
    TEST_ASSERT(buf.readable_bytes() == 50, "Should have 10 + 40 readable");
    TEST_ASSERT(buf.internal_capacity() == cap_before, "Should not have reallocated");
    std::string s = buf.retrieve_all_as_string();
    TEST_ASSERT(s == std::string(10, 'a') + std::string(40, 'b'), "Internal move corrupted data");
    return true;
}

TEST_CASE(buffer_prepend_payload)
{
    Buffer buf;
    buf.append("world", 5);
    buf.prepend_int32(42);
    TEST_ASSERT(buf.readable_bytes() == 9, "Should have 9 bytes after prepend");
    int32_t val = buf.read_int32();
    TEST_ASSERT(val == 42, "prepend_int32 failed");
    TEST_ASSERT(buf.retrieve_all_as_string() == "world", "Payload after prepend corrupted");
    return true;
}

TEST_CASE(buffer_find_eol)
{
    Buffer buf;
    buf.append("line1\nline2", 11);
    const char* eol = buf.find_eol();
    TEST_ASSERT(eol != nullptr, "find_eol should locate newline");
    TEST_ASSERT(eol - buf.peek() == 5, "newline at position 5");
    return true;
}

TEST_CASE(buffer_readfd_spill)
{
    // readFd must spill into the stack extrabuf when payload exceeds the
    // buffer's writable space, then grow and keep all bytes in order.
    int fds[2];
    TEST_ASSERT(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair failed");

    const size_t kBig = 100 * 1024; // > kInitialSize (1024) and > extrabuf? no, exercises spill
    std::string payload(kBig, 'z');
    size_t off = 0;
    // Write in chunks from another thread so the socket buffer does not block.
    std::thread writer([&]() {
        while (off < payload.size()) {
            ssize_t w = ::write(fds[1], payload.data() + off, payload.size() - off);
            if (w <= 0) {
                break;
            }
            off += static_cast<size_t>(w);
        }
        ::close(fds[1]);
    });

    Buffer buf;
    std::string got;
    while (got.size() < kBig) {
        int err = 0;
        ssize_t n = buf.readFd(fds[0], &err);
        if (n <= 0) {
            break;
        }
        got += buf.retrieve_all_as_string();
    }
    writer.join();
    ::close(fds[0]);

    TEST_ASSERT(got.size() == kBig, "readFd should recover all bytes");
    TEST_ASSERT(got == payload, "readFd data mismatch");
    return true;
}

// ============== Timer Tests ==============
TEST_CASE(timer_basic)
{
    bool fired = false;
    TimerCallback cb = [&fired]() { fired = true; };

    Timestamp when = addTime(Timestamp::now(), 0.1); // 100ms later
    Timer timer(cb, when, 0); // One-shot timer

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    timer.run();

    TEST_ASSERT(fired == true, "Timer should have fired");
    return true;
}

TEST_CASE(timer_repeat)
{
    int count = 0;
    TimerCallback cb = [&count]() { count++; };

    Timestamp when = addTime(Timestamp::now(), 0.05); // 50ms
    Timer timer(cb, when, 0.05); // Repeat every 50ms

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    timer.run();

    TEST_ASSERT(count >= 1, "Timer should have fired at least once");
    return true;
}

TEST_CASE(timer_num_created)
{
    int64_t before = Timer::num_created();

    bool fired = false;
    TimerCallback cb = [&fired]() { fired = true; };
    Timestamp when = addTime(Timestamp::now(), 1.0);
    Timer timer(cb, when, 0);

    int64_t after = Timer::num_created();
    TEST_ASSERT(after > before, "Timer count should increase");
    return true;
}

int main()
{
    std::cout << "========== NET MODULE TESTS ==========" << std::endl;

    // Endian tests
    RUN_TEST(endian_host_to_network);
    RUN_TEST(endian_network_to_host);
    RUN_TEST(endian_16bit);

    // InetAddress tests
    RUN_TEST(inet_address_constructor);
    RUN_TEST(inet_address_ip);
    RUN_TEST(inet_address_to_ip_port);

    // Buffer tests
    RUN_TEST(buffer_basic);
    RUN_TEST(buffer_append);
    RUN_TEST(buffer_retrieve);
    RUN_TEST(buffer_retrieve_int);
    RUN_TEST(buffer_prepend);
    RUN_TEST(buffer_find_crlf);
    RUN_TEST(buffer_grow);
    RUN_TEST(buffer_internal_move);
    RUN_TEST(buffer_prepend_payload);
    RUN_TEST(buffer_find_eol);
    RUN_TEST(buffer_readfd_spill);

    // Timer tests
    RUN_TEST(timer_basic);
    RUN_TEST(timer_repeat);
    RUN_TEST(timer_num_created);

    TEST_SUMMARY();
    return TEST_RESULT();
}
