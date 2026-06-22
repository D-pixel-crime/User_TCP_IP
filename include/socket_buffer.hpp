#pragma once
#include "network_device.hpp"
#include "intrusive_queue.hpp"
#include "route.hpp"
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstddef>

class SkBuff
{
public:
    list_head list_node;
    Rt_Entry *rt;
    Network_Device *dev;
    /*
       Rather than counting 'refcnt',
       I think its better to use 'SkBuff*' using 'shared_ptr', then no need to keep this in check.
    */
    std::atomic<int> refcnt;
    /****************************************************/
    uint16_t protocol;
    uint32_t len; // Includes headers + data
    uint32_t data_len;
    uint32_t seq;     // Sequence number of the packet
    uint32_t end_seq; // seq + data_len
    uint8_t *data;
    uint8_t *head;
    uint8_t *end;
    uint8_t *payload;

    SkBuff(size_t _size);

    static inline size_t getOffset__list_node()
    {
        return offsetof(SkBuff, SkBuff::list_node);
    }

    void reserve_headroom(size_t _headroomLen);

    uint8_t *push(size_t _dataLenToAdd);

    void reset_header();

    ~SkBuff();
};

void free_skb(SkBuff *skb)
{
    if (skb->refcnt <= 1)
    {
        delete skb;
        return;
    }
    skb->refcnt--;
}

inline void skb_queue_free(IntrusiveQueue<SkBuff> *queue)
{
    queue->clear_queue([&](SkBuff *skb)
                       {
        skb->refcnt--;
        free_skb(skb); });
}