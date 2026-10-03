#include "base/current_thread.h"
#include "base/timestamp.h"

#include <cxxabi.h>
#include <execinfo.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

using namespace xvorin::evix;

namespace detail {

__thread int t_cached_tid = 0;
__thread char t_tid_string[32];
__thread int t_tid_string_length = 6;
__thread const char* t_thread_name = "unknown";

} // namespace detail

static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

// Static member accessors
int& CurrentThread::t_cached_tid()
{
    return detail::t_cached_tid;
}

char* CurrentThread::t_tid_string()
{
    return detail::t_tid_string;
}

int& CurrentThread::t_tid_string_length()
{
    return detail::t_tid_string_length;
}

const char*& CurrentThread::t_thread_name()
{
    return detail::t_thread_name;
}

void CurrentThread::cache_tid()
{
    if (detail::t_cached_tid == 0) {
        detail::t_cached_tid = static_cast<pid_t>(::syscall(SYS_gettid));
        detail::t_tid_string_length = snprintf(detail::t_tid_string, sizeof detail::t_tid_string, "%5d ", detail::t_cached_tid);
    }
}

bool CurrentThread::is_main_thread()
{
    return tid() == ::getpid();
}

void CurrentThread::sleep_usec(int64_t usec)
{
    struct timespec ts = { 0, 0 };
    ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
    ::nanosleep(&ts, NULL);
}

std::string CurrentThread::stack_trace(bool demangle)
{
    std::string stack;
    const int max_frames = 200;
    void* frame[max_frames];
    int nptrs = ::backtrace(frame, max_frames);
    char** strings = ::backtrace_symbols(frame, nptrs);

    if (strings) {
        size_t len = 256;
        char* demangled = demangle ? static_cast<char*>(::malloc(len)) : nullptr;

        for (int i = 1; i < nptrs; ++i) {
            if (demangle) {
                char* left_par = nullptr;
                char* plus = nullptr;
                for (char* p = strings[i]; *p; ++p) {
                    if (*p == '(')
                        left_par = p;
                    else if (*p == '+')
                        plus = p;
                }
                if (left_par && plus) {
                    *plus = '\0';
                    int status = 0;
                    char* ret = abi::__cxa_demangle(left_par + 1, demangled, &len, &status);
                    *plus = '+';
                    if (status == 0) {
                        demangled = ret;
                        stack.append(strings[i], left_par + 1);
                        stack.append(demangled);
                        stack.append(plus);
                        stack.push_back('\n');
                        continue;
                    }
                }
            }
            stack.append(strings[i]);
            stack.push_back('\n');
        }
        free(demangled);
        free(strings);
    }
    return stack;
}
