#pragma once
#include "sock.hpp"
#include "syshead.hpp"
#include "intrusive_queue.hpp"

enum class Socket_State : uint8_t
{
    SS_FREE,
    SS_UNCONNECTED,
    SS_CONNECTING,
    SS_CONNECTED,
    SS_DISCONNECTING
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

class Sock_Ops
{
public:
    std::function<int(Socket *sock, const sockaddr *addr)> connect;
    std::function<int(Socket *sock, const void *buff, int len)> write;
    std::function<int(Socket *sock, void *buff, int len)> read;
    std::function<int(Socket *sock)> close;
    std::function<int(Socket *sock)> free;
    std::function<int(Socket *sock)> abort;
    std::function<int(Socket *sock)> poll;
    std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> getpeername;
    std::function<int(Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addr_len)> getsockname;
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

template <typename T>
void *socket_garbage_collect(T *arg);

int socket_delete(Socket *sock);

void abort_sockets();

Socket *get_socket(const pid_t &_pid, const uint32_t &_fd);