#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

// Test macros
#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL: " << msg << " (" << #cond << ")" << std::endl; \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define TEST_CASE(name) bool test_##name()

// Global test results
static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(name)                              \
    do {                                            \
        std::cout << "Running " << #name << "... "; \
        if (test_##name()) {                        \
            std::cout << "PASSED" << std::endl;     \
            g_passed++;                             \
        } else {                                    \
            std::cout << "FAILED" << std::endl;     \
            g_failed++;                             \
        }                                           \
    } while (0)

#include "net/buffer.h"
#include "net/endian.h"
#include "net/event_loop.h"
#include "net/inet_address.h"
#include "net/socket.h"
#include "net/timer.h"

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

    // Timer tests
    RUN_TEST(timer_basic);
    RUN_TEST(timer_repeat);
    RUN_TEST(timer_num_created);

    std::cout << "========== SUMMARY ==========" << std::endl;
    std::cout << "Passed: " << g_passed << std::endl;
    std::cout << "Failed: " << g_failed << std::endl;

    return g_failed > 0 ? 1 : 0;
}
