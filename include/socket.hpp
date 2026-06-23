#pragma once
#include "sock.hpp"
#include "syshead.hpp"
#include "intrusive_queue.hpp"
#include "ipc.hpp"

inline void socket_dbg(const Socket *sock,
                       std::string_view msg,
                       const std::source_location loc = std::source_location::current())
{
  std::cout << std::format(
                   "Socket fd {} pid {} state {} sk_state {} flags {} poll {} sport {} dport {} "
                   "recv-q {} send-q {}: {} - {}:{}\n",
                   sock->fd,
                   sock->pid,
                   sock->state,
                   sock->sk->state,
                   sock->flags,
                   sock->sk->poll_events,
                   sock->sk->sport,
                   sock->sk->dport,
                   sock->sk->receive_queue.size(),
                   sock->sk->write_queue.size(),
                   msg,
                   loc.file_name(),
                   loc.line())
            << std::flush;
}

enum class Socket_State : uint8_t
{
  SS_FREE,
  SS_UNCONNECTED,
  SS_CONNECTING,
  SS_CONNECTED,
  SS_DISCONNECTING
};

class Sock_Ops
{
public:
  std::function<int(Socket *sock, const sockaddr *addr, int addr_len, int flags)> connect;
  std::function<int(Socket *sock, const void *buff, int len)> write;
  std::function<int(Socket *sock, void *buff, int len)> read;
  std::function<int(Socket *sock)> close;
  std::function<int(Socket *sock)> free;
  std::function<int(Socket *sock)> abort;
  std::function<int(Socket *sock)> poll;
  std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> getpeername;
  std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> getsockname;

  Sock_Ops(
      std::function<int(Socket *sock, const sockaddr *addr, int addr_len, int flags)> _connect,
      std::function<int(Socket *sock, const void *buff, int len)> _write,
      std::function<int(Socket *sock, void *buff, int len)> _read,
      std::function<int(Socket *sock)> _close,
      std::function<int(Socket *sock)> _free,
      std::function<int(Socket *sock)> _abort,
      std::function<int(Socket *sock)> _poll,
      std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> _getpeername,
      std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> _getsockname) : connect(_connect), write(_write), read(_read), close(_close), free(_free), abort(_abort), poll(_poll), getpeername(_getpeername), getsockname(_getsockname) {}
};

class Socket
{
public:
  list_head node;
  int fd;
  pid_t pid;
  std::atomic<int> refCnt;
  Socket_State state;
  short type;
  int flags;
  Sock *sk;
  Sock_Ops *ops;
  Wait_Lock sleep;
  std::shared_mutex lock;

  static size_t getOffset__list_node()
  {
    return offsetof(Socket, node);
  }

  Socket(pid_t _pid);
};

class Sock_Type
{
public:
  Sock_Ops *sock_ops;
  Net_Ops *net_ops;
  int type;
  int protocol;

  Sock_Type(Sock_Ops *_sock_ops, Net_Ops *_net_ops, const int &_type, const int &_protocol) : sock_ops(_sock_ops), net_ops(_net_ops), type(_type), protocol(_protocol) {}
};

class Net_Family
{
public:
  std::function<int(Socket *sock, int protocl)> create;

  Net_Family(std::function<int(Socket *sock, int protocl)> _create) : create(_create) {}
};

void socket_debug();

int socket_rd_acquire(Socket *sock);

int socket_wr_acquire(Socket *sock);

int socket_release(Socket *sock);

int socket_free(Socket *sock);

Socket *socket_find(Socket *query);

template <typename T>
void *socket_garbage_collect(T *arg);

int socket_delete(Socket *sock);

void abort_sockets();

Socket *get_socket(const pid_t &_pid, const uint32_t &_fd);

Socket *socket_lookup(const uint16_t &remote_port, const uint16_t &local_port);

int _socket(const pid_t &pid, const int &domain, const int &type, const int &protocol);

int _connect(const pid_t &pid, const int &sockfd, const sockaddr *addr, const socklen_t &addr_len);

int _write(const pid_t &pid, const int &sockfd, const void *buff, const unsigned int &count);

int _read(const pid_t &pid, const int &sockfd, void *buff, const unsigned int &count);

int _close(const pid_t &pid, const int &sockfd);

int _poll(const pid_t &pid, pollfd fds[], const nfds_t &nfds, int timeout);

template <typename... Args>
int _fcntl(const pid_t &pid, const int &fil_des, const int &cmd, Args... args);

int _getsockopt(const pid_t &pid, const int &fd, const int &level, const int &optname, void *optval, socklen_t *optlen);

int _getpeername(const pid_t &pid, const int &socket, sockaddr *__restrict_arr address, socklen_t *__restrict_arr address_len);

int _getsockname(const pid_t &pid, const int &socket, sockaddr *__restrict_arr address, socklen_t *__restrict_arr address_len);