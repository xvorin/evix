#pragma once

#include <string>

namespace xvorin::evix {

size_t base64_encode(const char* src, size_t len, char* dst);
size_t base64_encode(const char* src, size_t len, std::string& dst);
size_t base64_encode(const std::string& src, std::string& dst);

std::string base64_encode(const char* src, size_t len);
std::string base64_encode(const std::string& src);

size_t base64_decode(const char* src, size_t len, char* dst);
size_t base64_decode(const char* src, size_t len, std::string& dst);
size_t base64_decode(const std::string& src, std::string& dst);

std::string base64_decode(const char* src, size_t len);
std::string base64_decode(const std::string& src);

} // namespace xvorin::evix
