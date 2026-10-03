#include "base/string_util.h"

#include <stdarg.h>

using namespace xvorin::evix;

namespace xvorin::evix {

size_t string_format_append_impl(std::string& dst, const char* format, va_list ap)
{
    size_t bufsz = 1024;
    int result = 0;
    va_list backup_ap;
    while (true) {
        char buffer[bufsz];
        va_copy(backup_ap, ap);
        result = vsnprintf(buffer, sizeof(buffer), format, backup_ap);
        va_end(backup_ap);
        if (result < static_cast<int>(bufsz)) {
            result = (result <= 0) ? 0 : (dst.append(buffer, result), result);
            break;
        }
        bufsz = result + 1;
    }
    return result;
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
