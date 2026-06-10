#pragma once
#include "network_device.hpp"
#include "socket_buffer.hpp"
#include "arp.hpp"
#include "ip.hpp"
#include "route.hpp"

int dst_neigh_output(SkBuff *skb)
{
    Ip_Hdr *iphdr = ip_hdr(skb);
    Network_Device *dev = skb->dev;
    Rt_Entry *rt = skb->rt;
    uint32_t saddr = ntohl(iphdr->saddr);
    uint32_t daddr = ntohl(iphdr->daddr);

    if (rt->flags & RT_GATEWAY)
    {
        daddr = rt->gateway;
    }

    uint8_t *dmac = arp_get_hwaddr(daddr);
    if (dmac)
    {
        return netdev_transmit(skb, dmac, ETH_P_IP);
    }

    arp_request(saddr, daddr, dev);
    return 0;
}