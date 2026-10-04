#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace xvorin::evix {

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(size_t s = 0)
        : limit_(s)
        , queue_()
    {
    }

    bool put(const T& x)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(x);
        const bool dropped = limit_ && queue_.size() > limit_;
        if (dropped) {
            queue_.pop_front();
            ++drops_;
        }
        not_empty_.notify_one();
        return dropped;
    }

    bool put(T&& x)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(x));
        const bool dropped = limit_ && queue_.size() > limit_;
        if (dropped) {
            queue_.pop_front();
            ++drops_;
        }
        not_empty_.notify_one();
        return dropped;
    }

    T take()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        T front(std::move(queue_.front()));
        queue_.pop_front();
        return front;
    }

    bool take(T& front, double seconds)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds));
        if (!not_empty_.wait_for(lock, ns, [this] { return !queue_.empty(); })) {
            return false;
        }
        front = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    size_t drops() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return drops_;
    }

private:
    BlockingQueue(const BlockingQueue&) = delete;
    void operator=(const BlockingQueue&) = delete;

private:
    const size_t limit_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::deque<T> queue_;
    size_t drops_ = 0;
};

} // namespace xvorin::evix
