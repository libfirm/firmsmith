#include "cfb.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

// Functions

static cfb_lmem_t *new_cfb_lmem(cfb_t *cfb) {
    assert(cfb != NULL);
    cfb_lmem_t *lmem = malloc(sizeof(cfb_lmem_t));
    INIT_LIST_HEAD(&lmem->head);
    lmem->cfb = cfb;
    return lmem;
}

temporary_t *new_temporary(ir_node *node, temp_type type) {
    temporary_t *temp = malloc(sizeof(temporary_t));
    INIT_LIST_HEAD(&temp->head);
    temp->node = node;
    temp->type = type;
    temp->resolved = 0;
    return temp;
}

void cfb_add_temporary(cfb_t *cfb, ir_node *node, temp_type type) {
    ;//printf("adding %s (%ld) to irb %ld (%d temps)\n",  get_irn_opname(node), get_irn_node_nr(node), get_irn_node_nr(cfb->irb), cfb->n_temporaries);
    temporary_t *temp = new_temporary(node, type);
    list_add_tail(&temp->head, &cfb->temporaries);
    cfb->n_temporaries += 1;
    cfb->n_nodes += 1;
}

static void cfb_list_del(struct list_head* head) {
    struct list_head *prev = head->prev;
    struct list_head *next = head->next;
    prev->next = next;
    next->prev = prev;
}

static void cfb_del_succ(cfb_t *cfb, cfb_lmem_t *lmem) {
    int dels = 0;
    cfb_for_each_predecessor(lmem->cfb, pred_lmem) {
        if (pred_lmem && pred_lmem->cfb == cfb) {
            // Remove predecessor edge
            lmem->cfb->n_predecessors--;
            cfb_list_del(&pred_lmem->head);
            dels++;
        }
    }
    // Remove successor edge
    cfb->n_successors--;
    cfb_list_del(&lmem->head);
    assert(dels == 1);
}


void healthy_predecessors(cfb_t *cfb) {
    cfb_for_each_predecessor(cfb, lmem) {
        assert(lmem != NULL);
        assert((lmem->head).next != NULL);
        assert((lmem->head).prev != NULL);
        assert(lmem->cfb != NULL);
        healthy_predecessors(lmem->cfb);
    }
}

cfb_t *new_cfb() {
    cfb_t *new_block = malloc(sizeof(cfb_t));
    assert(new_block != 0);
    memset(new_block, 0, sizeof(cfb_t));
    new_block->index = -1;
    new_block->n_successors = 0;
    new_block->n_predecessors = 0;
    new_block->n_temporaries = 0;
    new_block->visited = 0;
    INIT_LIST_HEAD(&new_block->predecessors);
    INIT_LIST_HEAD(&new_block->successors);
    INIT_LIST_HEAD(&new_block->temporaries);
    return new_block;
}

static void cfb_add_pred(cfb_t *cfb, cfb_t *pred) {
    cfb_lmem_t *pred_lmem = new_cfb_lmem(pred);
    list_add_tail(&pred_lmem->head, &cfb->predecessors);
    cfb->n_predecessors++;
}

void cfb_add_succ(cfb_t *cfb, cfb_t *succ) {
    assert(cfb->n_successors + 1 <= MAX_SUCCESSORS);
    cfb->n_successors++;
    cfb_lmem_t *succ_lmem = new_cfb_lmem(succ);
    list_add_tail(&succ_lmem->head, &cfb->successors);
    cfb_add_pred(succ, cfb);
}

void cfb_transform_T1(cfb_t *cfb) {
    /* Introduce loop */
    //assert(cfb->loop == 0);
    cfb_add_succ(cfb, cfb);
    //cfb->loop = 1;
}

cfb_t *cfb_transform_T2a(cfb_t *cfb) {
    assert(cfb->n_successors <= 2);
    cfb_t *new_block = new_cfb();
    int i = 0;
    cfb_for_each_successor(cfb, lmem) {
        ;//printf("%d\t%p\t%d\n", i, lmem->cfb, lmem->cfb->index);
        i += 1;
        (void)i;
        /* Remove original edge to successor */
        cfb_del_succ(cfb, lmem);
        /* Add edge to new block */
        cfb_add_succ(new_block, lmem->cfb);
        free(lmem);
    }
    cfb_add_succ(cfb, new_block);
    return new_block;
}

cfb_t *cfb_transform_T2b(cfb_t *cfb) {
    assert(cfb->n_successors > 0 && cfb->n_successors <= 2);
    cfb_t *new_block = new_cfb();
    if (cfb->n_successors == 1) {
        cfb_lmem_t *lmem_first = cfb_lmem_first_successor(cfb);
        cfb_t *succ = lmem_first->cfb;
        cfb_add_succ(cfb, new_block);
        cfb_add_succ(new_block, succ);
    } else if (cfb->n_successors == 2) {
        cfb_lmem_t *lmem_first = cfb_lmem_first_successor(cfb);
        cfb_t *succ = lmem_first->cfb;
        // Add edge from new block
        cfb_add_succ(new_block, succ);
        // Delete edge from original block
        cfb_del_succ(cfb, lmem_first);
        free(lmem_first);
        cfb_add_succ(cfb, new_block);
    }
    return new_block;
}

cfb_t *cfb_transform_T2c(cfb_t *cfb) {
    cfb_t *new_block = new_cfb();
    cfb_add_succ(cfb, new_block);
    return new_block;
}

// Visit counter

static int __visit_counter = 0;

int get_visit_counter(void) {
    return __visit_counter;
}

void inc_visit_counter(void) {
    __visit_counter++;
}

// Print cfb tree

static int print_indent = 0;

static void cfb_print_pre(cfb_t *block) {
    for (int i = 0; i < print_indent; ++i) {
        printf(" ");
    }
    printf("%d [%d P; %d S]\n", block->index, block->n_predecessors, block->n_successors);
    print_indent += 1;
}

static void cfb_print_visited(cfb_t *block) {
    for (int i = 0; i < print_indent; ++i) {
        printf("._");
    }
    printf("%d [%d P; %d S] [loop]\n", block->index, block->n_predecessors, block->n_successors);
}

static void cfb_print_post(cfb_t *block) {
    (void)block;
    print_indent -= 1;
}

void cfb_print(cfb_t *cfb) {
    cfb_walk_successors(cfb, cfb_print_pre, cfb_print_post, cfb_print_visited);
    printf("\n");
}

// Walk predecessor

static void _cfb_walk_predecessors(cfb_t *node, cfb_walker_func pre, cfb_walker_func post, cfb_walker_func visited) {
    if (node->visited >= get_visit_counter()) {
        if (visited) {
            visited(node);
        }
    } else {
        node->visited = get_visit_counter();

        // Pre iteration hook
        if (pre) {
            pre(node);
        }

        cfb_for_each_predecessor(node, iterator) {
            cfb_t *predecessor = cfb_get_predecessor(node, iterator);
            _cfb_walk_predecessors(predecessor, pre, post, visited);
        }

        // Post iteration hook
        if (post) {
            post(node);
        }
    }
}

void cfb_walk_predecessors(cfb_t *node, cfb_walker_func pre, cfb_walker_func post, cfb_walker_func visited) {
    inc_visit_counter();
    _cfb_walk_predecessors(node, pre, post, visited);
}

// Walk successor

static void _cfb_walk_successors(cfb_t *node, cfb_walker_func pre, cfb_walker_func post, cfb_walker_func visited) {
    if (node->visited >= get_visit_counter()) {
        if (visited) {
            visited(node);
        }
    } else {
        // Pre iteration hook
        if (pre) {
            pre(node);
        }

        node->visited = get_visit_counter();

        cfb_for_each_successor(node, iterator) {
            cfb_t *successor = cfb_get_successor(node, iterator);
            _cfb_walk_successors(successor, pre, post, visited);
        }

        // Post iteration hook
        if (post) {
            post(node);
        }
    }
}

void cfb_walk_successors(cfb_t *node, cfb_walker_func pre, cfb_walker_func post, cfb_walker_func visited) {
    inc_visit_counter();
    _cfb_walk_successors(node, pre, post, visited);
}
