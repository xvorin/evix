#include "evix/base/string_util.h"

#include <stdarg.h>

using namespace xvorin::evix;

namespace xvorin::evix {

size_t string_format_append_impl(std::string& dst, const char* format, va_list ap)
{
    // Measure the formatted length first (vsnprintf into a null buffer), then
    // format straight into the destination string's own heap storage. The old
    // implementation sized a stack VLA (`char buffer[bufsz]`) to the output
    // length, which overflowed the stack for large results -- and crashed even
    // on modest results when running on a small worker-thread stack.
    va_list backup_ap;
    va_copy(backup_ap, ap);
    const int needed = vsnprintf(nullptr, 0, format, backup_ap);
    va_end(backup_ap);

    if (needed <= 0) {
        return 0;
    }

    const size_t old_size = dst.size();
    dst.resize(old_size + static_cast<size_t>(needed));
    // vsnprintf writes `needed` chars plus a trailing '\0'; the '\0' lands on
    // dst[old_size + needed], the terminator slot std::string keeps writable.
    vsnprintf(&dst[old_size], static_cast<size_t>(needed) + 1, format, ap);
    return static_cast<size_t>(needed);
}

size_t string_format_append(std::string& dst, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    size_t result = string_format_append_impl(dst, format, ap);
    va_end(ap);
    return result;
}

size_t string_format(std::string& dst, const char* format, ...)
{
    dst.clear();
    va_list ap;
    va_start(ap, format);
    size_t result = string_format_append_impl(dst, format, ap);
    va_end(ap);
    return result;
}

std::string string_format(const char* format, ...)
{
    std::string result;
    va_list ap;
    va_start(ap, format);
    string_format_append_impl(result, format, ap);
    va_end(ap);
    return result;
}

} // namespace xvorin::evix
