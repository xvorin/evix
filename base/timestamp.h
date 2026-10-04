#pragma once

#include <string>

namespace xvorin::evix {

class Timestamp {
public:
    Timestamp()
        : microSecondsSinceEpoch_(0)
    {
    }

    explicit Timestamp(int64_t microSecondsSinceEpochArg)
        : microSecondsSinceEpoch_(microSecondsSinceEpochArg)
    {
    }

    void swap(Timestamp& that)
    {
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

    std::string toString() const;
    std::string toFormattedString(bool showMicroseconds = true) const;

    bool valid() const
    {
        return microSecondsSinceEpoch_ > 0;
    }

    int64_t microSecondsSinceEpoch() const
    {
        return microSecondsSinceEpoch_;
    }

    int64_t milliSecondsSinceEpoch() const
    {
        return microSecondsSinceEpoch_ / kMilliSecondsPerSecond;
    }

    time_t secondsSinceEpoch() const
    {
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    }

    static Timestamp now();

    static Timestamp invalid()
    {
        return Timestamp();
    }

    static Timestamp fromUnixTime(time_t t)
    {
        return fromUnixTime(t, 0);
    }

    static Timestamp fromUnixTime(time_t t, int microseconds)
    {
        return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
    }

    static Timestamp mktime(const unsigned int year0, const unsigned int mon0, const unsigned int day,
        const unsigned int hour, const unsigned int min, const unsigned int sec)
    {
        unsigned int mon = mon0, year = year0;
        if (0 >= (int)(mon -= 2)) {
            mon += 12;
            year -= 1;
        }
        int64_t ts_s = (int64_t)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) + year * 365 - 719499;
        ts_s = ((ts_s * 24 + hour) * 60 + min) * 60 + sec;
        return Timestamp(ts_s * 1000 * 1000);
    }

    static const int kMilliSecondsPerSecond = 1000;
    static const int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline double timeDifference(Timestamp high, Timestamp low)
{
    int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
    return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

} // namespace xvorin::evix
