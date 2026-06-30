#include "../include/timer.hpp"
#include "../include/socket.hpp"
#include "../include/utils.hpp"

std::shared_mutex rw_mutex;
std::mutex norm_mutex;
IntrusiveQueue<Timer> timers_queue(Timer::getOffset__list_node());
int tick = 0;

void timer_debug() {
  int cnt = 0;
  {
    std::unique_lock<std::mutex> lock(norm_mutex);

    list_for_each(timers_queue.get_queue_list_head(), [&](list_head * /*pos*/) {
      cnt++;
      return false;
    });
  }

  print_debug(std::format("TIMERS: Total amount currently - {}.", cnt));
}

int timer_get_tick() {
  std::shared_lock<std::shared_mutex> rd_lock(rw_mutex);

  return tick;
}

Timer::Timer(const uint32_t &_expires, std::function<void()> _handler,
             Timer_Type _type)
    : refcnt{_type == Timer_Type::Oneshot ? 0 : 1}, cancelled{0},
      handler{_handler} {
  int tick = timer_get_tick();
  expires = tick + _expires;

  if (static_cast<int>(expires) < tick) {
    print_err("ERR(Timer): Timer expiry integer wrap-around/overflow - {}.",
              expires);
  }
}

Timer *Timer::create(const uint32_t &_expires, std::function<void()> _handler,
                     Timer_Type _type) {
  Timer *t = new Timer(_expires, _handler, _type);

  {
    std::unique_lock<std::mutex> lock(norm_mutex);
    timers_queue.queue_add_tail(&t->node);
  }

  return t;
}

void Timer::release() {
  std::unique_lock<std::mutex> gaurd(lock);

  if (refcnt > 0) {
    refcnt--;
  }
}

void Timer::cancel() {
  std::unique_lock<std::mutex> gaurd(lock);

  if (refcnt > 0) {
    refcnt--;
  }

  cancelled = 1;
}

void timer_free(Timer *t) { delete t; }

void timers_tick() {
  std::unique_lock<std::mutex> lock(norm_mutex);

  list_for_each_safe(timers_queue.get_queue_list_head(), [&](list_head *pos) {
    if (!pos) {
      return false;
    }

    Timer *t = list_entry<Timer>(pos, Timer::getOffset__list_node());

    {
      std::unique_lock<std::mutex> timerLock(t->lock);

      if (t->cancelled && static_cast<int>(t->expires) < tick) {
        t->cancelled = 1;
        std::thread th(t->handler);
        th.detach();
      }

      if (t->cancelled && t->refcnt == 0) {
        timers_queue.queue_del(&t->node);
        timer_free(t);
      }
    }

    return false;
  });
}

void *timers_start() {
  while (true) {
    try {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
    }

    {
      std::unique_lock<std::shared_mutex> wr_lock(rw_mutex);
      tick += 10;
    }
    timers_tick();

    if (tick % 5000 == 0) {
      socket_debug();
      timer_debug();
    }
  }
}