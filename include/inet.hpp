#pragma once
#include "sock.hpp"
#include "socket.hpp"
#include "syshead.hpp"

int inet_create(Socket *sock, const int &protocol);

int inet_socket(Socket *sock, const int &protocol);

int inet_stream_connect(Socket *sock, const sockaddr *addr, int addr_len, int flags);

int inet_write(Socket *sock, const void *buff, int len);

int inet_read(Socket *sock, void *buff, int len);

int inet_close(Socket *sock);

int inet_free(Socket *sock);

int inet_abort(Socket *sock);

int inet_getpeername(Socket *sock, sockaddr *__restrict_arr address, socklen_t *__restrict_arr address_len);

int inet_getsockname(Socket *sock, sockaddr *__restrict_arr address, socklen_t *__restrict_arr address_len);

Sock *inet_lookup(SkBuff *skb, const uint16_t &sport, const uint16_t &dport);