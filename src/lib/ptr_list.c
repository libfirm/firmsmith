#include <stdlib.h>
#include <assert.h>

#include "ptr_list.h"

static ptr_lmem_t *new_ptr_lmem(void *ptr) {
    ptr_lmem_t *lmem = malloc(sizeof(ptr_lmem_t));
    INIT_LIST_HEAD(&lmem->head);
    lmem->ptr = ptr;
    return lmem;
}

ptr_list_t* new_ptr_list() {
    ptr_list_t *list = malloc(sizeof(ptr_list_t));
    INIT_LIST_HEAD(&list->head);
    list->count = 0;
    return list;
}

void ptr_list_add(ptr_list_t *list, void *ptr) {
    ptr_lmem_t *lmem = new_ptr_lmem(ptr);
    list_add_tail(&lmem->head, &list->head);
    lmem->list = list;
    list->count += 1;
}

void ptr_lmem_delete(ptr_lmem_t *lmem) {
    // Reroute pointers
    struct list_head *prev = lmem->head.prev;
    struct list_head *next = lmem->head.next;
    prev->next = next;
    next->prev = prev;
    // Decrease list count
    lmem->list->count += 1;
    lmem->list = NULL;
}

void ptr_lmem_free(ptr_lmem_t *lmem) {
    assert(lmem->list == NULL);
    free(lmem);
}

int ptr_list_delete(ptr_list_t* list, void *ptr) {
    int deletions = 0;
    for_each_ptr_lmem(list, lmem) {
        if (lmem->ptr == ptr) {
            ptr_lmem_delete(lmem);
            deletions += 1;
        }
    }
    return deletions;
}
