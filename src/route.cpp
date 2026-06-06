#include "../include/route.hpp"

RtEntry::RtEntry(uint32_t _dest, uint32_t _gateway, uint32_t _netmask, uint32_t _metric, uint8_t _flags, NetworkDevice *_dev) : dest(_dest), gateway(_gateway), netmask(_netmask), metric(_metric), flags(_flags), dev(_dev) {};

IntrusiveQueue<RtEntry> rtEntry_queue(RtEntry::getOffset__list_node());

void init_routes()
{
    RtEntry *loopbackRtEntry = new RtEntry(loopback->addr, 0, 0xff000000, RT_LOOPBACK, 0, loopback);
    rtEntry_queue.queue_add(loopbackRtEntry->getAddr__list_node());

    RtEntry *netdevRtEntry = new RtEntry(netdev->addr, 0, 0xffffff00, RT_HOST, 0, netdev);
    rtEntry_queue.queue_add(netdevRtEntry->getAddr__list_node());

    /*To be implemented
    route_add(0, ip_parse(tapaddr), 0, RT_GATEWAY, 0, netdev);
    */
}

RtEntry *route_lookup(uint32_t _destAddr)
{
    list_head *head = (rtEntry_queue.queue_first_entry())->getAddr__list_node();

    RtEntry *toRet = nullptr;

    list_for_each(head, [&](list_head *curr)
                  {
        toRet = list_entry<RtEntry>(curr, RtEntry::getOffset__list_node());
        if((_destAddr & toRet->getNetmask()) == (toRet->getDest() & toRet->getNetmask())){
            return true;
        }
        return false; });

    return toRet;
}