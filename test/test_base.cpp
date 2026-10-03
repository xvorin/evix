#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
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

#include "base/base64.h"
#include "base/blocking_queue.h"
#include "base/class_registrar.h"
#include "base/count_down_latch.h"
#include "base/current_thread.h"
#include "base/hex2string.h"
#include "base/process_info.h"
#include "base/signal_slot.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/timestamp.h"
#include "base/weak_callback.h"

using namespace xvorin::evix;

// ============== Timestamp Tests ==============
TEST_CASE(timestamp_now)
{
    Timestamp ts = Timestamp::now();
    TEST_ASSERT(ts.microSecondsSinceEpoch() > 0, "Timestamp should be positive");
    return true;
}

TEST_CASE(timestamp_add)
{
    Timestamp ts = Timestamp::fromUnixTime(1000); // 1000 seconds
    Timestamp ts2 = addTime(ts, 5.0); // add 5 seconds
    TEST_ASSERT(ts2.microSecondsSinceEpoch() == ts.microSecondsSinceEpoch() + 5000000, "Timestamp add failed");
    return true;
}

TEST_CASE(timestamp_format)
{
    Timestamp ts = Timestamp::fromUnixTime(0);
    std::string str = ts.toString();
    TEST_ASSERT(!str.empty(), "Timestamp format should not be empty");
    return true;
}

TEST_CASE(timestamp_valid)
{
    Timestamp ts = Timestamp::now();
    TEST_ASSERT(ts.valid() == true, "Valid timestamp should be valid");
    return true;
}

TEST_CASE(timestamp_invalid)
{
    Timestamp ts = Timestamp::invalid();
    TEST_ASSERT(ts.valid() == false, "Invalid timestamp should not be valid");
    return true;
}

// ============== String Util Tests ==============
TEST_CASE(string_util_to_str_int)
{
    std::string s = xvorin::evix::to_str(123);
    TEST_ASSERT(s == "123", "to_str int failed");
    return true;
}

TEST_CASE(string_util_to_str_double)
{
    std::string s = xvorin::evix::to_str(123.45);
    TEST_ASSERT(s.find("123.45") != std::string::npos, "to_str double failed");
    return true;
}

TEST_CASE(string_util_str_to)
{
    int val = xvorin::evix::str_to<int>("456");
    TEST_ASSERT(val == 456, "str_to int failed");
    return true;
}

TEST_CASE(string_util_to_lower)
{
    std::string s = "HELLO";
    std::string lower = xvorin::evix::to_lower(s);
    TEST_ASSERT(lower == "hello", "to_lower failed");
    return true;
}

TEST_CASE(string_util_to_upper)
{
    std::string s = "hello";
    std::string upper = xvorin::evix::to_upper(s);
    TEST_ASSERT(upper == "HELLO", "to_upper failed");
    return true;
}

TEST_CASE(string_util_format)
{
    std::string s = xvorin::evix::string_format("Hello %s %d", "World", 123);
    TEST_ASSERT(s == "Hello World 123", "string_format failed");
    return true;
}

// ============== Process Info Tests ==============
TEST_CASE(process_info_pid)
{
    pid_t pid = ProcessInfo::pid();
    TEST_ASSERT(pid > 0, "PID should be positive");
    return true;
}

TEST_CASE(process_info_uid)
{
    uid_t uid = ProcessInfo::uid();
    TEST_ASSERT(uid != (uid_t)-1, "UID should be valid");
    return true;
}

TEST_CASE(process_info_hostname)
{
    std::string hostname = ProcessInfo::hostname();
    TEST_ASSERT(!hostname.empty(), "Hostname should not be empty");
    return true;
}

TEST_CASE(process_info_page_size)
{
    int page_size = ProcessInfo::page_size();
    TEST_ASSERT(page_size > 0, "Page size should be positive");
    return true;
}

TEST_CASE(process_info_clock_ticks)
{
    int ticks = ProcessInfo::clock_ticks_per_second();
    TEST_ASSERT(ticks > 0, "Clock ticks should be positive");
    return true;
}

TEST_CASE(process_info_proc_status)
{
    std::string status = ProcessInfo::proc_status();
    TEST_ASSERT(!status.empty(), "Process status should not be empty");
    return true;
}

TEST_CASE(process_info_proc_stat)
{
    std::string stat = ProcessInfo::proc_stat();
    TEST_ASSERT(!stat.empty(), "Process stat should not be empty");
    return true;
}

TEST_CASE(process_info_exe_path)
{
    std::string path = ProcessInfo::exe_path();
    TEST_ASSERT(!path.empty(), "Exe path should not be empty");
    return true;
}

TEST_CASE(process_info_num_threads)
{
    int threads = ProcessInfo::num_threads();
    TEST_ASSERT(threads > 0, "Number of threads should be positive");
    return true;
}

// ============== Current Thread Tests ==============
TEST_CASE(current_thread_tid)
{
    int tid = CurrentThread::tid();
    TEST_ASSERT(tid > 0, "TID should be positive");
    return true;
}

TEST_CASE(current_thread_name)
{
    const char* name = CurrentThread::name();
    TEST_ASSERT(name != nullptr, "Thread name should not be null");
    return true;
}

TEST_CASE(current_thread_is_main)
{
    bool is_main = CurrentThread::is_main_thread();
    TEST_ASSERT(is_main == true, "Should be main thread in main");
    return true;
}

// ============== Hex String Tests ==============
TEST_CASE(hex_encode)
{
    std::string input = "Hello";
    std::string encoded = xvorin::evix::encode_to_hex_string(input);
    TEST_ASSERT(encoded == "48656C6C6F", "Hex encoding failed");
    return true;
}

TEST_CASE(hex_decode)
{
    std::string encoded = "48656c6c6f";
    std::string decoded = xvorin::evix::decode_hex_string(encoded);
    TEST_ASSERT(decoded == "Hello", "Hex decoding failed");
    return true;
}

// ============== Base64 Tests ==============
TEST_CASE(base64_encode_basic)
{
    std::string input = "Hello";
    std::string encoded = xvorin::evix::base64_encode(input);
    TEST_ASSERT(encoded == "SGVsbG8=", "Base64 encoding failed");
    return true;
}

TEST_CASE(base64_decode_basic)
{
    std::string encoded = "SGVsbG8=";
    std::string decoded = xvorin::evix::base64_decode(encoded);
    TEST_ASSERT(decoded == "Hello", "Base64 decoding failed");
    return true;
}

TEST_CASE(base64_encode_decode)
{
    // Test roundtrip
    std::string original = "The quick brown fox jumps over the lazy dog";
    std::string encoded = xvorin::evix::base64_encode(original);
    std::string decoded = xvorin::evix::base64_decode(encoded);
    TEST_ASSERT(decoded == original, "Base64 roundtrip failed");
    return true;
}

TEST_CASE(base64_encode_empty)
{
    std::string input = "";
    std::string encoded = xvorin::evix::base64_encode(input);
    TEST_ASSERT(encoded == "", "Base64 encoding empty string failed");
    return true;
}

TEST_CASE(base64_decode_empty)
{
    std::string encoded = "";
    std::string decoded = xvorin::evix::base64_decode(encoded);
    TEST_ASSERT(decoded == "", "Base64 decoding empty string failed");
    return true;
}

// ============== Blocking Queue Tests ==============
TEST_CASE(blocking_queue_basic)
{
    xvorin::evix::BlockingQueue<int> queue(10);

    queue.put(1);
    queue.put(2);
    queue.put(3);

    int val = queue.take();
    TEST_ASSERT(val == 1, "Blocking queue take failed");

    val = queue.take();
    TEST_ASSERT(val == 2, "Blocking queue take failed");

    val = queue.take();
    TEST_ASSERT(val == 3, "Blocking queue take failed");

    return true;
}

TEST_CASE(blocking_queue_size)
{
    xvorin::evix::BlockingQueue<int> queue(10);

    queue.put(1);
    queue.put(2);

    TEST_ASSERT(queue.size() == 2, "Size should be 2");

    queue.take();
    TEST_ASSERT(queue.size() == 1, "Size should be 1");

    return true;
}

TEST_CASE(blocking_queue_drop)
{
    xvorin::evix::BlockingQueue<int> queue(2);

    queue.put(1);
    queue.put(2);
    queue.put(3); // Should drop first element

    TEST_ASSERT(queue.size() == 2, "Size should be 2 (limited)");
    TEST_ASSERT(queue.drops() == 1, "Should have 1 drop");

    return true;
}

// ============== Count Down Latch Tests ==============
TEST_CASE(count_down_latch_basic)
{
    xvorin::evix::CountDownLatch latch(3);

    TEST_ASSERT(latch.get_count() == 3, "Initial count should be 3");

    latch.count_down();
    TEST_ASSERT(latch.get_count() == 2, "Count should be 2");

    latch.count_down();
    latch.count_down();

    latch.wait();
    TEST_ASSERT(latch.get_count() == 0, "Count should be 0 after wait");

    return true;
}

// ============== Weak Callback Tests ==============
TEST_CASE(weak_callback_basic)
{
    struct TestClass {
        int value = 42;
        void getValue() { (void)value; }
    };

    auto obj = std::make_shared<TestClass>();
    auto cb = xvorin::evix::makeWeakCallback(obj, &TestClass::getValue);

    // Object is alive, callback should work
    cb();

    // Delete object
    obj.reset();

    // Object is dead, callback should not crash (just do nothing)
    cb();

    return true;
}

// ============== Singleton Tests ==============
TEST_CASE(singleton_basic)
{
    struct TestSingleton {
        int value = 100;
    };

    auto& s1 = xvorin::evix::Singleton<TestSingleton>::instance();
    auto& s2 = xvorin::evix::Singleton<TestSingleton>::instance();

    TEST_ASSERT(&s1 == &s2, "Singleton should return same instance");

    return true;
}

// ============== Signal/Slot Tests ==============
TEST_CASE(signal_slot_basic)
{
    xvorin::evix::Signal<void(int)> signal;
    int result = 0;

    auto conn = signal.connect([&](int x) {
        result = x * 2;
    });

    signal.call(5);

    TEST_ASSERT(result == 10, "Signal/Slot call failed");

    return true;
}

TEST_CASE(signal_slot_disconnect)
{
    xvorin::evix::Signal<void(int)> signal;
    int call_count = 0;

    auto conn = signal.connect([&](int x) {
        (void)x;
        call_count++;
    });

    signal.call(1);
    TEST_ASSERT(call_count == 1, "First call should work");

    conn.reset();
    signal.call(2);

    TEST_ASSERT(call_count == 1, "After disconnect, call count should remain 1");

    return true;
}

// ============== Class Registrar Tests ==============
TEST_CASE(class_registrar_basic)
{
    struct Base {
        virtual ~Base() = default;
        virtual int getValue() const = 0;
    };

    struct Derived : Base {
        int value = 42;
        int getValue() const override { return value; }
    };

    // Register
    xvorin::evix::ClassRegistrar<Base>::register_object<Derived>("test");

    // Create
    auto obj = xvorin::evix::ClassRegistrar<Base>::create_instance("test");
    TEST_ASSERT(obj != nullptr, "Create instance failed");
    TEST_ASSERT(obj->getValue() == 42, "Get value failed");

    // Get
    auto obj2 = xvorin::evix::ClassRegistrar<Base>::get_instance("test");
    TEST_ASSERT(obj2 != nullptr, "Get instance failed");

    return true;
}

int main()
{
    std::cout << "========== BASE MODULE TESTS ==========" << std::endl;

    // Timestamp tests
    RUN_TEST(timestamp_now);
    RUN_TEST(timestamp_add);
    RUN_TEST(timestamp_format);
    RUN_TEST(timestamp_valid);
    RUN_TEST(timestamp_invalid);

    // String util tests
    RUN_TEST(string_util_to_str_int);
    RUN_TEST(string_util_to_str_double);
    RUN_TEST(string_util_str_to);
    RUN_TEST(string_util_to_lower);
    RUN_TEST(string_util_to_upper);
    RUN_TEST(string_util_format);

    // Process info tests
    RUN_TEST(process_info_pid);
    RUN_TEST(process_info_uid);
    RUN_TEST(process_info_hostname);
    RUN_TEST(process_info_page_size);
    RUN_TEST(process_info_clock_ticks);
    RUN_TEST(process_info_proc_status);
    RUN_TEST(process_info_proc_stat);
    RUN_TEST(process_info_exe_path);
    RUN_TEST(process_info_num_threads);

    // Current thread tests
    RUN_TEST(current_thread_tid);
    RUN_TEST(current_thread_name);
    RUN_TEST(current_thread_is_main);

    // Hex string tests
    RUN_TEST(hex_encode);
    RUN_TEST(hex_decode);

    // Base64 tests
    RUN_TEST(base64_encode_basic);
    RUN_TEST(base64_decode_basic);
    RUN_TEST(base64_encode_decode);
    RUN_TEST(base64_encode_empty);
    RUN_TEST(base64_decode_empty);

    // Blocking queue tests
    RUN_TEST(blocking_queue_basic);
    RUN_TEST(blocking_queue_size);
    RUN_TEST(blocking_queue_drop);

    // Count down latch tests
    RUN_TEST(count_down_latch_basic);

    // Weak callback tests
    RUN_TEST(weak_callback_basic);

    // Singleton tests
    RUN_TEST(singleton_basic);

    // Signal/Slot tests
    RUN_TEST(signal_slot_basic);
    RUN_TEST(signal_slot_disconnect);

    // Class registrar tests
    RUN_TEST(class_registrar_basic);

    std::cout << "========== SUMMARY ==========" << std::endl;
    std::cout << "Passed: " << g_passed << std::endl;
    std::cout << "Failed: " << g_failed << std::endl;

    return g_failed > 0 ? 1 : 0;
}
