#include "../include/tcp.hpp"
#include "../include/tcp_data.hpp"

int tcp_parse_opts(Tcp_Sock *tsk, Tcp_Hdr *tcphdr)
{
    uint8_t *ptr = tcphdr->data;
    uint8_t optlen = tcphdr->tcp_hlen() - 20;
    bool sack_seen = false, tspot_seen = false;

    Tcp_Opt_Mss *opt_mss = nullptr;

    while (optlen > 0 && optlen < 20)
    {
        switch (*ptr)
        {
        case TCP_OPT_MSS:
            opt_mss = reinterpret_cast<Tcp_Opt_Mss *>(ptr);
            uint16_t mss = ntohs(opt_mss->mss);
            if (mss > 536 && mss <= 1460)
            {
                tsk->smss = mss;
            }

            ptr += sizeof(Tcp_Opt_Mss);
            optlen -= 4;
            break;

        case TCP_OPT_NOOP:
            ptr++;
            optlen--;
            break;

        case TCP_OPT_SACK_OK:
            sack_seen = true;
            optlen--;
            break;

        case TCP_OPT_TS:
            tspot_seen = true;
            optlen--;
            break;

        default:
            print_err("ERR(tcp_parse_opts): Unsupported TCP-OPTS.");
            optlen--;
            break;
        }

        if (!tspot_seen)
        {
            tsk->tsopt = 0;
        }

        if (sack_seen && tsk->sackok)
        {
            tsk->sacks_allowed = tsk->tsopt ? 3 : 4;
        }
        else
        {
            tsk->sackok = 0;
        }
    }

    return 0;
}

int tcp_clean_rto_queue(Sock *sk, const uint32_t &una)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    SkBuff *skb = nullptr;

    while (skb = sk->write_queue.queue_first_entry())
    {
        if (skb->seq > 0 && skb->end_seq <= una)
        {
            sk->write_queue.dequeue();
            skb->refcnt--;
            free_skb(skb);
            tsk->inflight = std::max((uint)0, tsk->inflight - 1);
        }
        else
        {
            break;
        }
    }

    if (!skb || !tsk->inflight)
    {
        tcp_stop_rto_timer(tsk);
    }

    return 0;
}

inline int tcp_drop(Sock *sk, SkBuff *skb)
{
    free_skb(skb);
    return 0;
}

int tcp_verify_segment(Tcp_Sock *tsk, Tcp_Hdr *tcphdr, SkBuff *skb)
{
    Tcb *tcb = &tsk->tcb;

    if (skb->data_len > 0 && !tcb->rcv_wnd)
    {
        return 0;
    }

    if (tcphdr->seq < tcb->rcv_nxt || tcphdr->seq > (tcb->rcv_nxt + tcb->rcv_wnd))
    {
        /*To be implemented:
            tcpsock_dbg("Received invalid segment", (&tsk->sk));
        */
        return 0;
    }

    return 1;
}

void tcp_reset(Sock *sk)
{
    sk->poll_events = (POLLOUT | POLLWRNORM | POLLERR | POLLHUP);

    switch ((Tcp_States)sk->state)
    {
    case Tcp_States::TCP_SYN_SENT:
        sk->err = -ECONNREFUSED;
        break;

    case Tcp_States::TCP_CLOSE_WAIT:
        sk->err = -EPIPE;
        break;

    case Tcp_States::TCP_CLOSE:
        return;

    default:
        sk->err = -ECONNRESET;
        break;
    }

    tcp_done(sk);
}

inline int tcp_discard(Tcp_Sock *tsk, SkBuff *skb, Tcp_Hdr *tcphdr)
{
    free_skb(skb);
    return 0;
}

inline int tcp_listen(Tcp_Sock *tsk, SkBuff *skb, Tcp_Hdr *tcphdr)
{
    free_skb(skb);
    return 0;
}

int tcp_synsent(Tcp_Sock *tsk, SkBuff *skb, Tcp_Hdr *tcphdr)
{
    Tcb *tcb = &tsk->tcb;
    Sock *sk = &tsk->sk;

    /*To be implemented:
        tcpsock_dbg("state is synsent", sk);
    */

    if (tcphdr->ack)
    {
        if (tcphdr->ack_seq <= tcb->iss || tcphdr->ack_seq > tcb->snd_nxt)
        {
            /*To be implemented:
                tcpsock_dbg("ACK is unacceptable", sk);
            */
            if (!tcphdr->rst)
            {
                /*To be implemented:
                    Reset Behaviour
                */
            }
        }
        if (tcphdr->ack_seq < tcb->snd_una || tcphdr->ack_seq > tcb->snd_nxt)
        {
            /*To be implemented:
            tcpsock_dbg("ACK is unacceptable", sk);
            Reset Behaviour
            */
        }

        tcp_drop(sk, skb);
        return 0;
    }

    if (tcphdr->rst)
    {
        tcp_reset(sk);
        tcp_drop(sk, skb);
        return 0;
    }

    if (!tcphdr->syn)
    {
        tcp_drop(sk, skb);
        return 0;
    }

    tcb->rcv_nxt = tcphdr->seq + 1;
    tcb->irs = tcphdr->seq;
    if (tcphdr->ack)
    {
        tcb->snd_una = tcphdr->ack_seq;
        tcp_clean_rto_queue(sk, tcb->snd_una);
    }

    if (tcb->snd_una > tcb->iss)
    {
        /*To be implemented:
            tcp_set_state(sk, TCP_ESTABLISHED);
        */
        tcb->snd_una = tcb->snd_nxt;
        tsk->backoff = 0;

        /* RFC 6298: Sender SHOULD set RTO <= 1 second */
        tsk->rto = 1000;
        /*To be implemented:
             tcp_send_ack(&tsk->sk);
        */
        tcp_rearm_user_timeout(sk);
        tcp_parse_opts(tsk, tcphdr);
        sock_connected(sk);
    }
    else
    {
        /*To be implemented:
            tcp_set_state(sk, TCP_SYN_RECEIVED);
        */
        tcb->snd_una = tcb->iss;
        /*To be implemented:
             tcp_send_synack(&tsk->sk);
        */
    }

    tcp_drop(sk, skb);
    return 0;
}

int tcp_closed(Tcp_Sock *tsk, SkBuff *skb, Tcp_Hdr *tcphdr)
{
    /*
     * CLOSED State Packet Handling (RFC 793/9293):
     * 1. Drop the incoming segment.
     * 2. If it was a RST, drop silently and return.
     * 3. If it wasn't a RST, respond with a valid matching RST:
     * - ACK off: <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
     * - ACK on:  <SEQ=SEG.ACK><CTL=RST>
     */

    /*To be implemented:
        tcpsock_dbg("state is closed", (&tsk->sk));
    */

    if (tcphdr->rst)
    {
        tcp_discard(tsk, skb, tcphdr);
        return 0;
    }

    int rc = -1;
    /*To be implemented:
        int rc = tcp_send_reset(tsk);
    */
    free_skb(skb);
    return rc;
}

int tcp_input_state(Sock *sk, Tcp_Hdr *tcphdr, SkBuff *skb)
{
    Tcp_Sock *tsk = tcp_sk(sk);
    Tcb *tcb = &tsk->tcb;

    /*To be implemented:
        tcpsock_dbg("input state", sk);
    */

    switch ((Tcp_States)sk->state)
    {
    case Tcp_States::TCP_CLOSE:
        return tcp_closed(tsk, skb, tcphdr);

    case Tcp_States::TCP_LISTEN:
        return tcp_listen(tsk, skb, tcphdr);

    case Tcp_States::TCP_SYN_SENT:
        return tcp_synsent(tsk, skb, tcphdr);
    }

    if (!tcp_verify_segment(tsk, tcphdr, skb))
    {
        /*
         * RFC 793: If the incoming segment is unacceptable:
         * - If RST is set: drop the segment and return.
         * - Otherwise: send an ACK in reply.
         */
        if (!tcphdr->rst)
        {
            /*To be implemented:
                tcp_send_ack(sk);
            */
        }

        return tcp_drop(sk, skb);
    }

    if (tcphdr->rst)
    {
        free_skb(skb);
        tcp_enter_time_wait(sk);
        tsk->sk.ops->recv_notify(&tsk->sk);
        return 0;
    }

    /*To be implemented:
        3rd check: Security and Precedence
    */

    if (tcphdr->syn)
    {
        /* RFC 5961 Section 4.2 */
        /*To be implemented:
            tcp_send_challenge_ack(sk, skb);
        */
        return tcp_drop(sk, skb);
    }

    if (!tcphdr->ack)
    {
        return tcp_drop(sk, skb);
    }

    switch ((Tcp_States)sk->state)
    {
    case Tcp_States::TCP_SYN_RECEIVED:
        if (tcb->snd_una <= tcphdr->ack_seq && tcphdr->ack_seq < tcb->snd_nxt)
        {
            /*To be implemented:
                tcp_set_state(sk, TCP_ESTABLISHED);
            */
        }
        else
        {
            return tcp_drop(sk, skb);
        }

    case Tcp_States::TCP_ESTABLISHED:
    case Tcp_States::TCP_FIN_WAIT_1:
    case Tcp_States::TCP_CLOSE_WAIT:
    case Tcp_States::TCP_CLOSING:
    case Tcp_States::TCP_LAST_ACK:
        if (tcb->snd_una < tcphdr->ack_seq && tcphdr->ack_seq < tcb->snd_nxt)
        {
            /*
             * RFC 793: Remove any segments from the retransmission
             * queue that are now entirely acknowledged.
             */
            tcb->snd_una = tcphdr->ack_seq;
            tcp_rtt(tsk);
            tcp_clean_rto_queue(sk, tcb->snd_una);
        }

        if (tcphdr->ack_seq < tcb->snd_una)
        {
            /* Ignore duplicate ack */
            return tcp_drop(sk, skb);
        }

        if (tcphdr->ack_seq > tcb->snd_nxt)
        {
            /*
             * RFC 793: If ACK acknowledges unsent data:
             * - Send an ACK, drop the segment, and return.
             *
             * NOTE: Linux drops the segment without replying.
             */
            return tcp_drop(sk, skb);
        }

        if (tcb->snd_una < tcphdr->ack_seq && tcphdr->ack_seq <= tcb->snd_nxt)
        {
            /*To be implemented:
                Update Send Window
            */
        }

        break;
    }

    if (sk->write_queue.isEmpty())
    {
        switch ((Tcp_States)sk->state)
        {
        case Tcp_States::TCP_FIN_WAIT_1:
            /*To be implemented:
                tcp_set_state(sk, TCP_FIN_WAIT_2);
            */

        case Tcp_States::TCP_FIN_WAIT_2:
            break;

        case Tcp_States::TCP_CLOSING:
            /* In addition to the processing for the ESTABLISHED state, if
             * the ACK acknowledges our FIN then enter the TIME-WAIT state,
               otherwise ignore the segment. */
            /*To be implemented:
                tcp_set_state(sk, TCP_TIME_WAIT);
            */
            break;

        case Tcp_States::TCP_LAST_ACK:
            free_skb(skb);
            return tcp_done(sk);

        case Tcp_States::TCP_TIME_WAIT:
            /*
             * TIME-WAIT State: Only a remote FIN retransmission can arrive.
             * - Send an ACK in response.
             * - Restart the 2 MSL timeout.
             */
            if (tcb->rcv_nxt == tcphdr->seq)
            {
                /*To be implemented:
                    tcpsock_dbg("Remote FIN retransmitted?", sk);
                */
                tsk->flags |= TCP_FIN;
                /*To be implemented:
                     tcp_send_ack(sk);
                 */
            }
            break;
        }
    }

    if (tcphdr->urg)
    {
        /*To be implemented:
            Check the urg bit
        */
    }

    int expected = skb->seq == tcb->rcv_nxt;

    switch ((Tcp_States)sk->state)
    {
    case Tcp_States::TCP_ESTABLISHED:
    case Tcp_States::TCP_FIN_WAIT_1:
    case Tcp_States::TCP_FIN_WAIT_2:
        if (tcphdr->psh || skb->data_len > 0)
        {
            tcp_data_queue(tsk, tcphdr, skb);
        }
        break;

    case Tcp_States::TCP_CLOSE_WAIT:
    case Tcp_States::TCP_CLOSING:
    case Tcp_States::TCP_LAST_ACK:
    case Tcp_States::TCP_TIME_WAIT:
        /*
         * Invalid State Exception: Remote FIN already received.
         * - Ignore the segment text.
         */
        break;
    }

    if (tcphdr->fin && expected)
    {
        /*To be implemented:
            tcpsock_dbg("Received in-sequence FIN", sk);
        */
        switch ((Tcp_States)sk->state)
        {
        case Tcp_States::TCP_CLOSE:
        case Tcp_States::TCP_LISTEN:
        case Tcp_States::TCP_SYN_SENT:
            tcp_drop(sk, skb);
            return 0;
        }

        tcb->rcv_nxt++;
        tsk->flags |= TCP_FIN;
        sk->poll_events |= (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);

        /*To be implemented:
         tcp_send_ack(sk);
        */
        tsk->sk.ops->recv_notify(&tsk->sk);

        switch ((Tcp_States)sk->state)
        {
        case Tcp_States::TCP_ESTABLISHED:
        case Tcp_States::TCP_SYN_RECEIVED:
            /*To be implemented:
                tcp_set_state(sk, TCP_CLOSE_WAIT);
            */
            break;

        case Tcp_States::TCP_FIN_WAIT_1:
            /*
             * RFC 793 (FIN-WAIT-1): If our FIN is ACKed:
             * - Enter TIME-WAIT, start time-wait timer, and stop all other timers.
             * - Otherwise: enter CLOSING state.
             */
            if (sk->write_queue.isEmpty())
            {
                tcp_enter_time_wait(sk);
            }
            else
            {
                /*To be implemented:
                    tcp_set_state(sk, TCP_CLOSING);
                */
            }
            break;

        case Tcp_States::TCP_FIN_WAIT_2:
            /*
             * RFC 793: Enter TIME-WAIT state.
             * - Start the time-wait timer.
             * - Stop all other timers.
             */
            tcp_enter_time_wait(sk);
            break;

        case Tcp_States::TCP_CLOSE_WAIT:
        case Tcp_States::TCP_CLOSING:
        case Tcp_States::TCP_LAST_ACK:
            break;

        case Tcp_States::TCP_TIME_WAIT:
            /*To be implemented:
             Dont change state. Restart the 2MSL timer-wait
            */
            break;
        }
    }

    switch ((Tcp_States)sk->state)
    {
    case Tcp_States::TCP_ESTABLISHED:
    case Tcp_States::TCP_FIN_WAIT_1:
    case Tcp_States::TCP_FIN_WAIT_2:
        if (expected)
        {
            tcp_stop_delack_timer(tsk);

            int pending = std::min(sk->write_queue.size(), 3);
            /*
             * RFC 1122: Delayed ACK Implementation:
             * - Max Delay: MUST be < 0.5 seconds.
             * - Full-sized Stream: SHOULD ACK at least every 2nd segment.
             */
            if (!tsk->inflight && pending > 0)
            {
                /*To be implemented:
                    tcp_send_next(sk, pending);
                */
                tsk->inflight += pending;
                /*To be implemented:
                     tcp_rearm_rto_timer(tsk);
                 */
            }
            else if (tcphdr->psh || (skb->data_len > 1000 && ++tsk->delacks > 1))
            {
                tsk->delack = 0;
                /*To be implemented:
                    tcp_send_ack(sk);
                */
            }
            else if (skb->data_len > 0)
            {
                tsk->delack = Timer::create(200, [tsk]()
                                            {
                        /*To be implemented:
                            tcp_send_delack(&tsk->sk);
                        */ }, Timer_Type::Trackable);
            }
        }
    }

    free_skb(skb);
    return 0;
}

int tcp_receive(Tcp_Sock *tsk, void *buff, const int &len)
{
    int rlen = 0, currlen = 0;
    Sock *sk = &tsk->sk;
    Socket *sock = sk->sock;
    std::memset(buff, 0, len);

    while (rlen < len)
    {
        currlen = tcp_data_dequeue(tsk, buff + rlen, len - rlen);
        rlen += currlen;
        if (tsk->flags & TCP_PSH)
        {
            tsk->flags &= ~TCP_PSH;
            break;
        }

        if ((tsk->flags & TCP_FIN) || rlen == len)
        {
            break;
        }

        if (sock->flags & O_NONBLOCK)
        {
            if (!rlen)
            {
                rlen = -EAGAIN;
            }
            break;
        }
        else
        {
            /*To be implemented:
                pthread_mutex_lock(&tsk->sk.recv_wait.lock);
            */
            socket_release(sock);
            /*To be implemented:
             wait_sleep(&tsk->sk.recv_wait);
             pthread_mutex_unlock(&tsk->sk.recv_wait.lock);
            */
            socket_wr_acquire(sock);
        }
    }

    if (rlen >= 0)
    {
        tcp_rearm_user_timeout(sk);
    }

    return rlen;
}