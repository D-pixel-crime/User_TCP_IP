#pragma once
#include "tcp.hpp"
#include "socket_buffer.hpp"

int tcp_data_dequeue(Tcp_Sock *tsk, void *user_buff, const int &userlen);

int tcp_data_queue(Tcp_Sock *tsk, Tcp_Hdr *tcphdr, SkBuff *skb);
