#include "../include/inet.hpp"
#include "../include/socket.hpp"
#include "../include/tcp.hpp"

extern Net_Ops tcp_ops;
inline static int INET_OPS = 1;

static Net_Family inet(
    [](Socket *sock, int protocol) -> int
    {
        return inet_create(sock, protocol);
    });

static Sock_Ops inet_stream_ops(
    [](Socket *sock, const sockaddr *addr, int addrlen, int flags) -> int
    {
        return inet_stream_connect(sock, addr, addrlen, flags);
    },
    [](Socket *sock, const void *buff, int len) -> int
    {
        return inet_write(sock, buff, len);
    },
    [](Socket *sock, void *buff, int len) -> int
    {
        return inet_read(sock, buff, len);
    },
    [](Socket *sock) -> int
    {
        return inet_close(sock);
    },
    [](Socket *sock) -> int
    {
        return inet_free(sock);
    },
    [](Socket *sock) -> int
    {
        return inet_abort(sock);
    },
    NULL,
    [](Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addrlen) -> int
    {
        return inet_getpeername(sock, addr, addrlen);
    },
    [](Socket *sock, sockaddr *__restrict_arr addr, socklen_t *__restrict_arr addrlen) -> int
    {
        return inet_getsockname(sock, addr, addrlen);
    });

static Sock_Type inet_ops[] = {
    Sock_Type(&inet_stream_ops, &tcp_ops, SOCK_STREAM, IPPROTO_TCP)};

int inet_create(Socket *sock, const int &protocol)
{
    Sock_Type *skt = nullptr;

    for (int i = 0; i < INET_OPS; i++)
    {
        if (inet_ops[i].type & sock->type)
        {
            skt = &inet_ops[i];
            break;
        }
    }

    if (!skt)
    {
        print_err("Could not find socktype for socket\n");
        return 1;
    }

    sock->ops = skt->sock_ops;

    Sock *sk = sk_alloc(skt->net_ops, protocol);
    sk->protocol = protocol;

    sock_init_data(sock, sk);

    return 0;
}

int inet_socket(Socket *sock, const int &protocol)
{
    return 0;
}

int inet_stream_connect(Socket *sock, const sockaddr *addr, int addrlen, int flags)
{
    Sock *sk = sock->sk;

    if (addrlen < sizeof(addr->sa_family))
    {
        return -EINVAL;
    }

    if (addr->sa_family == AF_UNSPEC)
    {
        sk->ops->disconnect(sk, flags);
        return -EAFNOSUPPORT;
    }

    switch (sock->state)
    {
    default:
        sk->err = -EINVAL;
        return sk->err;
    case Socket_State::SS_CONNECTED:
        sk->err = -EISCONN;
        return sk->err;
    case Socket_State::SS_CONNECTING:
        sk->err = -EALREADY;
        return sk->err;
    case Socket_State::SS_UNCONNECTED:
        sk->err = -EISCONN;
        if (sk->state != (int)Tcp_State::TCP_CLOSE)
        {
            return sk->err;
        }

        sk->ops->connect(sk, addr, addrlen, flags);
        sock->state = Socket_State::SS_CONNECTING;
        sk->err = -EINPROGRESS;

        if (sock->flags & O_NONBLOCK)
        {
            return sk->err;
        }

        {
            std::unique_lock<std::shared_mutex> wr_lock(sock->lock);
            while (sock->state == Socket_State::SS_CONNECTING && sk->err == -EINPROGRESS)
            {
                socket_release(sock);
                /*To be implemented:
                    wait_sleep(&sock->sleep);
                */
                socket_wr_acquire(sock);
            }
        }
        socket_wr_acquire(sock);

        switch (sk->err)
        {
        case -ETIMEDOUT:
        case -ECONNREFUSED:
            return sk->err;
        }

        if (sk->err != 0)
        {
            return sk->err;
        }

        sock->state = Socket_State::SS_CONNECTED;
        break;
    }

    return sk->err;
}

int inet_write(Socket *sock, const void *buff, int len)
{
    Sock *sk = sock->sk;
    return sk->ops->write(sk, buff, len);
}

int inet_read(Socket *sock, void *buff, int len)
{
    Sock *sk = sock->sk;
    return sk->ops->read(sk, buff, len);
}

int inet_close(Socket *sock)
{
    if (!sock)
    {
        return 0;
    }

    Sock *sk = sock->sk;
    return sock->sk->ops->close(sk);
}

int inet_free(Socket *sock)
{
    Sock *sk = sock->sk;
    sock_free(sk);
    free(sock->sk);

    return 0;
}

int inet_abort(Socket *sock)
{
    Sock *sk = sock->sk;
    if (sk)
    {
        sk->ops->abort(sk);
    }

    return 0;
}

int inet_getpeername(Socket *sock, sockaddr *__restrict_arr address, socklen_t *__restrict_arr address_len)
{
    Sock *sk = sock->sk;
    if (!sk)
    {
        return -1;
    }

    sockaddr_in *res = reinterpret_cast<sockaddr_in *>(address);
    res->sin_family = AF_INET;
    res->sin_port = htons(sk->dport);
    res->sin_addr.s_addr = htonl(sk->daddr);
    *address_len = sizeof(sockaddr_in);

    /*To be implemented:
        inet_dbg(sock, "geetpeername sin_family %d sin_port %d sin_addr %d addrlen %d",
             res->sin_family, ntohs(res->sin_port), ntohl(res->sin_addr.s_addr), *address_len);
    */

    return 0;
}

int inet_getsockname(Socket *sock, sockaddr *__restrict_arr address, socklen_t *__restrict_arr address_len)
{
    Sock *sk = sock->sk;
    if (!sk)
    {
        return -1;
    }

    sockaddr_in *res = reinterpret_cast<sockaddr_in *>(address);
    res->sin_family = AF_INET;
    res->sin_port = htons(sk->sport);
    res->sin_addr.s_addr = htonl(sk->saddr);
    *address_len = sizeof(sockaddr_in);

    /*To be implemented:
        inet_dbg(sock, "getsockname sin_family %d sin_port %d sin_addr %d addrlen %d",
             res->sin_family, ntohs(res->sin_port), ntohl(res->sin_addr.s_addr), *address_len);
    */

    return 0;
}

Sock *inet_lookup(SkBuff *skb, const uint16_t &sport, const uint16_t &dport)
{
    Socket *sock = socket_lookup(sport, dport);
    if (!sock)
    {
        return nullptr;
    }

    return sock->sk;
}