#include "../include/timer.hpp"

std::shared_mutex rw_mutex;
std::mutex norm_mutex;
int tick = 0;
IntrusiveQueue<Timer> timers(Timer::getOffset__list_node());

int timer_get_tick()
{
    std::shared_lock<std::shared_mutex> rwlock(rw_mutex);

    return tick;
}

Timer::Timer(const uint32_t &_expires, void *(*_handler)(void *), void *_arg) : refcnt{1}, cancelled{0}, handler{_handler}, arg{_arg}
{
    int tick = timer_get_tick();
    expires = tick + _expires;

    if (expires < tick)
    {
        print_err("ERR(Timer): Timer expiry integer wrap-around/overflow - {}.", expires);
    }

    std::lock_guard<std::mutex> lock(norm_mutex);
    timers.queue_add_tail(&node);
}