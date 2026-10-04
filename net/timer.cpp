#include "evix/net/timer.h"

#include <atomic>

using namespace xvorin::evix;
using namespace xvorin::evix::net;

std::atomic<int64_t> Timer::s_num_created_;

void Timer::restart(Timestamp now)
{
    if (repeat_) {
        double diff = timeDifference(now, expiration_);
        if (diff < interval_) {
            expiration_ = addTime(expiration_, interval_);
        } else {
            expiration_ = addTime(now, interval_);
        }
    } else {
        expiration_ = Timestamp::invalid();
    }
}
