#include "../include/tcp.hpp"
#include "../include/tcp_data.hpp"

SkBuff *tcp_alloc_skb(const int &opt_len, const int &size)
{
    int reserveSize = Eth_Hdr::getSize() + Ip_Hdr::getSize() + Tcp_Hdr::getSize() + opt_len + size;

    SkBuff *skb = new SkBuff(reserveSize);
    skb->reserve_headroom(reserveSize);
    skb->protocol = IP_TCP;
    skb->data_len = size;
    skb->seq = 0;

    return skb;
}

int tcp_write_syn_options(Tcp_Hdr *tcphdr, Tcp_Options *opts, const int &opt_len)
{
    Tcp_Opt_Mss *opt_mss = reinterpret_cast<Tcp_Opt_Mss *>(tcphdr->data);
    opt_mss->kind = TCP_OPT_MSS;
    opt_mss->len = TCP_OPTLEN_MSS;
    opt_mss->mss = htons(opts->mss);

    uint32_t i = sizeof(Tcp_Opt_Mss);
    if (opts->mss)
    {
        tcphdr->data[i++] = TCP_OPT_NOOP;
        tcphdr->data[i++] = TCP_OPT_NOOP;
        tcphdr->data[i++] = TCP_OPT_SACK_OK;
        tcphdr->data[i++] = TCP_OPTLEN_SACK;
    }

    tcphdr->hl = Tcp_Hdr::getDoffset() + (opt_len / 4);

    return 0;
}

int tcp_syn_options(Sock *sk, Tcp_Options *opts)
{
    Tcp_Sock *tsk = tcp_sk(sk);

    opts->mss = tsk->rmss;
    int opt_len = TCP_OPTLEN_MSS;

    if (tsk->sackok)
    {
        opts->sack = 1;
        opt_len += TCP_OPT_NOOP * 2;
        opt_len += TCP_OPTLEN_SACK;
    }
    else
    {
        opts->sack = 0;
    }

    return opt_len;
}

int tcp_write_options(Tcp_Sock *tsk, Tcp_Hdr *tcphdr)
{
    uint8_t *ptr = tcphdr->data;
    if (!tsk->sackok || !tsk->sacks[0].left)
    {
        return 0;
    }

    *ptr++ = TCP_OPT_NOOP;
    *ptr++ = TCP_OPT_NOOP;
    *ptr++ = TCP_OPT_SACK;
    *ptr++ = 2 + tsk->sacklen * 8;

    Tcp_Sack_Block *sb = reinterpret_cast<Tcp_Sack_Block *>(ptr);

    for (int i = tsk->sacklen - 1; i >= 0; i--)
    {
        sb->left = htonl(tsk->sacks[i].left);
        tsk->sacks[i].left = 0;
        sb->right = htonl(tsk->sacks[i].right);
        tsk->sacks[i].right = 0;

        sb++;
        ptr += sizeof(Tcp_Sack_Block);
    }

    tsk->sacklen = 0;
    return 0;
}

int tcp_transmit_skb(Sock *sk, SkBuff *skb, const uint32_t &seq)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    Tcb *tcb = &tsk->tcb;
    Tcp_Hdr *tcphdr = tcp_hdr(skb);

    if (!tcphdr->hl)
    {
        tcphdr->hl = Tcp_Hdr::getDoffset();
    }

    skb->push(4 * tcphdr->hl);

    tcphdr->sport = sk->sport;
    tcphdr->dport = sk->dport;
    tcphdr->seq = seq;
    tcphdr->ack_seq = tcb->rcv_nxt;
    tcphdr->csum = 0;
    tcphdr->urp = 0;

    if (tcphdr->hl > 5)
    {
        tcp_write_options(tsk, tcphdr);
    }

    tcp_out_dbg(tcphdr, sk, skb);

    tcphdr->sport = htons(tcphdr->sport);
    tcphdr->dport = htons(tcphdr->dport);
    tcphdr->seq = htonl(tcphdr->seq);
    tcphdr->ack_seq = htonl(tcphdr->ack_seq);
    tcphdr->win = htons(tcphdr->win);
    tcphdr->csum = htons(tcphdr->csum);
    tcphdr->urp = htons(tcphdr->urp);
    tcphdr->csum = tcp_v4_checksum(skb, htonl(sk->saddr), htonl(sk->daddr));

    return ip_output(sk, skb);
}

int tcp_queue_transmit_skb(Sock *sk, SkBuff *skb)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    Tcb *tcb = &tsk->tcb;
    Tcp_Hdr *tcphdr = tcp_hdr(skb);

    if (sk->write_queue.isEmpty())
    {
        tcp_rearm_rto_timer(tsk);
    }

    int rc = 0;
    if (!tsk->inflight)
    {
        /* Store sequence information into the socket buffer */
        rc = tcp_transmit_skb(sk, skb, tcb->snd_nxt);
        tsk->inflight++;
        skb->seq = tcb->snd_nxt;
        tcb->snd_nxt += skb->data_len;
        skb->end_seq = tcb->snd_nxt;

        if (tcphdr->fin)
        {
            tcb->snd_nxt++;
        }
    }

    sk->write_queue.queue_add_tail(&skb->list_node);

    return rc;
}

int tcp_send_synsack(Sock *sk)
{
    if (sk->state != (int)Tcp_State::TCP_SYN_SENT)
    {
        print_err("ERR(tcp_send_synsack): Socket not in correct state (SYN_SENT)");
        return 1;
    }

    Tcb *tcb = &tcp_sk(sk)->tcb;
    int rc = 0;

    SkBuff *skb = tcp_alloc_skb(0, 0);
    Tcp_Hdr *tcphdr = tcp_hdr(skb);

    tcphdr->syn = 1;
    tcphdr->ack = 1;

    rc = tcp_transmit_skb(sk, skb, tcb->snd_nxt);
    free_skb(skb);

    return rc;
}

void *tcp_send_delack(void *arg)
{
    Sock *sk = reinterpret_cast<Sock *>(arg);

    {
        socket_wr_acquire(sk->sock);

        Tcp_Sock *tsk = tcp_sk(sk);
        tsk->delacks = 0;
        tcp_release_delack_timer(tsk);

        tcp_send_ack(sk);

        socket_release(sk->sock);
    }

    return nullptr;
}

int tcp_send_next(Sock *sk, const int &amount)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    Tcb *tcb = &tsk->tcb;
    Tcp_Hdr *tcphdr;
    int i = 0;
    int ret = 0;

    list_for_each_safe(sk->write_queue.get_queue_list_head(), [&](list_head *pos)
                       {
        if(++i < amount){
            return true;
        }

        SkBuff*next = list_entry<SkBuff>(pos, SkBuff::getOffset__list_node());
        if(!next){
            ret = -1;
            return true;
        }

        next->reset_header();
        tcp_transmit_skb(sk, next, tcb->snd_nxt);

        next->seq = tcb->snd_nxt;
        tcb->snd_nxt += next->data_len;
        next->end_seq = tcb->snd_nxt;

        tcphdr = tcp_hdr(next);
        if(tcphdr->fin){
            tcb->snd_nxt++;
        } });

    return ret;
}

int tcp_options_len(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    uint8_t opt_len = 0;

    if (tsk->sackok && tsk->sacklen > 0)
    {
        for (int i = 0; i < tsk->sacklen; i++)
        {
            if (tsk->sacks[i].left != 0)
            {
                opt_len += 8;
            }
        }

        opt_len += 2;
    }

    while (opt_len % 4 > 0)
    {
        opt_len++;
    }

    return opt_len;
}

int tcp_send_ack(Sock *sk)
{
    if (sk->state == (int)Tcp_State::TCP_CLOSE)
    {
        return 0;
    }

    Tcb *tcb = &tcp_sk(sk)->tcb;
    int opt_len = tcp_options_len(sk);

    SkBuff *skb = tcp_alloc_skb(opt_len, 0);

    Tcp_Hdr *tcphdr = tcp_hdr(skb);
    tcphdr->ack = 1;
    tcphdr->hl = Tcp_Hdr::getDoffset() + (opt_len / 4);

    int rc = tcp_transmit_skb(sk, skb, tcb->snd_nxt);
    free_skb(skb);

    return rc;
}

int tcp_send_syn(Sock *sk)
{
    if (sk->state != (int)Tcp_State::TCP_SYN_SENT && sk->state != (int)Tcp_State::TCP_CLOSE && sk->state != (int)Tcp_State::TCP_LISTEN)
    {
        print_err("ERR(tcp_send_sync): Socket not in correct state (closed or listen)");
        return 1;
    }

    // 1st property set to '0'
    Tcp_Options opts = {0};
    int tcp_options_len = 0;

    tcp_options_len = tcp_syn_options(sk, &opts);
    SkBuff *skb = tcp_alloc_skb(tcp_options_len, 0);
    Tcp_Hdr *tcphdr = tcp_hdr(skb);

    tcp_write_syn_options(tcphdr, &opts, tcp_options_len);
    sk->state = (int)Tcp_State::TCP_SYN_SENT;
    tcphdr->syn = 1;

    return tcp_queue_transmit_skb(sk, skb);
}

int tcp_send_fin(Sock *sk)
{
    if (sk->state == (int)Tcp_State::TCP_CLOSE)
    {
        return 0;
    }

    int rc = 0;

    SkBuff *skb = tcp_alloc_skb(0, 0);

    Tcp_Hdr *tcphdr = tcp_hdr(skb);
    tcphdr->fin = 1;
    tcphdr->ack = 1;

    return tcp_queue_transmit_skb(sk, skb);
}

void tcp_select_initial_window(uint32_t *rcv_wnd)
{
    *rcv_wnd = 44477;
}

void tcp_notify_user(Sock *sk)
{
    switch ((Tcp_State)sk->state)
    {
    case Tcp_State::TCP_CLOSE_WAIT:
        sk->sock->sleep.wakeup();
        break;
    }
}

void *tcp_connect_rto(void *arg)
{
    Tcp_Sock *tsk = tcp_sk(arg);
    Tcb *tcb = &tsk->tcb;
    Sock *sk = &tsk->sk;

    {
        socket_wr_acquire(sk->sock);

        tcp_release_rto_timer(tsk);

        if (sk->state == (int)Tcp_State::TCP_SYN_SENT)
        {
            if (tsk->backoff > TCP_CONN_RETRIES)
            {
                tsk->sk.err = -ETIMEDOUT;
                sk->poll_events |= (POLLOUT | POLLERR | POLLHUP);
                tcp_done(sk);
            }
            else
            {
                SkBuff *skb = write_queue_head(sk);
                if (skb)
                {
                    skb->reset_header();
                    tcp_transmit_skb(sk, skb, tcb->snd_una);

                    tsk->backoff++;
                    tcp_rearm_rto_timer(tsk);
                }
            }
        }
        else
        {
            print_err("ERR(tcp_connect_rto): TCP connect RTO triggered even when not in SYNSENT.");
        }

        socket_release(sk->sock);
    }

    return nullptr;
}

void *tcp_retransmission_timeout(void *arg)
{
    Tcp_Sock *tsk = tcp_sk(arg);
    Tcb *tcb = &tsk->tcb;
    Sock *sk = &tsk->sk;

    socket_wr_acquire(sk->sock);

    tcp_release_rto_timer(tsk);

    SkBuff *skb = write_queue_head(sk);

    if (!skb)
    {
        tsk->backoff = 0;
        tcpsock_dbg("TCP RTO queue empty, notifying user", sk);
        tcp_notify_user(sk);
        socket_release(sk->sock);
        return nullptr;
    }

    Tcp_Hdr *tcphdr = tcp_hdr(skb);
    skb->reset_header();

    tcp_transmit_skb(sk, skb, tcb->snd_una);
    /**
     * RFC 6298: 2.5
     * A maximum value MAY be placed on RTO,
     * provided it is at least 60 seconds.
     */
    if (tsk->rto > 60000)
    {
        tcp_done(sk);
        tsk->sk.err = -ETIMEDOUT;
        sk->poll_events = (POLLOUT | POLLERR | POLLHUP);
        socket_release(sk->sock);
        return nullptr;
    }
    else
    {
        /* RFC 6298: Section 5.5 double RTO time */
        tsk->rto *= 2;
        tsk->backoff++;
        tsk->retransmit = Timer::create(tsk->rto, [tsk]()
                                        { tcp_retransmission_timeout(tsk); }, Timer_Type::Trackable);

        if (tcphdr->fin)
        {
            tcp_handle_fin_state(sk);
        }
    }

    socket_release(sk->sock);
    return nullptr;
}

void tcp_rearm_rto_timer(Tcp_Sock *tsk)
{
    Sock *sk = &tsk->sk;
    tcp_release_rto_timer(tsk);

    if (sk->state == (int)Tcp_State::TCP_SYN_SENT)
    {
        tsk->retransmit = Timer::create(TCP_SYN_BACKOFF << tsk->backoff, [tsk]()
                                        { tcp_connect_rto(tsk); }, Timer_Type::Trackable);
    }
    else
    {
        tsk->retransmit = Timer::create(tsk->rto, [tsk]()
                                        { tcp_retransmission_timeout(tsk); }, Timer_Type::Trackable);
    }
}

int tcp_connect(Sock *sk)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    Tcb *tcb = &tsk->tcb;

    tsk->tcp_header_len = Tcp_Hdr::getSize();
    tcb->iss = generate_iss();
    tcb->snd_wnd = tcb->snd_wl1 = 0;

    tcb->snd_una = tcb->iss;
    tcb->snd_up = tcb->iss;
    tcb->snd_nxt = tcb->iss;

    tcp_select_initial_window(&tsk->tcb.rcv_wnd);

    int rc = tcp_send_syn(sk);
    tcb->snd_nxt++;

    return rc;
}

int tcp_send(Tcp_Sock *tsk, const void *buff, const int &len)
{
    int slen = len;
    int mss = tsk->smss;
    int dlen = 0;
    SkBuff *skb = nullptr;
    Tcp_Hdr *tcphdr = nullptr;

    while (slen > 0)
    {
        dlen = slen > mss ? mss : len;
        slen -= dlen;

        skb = tcp_alloc_skb(0, dlen);
        skb->push(dlen);
        std::memcpy(skb->data, buff, dlen);

        buff += dlen;

        tcphdr = tcp_hdr(skb);
        tcphdr->ack = 1;

        if (!slen)
        {
            tcphdr->psh = 1;
        }

        if (tcp_queue_transmit_skb(&tsk->sk, skb) == -1)
        {
            throw std::runtime_error("ERR(tcp_send): Error in queueing skb.");
        }
    }

    tcp_rearm_user_timeout(&tsk->sk);
    return len;
}

int tcp_send_reset(Tcp_Sock *tsk)
{
    SkBuff *skb = tcp_alloc_skb(0, 0);
    Tcp_Hdr *tcphdr = tcp_hdr(skb);
    Tcb *tcb = &tsk->tcb;

    tcphdr->rst = 1;
    tcb->snd_una = tcb->snd_nxt;

    int rc = tcp_transmit_skb(&tsk->sk, skb, tcb->snd_nxt);
    free_skb(skb);

    return rc;
}

int tcp_send_challenge_ack(Sock *sk, SkBuff *skb)
{
    /*To be implemented:
        Implement this.
    */
    return 0;
}

int tcp_queue_fin(Sock *sk)
{
    SkBuff *skb = tcp_alloc_skb(0, 0);
    Tcp_Hdr *tcphdr = tcp_hdr(skb);

    tcphdr->fin = 1;
    tcphdr->ack = 1;

    int rc = tcp_queue_transmit_skb(sk, skb);
    tcpsock_dbg("Queueing fin", sk);

    return rc;
}