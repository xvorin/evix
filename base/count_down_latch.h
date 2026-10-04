#pragma once

#include <condition_variable>
#include <mutex>

namespace xvorin::evix {

class CountDownLatch {
public:
    explicit CountDownLatch(int count)
        : count_(count)
    {
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return count_ <= 0; });
    }

    int wait(double timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait_for(lock,
            std::chrono::duration<double>(timeout),
            [this] { return count_ <= 0; });
        return count_;
    }

    void count_down()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        --count_;
        if (count_ == 0) {
            not_empty_.notify_all();
        }
    }

    int get_count() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    CountDownLatch(const CountDownLatch&) = delete;
    void operator=(const CountDownLatch&) = delete;

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    int count_;
};

} // namespace xvorin::evix
