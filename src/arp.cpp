#include "../include/arp.hpp"
#include "../include/network_device.hpp"
#include "../include/socket_buffer.hpp"
#include "../include/intrusive_queue.hpp"
#include <mutex>

std::mutex arp_cache_mutex;

uint8_t broadcast_hw[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
IntrusiveQueue<Arp_Cache_Entry> arp_cache_entry_queue(Arp_Cache_Entry::getOffset__list_node());

/*To be implemented
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
*/

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

void arp_rcv(SkBuff *skb)
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

    /* To be implemented
    if (!(netdev = netdev_get(arpdata->dip))) {
        printf("ARP was not for us\n");
        goto drop_pkt;
    }
    */

    if (!merge && !insert_arp_translation_table(arphdr, arpdata))
    {
        print_err("ERR: No free space in ARP translation table.");
        free_skb(skb);
        return;
    }

    switch (arphdr->opcode)
    {
    case ARP_REQUEST:
        /* To be implemented:
            arp_reply(skb, netdev);
        */
        return;

    default:
        print_err("ARP: Opcode NOT supported!");
        free_skb(skb);
        return;
    }
}

int arp_request(uint32_t sip, uint32_t dip, NetworkDevice *netdev)
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

    std::memcpy(arpdata->smac, netdev->hwaddr, netdev->addr_len);
    arpdata->sip = sip;

    std::memcpy(arpdata->dmac, broadcast_hw, netdev->addr_len);
    arpdata->dip = dip;

    arphdr = reinterpret_cast<Arp_Hdr *>(skb->push(Arp_Hdr::getSize()));

    arp_debug("Request", arphdr);
    arphdr->opcode = htons(ARP_REQUEST);
    arphdr->hwtype = htons(ARP_ETHERNET);
    arphdr->protype = htons(ETH_P_IP);
    arphdr->hwsize = netdev->addr_len;
    arphdr->prosize = 4;

    arpdata_debug("Request", arpdata);
    arpdata->sip = htons(arpdata->sip);
    arpdata->dip = htons(arpdata->dip);

    /* To be implemented
    rc = netdev_transmit(skb, broadcast_hw, ETH_P_ARP);
    */

    free_skb(skb);
    return rc;
}