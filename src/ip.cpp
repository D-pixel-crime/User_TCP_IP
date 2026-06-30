#include "../include/ip.hpp"
#include "../include/dst.hpp"
#include "../include/icmp_v4.hpp"
#include "../include/route.hpp"
#include "../include/socket_buffer.hpp"
#include "../include/tcp_data.hpp" // IWYU pragma: keep

int ip_rcv(SkBuff *skb) {
  Ip_Hdr *iphdr = ip_hdr(skb);

  if (iphdr->version != IPV4) {
    print_err("ERR(ip_rcv): Datagram version was not IPv4.");
    free_skb(skb);
    return 0;
  }

  if (iphdr->ihl < 5) {
    print_err("ERR(ip_rcv): IPv4 header length must be atleast 5 bytes.");
    free_skb(skb);
    return 0;
  }

  if (iphdr->ttl == 0) {
    /*To be implemented
    Instead send an ICMP error
    */
    print_err("ERR(ip_rcv): TTL(Time To Live) to the packet has expired.");
    free_skb(skb);
    return 0;
  }

  uint16_t csum = checksum(iphdr, iphdr->ihl * 4, 0);
  if (csum != 0) {
    print_err("ERR(ip_rcv): Invalid Checksum.");
    free_skb(skb);
    return 0;
  }

  /* To be implemented
  Check fragmentation and for possible reassembly
  */

  iphdr->saddr = ntohl(iphdr->saddr);
  iphdr->daddr = ntohl(iphdr->daddr);
  iphdr->len = ntohs(iphdr->len);
  iphdr->id = ntohs(iphdr->id);

  ip_debug("IP-In", iphdr);

  switch (iphdr->proto) {
  case ICMPV4:
    icmpv4_incoming(skb);
    return 0;

  case IP_TCP:
    tcp_in(skb);
    return 0;

  default:
    print_err("ERR(ip_rcv): Unsupported IPv4 Header Protocol.");
  }

  free_skb(skb);
  return 0;
}

int ip_output(Sock *sk, SkBuff *skb) {
  Ip_Hdr *iphdr = ip_hdr(skb);

  Rt_Entry *rt = route_lookup(sk->daddr);

  if (!rt) {
    print_err("ERR(ip_output): IP-Route lookup failed.");
    free_skb(skb);
    return 0;
  }

  skb->dev = rt->dev;
  skb->rt = rt;

  skb->push(Ip_Hdr::getSize());

  iphdr->ihl = 0x05;
  iphdr->version = IPV4;
  iphdr->tos = 0;
  iphdr->len = skb->len;
  iphdr->frag_offset = 0x4000; // Do not fragment flag
  iphdr->ttl = 64; // Linux Default, 255 for Cisco router default, 128 for
                   // Windows default
  iphdr->proto = skb->protocol;

  ip_debug("IP-Out", iphdr);

  iphdr->len = htons(iphdr->len);
  iphdr->id = htons(iphdr->id);
  iphdr->saddr = htonl(iphdr->saddr);
  iphdr->daddr = htonl(iphdr->daddr);
  iphdr->csum = htons(iphdr->csum);
  iphdr->frag_offset = htons(iphdr->frag_offset);

  iphdr->csum = checksum(iphdr, iphdr->len * 4, 0);

  return dst_neigh_output(skb);
}