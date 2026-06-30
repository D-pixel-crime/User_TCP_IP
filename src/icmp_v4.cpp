#include "../include/icmp_v4.hpp"
#include "../include/ethernet.hpp"
#include "../include/ip.hpp"
#include "../include/sock.hpp"
#include "../include/socket_buffer.hpp"
#include "../include/utils.hpp"

void icmpv4_incoming(SkBuff *skb) {
  Ip_Hdr *iphdr = ip_hdr(skb);

  ICMPv4 *icmp = reinterpret_cast<ICMPv4 *>(iphdr);

  /*To be implemented
  Check Csum
  */

  switch (icmp->type) {
  case ICMP_V4_ECHO:
    icmpv4_reply(skb);
    return;

  case ICMP_V4_DEST_UNREACHABLE:
    print_err("ERR(icmpv4_incoming): Destination Unreachable code-{}.\n Please "
              "check your routes and firewall rules.",
              icmp->code);
    free_skb(skb);
    return;

  default:
    print_err("ERR(icmpv4_incoming): Unsupported type-{}.", icmp->type);
    free_skb(skb);
    return;
  }
}

void icmpv4_reply(SkBuff *skb) {
  Ip_Hdr *iphdr = ip_hdr(skb);

  Sock *sk = new Sock();

  uint16_t icmp_len = iphdr->ip_len();

  skb->reserve_headroom(Eth_Hdr::getSize() + Ip_Hdr::getSize() + icmp_len);
  skb->push(icmp_len);

  ICMPv4 *icmp = reinterpret_cast<ICMPv4 *>(iphdr);

  icmp->type = ICMP_V4_REPLY;
  icmp->csum = 0;
  icmp->csum = checksum(icmp, icmp_len, 0);

  skb->protocol = ICMPV4;

  sk->daddr = iphdr->saddr;

  ip_output(sk, skb);

  free_skb(skb);
}