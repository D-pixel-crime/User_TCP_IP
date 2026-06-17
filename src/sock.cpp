#include "../include/sock.hpp"

Net_Ops::Net_Ops(std::function<Sock *(int protocol)> _allock_sock, std::function<int(Sock *sk)> _init,
                 std::function<int(Sock *sk, const sockaddr *addr, int addr_len, int flags)> _connect,
                 std::function<int(Sock *sk, int flags)> _disconnect,
                 std::function<int(Sock *sk, const void *buff, int len)> _write,
                 std::function<int(Sock *sk, void *buff, int len)> _read,
                 std::function<int(Sock *sk)> _recv_notify,
                 std::function<int(Sock *sk)> _close,
                 std::function<int(Sock *sk)> _abort) : alloc_sock{_allock_sock},
                                                        init{_init},
                                                        connect{_connect},
                                                        disconnect{_disconnect},
                                                        write{_write},
                                                        read{_read},
                                                        recv_notify{_recv_notify},
                                                        close{_close},
                                                        abort{_abort}
{
}

Sock *sk_alloc(Net_Ops *ops, const int &protocol)
{
    Sock *sk = ops->alloc_sock(protocol);
    sk->ops = ops;
    return sk;
}

void sock_init_data(Socket *sock, Sock *sk)
{
    sock->sk = sk;
    sk->sock = sock;

    /*To be implemented
        wait_init(&sk->recv_wait);
    */
    sk->poll_events = 0;
    sk->ops->init(sk);
}

void sock_free(Sock *sk)
{
    skb_queue_free(&sk->receive_queue);
    skb_queue_free(&sk->write_queue);
}

void sock_connected(Sock *sk)
{
    Socket *sock = sk->sock;

    sock->state = Socket_State::SS_CONNECTED;
    sk->err = 0;
    sk->poll_events = (POLLOUT | POLLWRNORM | POLLWRBAND);

    /*To be implemented:
        wait_wakeup(&sock->sleep);
    */
}