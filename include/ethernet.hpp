#pragma once
#include "socket_buffer.hpp"
#include "syshead.hpp"
#include <array>
#include <vector>

class __attribute__((packed)) Eth_Hdr
{
private:
    std::array<uint8_t, 6> dest_mac;
    std::array<uint8_t, 6> src_mac;
    uint16_t ethertype;
    /* flexible array member: use size 0 for C++ compatibility */
    uint8_t payload[0];

public:
    static inline size_t getSize()
    {
        return sizeof(Eth_Hdr);
    }

    static inline Eth_Hdr *ethHdr_from_skb(SkBuff *skb)
    {
        auto hdr = reinterpret_cast<Eth_Hdr *>(skb);
        if (hdr)
        {
            hdr->ethertype = ntohs(hdr->ethertype);
        }

        return hdr;
    }

    inline uint16_t getEthertype()
    {
        return ethertype;
    }

    inline uint8_t *getPayload()
    {
        return payload;
    }

    template <typename... Args>
    inline void eth_debug(std::format_string<Args...> fmt, Args &&...args, const std::source_location loc = std::source_location::current())
    {
        std::string dmac = std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                                       dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);
        std::string smac = std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                                       src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);

        std::cout << std::format("Debugging Eth_Hdr!\nDest-MAC: {}\nSourc-MAC: {}\n", dmac, smac) << std::format(fmt, std::forward<Args>(args)...) << std::format(" - {}:{}\n", loc.file_name(), loc.line()) << std::endl;
    }
};