#pragma once

#include <functional>
#include <string>

namespace xvorin::evix {

enum class LogLevel {
    kTrace,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kFatal,
};

///
/// A lightweight, injectable logging facade.
///
/// The library never hard-codes an output target: it formats the message and
/// hands it to the current handler. Embedders can route evix logs into their
/// own logging system (glog, afl, spdlog, ...) via set_log_handler(). The
/// default handler writes to stderr. kFatal always aborts after the handler
/// returns.
///
/// @param level   severity of the record
/// @param file    source file (__FILE__)
/// @param line    source line (__LINE__)
/// @param msg     already-formatted message, without trailing newline
using LogHandler = std::function<void(LogLevel level, const char* file, int line, const std::string& msg)>;

/// Install a custom handler. Passing a null handler restores the default
/// stderr sink. Thread safe.
void set_log_handler(LogHandler handler);

/// Lowest level that is emitted. Records below @p level are dropped cheaply.
/// Thread safe. Default is kInfo.
void set_log_level(LogLevel level);

LogLevel log_level();

namespace detail {

void log_emit(LogLevel level, const char* file, int line, const char* fmt, ...) __attribute__((format(printf, 4, 5)));

} // namespace detail

} // namespace xvorin::evix

#define EXL(level, ...)                                                                                                \
    do {                                                                                                               \
        if ((level) >= ::xvorin::evix::log_level()) {                                                                  \
            ::xvorin::evix::detail::log_emit((level), __FILE__, __LINE__, __VA_ARGS__);                                \
        }                                                                                                              \
    } while (0)

#define EXLT(...) EXL(::xvorin::evix::LogLevel::kTrace, __VA_ARGS__)
#define EXLD(...) EXL(::xvorin::evix::LogLevel::kDebug, __VA_ARGS__)
#define EXLI(...) EXL(::xvorin::evix::LogLevel::kInfo, __VA_ARGS__)
#define EXLW(...) EXL(::xvorin::evix::LogLevel::kWarn, __VA_ARGS__)
#define EXLE(...) EXL(::xvorin::evix::LogLevel::kError, __VA_ARGS__)
// kFatal is always emitted and aborts the process after the handler returns.
#define EXLF(...) ::xvorin::evix::detail::log_emit(::xvorin::evix::LogLevel::kFatal, __FILE__, __LINE__, __VA_ARGS__)
