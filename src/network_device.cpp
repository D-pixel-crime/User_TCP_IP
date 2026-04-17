#include "network_device.hpp"

void network_device_init()
{
    loopback = new NetworkDevice("127.0.0.1", "00:00:00:00:00:00", 1500);
    netdev = new NetworkDevice("10.0.0.10", "00:0c:29:6d:50:25", 1500);
}