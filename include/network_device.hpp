#pragma once
#include "syshead.hpp"
#include "utils.hpp"

constexpr size_t BUFFLEN = 1600;
constexpr size_t MAX_ADDR_LEN = 32;

/**
 * Prints a formatted debug message specific to network devices with source location information.
 */
template <typename... Args>
inline void netdev_debug(std::format_string<Args...> fmt, Args &&...args, const std::source_location loc = std::source_location::current())
{
    std::cout << std::format(fmt, std::forward<Args>(args)...) << std::format(" - {}:{}\n", loc.file_name(), loc.line()) << std::endl;
}

class NetworkDevice
{
private:
    uint32_t addr;
    uint8_t addr_len;
    std::array<uint8_t, 6> hwaddr; // MAC Address
    uint32_t mtu;

public:
    NetworkDevice(std::string_view _addr, std::string_view _hwaddr, uint32_t _mtu);

    uint32_t getAddr()
    {
        return addr;
    }
};

inline NetworkDevice *loopback = nullptr;
inline NetworkDevice *netdev = nullptr;

void network_device_init();

/* To be implemented
int netdev_transmit(struct sk_buff *skb, uint8_t *dst, uint16_t ethertype);
void *netdev_rx_loop();
void free_netdev();
struct netdev *netdev_get(uint32_t sip);
*/