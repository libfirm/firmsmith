#ifndef UTILS_H
#define UTILS_H

static void _list_del(struct list_head* head) {
    struct list_head *prev = head->prev;
    struct list_head *next = head->next;
    prev->next = next;
    next->prev = prev;
}

#endif
