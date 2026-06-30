#pragma once
#include <source_location>
#include <system_headers.hpp> // IWYU pragma: keep

class SkBuff;

inline constexpr size_t BUFFLEN = 1600;
inline constexpr size_t MAX_ADDR_LEN = 32;

/**
 * Prints a formatted debug message specific to network devices with source
 * location information.
 */
template <typename... Args>
inline void
netdev_debug(std::format_string<Args...> fmt, Args &&...args,
             const std::source_location loc = std::source_location::current()) {
  std::cout << std::format(fmt, std::forward<Args>(args)...)
            << std::format(" - {}:{}\n", loc.file_name(), loc.line())
            << std::endl;
}

class Network_Device {
public:
  uint32_t addr;
  size_t addr_len;
  uint8_t hwaddr[6]; // MAC Address
  uint32_t mtu;

  Network_Device(const std::string &_addr, const std::string &_hwaddr,
                 uint32_t _mtu);
};

inline Network_Device *loopback = nullptr;
inline Network_Device *netdev = nullptr;

void network_device_init();

int netdev_transmit(SkBuff *skb, uint8_t *dst_hw, const uint16_t &ethertype);

int netdev_receive(SkBuff *skb);

void *netdev_rx_loop();

Network_Device *netdev_get(const uint32_t &sip);

void free_netdev();