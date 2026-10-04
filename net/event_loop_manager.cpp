/*
 * EventLoopManager.cc
 *
 *  Created on: 2020年8月5日
 *      Author: chewenyao
 */

#include "evix/net/event_loop_manager.h"

#include "evix/base/count_down_latch.h"
#include "evix/base/logging.h"
#include "evix/net/event_loop.h"

#include <memory>
#include <mutex>

#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

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

    // Only a main_loop=true construction may initialize the process-wide main
    // loop. Keeping the main_loop test outside call_once means an earlier
    // main_loop=false construction no longer consumes s_InitOnce and silently
    // prevents a later main_loop=true one from ever creating s_MainLoop.
    if (main_loop) {
        std::call_once(ctx.s_InitOnce, [&]() {
            // The process-wide main loop lives for the whole process and runs on
            // the thread that calls run(). Give it a no-op deleter so it is never
            // destroyed during C++ static teardown: ~EventLoop asserts it runs on
            // the loop's own thread, but static destructors run on whatever thread
            // happens to call exit(), which would abort the process. The OS
            // reclaims its fds and memory at exit, so leaking it here is safe.
            ctx.s_MainLoop = std::shared_ptr<xvorin::evix::net::EventLoop>(
                new xvorin::evix::net::EventLoop(), [](xvorin::evix::net::EventLoop* p) { p->quit(); });
            std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
            ctx.s_Thread2EventLoopMap[pthread_self()] = ctx.s_MainLoop;
        });
    }
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

    // Drop the name->loop entries so a concurrent get_event_loop(name) cannot
    // hand back a loop whose loop() we just asked to return.
    {
        std::lock_guard<std::mutex> guard(ctx.s_Name2EventLoopMapLock);
        ctx.s_Name2EventLoopMap.clear();
    }

    if (ctx.s_MainLoop) {
        ctx.s_MainLoop->quit();
    }
    ctx.s_MainLoopRunning = false;
}

int EventLoopManager::createCustomAttributes(const std::string& name, EvmThreadCreateType unique, int stack_size,
    int policy, int priority, std::vector<bool> affinity)
{
    if (name.empty()) {
        return -1;
    }

    stack_size *= 1024; /* stack_size KBytes */
    stack_size = std::max((int)PTHREAD_STACK_MIN, stack_size);
    // Clamp in the size_t domain: get_upper_limit() may return RLIM_INFINITY
    // (~0UL), and casting that to int yields a negative value that std::min
    // would wrongly select as the stack size.
    size_t stack_limit = get_upper_limit(RLIMIT_STACK, 8192 * 1024);
    if (stack_limit < static_cast<size_t>(stack_size)) {
        stack_size = static_cast<int>(stack_limit);
    }

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
    EXLT("createCustomAttributes for %s(%d,%d,%d,%d)", name.c_str(), (int)attr.unique, attr.stack_size, attr.policy,
        attr.priority);
    return 0;
}

std::shared_ptr<xvorin::evix::net::EventLoop> EventLoopManager::get_event_loop(const std::string& name)
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
            EXLW("Uses main loop for %s's request!", name.c_str());
            assert(ctx.s_MainLoop);
            retval = ctx.s_MainLoop;
        }
        EXLT("thread create done for %s!", name.c_str());
        return retval;
    }

    std::lock_guard<std::mutex> guard(ctx.s_Name2EventLoopMapLock);
    auto loopIter = ctx.s_Name2EventLoopMap.find(name);
    if (loopIter != ctx.s_Name2EventLoopMap.end()) {
        retval = loopIter->second.lock();
    }

    if (retval) {
        EXLT("unique thread had been created for %s!", name.c_str());
        return retval;
    }

    retval = create_event_loop(attr);
    if (!retval) {
        EXLW("Uses main loop for %s's unique request!", name.c_str());
        assert(ctx.s_MainLoop);
        retval = ctx.s_MainLoop;
    }
    ctx.s_Name2EventLoopMap[name] = retval;
    EXLT("unique thread create done for %s!", name.c_str());
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

std::shared_ptr<EventLoop> EventLoopManager::create_event_loop(const ThreadAttribute& attr)
{
    std::shared_ptr<EventLoop> retval = nullptr;
    CountDownLatch cdl(1);
    void* args[] = { &cdl, &retval };
    pthread_t tid;
    struct sched_param param = {};
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
    while (true) {
        int retcode = pthread_create(&tid, &pAttr, create_event_loop_detail, args);
        if (0 != retcode && retry-- > 0) {
            EXLW("pthread_create failure(%s), retry(%d/%d)!", strerror(retcode), ctx.PTHREAD_CREATE_RETRY_TIME - retry,
                ctx.PTHREAD_CREATE_RETRY_TIME);
            usleep(1 * 1000);
            continue;
        }

        pthread_attr_destroy(&pAttr);

        if (0 != retcode) {
            EXLE("pthread_create error, reason:%s!", strerror(retcode));
            return nullptr;
        }

        if (!attr.affinity.empty()) {
            cpu_set_t mask;
            CPU_ZERO(&mask);
            for (size_t i = 0; i < attr.affinity.size() && i < CPU_SETSIZE; i++) {
                if (attr.affinity[i]) {
                    CPU_SET(i, &mask);
                }
            }
            retcode = pthread_setaffinity_np(tid, sizeof(mask), &mask);
            if (0 != retcode) {
                EXLW("pthread_setaffinity_np error, reason:%s!", strerror(retcode));
            }
        }
        break;
    }

    /* waiting for new EventLoop */
    cdl.wait();
    assert(retval);

    int detach_ret = pthread_detach(tid);
    if (0 != detach_ret) {
        EXLW("pthread_detach(tid=%lu) error! reason:%s", tid, strerror(detach_ret));
    }
    EXLT("New thread(tid=%lu) created!", tid);
    return retval;
}

void* EventLoopManager::create_event_loop_detail(void* arg)
{
    pthread_t tid = pthread_self();
    CountDownLatch& cdl = *(reinterpret_cast<CountDownLatch*>(((char**)arg)[0]));
    std::shared_ptr<EventLoop>& retloop = *(reinterpret_cast<std::shared_ptr<EventLoop>*>(((char**)arg)[1]));
    // assert(cdl && retloop);

    // The EventLoop itself lives on this worker thread's stack, so it is both created
    // and destroyed here -- ~EventLoop()'s assert_in_loop_thread() always holds.
    EventLoop loop;
    // clang-format off
    retloop = std::shared_ptr<EventLoop>(&loop, [](EventLoop* p) { p->quit(); });
    std::weak_ptr<EventLoop> loop_guard = retloop;
    // clang-format on

    auto& ctx = get_context();
    {
        std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
        ctx.s_Thread2EventLoopMap[tid] = retloop;
    }

    /* notify main thread going */
    cdl.count_down();

    install_trace_back_signals();

    /* working */
    loop.loop();

    {
        std::lock_guard<std::mutex> guard(ctx.s_Thread2EventLoopMapLock);
        auto iter = ctx.s_Thread2EventLoopMap.find(tid);
        if (iter != ctx.s_Thread2EventLoopMap.end()) {
            ctx.s_Thread2EventLoopMap.erase(iter);
        }
    }

    while (!loop_guard.expired()) {
        usleep(1000);
    }

    EXLT("thread %lu exited!", tid);
    return nullptr;
}

void EventLoopManager::install_trace_back_signals()
{
    signal(SIGPIPE, SIG_IGN);
}

size_t EventLoopManager::get_upper_limit(int type, size_t dft)
{
    struct rlimit rlim = {};
    if (0 != getrlimit(type, &rlim)) {
        return dft;
    }
    return std::min(rlim.rlim_cur, rlim.rlim_max);
}
