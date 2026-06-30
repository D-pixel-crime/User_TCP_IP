#include "../include/socket_buffer.hpp"

IntrusiveQueue<SkBuff> sk_buff_queue(SkBuff::getOffset__list_node());

SkBuff::SkBuff(size_t _size)
    : refcnt(0), protocol(0), len(0), data_len(0), seq(0), end_seq(0),
      data(new uint8_t[_size]()) {
  head = data;
  end = data + _size;
  payload = nullptr;
}

SkBuff::~SkBuff() { delete[] data; }

inline void SkBuff::reserve_headroom(size_t _headroomLen) {
  data += _headroomLen;
}

inline uint8_t *SkBuff::push(size_t _dataLenToAdd) {
  data -= _dataLenToAdd;
  len += _dataLenToAdd;

  return this->data;
}

inline void SkBuff::reset_header() {
  data = end - data_len;
  len = data_len;
}
