#pragma once
#include "syshead.hpp"
#include "utils.hpp"
#include "ethernet.hpp"
#include "network_device.hpp"
#include "socket_buffer.hpp"
#include "intrusive_queue.hpp"

inline constexpr auto ARP_ETHERNET = 0x0001;
inline constexpr auto ARP_IPV4 = 0x0800;
inline constexpr auto ARP_REPLY = 0x0001;
inline constexpr auto ARP_REQUEST = 0x0002;

inline constexpr size_t ARP_CACHE_LEN = 32;
inline constexpr int ARP_FREE = 0;
inline constexpr int ARP_WAITING = 1;
inline constexpr int ARP_RESOLVED = 2;

inline void arp_debug(std::string_view str, const Arp_Hdr *hdr, const std::source_location loc = std::source_location::current())
{
    std::cout << std::format("ARP {} (hwtype: {}, protype: {:04x}, hwsize: {}, prosize: {}, opcode: {:04x}) - {}:{}\n",
                             str,
                             hdr->hwtype,
                             hdr->protype,
                             hdr->hwsize,
                             hdr->prosize,
                             hdr->opcode,
                             loc.file_name(),
                             loc.line())
              << std::flush;
}

inline void arpdata_debug(std::string_view str, const Arp_Ipv4 *data, const std::source_location loc = std::source_location::current())
{
    std::cout << std::format("ARP data {} (smac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, "
                             "sip: {}.{}.{}.{}, dmac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, "
                             "dip: {}.{}.{}.{}) - {}:{}\n",
                             str,
                             data->smac[0], data->smac[1], data->smac[2], data->smac[3], data->smac[4], data->smac[5],
                             (data->sip >> 24) & 0xFF, (data->sip >> 16) & 0xFF, (data->sip >> 8) & 0xFF, data->sip & 0xFF,
                             data->dmac[0], data->dmac[1], data->dmac[2], data->dmac[3], data->dmac[4], data->dmac[5],
                             (data->dip >> 24) & 0xFF, (data->dip >> 16) & 0xFF, (data->dip >> 8) & 0xFF, data->dip & 0xFF,
                             loc.file_name(),
                             loc.line())
              << std::flush;
}

inline void arpcache_debug(std::string_view str, const Arp_Cache_Entry *entry, const std::source_location loc = std::source_location::current())
{
    std::cout << std::format("ARP cache {} (hwtype: {}, sip: {}.{}.{}.{}, "
                             "smac: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, state: {}) - {}:{}\n",
                             str,
                             entry->hwtype,
                             (entry->sip >> 24) & 0xFF, (entry->sip >> 16) & 0xFF, (entry->sip >> 8) & 0xFF, entry->sip & 0xFF,
                             entry->smac[0], entry->smac[1], entry->smac[2], entry->smac[3], entry->smac[4], entry->smac[5],
                             entry->state,
                             loc.file_name(),
                             loc.line())
              << std::flush;
}

class __attribute__((packed)) Arp_Hdr
{
public:
    uint16_t hwtype;
    uint16_t protype;
    size_t hwsize;
    uint8_t prosize;
    uint16_t opcode;
    uint8_t data[];

public:
    static size_t getSize()
    {
        return sizeof(Arp_Hdr);
    }
};

class __attribute__((packed)) Arp_Ipv4
{
public:
    uint8_t smac[6];
    uint32_t sip;
    uint8_t dmac[6];
    uint32_t dip;

public:
    static size_t getSize()
    {
        return sizeof(Arp_Ipv4);
    }
};

class Arp_Cache_Entry
{
public:
    list_head list;
    uint16_t hwtype;
    uint32_t sip;
    uint8_t smac[6];
    uint32_t state;

    Arp_Cache_Entry(const uint16_t &_hwtype, const uint32_t &_sip, const uint8_t *_smac, const uint32_t &_state) : hwtype{_hwtype}, sip{_sip}, state{_state}
    {
        std::memcpy(smac, _smac, 6);
    }

    static size_t getOffset__list_node()
    {
        return offsetof(Arp_Cache_Entry, list);
    }
};

inline Arp_Hdr *arp_hdr(SkBuff *skb)
{
    return reinterpret_cast<Arp_Hdr *>(skb->head + Eth_Hdr::getSize());
}

/* To be implemented
void free_arp();
*/

void arp_rcv(SkBuff *skb);

int arp_request(const uint32_t &sip, const uint32_t &dip, Network_Device *netdev);

void arp_reply(SkBuff *skb, Network_Device *netdev);

uint8_t *arp_get_hwaddr(const uint32_t &sip);