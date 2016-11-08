#ifndef PTR_LIST_H
#define PTR_LIST_H

#include <libfirm/firm.h>
#include <libfirm/adt/list.h>

#define for_each_ptr_lmem(list, iterator) list_for_each_entry(ptr_lmem_t, iterator, (&list->head), head)

struct plist_t;

typedef struct ptr_lmem_t {
    struct list_head head;
    struct plist_t *list;
    void* ptr;
} ptr_lmem_t;

typedef struct plist_t {
    struct list_head head; 
    int count;
} plist_t;


plist_t* new_plist(void);
void plist_add(plist_t *list, void *ptr);
void ptr_lmem_delete(ptr_lmem_t *lmem);
void ptr_lmem_free(ptr_lmem_t *lmem);
int plist_delete(plist_t* list, void *ptr);

#endif
