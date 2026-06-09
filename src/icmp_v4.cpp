#include "../include/icmp_v4.hpp"
#include "../include/socket_buffer.hpp"
#include "../include/ip.hpp"
#include "../include/ethernet.hpp"

class Sock;

void icmpv4_incoming(SkBuff *skb)
{
    /* To be implemented
    struct iphdr *iphdr = ip_hdr(skb);
    */
    Ip_Hdr *iphdr;

    ICMPv4 *icmp = reinterpret_cast<ICMPv4 *>(iphdr);

    /*To be implemented
    Check Csum
    */

    switch (icmp->type)
    {
    case ICMP_V4_ECHO:
        icmpv4_reply(skb);
        return;

    case ICMP_V4_DEST_UNREACHABLE:
        print_err("ICMPv4: Destination Unreachable code-{}.\n Please check your routes and firewall rules.", icmp->code);
        free_skb(skb);
        return;

    default:
        print_err("ICMPv4: Unsupported type-{}.", icmp->type);
        free_skb(skb);
        return;
    }
}

void icmpv4_reply(SkBuff *skb)
{
    /* To be implemented
    struct iphdr *iphdr = ip_hdr(skb);
    */
    Ip_Hdr *iphdr;

    /*To be implemented
    struct sock sk;
    memset(&sk, 0, sizeof(struct sock));
    */
    Sock *sk;

    uint16_t icmp_len = iphdr->ip_len();

    skb->reserve_headroom(Eth_Hdr::getSize() + Ip_Hdr::getSize() + icmp_len);
    skb->push(icmp_len);

    ICMPv4 *icmp = reinterpret_cast<ICMPv4 *>(iphdr);

    icmp->type = ICMP_V4_REPLY;
    icmp->csum = 0;
    icmp->csum = checksum(icmp, icmp_len, 0);

    skb->protocol = ICMPV4;
    /*To be implemented
    sk.daddr = ipdhdr->saddr;
    */

    /*To be implemented
    ip_output(&sk, skb);
    */

    free_skb(skb);
}