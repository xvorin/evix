#include "test_framework.h"

#include "evix/base/base64.h"
#include "evix/base/blocking_queue.h"
#include "evix/base/class_registrar.h"
#include "evix/base/count_down_latch.h"
#include "evix/base/current_thread.h"
#include "evix/base/hex2string.h"
#include "evix/base/process_info.h"
#include "evix/base/signal_slot.h"
#include "evix/base/singleton.h"
#include "evix/base/string_util.h"
#include "evix/base/thread_pool.h"
#include "evix/base/timestamp.h"
#include "evix/base/weak_callback.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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
        void getValue()
        {
            (void)value;
        }
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
        int getValue() const override
        {
            return value;
        }
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

// ============== Base64 / Hex negative & edge cases ==============
TEST_CASE(base64_decode_invalid)
{
    // '!' is not a base64 alphabet character.
    std::string decoded = xvorin::evix::base64_decode(std::string("abc!"));
    TEST_ASSERT(decoded.empty(), "Invalid base64 should decode to empty");
    return true;
}

TEST_CASE(base64_roundtrip_binary)
{
    std::string original;
    for (int i = 0; i < 256; ++i) {
        original.push_back(static_cast<char>(i));
    }
    std::string encoded = xvorin::evix::base64_encode(original);
    std::string decoded = xvorin::evix::base64_decode(encoded);
    TEST_ASSERT(decoded == original, "Base64 binary roundtrip failed");
    return true;
}

TEST_CASE(hex_roundtrip_binary)
{
    std::string original;
    for (int i = 0; i < 256; ++i) {
        original.push_back(static_cast<char>(i));
    }
    std::string encoded = xvorin::evix::encode_to_hex_string(original);
    TEST_ASSERT(encoded.size() == original.size() * 2, "Hex length should double");
    std::string decoded = xvorin::evix::decode_hex_string(encoded);
    TEST_ASSERT(decoded == original, "Hex binary roundtrip failed");
    return true;
}

// ============== Blocking Queue advanced ==============
TEST_CASE(blocking_queue_mpsc)
{
    xvorin::evix::BlockingQueue<int> queue;
    const int kProducers = 4;
    const int kPerProducer = 1000;
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, kPerProducer]() {
            for (int i = 0; i < kPerProducer; ++i) {
                queue.put(i);
            }
        });
    }

    long long sum = 0;
    for (int i = 0; i < kProducers * kPerProducer; ++i) {
        sum += queue.take();
    }
    for (auto& t : producers) {
        t.join();
    }

    const long long expected = (long long)kProducers * (kPerProducer - 1) * kPerProducer / 2;
    TEST_ASSERT(sum == expected, "MPSC queue lost or corrupted items");
    TEST_ASSERT(queue.size() == 0, "Queue should be drained");
    return true;
}

TEST_CASE(blocking_queue_timed_take)
{
    xvorin::evix::BlockingQueue<int> queue;
    int out = -1;

    // Empty queue: timed take must time out and leave out untouched.
    bool got = queue.take(out, 0.05);
    TEST_ASSERT(!got, "Timed take on empty queue should time out");

    queue.put(99);
    got = queue.take(out, 1.0);
    TEST_ASSERT(got, "Timed take should succeed when item present");
    TEST_ASSERT(out == 99, "Timed take returned wrong value");
    return true;
}

TEST_CASE(blocking_queue_move_only)
{
    xvorin::evix::BlockingQueue<std::unique_ptr<int>> queue;
    queue.put(std::make_unique<int>(7));
    auto p = queue.take();
    TEST_ASSERT(p && *p == 7, "Move-only queue element corrupted");
    return true;
}

TEST_CASE(blocking_queue_drop_return)
{
    xvorin::evix::BlockingQueue<int> queue(1);
    bool d1 = queue.put(1);
    bool d2 = queue.put(2); // overflows, drops oldest
    TEST_ASSERT(!d1, "First put should not drop");
    TEST_ASSERT(d2, "Second put should report a drop");
    TEST_ASSERT(queue.size() == 1, "Queue should stay at limit");
    TEST_ASSERT(queue.drops() == 1, "Drop count should be 1");
    TEST_ASSERT(queue.take() == 2, "Oldest element should have been dropped");
    return true;
}

// ============== Count Down Latch multithread ==============
TEST_CASE(count_down_latch_threads)
{
    xvorin::evix::CountDownLatch latch(3);
    std::atomic<int> ran { 0 };
    std::vector<std::thread> workers;
    for (int i = 0; i < 3; ++i) {
        workers.emplace_back([&]() {
            ran++;
            latch.count_down();
        });
    }
    latch.wait();
    TEST_ASSERT(latch.get_count() == 0, "Latch should reach zero");
    TEST_ASSERT(ran.load() == 3, "All workers should have run before wait returned");
    for (auto& t : workers) {
        t.join();
    }
    return true;
}

// ============== Signal/Slot advanced ==============
TEST_CASE(signal_slot_multi)
{
    xvorin::evix::Signal<void(int)> signal;
    int a = 0;
    int b = 0;
    auto c1 = signal.connect([&](int x) { a += x; });
    auto c2 = signal.connect([&](int x) { b += x * 2; });
    TEST_ASSERT(signal.count() == 2, "Signal should have 2 slots");
    signal.call(3);
    TEST_ASSERT(a == 3 && b == 6, "Both slots should fire");
    return true;
}

TEST_CASE(signal_slot_tied_lifetime)
{
    xvorin::evix::Signal<void(int)> signal;
    int calls = 0;
    auto owner = std::make_shared<int>(0);
    auto conn = signal.connect([&](int) { calls++; }, owner);

    signal.call(1);
    TEST_ASSERT(calls == 1, "Tied slot should fire while owner alive");

    owner.reset(); // owner gone: tied slot must be skipped
    signal.call(1);
    TEST_ASSERT(calls == 1, "Tied slot must not fire after owner destroyed");
    return true;
}

TEST_CASE(signal_slot_auto_disconnect)
{
    xvorin::evix::Signal<void(int)> signal;
    int calls = 0;
    {
        auto conn = signal.connect([&](int) { calls++; });
        signal.call(1);
        TEST_ASSERT(calls == 1, "Slot should fire in scope");
    }
    // Slot handle dropped -> weak ref expires, cleaned up on next call.
    signal.call(1);
    TEST_ASSERT(calls == 1, "Expired slot must not fire");
    return true;
}

// ============== Singleton advanced ==============
TEST_CASE(singleton_multi_slot)
{
    struct S {
        int v = 0;
    };
    auto& a = xvorin::evix::Singleton<S, 2>::instance<0>();
    auto& b = xvorin::evix::Singleton<S, 2>::instance<1>();
    a.v = 11;
    b.v = 22;
    TEST_ASSERT(&a != &b, "Distinct slots must be distinct instances");
    TEST_ASSERT((xvorin::evix::Singleton<S, 2>::instance<0>().v == 11), "Slot 0 should persist");
    TEST_ASSERT((xvorin::evix::Singleton<S, 2>::instance<1>().v == 22), "Slot 1 should persist");
    return true;
}

TEST_CASE(singleton_thread_safe_init)
{
    struct S {
        int v = 123;
    };
    std::vector<std::thread> threads;
    std::vector<S*> ptrs(8, nullptr);
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([i, &ptrs]() {
            ptrs[i] = &xvorin::evix::Singleton<S>::instance();
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    for (int i = 1; i < 8; ++i) {
        TEST_ASSERT(ptrs[i] == ptrs[0], "All threads must observe the same instance");
    }
    return true;
}

// ============== Thread Pool ==============
TEST_CASE(thread_pool_runs_all_tasks)
{
    xvorin::evix::ThreadPool pool("test");
    pool.start(4);
    std::atomic<int> counter { 0 };
    const int kTasks = 2000;
    for (int i = 0; i < kTasks; ++i) {
        pool.run([&counter] { counter.fetch_add(1); });
    }
    pool.stop();
    TEST_ASSERT(counter.load() == kTasks, "Thread pool must run every task exactly once");
    return true;
}

TEST_CASE(thread_pool_inline_when_no_threads)
{
    xvorin::evix::ThreadPool pool("inline");
    // start() never called -> run() executes synchronously in the caller.
    int value = 0;
    pool.run([&value] { value = 42; });
    TEST_ASSERT(value == 42, "With no workers, run() must execute inline");
    return true;
}

TEST_CASE(thread_pool_thread_init_callback)
{
    xvorin::evix::ThreadPool pool("init");
    std::atomic<int> inits { 0 };
    pool.set_thread_init_callback([&inits] { inits.fetch_add(1); });
    pool.start(3);
    pool.stop();
    TEST_ASSERT(inits.load() == 3, "Thread init callback must run once per worker");
    return true;
}

TEST_CASE(thread_pool_bounded_backpressure)
{
    xvorin::evix::ThreadPool pool("bounded");
    pool.set_max_queue_size(4);
    pool.start(2);
    std::atomic<int> done { 0 };
    const int kTasks = 200;
    for (int i = 0; i < kTasks; ++i) {
        pool.run([&done] {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            done.fetch_add(1);
        });
    }
    pool.stop();
    TEST_ASSERT(done.load() == kTasks, "All tasks must complete under backpressure");
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

    // Base64 / Hex negative & edge cases
    RUN_TEST(base64_decode_invalid);
    RUN_TEST(base64_roundtrip_binary);
    RUN_TEST(hex_roundtrip_binary);

    // Blocking queue advanced
    RUN_TEST(blocking_queue_mpsc);
    RUN_TEST(blocking_queue_timed_take);
    RUN_TEST(blocking_queue_move_only);
    RUN_TEST(blocking_queue_drop_return);

    // Count down latch multithread
    RUN_TEST(count_down_latch_threads);

    // Signal/Slot advanced
    RUN_TEST(signal_slot_multi);
    RUN_TEST(signal_slot_tied_lifetime);
    RUN_TEST(signal_slot_auto_disconnect);

    // Singleton advanced
    RUN_TEST(singleton_multi_slot);
    RUN_TEST(singleton_thread_safe_init);

    // Thread pool
    RUN_TEST(thread_pool_runs_all_tasks);
    RUN_TEST(thread_pool_inline_when_no_threads);
    RUN_TEST(thread_pool_thread_init_callback);
    RUN_TEST(thread_pool_bounded_backpressure);

    TEST_SUMMARY();
    return TEST_RESULT();
}
