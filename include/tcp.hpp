#pragma once
#include "syshead.hpp"
#include "socket_buffer.hpp"
#include "intrusive_queue.hpp"
#include "ethernet.hpp"
#include "ip.hpp"
#include "timer.hpp"
#include "sock.hpp"

class Tcp_Hdr;

inline constexpr std::array<const std::string_view, 11> tcp_dbg_states = {
    "TCP_LISTEN",
    "TCP_SYN_SENT",
    "TCP_SYN_RECEIVED",
    "TCP_ESTABLISHED",
    "TCP_FIN_WAIT_1",
    "TCP_FIN_WAIT_2",
    "TCP_CLOSE",
    "TCP_CLOSE_WAIT",
    "TCP_CLOSING",
    "TCP_LAST_ACK",
    "TCP_TIME_WAIT",
};

inline void tcp_in_dbg(Tcp_Hdr *tcphdr, Sock *sk, SkBuff *skb)
{
    print_debug(std::format(
        "TCP {}.{}.{}.{}.{} > {}.{}.{}.{}.{}: Flags [S{}A{}P{}F{}R{}], seq {}:{}, ack {}, win {} rto {} boff {}",
        static_cast<uint8_t>(sk->daddr >> 24), static_cast<uint8_t>(sk->daddr >> 16),
        static_cast<uint8_t>(sk->daddr >> 8), static_cast<uint8_t>(sk->daddr >> 0), sk->dport,
        static_cast<uint8_t>(sk->saddr >> 24), static_cast<uint8_t>(sk->saddr >> 16),
        static_cast<uint8_t>(sk->saddr >> 8), static_cast<uint8_t>(sk->saddr >> 0), sk->sport,
        static_cast<uint8_t>(tcphdr->syn), static_cast<uint8_t>(tcphdr->ack),
        static_cast<uint8_t>(tcphdr->psh), static_cast<uint8_t>(tcphdr->fin),
        static_cast<uint8_t>(tcphdr->rst),
        tcphdr->seq - tcp_sk(sk)->tcb.irs,
        tcphdr->seq + skb->data_len - tcp_sk(sk)->tcb.irs,
        tcphdr->ack_seq - tcp_sk(sk)->tcb.iss, tcphdr->win,
        tcp_sk(sk)->rto, tcp_sk(sk)->backoff));
}

inline void tcp_out_dbg(Tcp_Hdr *tcphdr, Sock *sk, SkBuff *skb)
{
    print_debug(std::format(
        "TCP {}.{}.{}.{}.{} > {}.{}.{}.{}.{}: Flags [S{}A{}P{}F{}R{}], seq {}:{}, ack {}, win {} rto {} boff {}",
        static_cast<uint8_t>(sk->saddr >> 24), static_cast<uint8_t>(sk->saddr >> 16),
        static_cast<uint8_t>(sk->saddr >> 8), static_cast<uint8_t>(sk->saddr >> 0), sk->sport,
        static_cast<uint8_t>(sk->daddr >> 24), static_cast<uint8_t>(sk->daddr >> 16),
        static_cast<uint8_t>(sk->daddr >> 8), static_cast<uint8_t>(sk->daddr >> 0), sk->dport,
        static_cast<uint8_t>(tcphdr->syn), static_cast<uint8_t>(tcphdr->ack),
        static_cast<uint8_t>(tcphdr->psh), static_cast<uint8_t>(tcphdr->fin),
        static_cast<uint8_t>(tcphdr->rst),
        tcphdr->seq - tcp_sk(sk)->tcb.iss,
        tcphdr->seq + skb->data_len - tcp_sk(sk)->tcb.iss,
        tcphdr->ack_seq - tcp_sk(sk)->tcb.irs, tcphdr->win,
        tcp_sk(sk)->rto, tcp_sk(sk)->backoff));
}

inline void tcpsock_dbg(std::string_view msg, Sock *sk)
{
    print_debug(std::format(
        "TCP x:{} > {}.{}.{}.{}.{} (snd_una {}, snd_nxt {}, snd_wnd {}, "
        "snd_wl1 {}, snd_wl2 {}, rcv_nxt {}, rcv_wnd {} recv-q {} send-q {} "
        "rto {} boff {}) state {}: {}",
        sk->sport,
        static_cast<uint8_t>(sk->daddr >> 24), static_cast<uint8_t>(sk->daddr >> 16),
        static_cast<uint8_t>(sk->daddr >> 8), static_cast<uint8_t>(sk->daddr >> 0), sk->dport,
        tcp_sk(sk)->tcb.snd_una - tcp_sk(sk)->tcb.iss,
        tcp_sk(sk)->tcb.snd_nxt - tcp_sk(sk)->tcb.iss, tcp_sk(sk)->tcb.snd_wnd,
        tcp_sk(sk)->tcb.snd_wl1, tcp_sk(sk)->tcb.snd_wl2,
        tcp_sk(sk)->tcb.rcv_nxt - tcp_sk(sk)->tcb.irs, tcp_sk(sk)->tcb.rcv_wnd,
        sk->receive_queue.size(), sk->write_queue.size(),
        tcp_sk(sk)->rto, tcp_sk(sk)->backoff,
        tcp_dbg_states[sk->state], msg));
}

inline constexpr int TCP_FIN = 0x01;
inline constexpr int TCP_SYN = 0x02;
inline constexpr int TCP_RST = 0x04;
inline constexpr int TCP_PSH = 0x08;
inline constexpr int TCP_ACK = 0x10;

inline constexpr int TCP_URG = 0x20;
inline constexpr int TCP_ECN = 0x40;
inline constexpr int TCP_WIN = 0x80;

inline constexpr int TCP_SYN_BACKOFF = 500;
inline constexpr int TCP_CONN_RETRIES = 3;

inline constexpr int TCP_OPT_NOOP = 1;
inline constexpr int TCP_OPTLEN_MSS = 4;
inline constexpr int TCP_OPT_MSS = 2;
inline constexpr int TCP_OPT_SACK_OK = 4;
inline constexpr int TCP_OPT_SACK = 5;
inline constexpr int TCP_OPTLEN_SACK = 2;
inline constexpr int TCP_OPT_TS = 8;

inline constexpr int TCP_2MSL = 60000;
inline constexpr int TCP_USER_TIMEOUT = 180000;

class __attribute__((packed)) Tcp_Hdr
{
public:
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack_seq;
    uint8_t rsvd : 4, hl : 4;
    uint8_t fin : 1, syn : 1, rst : 1, psh : 1, ack : 1, urg : 1, ece : 1, cwr : 1;
    uint16_t win;
    uint16_t csum;
    uint16_t urp;
    uint8_t data[];

    static size_t getSize()
    {
        return sizeof(Tcp_Hdr);
    }

    static size_t getDoffset()
    {
        return getSize() / 4;
    }

    auto tcp_hlen()
    {
        return hl << 2;
    }
};

template <typename T>
Tcp_Hdr *tcp_hdr(T *skb)
{
    return reinterpret_cast<Tcp_Hdr *>(skb->head + Eth_Hdr::getSize() + Ip_Hdr::getSize());
}

class Tcp_Options
{
public:
    uint16_t options;
    uint16_t mss;
    uint8_t sack;
};

class __attribute__((packed)) Tcp_Opt_Mss
{
public:
    uint8_t kind;
    uint8_t len;
    uint16_t mss;
};

class __attribute__((packed)) Tcp_Ip_Hdr
{
public:
    uint32_t saddr;
    uint32_t daddr;
    uint8_t zero;
    uint8_t proto;
    uint16_t tlen;
};

/**
 * @enum Tcp_State
 * @brief RFC 793 TCP state machine enumeration.
 * @note Explicitly backed by uint8_t for safe serialization and memory efficiency.
 */
enum class Tcp_State : uint8_t
{
    /** @brief Passive open; waiting for an incoming connection request. */
    TCP_LISTEN,

    /** @brief Active open; SYN sent, waiting for a matching connection request. */
    TCP_SYN_SENT,

    /** @brief SYN received and SYN-ACK sent; waiting for connection confirmation. */
    TCP_SYN_RECEIVED,

    /** @brief Connection established; normal data transfer phase. */
    TCP_ESTABLISHED,

    /** @brief Active close; FIN sent, waiting for remote FIN or ACK. */
    TCP_FIN_WAIT_1,

    /** @brief Active close; ACK received, waiting for remote FIN. */
    TCP_FIN_WAIT_2,

    /** @brief No active connection; nominal starting and ending point. */
    TCP_CLOSE,

    /** @brief Passive close; FIN received, waiting for local application close. */
    TCP_CLOSE_WAIT,

    /** @brief Simultaneous close; FIN sent and received, waiting for ACK. */
    TCP_CLOSING,

    /** @brief Passive close; final FIN sent, waiting for the final ACK. */
    TCP_LAST_ACK,

    /** @brief Active close complete; waiting 2*MSL to ensure remote peer received ACK. */
    TCP_TIME_WAIT
};

class Tcb
{
public:
    uint32_t snd_una;
    uint32_t snd_nxt;
    uint32_t snd_wnd;
    uint32_t snd_up;
    uint32_t snd_wl1;
    uint32_t snd_wl2;
    uint32_t iss;
    uint32_t rcv_nxt;
    uint32_t rcv_wnd;
    uint32_t rcv_up;
    uint32_t irs;
};

class __attribute__((packed)) Tcp_Sack_Block
{
public:
    uint32_t left;
    uint32_t right;
};

class Tcp_Sock
{
public:
    Sock sk;
    int fd;
    uint16_t tcp_header_len;
    Tcb tcb;
    uint8_t flags;
    uint8_t backoff;
    int32_t srtt;
    int32_t rttvar;
    uint32_t rto;
    Timer *retransmit;
    Timer *delack;
    Timer *keepalive;
    Timer *linger;
    uint8_t delacks;
    uint16_t rmss;
    uint16_t smss;
    uint16_t cwnd;
    uint32_t inflight;

    uint8_t sackok;
    uint8_t sacks_allowed;
    uint8_t sacklen;
    Tcp_Sack_Block sacks[4];

    uint8_t tsopt;

    IntrusiveQueue<SkBuff> ofo_queue;

    Tcp_Sock();
};

template <typename T>
Tcp_Sock *tcp_sk(T *sk)
{
    return reinterpret_cast<Tcp_Sock *>(sk);
}

Sock *tcp_alloc_sock();

int tcp_v4_init_sock(Sock *sk);

int tcp_init_sock(Sock *sk);

void tcp_set_state(Sock *sk, const uint32_t &state);

uint16_t generate_port();

int generate_iss();

int tcp_v4_connect(Sock *sk, const sockaddr *addr, const int &addr_len, const int &flags);

int tcp_disconnect(Sock *sk, const int &flags);

int tcp_write(Sock *sk, const void *buff, const int &len);

int tcp_read(Sock *sk, const void *buff, const int &len);

int tcp_recv_notify(Sock *sk);

int tcp_close(Sock *sk);

int tcp_abort(Sock *sk);

int tcp_free(Sock *sk);

int tcp_done(Sock *sk);

void tcp_stop_rto_timer(Tcp_Sock *tsk);

void tcp_release_rto_timer(Tcp_Sock *tsk);

void tcp_stop_delack_timer(Tcp_Sock *tsk);

void tcp_release_delack_timer(Tcp_Sock *tsk);

void tcp_clear_timers(Sock *sk);

void tcp_handle_fin_state(Sock *sk);

void tcp_enter_time_wait(Sock *sk);

void tcp_rearm_user_timeout(Sock *sk);

void tcp_rtt(Tcp_Sock *tsk);

int tcp_calculate_sacks(Tcp_Sock *tsk);

void tcp_init_segment(Tcp_Hdr *tcphdr, Ip_Hdr *iphdr, SkBuff *skb);

void tcp_clear_queues(Tcp_Sock *tsk);

void tcp_in(SkBuff *skb);

int tcp_udp_checksum(const uint32_t &saddr, const uint32_t &daddr, const uint8_t proto, const uint8_t *data, const uint16_t &len);

int tcp_v4_checksum(SkBuff *skb, const uint32_t &saddr, const uint32_t &daddr);

int tcp_parse_opts(Tcp_Sock *tsk, Tcp_Hdr *tcphdr);

int tcp_clean_rto_queue(Sock *sk, const uint32_t &una);

int tcp_verify_segment(Tcp_Sock *tsk, Tcp_Hdr *tcphdr, SkBuff *skb);

void tcp_reset(Sock *sk);

int tcp_synsent(Tcp_Sock *tsk, SkBuff *skb, Tcp_Hdr *tcphdr);

int tcp_closed(Tcp_Sock *tsk, SkBuff *skb, Tcp_Hdr *tcphdr);

int tcp_input_state(Sock *sk, Tcp_Hdr *tcphdr, SkBuff *skb);

int tcp_receive(Tcp_Sock *tsk, void *buff, const int &len);

SkBuff *tcp_alloc_skb(const int &optlen, const int &size);

int tcp_write_syn_options(Tcp_Hdr *tcphdr, Tcp_Options *opts, const int &opt_len);

int tcp_syn_options(Sock *sk, Tcp_Options *opts);

int tcp_write_options(Tcp_Sock *tsk, Tcp_Hdr *tcphdr);

int tcp_transmit_skb(Sock *sk, SkBuff *skb, const uint32_t &seq);

int tcp_queue_transmit_skb(Sock *sk, SkBuff *skb);

int tcp_send_synsack(Sock *sk);

void *tcp_send_delack(void *arg);

int tcp_send_next(Sock *sk, const int &amount);

int tcp_options_len(Sock *sk);

int tcp_send_ack(Sock *sk);

int tcp_send_syn(Sock *sk);

int tcp_send_fin(Sock *sk);

void tcp_select_initial_window(uint32_t *rcv_wnd);

void tcp_notify_user(Sock *sk);

void *tcp_connect_rto(void *arg);

void *tcp_retransmission_timeout(void *arg);

void tcp_rearm_rto_timer(Tcp_Sock *tsk);

int tcp_connect(Sock *sk);

int tcp_send(Tcp_Sock *tsk, const void *buff, const int &len);

int tcp_send_reset(Tcp_Sock *tsk);

int tcp_send_challenge_ack(Sock *sk, SkBuff *skb);

int tcp_queue_fin(Sock *sk);