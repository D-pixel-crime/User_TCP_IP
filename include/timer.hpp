#pragma once
#include "syshead.hpp"
#include "intrusive_queue.hpp"
#include "utils.hpp"

enum class TimerType : uint8_t
{
    Trackable,
    Oneshot
};

class Timer
{
private:
    Timer(const uint32_t &_expires, void *(*_handler)(void *), void *_arg, TimerType _type);

public:
    list_head node;
    std::atomic<int> refcnt;
    uint32_t expires;
    int cancelled;
    void *(*handler)(void *);
    void *arg;
    std::mutex lock;

    static Timer *create(const uint32_t &_expires, void *(*_handler)(void *), void *_arg, TimerType _type);

    static size_t getOffset__list_node()
    {
        return offsetof(Timer, node);
    }

    void release();

    void cancel();
};

int timer_get_tick();

void timer_free(Timer *t);

void *timers_start();