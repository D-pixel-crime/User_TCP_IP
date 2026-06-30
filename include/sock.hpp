#pragma once
#include "socket_buffer.hpp"
#include "system_headers.hpp" // IWYU pragma: keep
#include "wait.hpp"

class Sock;
class Socket;

class Net_Ops {
public:
  std::function<Sock *(int protocol)> alloc_sock;
  std::function<int(Sock *sk)> init;
  std::function<int(Sock *sk, const sockaddr *addr, int addr_len, int flags)>
      connect;
  std::function<int(Sock *sk, int flags)> disconnect;
  std::function<int(Sock *sk, const void *buff, int len)> write;
  std::function<int(Sock *sk, void *buff, int len)> read;
  std::function<int(Sock *sk)> recv_notify;
  std::function<int(Sock *sk)> close;
  std::function<int(Sock *sk)> abort;

  Net_Ops() = default;

  Net_Ops(std::function<Sock *(int protocol)> _allock_sock,
          std::function<int(Sock *sk)> _init,
          std::function<int(Sock *sk, const sockaddr *addr, int addr_len,
                            int flags)>
              _connect,
          std::function<int(Sock *sk, int flags)> _disconnect,
          std::function<int(Sock *sk, const void *buff, int len)> _write,
          std::function<int(Sock *sk, void *buff, int len)> _read,
          std::function<int(Sock *sk)> _recv_notify,
          std::function<int(Sock *sk)> _close,
          std::function<int(Sock *sk)> _abort);
};

class Sock {
public:
  Socket *sock;
  Net_Ops *ops;
  Wait_Lock recv_wait;
  IntrusiveQueue<SkBuff> receive_queue =
      IntrusiveQueue<SkBuff>(SkBuff::getOffset__list_node());
  IntrusiveQueue<SkBuff> write_queue =
      IntrusiveQueue<SkBuff>(SkBuff::getOffset__list_node());
  int protocol;
  int state;
  int err;
  short int poll_events;
  uint16_t sport;
  uint16_t dport;
  uint32_t saddr;
  uint32_t daddr;

  Sock() = default;

  Sock(const int &_protocol) : protocol{_protocol} {}
};

inline SkBuff *write_queue_head(Sock *sk) {
  return (sk->write_queue).queue_first_entry();
}

Sock *sk_alloc(Net_Ops *ops, const int &protocol);

void sock_init_data(Socket *sock, Sock *sk);

void sock_free(Sock *sk);

void sock_connected(Sock *sk);