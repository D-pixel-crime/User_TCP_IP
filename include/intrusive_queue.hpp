#pragma once
#include <cstdint>
#include <cstddef>

struct list_head
{
    list_head *prev;
    list_head *next;
};

template <typename T>
static inline T *list_entry(list_head *ptr, size_t offset)
{
    return reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(ptr) - offset);
}

template <typename FUNC>
static inline void list_for_each(list_head *head, FUNC &&f)
{
    for (list_head *pos = head->next; pos != head; pos = pos->next)
    {
        f(pos);
    }
}

template <typename FUNC>
static inline void list_for_each_safe(list_head *head, FUNC &&f)
{
    for (list_head *pos = head->next, *p = pos->next; pos != head; pos = p, p = pos->next)
    {
        f(pos);
    }
}

template <typename T>
class IntrusiveQueue
{
private:
    list_head head;
    size_t len;
    size_t offset;

public:
    IntrusiveQueue(size_t _offset) : len(0), offset(_offset)
    {
        head.next = &head;
        head.prev = &head;
    };

    inline bool isEmpty() const
    {
        return &head == head.next;
    }

    inline int size() const
    {
        return len;
    }

    inline T *queue_first_entry() const
    {
        if (isEmpty())
        {
            return nullptr;
        }
        return list_entry<T>(head.next, offset);
    }

    inline void queue_add(list_head *newListHead)
    {
        (head.next)->prev = newListHead;
        newListHead->prev = &head;
        newListHead->next = (head.next);
        (head.next) = newListHead;
        len++;
    }

    inline void queue_add_tail(list_head *newListTail)
    {
        (head.prev)->next = newListTail;
        newListTail->next = &head;
        newListTail->prev = (head.prev);
        (head.prev) = newListTail;
        len++;
    }

    inline void queue_del(list_head *toDel)
    {
        auto prev = toDel->prev;
        auto nxt = toDel->next;

        prev->next = nxt;
        nxt->prev = prev;
        len--;
    }
};