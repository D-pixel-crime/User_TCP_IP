#pragma once
#include "sock.hpp"
#include "syshead.hpp"

class Socket
{
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