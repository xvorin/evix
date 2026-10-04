#pragma once

/*
 * EventLoopManager.h
 *
 *  Created on: 2020年8月5日
 *      Author: chewenyao
 */

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace xvorin::evix::net {

class EventLoop;

enum EvmThreadCreateType {
    EvmUnique,
    EvmMulti,
};

class EventLoopManager {
public:
    struct ThreadAttribute {
        std::string name;
        EvmThreadCreateType unique;
        int policy;
        int priority;
        int stack_size;
        std::vector<bool> affinity;
    };

public:
    EventLoopManager(bool main_loop = true);

    virtual ~EventLoopManager();

    void run();
    void quit();

    int createCustomAttributes(const std::string& name, EvmThreadCreateType unique, int stack_size = 256,
        int policy = SCHED_OTHER, int priority = 0, std::vector<bool> affinity = {});

    std::shared_ptr<xvorin::evix::net::EventLoop> get_event_loop(const std::string& name = "");
    std::shared_ptr<xvorin::evix::net::EventLoop> get_current_event_loop();

private:
    EventLoopManager(const EventLoopManager&) = delete;
    void operator=(const EventLoopManager&) = delete;

private:
    std::shared_ptr<xvorin::evix::net::EventLoop> create_event_loop(const ThreadAttribute& attr);
    static void* create_event_loop_detail(void* arg);
    static void install_trace_back_signals();
    inline size_t get_upper_limit(int type, size_t dft);
};

} // namespace xvorin::evix::net
