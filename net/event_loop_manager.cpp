/*
 * EventLoopManager.cc
 *
 *  Created on: 2020年8月5日
 *      Author: chewenyao
 */

#include "net/event_loop_manager.h"

#include "base/count_down_latch.h"
#include "net/event_loop.h"

#include <cxxabi.h>
#include <execinfo.h>
#include <limits.h>
#include <mutex>
#include <set>
#include <signal.h>
#include <sstream>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#define AFLLOG(lvl, fmt, ...)
#define gLogT(...) AFLLOG(0, __VA_ARGS__)
#define gLogD(...) AFLLOG(1, __VA_ARGS__)
#define gLogI(...) AFLLOG(2, __VA_ARGS__)
#define gLogW(...) AFLLOG(3, __VA_ARGS__)
#define gLogE(...) AFLLOG(4, __VA_ARGS__)
#define gLogF(...) printf(__VA_ARGS__) // AFLLOG(5, __VA_ARGS__)

using namespace xvorin::evix::net;

struct EventLoopManagerContext {
    const int PTHREAD_CREATE_RETRY_TIME = 3;

    std::shared_ptr<xvorin::evix::net::EventLoop> s_MainLoop;
    std::atomic<bool> s_MainLoopRunning { false };
    std::mutex s_Thread2EventLoopMapLock;
    std::mutex s_Name2EventLoopMapLock;
    std::mutex s_ModuleThreadAttrsLock;

    /** store the pthread_t to event loop map for getCurrentEventLoop() interface */
    std::unordered_map<pthread_t, std::weak_ptr<xvorin::evix::net::EventLoop>> s_Thread2EventLoopMap;
    /** store the name to  event loop map for getEventLoop(name) interface */
    std::unordered_map<std::string, std::weak_ptr<xvorin::evix::net::EventLoop>> s_Name2EventLoopMap;
    /** store the name to thread attribute map for createEventLoop()  */
    std::unordered_map<std::string, EventLoopManager::ThreadAttribute> s_ModuleThreadAttrs;

    std::once_flag s_InitOnce;
};

static EventLoopManagerContext& get_context()
{
    static EventLoopManagerContext evmEtx;
    return evmEtx;
}

EventLoopManager::EventLoopManager(bool main_loop)
{
    auto& ctx = get_context();

    std::call_once(ctx.s_InitOnce, [&]() {
        if (main_loop) {
            ctx.s_MainLoop = std::make_shared<xvorin::evix::net::EventLoop>();
            std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
            ctx.s_Thread2EventLoopMap[pthread_self()] = ctx.s_MainLoop;
        }
    });
}

EventLoopManager::~EventLoopManager()
{
}

void EventLoopManager::run()
{
    auto& ctx = get_context();
    ctx.s_MainLoopRunning = true;
    if (ctx.s_MainLoop) {
        install_trace_back_signals();
        ctx.s_MainLoop->loop();
    } else {
        while (ctx.s_MainLoopRunning) {
            sleep(1);
        }
    }
}

void EventLoopManager::quit()
{
    auto& ctx = get_context();
    {
        std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
        for (auto& ev : ctx.s_Thread2EventLoopMap) {
            auto loop = ev.second.lock();
            if (loop) {
                loop->quit();
            }
        }
    }

    if (ctx.s_MainLoop) {
        ctx.s_MainLoop->quit();
    }
    ctx.s_MainLoopRunning = false;
}

int EventLoopManager::createCustomAttributes(const std::string& name,
    EvmThreadCreateType unique, int stack_size, int policy, int priority, std::vector<bool> affinity)
{
    if (name.empty()) {
        return -1;
    }

    stack_size *= 1024; /* stack_size KBytes */
    stack_size = std::max((int)PTHREAD_STACK_MIN, stack_size);
    stack_size = std::min(stack_size, (int)get_upper_limit(RLIMIT_STACK, 8192 * 1024));

    if (SCHED_FIFO != policy && SCHED_RR != policy && SCHED_OTHER != policy) {
        return -2;
    }

    if (SCHED_FIFO == policy || SCHED_RR == policy) {
        priority = std::max(priority, sched_get_priority_min(policy));
        priority = std::min(priority, sched_get_priority_max(policy));
    }

    ThreadAttribute attr;
    attr.name = name;
    attr.unique = unique;
    attr.stack_size = stack_size;
    attr.policy = policy;
    attr.priority = priority;
    attr.affinity = affinity;

    auto& ctx = get_context();
    {
        std::lock_guard<std::mutex> guard(ctx.s_ModuleThreadAttrsLock);
        ctx.s_ModuleThreadAttrs[name] = attr;
    }
    gLogT("createCustomAttributes for %s(%d,%d,%d,%d)", name.c_str(),
        (int)attr.unique, attr.stack_size, attr.policy, attr.priority);
    return 0;
}

std::shared_ptr<xvorin::evix::net::EventLoop> EventLoopManager::get_event_loop(
    const std::string& name)
{
    std::shared_ptr<xvorin::evix::net::EventLoop> retval = nullptr;
    ThreadAttribute attr;
    auto& ctx = get_context();
    {
        std::lock_guard<std::mutex> guard(ctx.s_ModuleThreadAttrsLock);
        auto attrIter = ctx.s_ModuleThreadAttrs.find(name);
        if (attrIter == ctx.s_ModuleThreadAttrs.end()) {
            assert(ctx.s_MainLoop);
            return ctx.s_MainLoop;
        }
        attr = attrIter->second;
    }

    if (xvorin::evix::net::EvmMulti == attr.unique) {
        retval = create_event_loop(attr);
        if (!retval) {
            gLogW("Uses main loop for %s's request!", name.c_str());
            assert(ctx.s_MainLoop);
            retval = ctx.s_MainLoop;
        }
        gLogT("thread create done for %s!", name.c_str());
        return retval;
    }

    std::lock_guard<std::mutex> guard(ctx.s_Name2EventLoopMapLock);
    auto loopIter = ctx.s_Name2EventLoopMap.find(name);
    if (loopIter != ctx.s_Name2EventLoopMap.end()) {
        retval = loopIter->second.lock();
    }

    if (retval) {
        gLogT("unique thread had been created for %s!", name.c_str());
        return retval;
    }

    retval = create_event_loop(attr);
    if (!retval) {
        gLogW("Uses main loop for %s's unique request!", name.c_str());
        assert(ctx.s_MainLoop);
        retval = ctx.s_MainLoop;
    }
    ctx.s_Name2EventLoopMap[name] = retval;
    gLogT("unique thread create done for %s!", name.c_str());
    return retval;
}

std::shared_ptr<xvorin::evix::net::EventLoop> EventLoopManager::get_current_event_loop()
{
    auto& ctx = get_context();
    std::shared_ptr<xvorin::evix::net::EventLoop> retval;
    pthread_t tid = pthread_self();
    {
        std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
        auto iter = ctx.s_Thread2EventLoopMap.find(tid);
        retval = (iter == ctx.s_Thread2EventLoopMap.end()) ? nullptr : iter->second.lock();
    }
    return retval;
}

using namespace xvorin::evix::net;

std::shared_ptr<EventLoop> EventLoopManager::create_event_loop(
    const ThreadAttribute& attr)
{
    std::shared_ptr<EventLoop> retval = nullptr;
    CountDownLatch cdl(1);
    void* args[] = { &cdl, &retval };
    pthread_t tid;
    struct sched_param param;
    pthread_attr_t pAttr;
    memset(&pAttr, 0x00, sizeof(pAttr));
    pthread_attr_init(&pAttr);
    pthread_attr_setscope(&pAttr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setstacksize(&pAttr, attr.stack_size);

    if (SCHED_FIFO == attr.policy || SCHED_RR == attr.policy) {
        pthread_attr_setschedpolicy(&pAttr, attr.policy);
        pthread_attr_getschedparam(&pAttr, &param);
        param.sched_priority = (0 == geteuid()) ? (attr.priority) : 0;
        pthread_attr_setschedparam(&pAttr, &param);
    }

    auto& ctx = get_context();
    int retry = ctx.PTHREAD_CREATE_RETRY_TIME;
    while (1) {
        int retcode = pthread_create(&tid, &pAttr, create_event_loop_detail, args);
        if (0 != retcode && (EAGAIN == errno || EWOULDBLOCK == errno)
            && retry-- > 0) {
            gLogW("pthread_create failure because of EAGAIN, retry(%d/%d)!",
                PTHREAD_CREATE_RETRY_TIME - retry,
                PTHREAD_CREATE_RETRY_TIME);
            usleep(1 * 1000);
            continue;
        }

        if (0 != retcode) {
            gLogE("pthread_create error, reason:%s!", strerror(retcode));
            return nullptr;
        }

        if (!attr.affinity.empty()) {
            cpu_set_t mask;
            CPU_ZERO(&mask);
            for (size_t i = 0; i < attr.affinity.size(); i++) {
                if (attr.affinity[i]) {
                    CPU_SET(i, &mask);
                }
            }
            retcode = pthread_setaffinity_np(tid, sizeof(mask), &mask);
            if (0 != retcode) {
                gLogW("pthread_setaffinity_np error, reason:%s!", strerror(retcode));
            }
        }
        break;
    }

    /* waiting for new EventLoop */
    cdl.wait();
    assert(retval);

    if (0 != pthread_detach(tid)) {
        pthread_attr_destroy(&pAttr);
        gLogW("pthread_detach(tid=%lu) error! reason:%s", tid, strerror(errno));
    }
    gLogT("New thread(tid=%lu) created!", tid);
    return retval;
}

void* EventLoopManager::create_event_loop_detail(void* arg)
{
    pthread_t tid = pthread_self();
    CountDownLatch* cdl = reinterpret_cast<CountDownLatch*>(((char**)arg)[0]);
    std::shared_ptr<EventLoop>* retloop = reinterpret_cast<std::shared_ptr<EventLoop>*>(((char**)arg)[1]);
    assert(cdl && retloop);

    auto loop = std::make_shared<EventLoop>();
    *retloop = loop;

    auto& ctx = get_context();
    {
        std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
        ctx.s_Thread2EventLoopMap[tid] = loop;
    }

    /* notify main thread going */
    cdl->count_down();

    /* detect if idle, and quit this thread */
    auto timer = loop->run_every(5, [&]() {   if(loop.use_count() <= 1) loop->quit(); });
    install_trace_back_signals();

    /* working */
    loop->loop();
    loop->cancel(timer);

    {
        std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
        auto iter = ctx.s_Thread2EventLoopMap.find(tid);
        if (iter != ctx.s_Thread2EventLoopMap.end()) {
            ctx.s_Thread2EventLoopMap.erase(iter);
        }
    }
    gLogT("thread %lu canceled!", tid);
    return nullptr;
}

void EventLoopManager::install_trace_back_signals()
{
    // std::set<int> tobeCaptured = { SIGILL, SIGABRT, SIGBUS, SIGFPE, SIGSEGV,
    //     SIGSYS, SIGSTKFLT, SIGXCPU, SIGXFSZ };
    // for (auto s : tobeCaptured) {
    //     signal(s, traceBack);
    // }
    signal(SIGPIPE, SIG_IGN);
}

void EventLoopManager::trace_back(int signo)
{
    static const int len = 256;
    void* buffer[len];
    int nptrs = ::backtrace(buffer, len);
    char** strings = ::backtrace_symbols(buffer, nptrs);

    if (!strings) {
        return;
    }

    std::stringstream ss;
    for (int i = 0; i < nptrs; ++i) {
        const std::string line(strings[i]); // ./test(_ZN6detail12c_do_nothingEfi+0x44) [0x401974]
        const std::string unmangle = demangle_name(line);
        ss << (unmangle.empty() ? line : unmangle) << std::endl;
    }
    free(strings);
    gLogF("exit by signal %d \n%s", signo, ss.str().c_str());
    _exit(signo);
}

std::string EventLoopManager::demangle_name(const std::string& mangled)
{
    std::string retval;
    int status = 0;
    static const size_t max_size = 2048;
    char temp[max_size];
    char unmangled[max_size];
    size_t buf_size = max_size;

    if (1 != sscanf(mangled.c_str(), "%*[^(]%*[^_]%127[^)+]", temp)) {
        return retval;
    }

    if ((nullptr != abi::__cxa_demangle(temp, unmangled, &buf_size, &status)) && (0 == status)) {
        retval.assign(unmangled, buf_size);
    }
    return retval;
}

size_t EventLoopManager::get_upper_limit(int type, size_t dft)
{
    struct rlimit rlim;
    if (0 != getrlimit(type, &rlim)) {
        return dft;
    }
    return std::min(rlim.rlim_cur, rlim.rlim_max);
}
