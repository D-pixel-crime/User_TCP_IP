#include "../include/tcp.hpp"
#include "../include/sock.hpp"
#include "../include/socket.hpp"
#include "../include/timer.hpp"

static std::random_device rd;
static std::mt19937 gen(rd());

std::shared_mutex tcp_shared_mutex;

Tcp_Sock::Tcp_Sock() : sackok{1}, rmss{1460}, smss{536}, ofo_queue(IntrusiveQueue<SkBuff>(SkBuff::getOffset__list_node()))
{
    sk.state = (int)Tcp_State::TCP_CLOSE;
}

Sock *tcp_alloc_sock()
{
    return reinterpret_cast<Sock *>(new Tcp_Sock());
}

int tcp_v4_init_sock(Sock *sk)
{
    tcp_init_sock(sk);
    return 0;
}

int tcp_init_sock(Sock *sk)
{
    return 0;
}

void tcp_set_state(Sock *sk, const uint32_t &state)
{
    sk->state = state;
}

uint16_t generate_port()
{
    int temp_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_fd < 0)
    {
        throw std::runtime_error("Failed to create temporary socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(0);

    if (::bind(temp_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        ::close(temp_fd);
        throw std::runtime_error("Failed to bind temporary socket");
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(temp_fd, reinterpret_cast<struct sockaddr *>(&addr), &len) < 0)
    {
        ::close(temp_fd);
        throw std::runtime_error("Failed to retrieve assigned port");
    }

    ::close(temp_fd);

    return ntohs(addr.sin_port);
}

int generate_iss()
{
    std::uniform_int_distribution<int> distrib(0, std::numeric_limits<int>::max());

    return distrib(gen);
}

int tcp_v4_connect(Sock *sk, const sockaddr *addr, const int &addrlen, const int &flags)
{
    uint16_t dport = (reinterpret_cast<const sockaddr_in *>(addr))->sin_port;
    uint32_t daddr = (reinterpret_cast<const sockaddr_in *>(addr))->sin_addr.s_addr;

    sk->dport = dport;
    sk->sport = generate_port();
    sk->daddr = daddr;

    return tcp_connect(sk);
}

int tcp_disconnect(Sock *sk, const int &flags)
{
    return 0;
}

int tcp_write(Sock *sk, const void *buff, const int &len)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    int ret = sk->err;

    if (ret)
    {
        return ret;
    }

    switch (sk->state)
    {
    case (int)Tcp_State::TCP_ESTABLISHED:
    case (int)Tcp_State::TCP_CLOSE_WAIT:
        break;

    default:
        ret = -EBADF;
        return ret;
    }

    return tcp_send(tsk, buff, len);
}

int tcp_read(Sock *sk, const void *buff, const int &len)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    int ret = -1;

    switch ((Tcp_State)sk->state)
    {
    case Tcp_State::TCP_CLOSE:
        return -EBADF;

    case Tcp_State::TCP_LISTEN:
    case Tcp_State::TCP_SYN_SENT:
    case Tcp_State::TCP_SYN_RECEIVED:
        /*
            Queue for requests after entering ESTABLISHED state. If full, return "error: insufficient resources".
        */
    case Tcp_State::TCP_ESTABLISHED:
    case Tcp_State::TCP_FIN_WAIT_1:
    case Tcp_State::TCP_FIN_WAIT_2:
        /*
            Queue request if incoming segments are insufficient.
        */
        break;

    case Tcp_State::TCP_CLOSE_WAIT:
        /*
            If no text is awaiting delivery, return "error: connection closing". Otherwise, satisfy the RECEIVE with any remaining text.
        */
        if (tsk->sk.receive_queue.isEmpty())
        {
            break;
        }
        if (tsk->flags & TCP_FIN)
        {
            tsk->flags &= ~TCP_FIN;
            return 0;
        }
        break;

    case Tcp_State::TCP_CLOSING:
    case Tcp_State::TCP_LAST_ACK:
    case Tcp_State::TCP_TIME_WAIT:
        return sk->err;

    default:
        break;
    }

    return -1;
}

int tcp_recv_notify(Sock *sk)
{
    /* To be implemented
    if (&(sk->recv_wait)) {
        return wait_wakeup(&sk->recv_wait);
    }
    */

    return -1;
}

int tcp_close(Sock *sk)
{
    switch ((Tcp_State)sk->state)
    {
    case Tcp_State::TCP_CLOSE:
    case Tcp_State::TCP_CLOSING:
    case Tcp_State::TCP_LAST_ACK:
    case Tcp_State::TCP_TIME_WAIT:
    case Tcp_State::TCP_FIN_WAIT_1:
    case Tcp_State::TCP_FIN_WAIT_2:
        sk->err = -EBADF;
        return -1;

    case Tcp_State::TCP_LISTEN:
    case Tcp_State::TCP_SYN_SENT:
    case Tcp_State::TCP_SYN_RECEIVED:
        return tcp_done(sk);

    case Tcp_State::TCP_ESTABLISHED:
        tcp_set_state(sk, (int)Tcp_State::TCP_FIN_WAIT_1);
        tcp_queue_fin(sk);
        break;

    case Tcp_State::TCP_CLOSE_WAIT:
        tcp_queue_fin(sk);
        break;

    default:
        print_err("ERR(tcp_close): Unsupported Tcp_State-{} for close.", sk->state);
        return -1;
    }

    return 0;
}

int tcp_abort(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);

    tcp_send_reset(tsk);

    return tcp_done(sk);
}

int tcp_free(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    tcp_clear_timers(sk);
    tcp_clear_queues(tsk);
    /*To be implemented:
        wait_wakeup(&sk->sock->sleep);
    */

    return 0;
}

int tcp_done(Sock *sk)
{

    tcp_set_state(sk, (int)Tcp_State::TCP_CLOSING);
    tcp_free(sk);
    return socket_delete(sk->sock);
}

void tcp_stop_rto_timer(Tcp_Sock *tsk)
{
    if (!tsk)
    {
        return;
    }

    tsk->retransmit->cancel();
    tsk->retransmit = nullptr;
    tsk->backoff = 0;
}

void tcp_release_rto_timer(Tcp_Sock *tsk)
{
    if (!tsk)
    {
        return;
    }

    tsk->retransmit->release();
    tsk->retransmit = nullptr;
}

void tcp_stop_delack_timer(Tcp_Sock *tsk)
{
    if (!tsk)
    {
        return;
    }

    tsk->delack->cancel();
    tsk->delack = nullptr;
}

void tcp_release_delack_timer(Tcp_Sock *tsk)
{
    if (!tsk)
    {
        return;
    }

    tsk->delack->release();
    tsk->delack = nullptr;
}

void tcp_clear_timers(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    tcp_stop_rto_timer(tsk);
    tcp_release_rto_timer(tsk);

    tsk->keepalive->cancel();
    tsk->keepalive = nullptr;
    tsk->linger->cancel();
    tsk->linger = nullptr;
}

void tcp_handle_fin_state(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);

    switch ((Tcp_State)sk->state)
    {
    case Tcp_State::TCP_CLOSE_WAIT:

        tcp_set_state(sk, (int)Tcp_State::TCP_LAST_ACK);
        break;

    case Tcp_State::TCP_ESTABLISHED:

        tcp_set_state(sk, (int)Tcp_State::TCP_FIN_WAIT_1);
        break;
    }
}

void *tcp_linger(void *arg)
{
    Sock *sk = reinterpret_cast<Sock *>(arg);

    {
        socket_wr_acquire(sk->sock);

        Tcp_Sock *tsk = tcp_sk(sk);
        /*To be implemented:
            tcpsock_dbg("TCP time-wait timeout, freeing TCB", sk);
        */
        tsk->linger->cancel();
        tsk->linger = nullptr;
        tcp_done(sk);

        socket_release(sk->sock);
    }

    return nullptr;
}

void *tcp_user_timeout(void *arg)
{
    Sock *sk = reinterpret_cast<Sock *>(arg);

    {
        socket_wr_acquire(sk->sock);

        Tcp_Sock *tsk = tcp_sk(sk);
        /*To be implemented:
            tcpsock_dbg("TCP user timeout, freeing TCB and aborting conn", sk);
        */
        tsk->linger->cancel();
        tsk->linger = nullptr;
        tcp_abort(sk);

        socket_release(sk->sock);
    }

    return nullptr;
}

void tcp_enter_time_wait(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);

    tcp_set_state(sk, (int)Tcp_State::TCP_TIME_WAIT);

    tcp_clear_timers(sk);
    tsk->linger = Timer::create(TCP_2MSL, [sk]()
                                { tcp_linger(sk); }, Timer_Type::Trackable);
}

void tcp_rearm_user_timeout(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    if (sk->state == (int)Tcp_State::TCP_TIME_WAIT)
    {
        return;
    }

    tsk->linger->cancel();
    tsk->linger = Timer::create(TCP_USER_TIMEOUT, [sk]()
                                { tcp_user_timeout(sk); }, Timer_Type::Trackable);
}

void tcp_rtt(Tcp_Sock *tsk)
{
    if (tsk->backoff > 0 || !tsk->retransmit)
    {
        // Karn's Algorithm, do not measure retransmissions
        return;
    }

    int r = timer_get_tick() - (tsk->retransmit->expires - tsk->rto);
    if (r < 0)
    {
        return;
    }

    if (!tsk->srtt)
    {
        /* RFC6298 2.2 first measurement */
        tsk->srtt = r;
        tsk->rttvar = r / 2;
    }
    else
    {
        /* RFC6298 2.3 a subsequent measurement */
        double beta = 0.25, alpha = 0.125;
        tsk->rttvar = (1 - beta) * tsk->rttvar + beta * abs(tsk->srtt - r);
        tsk->srtt = (1 - alpha) * tsk->srtt + alpha * r;
    }

    int k = 4 * tsk->rttvar;

    /* As per RFC6298,RTO = min. 1 second. Linux uses 200ms */
    k = std::max(k, 200);

    tsk->rto = tsk->srtt + k;
}

int tcp_calculate_sacks(Tcp_Sock *tsk)
{
    Tcp_Sack_Block *sb = &tsk->sacks[tsk->sacklen];
    sb->left = sb->right = 0;

    list_for_each_safe(tsk->ofo_queue.get_queue_list_head(), [&](list_head *pos)
                       {
        SkBuff* next = list_entry<SkBuff>(pos, SkBuff::getOffset__list_node());
        
        if(!sb->left){
            sb->left = next->seq;
            tsk->sacklen++;
        }

        if(!sb->right || sb->right == next->seq){
            sb->right == next->end_seq;
        }else{
            if(tsk->sacklen >= tsk->sacks_allowed){
                return true;
            }

            sb = &tsk->sacks[tsk->sacklen];
            sb->left = next->seq;
            sb->right = next->end_seq;
            tsk->sacklen++;
        }

        return false; });

    return 0;
}

static Net_Ops tcp_ops(
    [](int protocol) -> Sock *
    {
        return tcp_alloc_sock();
    },
    [](Sock *sk) -> int
    {
        return tcp_v4_init_sock(sk);
    },
    [](Sock *sk, const sockaddr *addr, int addr_len, int flags) -> int
    {
        return tcp_v4_connect(sk, addr, addr_len, flags);
    },
    [](Sock *sk, int flags) -> int
    {
        return tcp_disconnect(sk, flags);
    },
    [](Sock *sk, const void *buff, int len) -> int
    {
        return tcp_write(sk, buff, len);
    },
    [](Sock *sk, void *buff, int len) -> int
    {
        return tcp_read(sk, buff, len);
    },
    [](Sock *sk) -> int
    {
        return tcp_recv_notify(sk);
    },
    [](Sock *sk) -> int
    {
        return tcp_close(sk);
    },
    [](Sock *sk) -> int
    {
        return tcp_abort(sk);
    });

void tcp_init_segment(Tcp_Hdr *tcphdr, Ip_Hdr *iphdr, SkBuff *skb)
{
    tcphdr->sport = ntohs(tcphdr->sport);
    tcphdr->dport = ntohs(tcphdr->dport);
    tcphdr->seq = ntohl(tcphdr->seq);
    tcphdr->ack_seq = ntohl(tcphdr->ack_seq);
    tcphdr->win = ntohs(tcphdr->win);
    tcphdr->csum = ntohs(tcphdr->csum);
    tcphdr->urp = ntohs(tcphdr->urp);

    skb->seq = tcphdr->seq;
    skb->data_len = iphdr->ip_len() + tcphdr->tcp_hlen();
    skb->len = skb->data_len + tcphdr->syn + tcphdr->fin;
    skb->end_seq = skb->seq + skb->data_len;
    skb->payload = tcphdr->data;
}

void tcp_clear_queues(Tcp_Sock *tsk)
{
    skb_queue_free(&tsk->ofo_queue);
}

void tcp_in(SkBuff *skb)
{
    Ip_Hdr *iphdr = ip_hdr(skb);
    Tcp_Hdr *tcphdr = tcp_hdr(skb);
    tcp_init_segment(tcphdr, iphdr, skb);

    Sock *sk;
    /*To be implemented:
        Sock *sk = inet_lookup(skb, tcphdr->sport, tcphdr->dport);
    */

    if (!sk)
    {
        print_err("ERR(tcp_in): No TCP-Socket for sport-{} and dport-{}.", tcphdr->sport, tcphdr->dport);
        free_skb(skb);
        return;
    }

    {
        socket_wr_acquire(sk->sock);

        /*To be implemented:
            tcp_in_dbg(th, sk, skb);
        */
        tcp_input_state(sk, tcphdr, skb);

        socket_release(sk->sock);
    }
}

int tcp_udp_checksum(const uint32_t &saddr, const uint32_t &daddr, const uint8_t proto, const uint8_t *data, const uint16_t &len)
{
    uint32_t sum = saddr;
    sum += daddr;
    sum += htons(proto);
    sum += htons(len);

    return checksum(data, len, sum);
}

int tcp_v4_checksum(SkBuff *skb, const uint32_t &saddr, const uint32_t &daddr)
{
    return tcp_udp_checksum(saddr, daddr, skb->protocol, skb->data, skb->len);
}