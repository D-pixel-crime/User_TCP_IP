#include "network_device.hpp"

NetworkDevice::NetworkDevice(std::string_view _addr, std::string_view _hwaddr, uint32_t _mtu) : mtu(mtu)
{
    addr = parse_ipv4_string(_addr);
    addr_len = sizeof(addr);
    hwaddr = parse_MAC_string(_hwaddr);
}

void network_device_init()
{
    loopback = new NetworkDevice("127.0.0.1", "00:00:00:00:00:00", 1500);
    netdev = new NetworkDevice("10.0.0.10", "00:0c:29:6d:50:25", 1500);
}