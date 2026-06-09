#pragma once
#include "syshead.hpp"
#include "socket_buffer.hpp"
#include "ethernet.hpp"
/* To be implemented
#include "sock.hpp"
*/

inline constexpr int IPV4 = 0x04;
inline constexpr int TCP_IP = 0x06;
inline constexpr int ICMPV4 = 0x01;

class __attribute__((packed)) Ip_Hdr
{
public:
    uint8_t ihl;
    uint8_t version = 4;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t ttl;
    uint8_t proto;
    uint16_t csum;
    uint32_t saddr;
    uint32_t daddr;
    uint8_t data[0];

    static size_t getSize()
    {
        return sizeof(Ip_Hdr);
    }

    uint16_t ip_len()
    {
        return len - (ihl * 4);
    }
};

Ip_Hdr *ip_hdr(SkBuff *skb)
{
    return reinterpret_cast<Ip_Hdr *>(skb);
}

/* To be implemented
int ip_rcv(struct sk_buff *skb);
int ip_output(struct sock *sk, struct sk_buff *skb);
*/