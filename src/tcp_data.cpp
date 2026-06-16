#include "../include/tcp_data.hpp"
#include "../include/intrusive_queue.hpp"
#include "../include/syshead.hpp"

void tcp_data_insert_ordered(IntrusiveQueue<SkBuff> *queue, SkBuff *skb)
{
    list_for_each_safe(queue->get_queue_list_head(), [&](list_head *pos)
                       {
        SkBuff *next = list_entry<SkBuff>(pos, SkBuff::getOffset__list_node());

        if(skb->seq < next->seq){
            if(skb->end_seq > next->seq){
                /*To be implemented:
                    Need to join skbs
                */
               print_err("ERR(tcp_data_insert_ordered): Unable to join skbs.");
            }else{
                skb->refcnt++;
                queue->queue_add_before(&skb->list_node, &next->list_node);
                return true;
            }
        }else if(skb->seq == next->seq){
            return true;
        }

        return false; });

    skb->refcnt++;
    queue->queue_add_tail(&skb->list_node);
}

void tcp_consume_ofo_queue(Tcp_Sock *tsk)
{
    Sock *sk = &tsk->sk;
    Tcb *tcb = &tsk->tcb;

    SkBuff *skb = nullptr;

    while ((skb = tsk->ofo_queue.queue_first_entry()) && tcb->rcv_nxt == skb->seq)
    {
        tcb->rcv_nxt += skb->data_len;
        tsk->ofo_queue.dequeue();
        tsk->ofo_queue.queue_add_tail(&skb->list_node);
    }
}

int tcp_data_dequeue(Tcp_Sock *tsk, void *user_buff, const int &userlen)
{
    Sock *sk = &tsk->sk;
    int rlen = 0;
    Tcp_Hdr *tcphdr = nullptr;

    while (!sk->receive_queue.isEmpty() && rlen < userlen)
    {
        SkBuff *skb = sk->receive_queue.queue_first_entry();
        if (!skb)
        {
            break;
        }

        tcphdr = tcp_hdr(skb);

        int dlen = (rlen + skb->data_len) > userlen ? (userlen - rlen) : skb->data_len;
        std::memcpy(user_buff, skb->payload, dlen);

        skb->data_len -= dlen;
        skb->payload += dlen;
        rlen += dlen;
        user_buff += dlen;

        if (!skb->data_len)
        {
            if (tcphdr->psh)
            {
                tsk->flags |= TCP_PSH;
            }
            sk->receive_queue.dequeue();
            skb->refcnt--;
            free_skb(skb);
        }
    }

    if (sk->receive_queue.isEmpty() && !(tsk->flags & TCP_FIN))
    {
        sk->poll_events &= ~POLLIN;
    }

    return rlen;
}

int tcp_data_queue(Tcp_Sock *tsk, Tcp_Hdr *tcphdr, SkBuff *skb)
{
    Sock *sk = &tsk->sk;
    Tcb *tcb = &tsk->tcb;

    if (!tcb->rcv_wnd)
    {
        free_skb(skb);
        return -1;
    }

    if (skb->seq == tcb->rcv_nxt)
    {
        tcb->rcv_nxt += skb->data_len;
        skb->refcnt++;

        sk->receive_queue.queue_add_tail(&skb->list_node);
        tcp_consume_ofo_queue(tsk);

        sk->poll_events |= (POLLIN | POLLPRI | POLLRDBAND | POLLRDNORM);
        tsk->sk.ops->recv_notify(&tsk->sk);
    }
    else
    {
        /* Valid in-window segment, but out of order. Queue for later processing. */
        tcp_data_insert_ordered(&tsk->ofo_queue, skb);

        if (tsk->sackok)
        {
            tcp_calculate_sacks(tsk);
        }

        /*
            Per RFC5581, send an immediate duplicate ACK on receiving an out-of-order segment to notify the sender of the missing sequence number.
        */
        /*To be implemented:
            tcp_send_ack(sk);
        */
    }

    return 0;
}