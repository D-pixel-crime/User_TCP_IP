#pragma once
#include "socket_buffer.hpp"

class __attribute__((packed)) Eth_Hdr {
public:
  uint8_t dmac[6];
  uint8_t smac[6];
  uint16_t ethertype;
  uint8_t payload[];

  static inline size_t getSize() { return sizeof(Eth_Hdr); }
};

inline Eth_Hdr *ethHdr_from_skb(SkBuff *skb) {
  auto hdr = reinterpret_cast<Eth_Hdr *>(skb);
  if (hdr) {
    hdr->ethertype = ntohs(hdr->ethertype);
  }

  return hdr;
}

template <typename... Args>
inline void
eth_debug(std::string_view str, Eth_Hdr *hdr,
          const std::source_location loc = std::source_location::current()) {
  std::string dmac = std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                                 hdr->dmac[0], hdr->dmac[1], hdr->dmac[2],
                                 hdr->dmac[3], hdr->dmac[4], hdr->dmac[5]);
  std::string smac = std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                                 hdr->smac[0], hdr->smac[1], hdr->smac[2],
                                 hdr->smac[3], hdr->smac[4], hdr->smac[5]);

  std::cout << std::format("Debugging Eth_Hdr!\nDest-MAC: {}\nSourc-MAC: {}\n",
                           dmac, smac)
            << str << std::format(" - {}:{}\n", loc.file_name(), loc.line())
            << std::endl;
}