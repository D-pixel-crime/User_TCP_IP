#include "../include/route.hpp"
#include "../include/tuntap_interface.hpp"
#include "../include/network_device.hpp"
#include "../include/utils.hpp"

Rt_Entry::Rt_Entry(uint32_t _dest, uint32_t _gateway, uint32_t _netmask, uint32_t _metric, uint8_t _flags, Network_Device *_dev) : dest(_dest), gateway(_gateway), netmask(_netmask), metric(_metric), flags(_flags), dev(_dev) {};

IntrusiveQueue<Rt_Entry> rtEntry_queue(Rt_Entry::getOffset__list_node());

void init_routes()
{
    Rt_Entry *loopbackRt_Entry = new Rt_Entry(loopback->addr, 0, 0xff000000, RT_LOOPBACK, 0, loopback);
    rtEntry_queue.queue_add_tail(&loopbackRt_Entry->list_node);

    Rt_Entry *netdevRt_Entry = new Rt_Entry(netdev->addr, 0, 0xffffff00, RT_HOST, 0, netdev);
    rtEntry_queue.queue_add_tail(&netdevRt_Entry->list_node);

    Rt_Entry *tapAddrRt_Entry = new Rt_Entry(0, parse_ipv4_string(TUNTAP_Interface::tap_addr), 0, RT_GATEWAY, 0, netdev);
    rtEntry_queue.queue_add_tail(&tapAddrRt_Entry->list_node);
}

Rt_Entry *route_lookup(const uint32_t &_destAddr)
{

    Rt_Entry *toRet = nullptr;

    list_for_each(rtEntry_queue.get_queue_list_head(), [&](list_head *curr)
                  {
        toRet = list_entry<Rt_Entry>(curr, Rt_Entry::getOffset__list_node());
        if((_destAddr & toRet->netmask) == (toRet->dest & toRet->netmask)){
            return true;
        }
        return false; });

    return toRet;
}

void free_routes()
{
    rtEntry_queue.clear_queue([](Rt_Entry *entry)
                              { delete entry; });
}