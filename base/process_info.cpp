#include "base/process_info.h"
#include "base/current_thread.h"

#include <algorithm>
#include <assert.h>
#include <dirent.h>
#include <fstream>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>

namespace xvorin::evix::detail {

__thread int t_num_opened_files = 0;

int fd_dir_filter(const struct dirent* d)
{
    if (::isdigit(d->d_name[0])) {
        ++t_num_opened_files;
    }
    return 0;
}

__thread std::vector<pid_t>* t_pids = NULL;

int task_dir_filter(const struct dirent* d)
{
    if (::isdigit(d->d_name[0])) {
        t_pids->push_back(atoi(d->d_name));
    }
    return 0;
}

int scan_dir(const char* dirpath, int (*filter)(const struct dirent*))
{
    struct dirent** namelist = NULL;
    int result = ::scandir(dirpath, &namelist, filter, alphasort);
    assert(namelist == NULL);
    return result;
}

Timestamp g_start_time = Timestamp::now();

int g_clock_ticks = static_cast<int>(::sysconf(_SC_CLK_TCK));

int g_page_size = static_cast<int>(::sysconf(_SC_PAGE_SIZE));

} // namespace xvorin::evix::detail

using namespace xvorin::evix;
using namespace xvorin::evix::detail;

pid_t ProcessInfo::pid()
{
    return ::getpid();
}

std::string ProcessInfo::pid_string()
{
    char buf[32];
    snprintf(buf, sizeof buf, "%d", pid());
    return buf;
}

uid_t ProcessInfo::uid()
{
    return ::getuid();
}

std::string ProcessInfo::username()
{
    struct passwd pwd;
    struct passwd* result = NULL;
    char buf[8192];
    const char* name = "unknownuser";
    getpwuid_r(uid(), &pwd, buf, sizeof buf, &result);
    if (result) {
        name = pwd.pw_name;
    }
    return name;
}

uid_t ProcessInfo::euid()
{
    return ::geteuid();
}

Timestamp ProcessInfo::start_time()
{
    return g_start_time;
}

int ProcessInfo::clock_ticks_per_second()
{
    return g_clock_ticks;
}

int ProcessInfo::page_size()
{
    return g_page_size;
}

bool ProcessInfo::is_debug_build()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

std::string ProcessInfo::hostname()
{
    char buf[256];
    if (::gethostname(buf, sizeof buf) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    } else {
        return "unknownhost";
    }
}

std::string ProcessInfo::procname()
{
    return std::string(procname(ProcessInfo::proc_stat()));
}

std::string_view ProcessInfo::procname(const std::string& stat)
{
    size_t lp = stat.find('(');
    size_t rp = stat.rfind(')');
    if (lp != std::string::npos && rp != std::string::npos && lp < rp) {
        return std::string_view(stat.data() + lp + 1, rp - lp - 1);
    }
    return {};
}

std::string ProcessInfo::proc_status()
{
    std::string result;
    std::ifstream file("/proc/self/status");
    if (file) {
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        if (size > 0) {
            result.reserve(static_cast<size_t>(size));
        }
        file.seekg(0, std::ios::beg);
        result.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
    return result;
}

std::string ProcessInfo::proc_stat()
{
    std::string result;
    std::ifstream file("/proc/self/stat");
    if (file) {
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        if (size > 0) {
            result.reserve(static_cast<size_t>(size));
        }
        file.seekg(0, std::ios::beg);
        result.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
    return result;
}

std::string ProcessInfo::thread_stat()
{
    char buf[64];
    snprintf(buf, sizeof buf, "/proc/self/task/%d/stat", CurrentThread::tid());
    std::string result;
    std::ifstream file(buf);
    if (file) {
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        if (size > 0) {
            result.reserve(static_cast<size_t>(size));
        }
        file.seekg(0, std::ios::beg);
        result.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
    return result;
}

std::string ProcessInfo::exe_path()
{
    std::string result;
    char buf[1024];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf);
    if (n > 0) {
        result.assign(buf, n);
    }
    return result;
}

int ProcessInfo::opened_files()
{
    t_num_opened_files = 0;
    scan_dir("/proc/self/fd", fd_dir_filter);
    return t_num_opened_files;
}

int ProcessInfo::max_open_files()
{
    struct rlimit rl;
    if (::getrlimit(RLIMIT_NOFILE, &rl)) {
        return opened_files();
    } else {
        return static_cast<int>(rl.rlim_cur);
    }
}

ProcessInfo::CpuTime ProcessInfo::cpu_time()
{
    ProcessInfo::CpuTime t;
    struct tms tms;
    if (::times(&tms) >= 0) {
        const double hz = static_cast<double>(clock_ticks_per_second());
        t.user_seconds = static_cast<double>(tms.tms_utime) / hz;
        t.system_seconds = static_cast<double>(tms.tms_stime) / hz;
    }
    return t;
}

int ProcessInfo::num_threads()
{
    int result = 0;
    std::string status = proc_status();
    size_t pos = status.find("Threads:");
    if (pos != std::string::npos) {
        result = ::atoi(status.c_str() + pos + 8);
    }
    return result;
}

std::vector<pid_t> ProcessInfo::threads()
{
    std::vector<pid_t> result;
    t_pids = &result;
    scan_dir("/proc/self/task", task_dir_filter);
    t_pids = NULL;
    std::sort(result.begin(), result.end());
    return result;
}
