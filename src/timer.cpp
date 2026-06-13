#include "../include/timer.hpp"

std::shared_mutex rw_mutex;
std::mutex norm_mutex;
IntrusiveQueue<Timer> timers_queue(Timer::getOffset__list_node());
int tick = 0;

int timer_get_tick()
{
    std::shared_lock<std::shared_mutex> rwlock(rw_mutex);

    return tick;
}

Timer::Timer(const uint32_t &_expires, std::function<void()> _handler, TimerType _type) : refcnt{_type == TimerType::Oneshot ? 0 : 1}, cancelled{0}, handler{_handler}
{
    int tick = timer_get_tick();
    expires = tick + _expires;

    if (expires < tick)
    {
        print_err("ERR(Timer): Timer expiry integer wrap-around/overflow - {}.", expires);
    }
}

Timer *Timer::create(const uint32_t &_expires, std::function<void()> _handler, TimerType _type)
{
    Timer *t = new Timer(_expires, _handler, _type);

    {
        std::lock_guard<std::mutex> lock(norm_mutex);
        timers_queue.queue_add_tail(&t->node);
    }
}

void Timer::release()
{
    std::lock_guard<std::mutex> gaurd(lock);

    if (refcnt > 0)
    {
        refcnt--;
    }
}

void Timer::cancel()
{
    std::lock_guard<std::mutex> gaurd(lock);

    if (refcnt > 0)
    {
        refcnt--;
    }

    cancelled = 1;
}

void timer_free(Timer *t)
{
    delete t;
}

void timers_tick()
{
    std::lock_guard<std::mutex> lock(norm_mutex);

    list_for_each_safe(timers_queue.get_queue_list_head(), [&](list_head *pos)
                       {
        if(!pos){
            return false;
        }

        Timer *t = list_entry<Timer>(pos, Timer::getOffset__list_node());
        
        {
            std::lock_guard<std::mutex> timerLock(t->lock);

        if(t->cancelled && t->expires < tick){
            t->cancelled = 1;
            /*To be implemented
                pthread_t th;
            pthread_create(&th, NULL, t->handler, t->arg);
            */
        }

        if(t->cancelled && t->refcnt == 0){
            timers_queue.queue_del(&t->node);
            timer_free(t);
        }
    } });
}

void *timers_start()
{
    while (true)
    {
        /* To be implemented
        if (usleep(10000) != 0) {
            perror("Timer usleep");
        }
        */

        {
            std::shared_lock<std::shared_mutex> lock(rw_mutex);
            tick += 10;
        }
        timers_tick();

        if (tick % 5000 == 0)
        {
            /*To be implemented
                socket_debug();
                timer_debug();
            */
        }
    }
}