#pragma once

#include "base/timestamp.h"

#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

namespace xvorin::evix {

class ProcessInfo {
public:
    static pid_t pid();
    static std::string pid_string();
    static uid_t uid();
    static std::string username();
    static uid_t euid();
    static Timestamp start_time();
    static int clock_ticks_per_second();
    static int page_size();
    static bool is_debug_build();
    static std::string hostname();
    static std::string procname();
    static std::string_view procname(const std::string& stat);
    static std::string proc_status();
    static std::string proc_stat();
    static std::string thread_stat();
    static std::string exe_path();
    static int opened_files();
    static int max_open_files();

    struct CpuTime {
        double user_seconds;
        double system_seconds;

        CpuTime()
            : user_seconds(0.0)
            , system_seconds(0.0)
        {
        }

        double total() const
        {
            return user_seconds + system_seconds;
        }
    };

    static CpuTime cpu_time();
    static int num_threads();
    static std::vector<pid_t> threads();

private:
    ProcessInfo(const ProcessInfo&) = delete;
    void operator=(const ProcessInfo&) = delete;
};

} // namespace xvorin::evix
