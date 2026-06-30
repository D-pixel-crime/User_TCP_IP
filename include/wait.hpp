#include "system_headers.hpp" // IWYU pragma: keep

class Wait_Lock {
public:
  std::mutex lock;
  std::condition_variable ready;
  bool sleeping = 0;

  Wait_Lock() = default;

  int sleep() {
    std::unique_lock<std::mutex> _lock(lock);

    sleeping = 1;

    ready.wait(_lock, [this]() { return !sleeping; });

    return 0;
  }

  int wakeup() {
    std::unique_lock<std::mutex> _lock(lock);

    sleeping = 0;

    ready.notify_one();

    return 0;
  }
};
