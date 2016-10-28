#ifndef CF_BB_H
#define CF_BB_H

#include <libfirm/adt/list.h>
#include <libfirm/firm.h>

#define MAX_PREDECESSORS 10
#define MAX_SUCCESSORS 2

#define cfb_for_each_successor(cfb, iterator) \
    list_for_each_entry(cfb_lmem_t, iterator, (&cfb->successors), head)

#define cfb_get_successor(__ignored, iterator) (iterator->cfb)

#define cfb_lmem_first_predecessor(cfb) \
    list_entry((cfb->predecessors).next, cfb_lmem_t, head)

#define cfb_first_predecessor(cfb) \
    (cfb_lmem_first_predecessor(cfb))->cfb

#define cfb_lmem_first_successor(cfb) \
    list_entry((cfb->successors).next, cfb_lmem_t, head)

#define cfb_lmem_last_successor(cfb) \
    list_entry((cfb->successors).prev, cfb_lmem_t, head)

#define cfb_for_each_predecessor(cfb, iterator) \
    list_for_each_entry(cfb_lmem_t, iterator, (&cfb->predecessors), head)

#define cfb_get_predecessor(__ignored, iterator) (iterator->cfb)

#define cfb_for_each_temp(cfb, iterator) \
    list_for_each_entry(temporary_t, iterator, (&cfb->temporaries), head)

#define cfb_get_temp(__ignored, iterator) (iterator->cfb)

/**
 * Control flow block for modelling control flow.
 */
typedef struct cfb_t {
    int index;
    int loop;
    int n_nodes;
    ir_node *irb;                          /**< ir_node block for construction */
    int n_predecessors;
    int n_successors;
    int n_temporaries;
    struct list_head predecessors; /**< predecessors */
    struct list_head successors;   /**< successors */
    struct list_head temporaries;
    int visited;
} cfb_t;

typedef enum temp_type {
    TEMPORARY_BOOLEAN,
    TEMPORARY_NUMBER,
    TEMPORARY_POINTER
} temp_type;

typedef struct temporary_t {
    struct list_head head;
    int resolved;
    temp_type type;
    ir_node *node;
} temporary_t;

typedef struct cfb_lmem_t {
    struct list_head head;
    cfb_t *cfb;
} cfb_lmem_t;

typedef void (*cfb_walker_func)(cfb_t*);

cfb_t *new_cfb(void);
temporary_t *new_temporary(ir_node *temp, temp_type type);

void cfb_add_temporary(cfb_t *cfb, ir_node *temp, temp_type type);
void cfb_add_succ(cfb_t *cfb, cfb_t *succ);
void cfb_print(cfb_t* block);

void cfb_transform_T1(cfb_t *block);
cfb_t *cfb_transform_T2a(cfb_t *cfb);
cfb_t *cfb_transform_T2b(cfb_t *cfb);
cfb_t *cfb_transform_T2c(cfb_t *cfb);

void healthy_predecessors(cfb_t *cfb);
void cfb_walk_predecessors(cfb_t *node, cfb_walker_func pre, cfb_walker_func post, cfb_walker_func visited);
void cfb_walk_successors(cfb_t *node, cfb_walker_func pre, cfb_walker_func post, cfb_walker_func visited);

int get_visit_counter(void);
void inc_visit_counter(void);

#endif
