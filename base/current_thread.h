#pragma once

#include <string>

namespace xvorin::evix {

class CurrentThread {
public:
    static void cache_tid();

    static int tid()
    {
        if (__builtin_expect(t_cached_tid() == 0, 0)) {
            cache_tid();
        }
        return t_cached_tid();
    }

    static const char* tid_string()
    {
        return t_tid_string();
    }

    static int tid_string_length()
    {
        return t_tid_string_length();
    }

    static const char* name()
    {
        return t_thread_name();
    }

    static bool is_main_thread();

    static void sleep_usec(int64_t usec);

    static std::string stack_trace(bool demangle);

private:
    CurrentThread(const CurrentThread&) = delete;
    void operator=(const CurrentThread&) = delete;

private:
    static int& t_cached_tid();
    static char* t_tid_string();
    static int& t_tid_string_length();
    static const char*& t_thread_name();
};

} // namespace xvorin::evix
