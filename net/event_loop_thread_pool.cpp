#include "evix/net/event_loop_thread_pool.h"

#include "evix/base/count_down_latch.h"
#include "evix/net/event_loop.h"

#include <cassert>
#include <mutex>

using namespace xvorin::evix::net;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop, std::string name)
    : base_loop_(base_loop)
    , name_(std::move(name))
    , started_(false)
    , num_threads_(0)
    , next_(0)
    , pool_(name_)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // Stop every IO loop first so its hosting worker task returns, then let the
    // worker pool join. loops_ is left untouched afterwards.
    for (EventLoop* loop : loops_) {
        loop->quit();
    }
    pool_.stop();
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    assert(!started_);
    started_ = true;

    if (num_threads_ == 0) {
        if (cb) {
            cb(base_loop_);
        }
        return;
    }

    // Each worker creates one EventLoop and runs it. We wait until all loops
    // are created and registered before returning, so get_next_loop() can be
    // used immediately and loops_ stays fixed for the pool's lifetime.
    CountDownLatch latch(num_threads_);
    std::mutex register_mutex;

    pool_.start(num_threads_);
    for (int i = 0; i < num_threads_; ++i) {
        pool_.run([this, &latch, &register_mutex, cb] {
            EventLoop loop;
            {
                std::lock_guard<std::mutex> guard(register_mutex);
                loops_.push_back(&loop);
            }
            if (cb) {
                cb(&loop);
            }
            latch.count_down();
            loop.loop();
            // loop quit(): fall out so the worker becomes idle; the loop is
            // destroyed here, in its own thread.
        });
    }
    latch.wait();
}

EventLoop* EventLoopThreadPool::get_next_loop()
{
    base_loop_->assert_in_loop_thread();
    EventLoop* loop = base_loop_;
    if (!loops_.empty()) {
        loop = loops_[next_];
        next_ = (next_ + 1) % loops_.size();
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::get_all_loops()
{
    if (loops_.empty()) {
        return { base_loop_ };
    }
    return loops_;
}
