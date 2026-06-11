#pragma once
#include "syshead.hpp"
#include "intrusive_queue.hpp"
#include "utils.hpp"

class Timer
{
public:
    list_head node;
    std::atomic<int> refcnt;
    uint32_t expires;
    int cancelled;

    void *(*handler)(void *);
    void *arg;

    std::mutex lock;

    Timer(const uint32_t &expire, void *(*handler)(void *), void *arg);

    static size_t getOffset__list_node()
    {
        return offsetof(Timer, node);
    }
};
