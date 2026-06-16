#pragma once
#include <cstdint>
#include <cstddef>

struct list_head
{
    list_head *prev{this};
    list_head *next{this};
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
        if (f(pos))
        {
            break;
        }
    }
}

template <typename FUNC>
static inline void list_for_each_safe(list_head *head, FUNC &&f)
{
    for (list_head *pos = head->next, *p = pos->next; pos != head; pos = p, p = pos->next)
    {
        if (f(pos))
        {
            break;
        }
    }
}

template <typename T>
class IntrusiveQueue
{
private:
    list_head head;
    size_t len;
    size_t offset;

    void __list_add(list_head *_new, list_head *prev, list_head *next)
    {
        next->prev = _new;
        _new->next = next;
        _new->prev = prev;
        prev->next = _new;
        len++;
    }

public:
    IntrusiveQueue(size_t _offset) : len(0), offset(_offset)
    {
        head.next = &head;
        head.prev = &head;
    };

    list_head *get_queue_list_head()
    {
        return &head;
    }

    bool isEmpty() const
    {
        return &head == head.next;
    }

    int size() const
    {
        return len;
    }

    T *queue_first_entry() const
    {
        if (isEmpty())
        {
            return nullptr;
        }
        return list_entry<T>(head.next, offset);
    }

    void queue_add(list_head *_newListHead)
    {
        __list_add(_newListHead, &head, head.next);
    }

    void queue_add_tail(list_head *_newListTail)
    {
        __list_add(_newListTail, head.prev, &head);
    }

    void queue_add_before(list_head *_newNode, list_head *_targetNode)
    {
        __list_add(_newNode, _targetNode->prev, _targetNode);
    }

    void queue_del(list_head *_toDel)
    {
        auto prev = _toDel->prev;
        auto nxt = _toDel->next;

        prev->next = nxt;
        nxt->prev = prev;
        _toDel->prev = _toDel->next = nullptr;
        len--;
    }

    T *dequeue()
    {
        T *toRet = queue_first_entry();
        if (toRet)
        {
            queue_del(head.next);
        }

        return toRet;
    }

    template <typename CLEANER>
    void clear_queue(CLEANER &&cleaner)
    {
        while (T *entry = dequeue())
        {
            cleaner(entry);
        }
    }
};