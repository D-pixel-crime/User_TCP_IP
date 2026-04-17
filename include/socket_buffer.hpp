#pragma once
#include "network_device.hpp"
#include "intrusive_queue.hpp"
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstddef>

class SkBuff
{
private:
    list_head list_node;
    NetworkDevice *dev;
    /* I think its better to use 'SkBuff*' using 'shared_ptr', then no need to keep this in check. */
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

public:
    static inline size_t getOffset__list_node()
    {
        return offsetof(SkBuff, SkBuff::list_node);
    }

    SkBuff(size_t _size);

    inline uint8_t *get_head();

    inline void reserve_headroom(size_t _headroomLen);

    inline void push(size_t _dataLenToAdd);

    inline void reset_header();

    ~SkBuff();
};
