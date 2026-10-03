#pragma once

#include <string.h>

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace xvorin::evix {

size_t string_format_append(std::string& dst, const char* format, ...);
size_t string_format(std::string& dst, const char* format, ...);
std::string string_format(const char* format, ...);

template <typename T>
inline std::string to_str(const T& t)
{
    std::ostringstream oss;
    oss << t;
    return oss.str();
}

template <typename T>
T str_to(const std::string& str)
{
    T t;
    std::istringstream iss(str);
    iss >> t;
    return t;
}

struct string_cmp_nocase {
    bool operator()(const std::string& lhs, const std::string& rhs) const
    {
        auto p = lhs.begin();
        auto p2 = rhs.begin();
        while (p != lhs.end() && p2 != rhs.end()) {
            if (toupper(*p) != toupper(*p2))
                return (toupper(*p) < toupper(*p2) ? 1 : 0);
            ++p;
            ++p2;
        }
        return (lhs.size() == rhs.size()) ? 0 : ((lhs.size() < rhs.size()) ? 1 : 0);
    }
};

inline std::string to_lower(const std::string& str)
{
    std::string t = str;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    return t;
}

inline std::string to_upper(const std::string& str)
{
    std::string t = str;
    std::transform(t.begin(), t.end(), t.begin(), ::toupper);
    return t;
}

inline bool starts_with(const std::string& str, const std::string& substr)
{
    return str.find(substr) == 0;
}

inline bool ends_with(const std::string& str, const std::string& substr)
{
    const auto pos = str.rfind(substr);
    return (pos != std::string::npos) && (pos == (str.length() - substr.length()));
}

inline bool equals(const std::string& lhs, const std::string& rhs)
{
    return (lhs) == (rhs);
}

inline std::string& trim_left(std::string& str, const char* delim = " ")
{
    str.erase(0, str.find_first_not_of(delim));
    return str;
}

inline std::string& trim_right(std::string& str, const char* delim = " ")
{
    str.erase(str.find_last_not_of(delim) + 1);
    return str;
}

inline std::string& trim(std::string& str, const char* delim = " ")
{
    trim_left(str, delim);
    trim_right(str, delim);
    return str;
}

inline std::string erase(std::string& str, char c = ' ')
{
    str.erase(std::remove_if(str.begin(), str.end(), [c](char x) { return x == c; }), str.end());
    return str;
}

inline std::string replace_all(std::string& str, const char* delim, const char* s = "")
{
    size_t len = strlen(delim);
    size_t pos = str.find(delim);
    while (pos != std::string::npos) {
        str.replace(pos, len, s);
        pos = str.find(delim, pos);
    }
    return str;
}

inline void split(const std::string& str,
    std::vector<std::string>& result,
    const std::string& delim = " ",
    bool insertEmpty = false)
{
    if (str.empty() || delim.empty()) {
        return;
    }
    std::string::const_iterator substart = str.begin();
    std::string::const_iterator subend = substart;
    while (true) {
        subend = std::search(substart, str.end(), delim.begin(), delim.end());
        std::string temp(substart, subend);
        if (!temp.empty()) {
            result.push_back(temp);
        } else if (insertEmpty) {
            result.push_back("");
        }
        if (subend == str.end())
            break;
        substart = subend + delim.size();
    }
}

template <typename SequenceSequenceT, typename Range1T>
inline typename SequenceSequenceT::value_type join(const SequenceSequenceT& Input, const Range1T& Separator)
{
    typedef typename SequenceSequenceT::value_type ResultT;
    typedef typename SequenceSequenceT::const_iterator InputIteratorT;
    InputIteratorT itBegin = Input.begin();
    InputIteratorT itEnd = Input.end();
    ResultT Result;
    if (itBegin != itEnd) {
        Result += *itBegin;
        ++itBegin;
    }
    for (; itBegin != itEnd; ++itBegin) {
        Result += Separator;
        Result += *itBegin;
    }
    return Result;
}

template <typename SequenceSequenceT, typename Range1T, typename PredicateT>
inline typename SequenceSequenceT::value_type join_if(const SequenceSequenceT& Input,
    const Range1T& Separator,
    PredicateT Pred)
{
    typedef typename SequenceSequenceT::value_type ResultT;
    typedef typename SequenceSequenceT::const_iterator InputIteratorT;
    InputIteratorT itBegin = Input.begin();
    InputIteratorT itEnd = Input.end();
    ResultT Result;
    while (itBegin != itEnd && !Pred(*itBegin))
        ++itBegin;
    if (itBegin != itEnd) {
        Result += *itBegin;
        ++itBegin;
    }
    for (; itBegin != itEnd; ++itBegin) {
        if (Pred(*itBegin)) {
            Result += Separator;
            Result += *itBegin;
        }
    }
    return Result;
}

} // namespace xvorin::evix
