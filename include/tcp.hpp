#pragma once
#include "syshead.hpp"
#include "socket_buffer.hpp"
#include "intrusive_queue.hpp"
#include "ethernet.hpp"
#include "ip.hpp"

inline constexpr int TCP_FIN = 0x01;
inline constexpr int TCP_SYN = 0x02;
inline constexpr int TCP_RST = 0x04;
inline constexpr int TCP_PSH = 0x08;
inline constexpr int TCP_ACK = 0x10;

inline constexpr int TCP_URG = 0x20;
inline constexpr int TCP_ECN = 0x40;
inline constexpr int TCP_WIN = 0x80;

inline constexpr int TCP_SYN_BACKOFF = 500;
inline constexpr int TCP_CONN_RESTRIES = 3;

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

    auto tcp_hlen()
    {
        return hl << 2;
    }
};

Tcp_Hdr *tcp_hdr(SkBuff *skb)
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
 * @enum Tcp_States
 * @brief RFC 793 TCP state machine enumeration.
 * @note Explicitly backed by uint8_t for safe serialization and memory efficiency.
 */
enum class Tcp_States : uint8_t
{
    /** @brief Invalid, uninitialized, or unknown state. Used for error handling. */
    UNSUPPORTED = 0,

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
    uint32_t snd_iss;
    uint32_t snd_rcv_nxt;
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
    /*To be implemented
    Sock sk;
    */
    int fd;
    uint16_t tcp_header_len;
    Tcb tcb;
    uint8_t flags;
    uint8_t backoff;
    int32_t srtt;
    int32_t rttvar;
    uint32_t rto;
    /*To be implemented
    Timer *retransmit;
    Timer *delack;
    Timer *keepalive;
    Timer *linger;
    */
    uint8_t delacks;
    uint16_t rmss;
    uint16_t smss;
    uint16_t cwnd;
    uint32_t inflight;

    uint8_t sackok;
    uint8_t sacks_allowed;
    uint8_t sacks_len;
    Tcp_Sack_Block sacks[4];

    uint8_t tsopt;

    IntrusiveQueue<SkBuff> ofo_queue;
};
