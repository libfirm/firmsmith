#ifndef PTR_LIST_H
#define PTR_LIST_H

#include <libfirm/firm.h>
#include <libfirm/adt/list.h>

#define for_each_ptr_lmem(list, iterator) list_for_each_entry(ptr_lmem_t, iterator, (&list->head), head)

struct ptr_list_t;

typedef struct ptr_lmem_t {
    struct list_head head;
    struct ptr_list_t *list;
    void* ptr;
} ptr_lmem_t;

typedef struct ptr_list_t {
    struct list_head head; 
    int count;
} ptr_list_t;


ptr_list_t* new_ptr_list(void);
void ptr_list_add(ptr_list_t *list, void *ptr);
void ptr_lmem_delete(ptr_lmem_t *lmem);
void ptr_lmem_free(ptr_lmem_t *lmem);
int ptr_list_delete(ptr_list_t* list, void *ptr);

#endif
