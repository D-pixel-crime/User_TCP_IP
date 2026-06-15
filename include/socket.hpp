#pragma once
#include "sock.hpp"
#include "syshead.hpp"
#include "intrusive_queue.hpp"

inline void socket_dbg(
    const Socket *sock,
    std::string_view msg,
    auto &&...args,
    const std::source_location loc = std::source_location::current())
{
    std::string custom_msg = std::vformat(msg, std::make_format_args(args...));

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
                     sock->sk->receive_queue.qlen,
                     sock->sk->write_queue.qlen,
                     custom_msg,
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
    std::function<int(Socket *sock, const sockaddr *addr, int addrlen, int flags)> connect;
    std::function<int(Socket *sock, const void *buff, int len)> write;
    std::function<int(Socket *sock, void *buff, int len)> read;
    std::function<int(Socket *sock)> close;
    std::function<int(Socket *sock)> free;
    std::function<int(Socket *sock)> abort;
    std::function<int(Socket *sock)> poll;
    std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> getpeername;
    std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> getsockname;
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
    /*To be implemented
        wait_lock sleep;
    */
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
};

class Net_Family
{
public:
    std::function<int(Socket *sock, int protocl)> create;
};

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

int _connect(const pid_t &pid, const int &sockfd, const sockaddr *addr, const socklen_t &addrlen);

int _write(const pid_t &pid, const int &sockfd, const void *buff, const unsigned int &count);

int _read(const pid_t &pid, const int &sockfd, void *buff, const unsigned int &count);

int _close(const pid_t &pid, const int &sockfd);