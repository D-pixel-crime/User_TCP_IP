#include "../include/socket_buffer.hpp"
#include "../include/network_device.hpp"
#include "../include/arp.hpp"
#include "../include/intrusive_queue.hpp"
#include <mutex>

std::mutex arp_cache_mutex;

uint8_t broadcast_hw[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
IntrusiveQueue<Arp_Cache_Entry> arp_cache_entry_queue(Arp_Cache_Entry::getOffset__list_node());

SkBuff *arp_alloc_skb()
{
    SkBuff *skb = new SkBuff(Eth_Hdr::getSize() + Arp_Hdr::getSize() + Arp_Ipv4::getSize());
    skb->reserve_headroom(Eth_Hdr::getSize() + Arp_Hdr::getSize() + Arp_Ipv4::getSize());
    skb->protocol = htons(ETH_P_ARP);

    return skb;
}

int insert_arp_translation_table(Arp_Hdr *hdr, Arp_Ipv4 *data)
{
    Arp_Cache_Entry *new_entry = new Arp_Cache_Entry(hdr->hwtype, data->sip, data->smac, ARP_RESOLVED);

    std::lock_guard<std::mutex> lock(arp_cache_mutex);
    arp_cache_entry_queue.queue_add_tail(&new_entry->list);

    return 0;
}

int update_arp_translation_table(Arp_Hdr *hdr, Arp_Ipv4 *data)
{
    std::lock_guard<std::mutex> lock(arp_cache_mutex);

    int found = 0;
    list_for_each(arp_cache_entry_queue.get_queue_list_head(), [&](list_head *pos)
                  {
        Arp_Cache_Entry *entry = list_entry<Arp_Cache_Entry>(pos, Arp_Cache_Entry::getOffset__list_node());
        if(entry->hwtype == hdr->hwtype && entry->sip == data->sip){
            std::memcpy(entry->smac, data->smac, 6);
            found = 1;
            return true;
        }
        return false; });

    return found;
}

void arp_receive(SkBuff *skb)
{
    Arp_Ipv4 *arpdata;
    NetworkDevice *netdev;
    int merge = 0;

    Arp_Hdr *arphdr = arp_hdr(skb);
    arphdr->hwtype = ntohs(arphdr->hwtype);
    arphdr->protype = ntohs(arphdr->protype);
    arphdr->opcode = ntohs(arphdr->opcode);
    arp_debug("In", arphdr);

    if (arphdr->hwtype != ARP_ETHERNET)
    {
        print_err("ARP: Unsupported protocol");
        free_skb(skb);
        return;
    }

    if (arphdr->protype != ARP_IPV4)
    {
        print_err("ARP: Unsupported protocol");
        free_skb(skb);
        return;
    }

    arpdata = reinterpret_cast<Arp_Ipv4 *>(arphdr->data);
    arpdata->sip = ntohs(arpdata->sip);
    arpdata->dip = ntohs(arpdata->dip);
    arpdata_debug("Receive", arpdata);

    merge = update_arp_translation_table(arphdr, arpdata);

    if (!(netdev = netdev_get(arpdata->dip)))
    {
        std::cout << "ARP was not for us\n";
        free_skb(skb);
        return;
    }

    if (!merge && !insert_arp_translation_table(arphdr, arpdata))
    {
        print_err("ERR: No free space in ARP translation table.");
        free_skb(skb);
        return;
    }

    switch (arphdr->opcode)
    {
    case ARP_REQUEST:
        arp_reply(skb, netdev);
        return;

    default:
        print_err("ARP: Opcode NOT supported!");
        free_skb(skb);
        return;
    }
}

int arp_request(const uint32_t &sip, const uint32_t &dip, NetworkDevice *netdev)
{
    Arp_Hdr *arphdr;
    Arp_Ipv4 *arpdata;
    int rc = 0;

    SkBuff *skb = arp_alloc_skb();
    if (!skb)
    {
        return -1;
    }

    skb->dev = netdev;

    arpdata = reinterpret_cast<Arp_Ipv4 *>(skb->push(Arp_Ipv4::getSize()));

    std::memcpy(arpdata->smac, netdev->hwaddr, sizeof(netdev->hwaddr));
    arpdata->sip = sip;

    std::memcpy(arpdata->dmac, broadcast_hw, sizeof(netdev->hwaddr));
    arpdata->dip = dip;

    arphdr = reinterpret_cast<Arp_Hdr *>(skb->push(Arp_Hdr::getSize()));

    arp_debug("Request", arphdr);
    arphdr->opcode = htons(ARP_REQUEST);
    arphdr->hwtype = htons(ARP_ETHERNET);
    arphdr->protype = htons(ETH_P_IP);
    arphdr->hwsize = sizeof(netdev->hwaddr);
    arphdr->prosize = 4;

    arpdata_debug("Request", arpdata);
    arpdata->sip = htons(arpdata->sip);
    arpdata->dip = htons(arpdata->dip);

    rc = netdev_transmit(skb, broadcast_hw, ETH_P_ARP);

    free_skb(skb);
    return rc;
}

void arp_reply(SkBuff *skb, NetworkDevice *netdev)
{
    Arp_Ipv4 *arpdata;
    Arp_Hdr *arphdr = arp_hdr(skb);

    skb->reserve_headroom(Eth_Hdr::getSize() + Arp_Hdr::getSize() + Arp_Ipv4::getSize());
    skb->push(Arp_Hdr::getSize() + Arp_Ipv4::getSize());

    arpdata = reinterpret_cast<Arp_Ipv4 *>(arphdr->data);

    std::memcpy(arpdata->dmac, arpdata->smac, 6);
    arpdata->dip = arpdata->sip;

    std::memcpy(arpdata->smac, netdev->hwaddr, 6);
    arpdata->sip = netdev->addr;

    arphdr->opcode = ARP_REPLY;

    arp_debug("Reply", arphdr);
    arphdr->opcode = htons(arphdr->opcode);
    arphdr->hwtype = htons(arphdr->hwtype);
    arphdr->protype = htons(arphdr->protype);

    arpdata_debug("Reply", arpdata);
    arpdata->sip = htonl(arpdata->sip);
    arpdata->dip = htonl(arpdata->dip);

    skb->dev = netdev;

    netdev_transmit(skb, arpdata->dmac, ETH_P_ARP);

    free_skb(skb);
}

uint8_t *arp_get_hwaddr(const uint32_t &sip)
{
    uint8_t *hwaddr = nullptr;

    std::lock_guard<std::mutex> lock(arp_cache_mutex);

    list_for_each(arp_cache_entry_queue.get_queue_list_head(), [&](list_head *pos)
                  {
        Arp_Cache_Entry* arp_entry = list_entry<Arp_Cache_Entry>(pos, Arp_Cache_Entry::getOffset__list_node());
        if(arp_entry->state == ARP_RESOLVED && arp_entry->sip == sip){
            arpcache_debug("Entry", arp_entry);

            std::memcpy(hwaddr, arp_entry->smac, 6);
            return;
        } });

    return hwaddr;
}

/*To be implemented
void free_arp()
{
    struct list_head *item, *tmp;
    struct arp_cache_entry *entry;

    list_for_each_safe(item, tmp, &arp_cache) {
        entry = list_entry(item, struct arp_cache_entry, list);
        list_del(item);

        free(entry);
    }
}
*/