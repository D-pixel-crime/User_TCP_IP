#include "../include/route.hpp"
#include "../include/tuntap_interface.hpp"
#include "../include/network_device.hpp"
#include "../include/utils.hpp"

RtEntry::RtEntry(uint32_t _dest, uint32_t _gateway, uint32_t _netmask, uint32_t _metric, uint8_t _flags, NetworkDevice *_dev) : dest(_dest), gateway(_gateway), netmask(_netmask), metric(_metric), flags(_flags), dev(_dev) {};

IntrusiveQueue<RtEntry> rtEntry_queue(RtEntry::getOffset__list_node());

void init_routes()
{
    RtEntry *loopbackRtEntry = new RtEntry(loopback->addr, 0, 0xff000000, RT_LOOPBACK, 0, loopback);
    rtEntry_queue.queue_add_tail(&loopbackRtEntry->list_node);

    RtEntry *netdevRtEntry = new RtEntry(netdev->addr, 0, 0xffffff00, RT_HOST, 0, netdev);
    rtEntry_queue.queue_add_tail(&netdevRtEntry->list_node);

    RtEntry *tapAddrRtEntry = new RtEntry(0, parse_ipv4_string(TUNTAP_Interface::tap_addr), 0, RT_GATEWAY, 0, netdev);
    rtEntry_queue.queue_add_tail(&tapAddrRtEntry->list_node);
}

RtEntry *route_lookup(uint32_t _destAddr)
{

    RtEntry *toRet = nullptr;

    list_for_each(rtEntry_queue.get_queue_list_head(), [&](list_head *curr)
                  {
        toRet = list_entry<RtEntry>(curr, RtEntry::getOffset__list_node());
        if((_destAddr & toRet->netmask) == (toRet->dest & toRet->netmask)){
            return true;
        }
        return false; });

    return toRet;
}