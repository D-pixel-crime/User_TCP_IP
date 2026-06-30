#pragma once
#include "intrusive_queue.hpp"
#include "system_headers.hpp" // IWYU pragma: keep

enum class Timer_Type : uint8_t { Trackable, Oneshot };

class Timer {
private:
  Timer(const uint32_t &_expires, std::function<void()> _handler,
        Timer_Type _type);

public:
  list_head node;
  std::atomic<int> refcnt;
  uint32_t expires;
  int cancelled;
  std::function<void()> handler;
  std::mutex lock;

  static Timer *create(const uint32_t &_expires, std::function<void()> _handler,
                       Timer_Type _type);

  static size_t getOffset__list_node() { return offsetof(Timer, node); }

  void release();

  void cancel();
};

void timer_debug();

int timer_get_tick();

void timer_free(Timer *t);

void *timers_start();