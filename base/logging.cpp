#include "evix/base/logging.h"

#include <atomic>
#include <mutex>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

namespace xvorin::evix {

namespace {

const char* level_name(LogLevel level)
{
    switch (level) {
    case LogLevel::kTrace:
        return "TRACE";
    case LogLevel::kDebug:
        return "DEBUG";
    case LogLevel::kInfo:
        return "INFO";
    case LogLevel::kWarn:
        return "WARN";
    case LogLevel::kError:
        return "ERROR";
    case LogLevel::kFatal:
        return "FATAL";
    default:
        return "?";
    }
}

void default_handler(LogLevel level, const char* file, int line, const std::string& msg)
{
    fprintf(stderr, "[%s] %s:%d %s\n", level_name(level), file, line, msg.c_str());
    fflush(stderr);
}

std::mutex& handler_mutex()
{
    static std::mutex m;
    return m;
}

LogHandler& handler()
{
    static LogHandler h = default_handler;
    return h;
}

std::atomic<LogLevel> g_level { LogLevel::kInfo };

} // namespace

void set_log_handler(LogHandler h)
{
    std::lock_guard<std::mutex> guard(handler_mutex());
    handler() = h ? std::move(h) : LogHandler(default_handler);
}

void set_log_level(LogLevel level)
{
    g_level.store(level, std::memory_order_relaxed);
}

LogLevel log_level()
{
    return g_level.load(std::memory_order_relaxed);
}

namespace detail {

void log_emit(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) {
        buf[0] = '\0';
    }

    LogHandler current;
    {
        std::lock_guard<std::mutex> guard(handler_mutex());
        current = handler();
    }
    if (current) {
        current(level, file, line, std::string(buf));
    }

    if (level == LogLevel::kFatal) {
        abort();
    }
}

} // namespace detail

} // namespace xvorin::evix
