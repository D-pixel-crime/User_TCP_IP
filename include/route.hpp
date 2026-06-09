#pragma once
#include "intrusive_queue.hpp"
#include "network_device.hpp"

constexpr uint8_t RT_LOOPBACK = 0x01;
constexpr uint8_t RT_GATEWAY = 0x02;
constexpr uint8_t RT_HOST = 0x04;
constexpr uint8_t RT_REJECT = 0x08;
constexpr uint8_t RT_UP = 0x10;

class Rt_Entry
{
public:
    list_head list_node;
    Network_Device *dev;
    uint32_t dest;
    uint32_t gateway;
    uint32_t netmask;
    uint32_t metric;
    uint8_t flags;

    Rt_Entry(uint32_t _dest, uint32_t _gateway, uint32_t _netmask, uint32_t _metric, uint8_t _flags, Network_Device *_dev);

    static size_t getOffset__list_node()
    {
        return offsetof(Rt_Entry, Rt_Entry::list_node);
    }
};

void init_routes();

Rt_Entry *route_lookup(const uint32_t &_destAddr);

/* To be implemented
void free_routes();
 */