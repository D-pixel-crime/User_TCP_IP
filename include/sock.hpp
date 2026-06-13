#pragma once
#include "socket.hpp"
#include "socket_buffer.hpp"
#include "syshead.hpp"

class Sock;

class Net_Ops
{
public:
    /*To be implemented:
    std::function<Sock*(int protocol)> alloc_sock;
    std::function<int(Sock* sk)> init;
    */
    std::function<int(Sock *sk, const sockaddr *addr, int addr_len, int flags)> connect;
    std::function<int(Sock *sk, int flags)> disconnect;
    std::function<int(Sock *sk, const void *buff, int len)> write;
    std::function<int(Sock *sk, void *buff, int len)> read;
    std::function<int(Sock *sk)> recv_notify;
    std::function<int(Sock *sk)> abort;
};

class Sock
{
public:
    Socket *sock;
    Net_Ops *ops;
    /*To be implemented
        wait_lock recv_wait;
    */
    IntrusiveQueue<SkBuff> receive_queue;
    IntrusiveQueue<SkBuff> write_queue;
    int protocol;
    int state;
    int err;
    short int poll_events;
    uint16_t sport;
    uint16_t dport;
    uint32_t saddr;
    uint32_t daddr;
};

SkBuff *write_queue_head(Sock *sk)
{
    return (sk->write_queue).queue_first_entry();
}
