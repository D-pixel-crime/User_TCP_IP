#include "socket_buffer.hpp"

IntrusiveQueue<SkBuff> sk_buff_queue(SkBuff::getOffset__list_node());

SkBuff::SkBuff(size_t _size) : data(new uint8_t[_size]), refcnt(0)
{
    head = data;
    end = data + _size;
}

SkBuff::~SkBuff()
{
    delete head;
    delete data;
    delete end;
    delete payload;
}

inline uint8_t *SkBuff::get_head()
{
    return head;
}

inline void SkBuff::reserve_headroom(size_t _headroomLen)
{
    data += _headroomLen;
}

inline void SkBuff::push(size_t _dataLenToAdd)
{
    data -= _dataLenToAdd;
    len += _dataLenToAdd;
}

inline void SkBuff::reset_header()
{
    data = end - data_len;
    len = data_len;
}
