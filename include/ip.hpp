#pragma once
#include "sock.hpp"
#include "socket_buffer.hpp"
#include "system_headers.hpp" // IWYU pragma: keep

inline constexpr int IPV4 = 0x04;
inline constexpr int IP_TCP = 0x06;
inline constexpr int ICMPV4 = 0x01;

class __attribute__((packed)) Ip_Hdr {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
  uint8_t version : 4, ihl : 4;
#else
  uint8_t ihl : 4, version : 4;
#endif
  uint8_t tos;
  uint16_t len;
  uint16_t id;
  uint16_t frag_offset;
  uint8_t ttl;
  uint8_t proto;
  uint16_t csum;
  uint32_t saddr;
  uint32_t daddr;
  uint8_t data[];

  static size_t getSize() { return sizeof(Ip_Hdr); }

  uint16_t ip_len() { return len - (ihl * 4); }
};

inline Ip_Hdr *ip_hdr(SkBuff *skb) { return reinterpret_cast<Ip_Hdr *>(skb); }

int ip_rcv(SkBuff *skb);

int ip_output(Sock *sk, SkBuff *skb);

inline void
ip_debug(std::string_view str, const Ip_Hdr *hdr,
         const std::source_location loc = std::source_location::current()) {
  std::cout << std::format("IP {} (ihl: {} version: {} tos: {} len {} id: {} "
                           "frag_offset: {} ttl: {} proto: {} csum: {:04x} "
                           "saddr: {}.{}.{}.{} daddr: {}.{}.{}.{}) - {}:{}\n",
                           str, hdr->ihl, hdr->version, hdr->tos, hdr->len,
                           hdr->id, hdr->frag_offset, hdr->ttl, hdr->proto,
                           hdr->csum, (hdr->saddr >> 24) & 0xFF,
                           (hdr->saddr >> 16) & 0xFF, (hdr->saddr >> 8) & 0xFF,
                           hdr->saddr & 0xFF, (hdr->daddr >> 24) & 0xFF,
                           (hdr->daddr >> 16) & 0xFF, (hdr->daddr >> 8) & 0xFF,
                           hdr->daddr & 0xFF, loc.file_name(), loc.line())
            << std::flush;
}