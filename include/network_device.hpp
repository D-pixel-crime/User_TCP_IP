#pragma once
#include "syshead.hpp"
#include "utils.hpp"

constexpr size_t BUFFLEN = 1600;
constexpr size_t MAX_ADDR_LEN = 32;

/**
 * Prints a formatted debug message specific to network devices with source location information.
 */
template <typename... Args>
inline void netdev_debug(std::format_string<Args...> fmt, Args &&...args)
{
    const std::source_location loc = std::source_location::current();
    std::cout << std::format(fmt, std::forward<Args>(args)...) << std::format(" - {}:{}\n", loc.file_name(), loc.line()) << std::endl;
}

class NetworkDevice
{
public:
    uint32_t addr;
    uint8_t addr_len;
    std::array<uint8_t, 6> hwaddr; // MAC Address
    uint32_t mtu;

    NetworkDevice(std::string_view _addr, std::string_view _hwaddr, uint32_t _mtu);
};

NetworkDevice *loopback;
NetworkDevice *netdev;