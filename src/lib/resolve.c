#include <libfirm/firm.h>
#include <stdlib.h>
#include <assert.h>

#include "cfb.h"
#include "resolve.h"
#include "utils.h"
#include "random.h"
#include "statistics.h"

int blocksize = 20;

static cfb_t *current_cfb = NULL;
static temporary_t *current_temporary = NULL;

static int is_dominated(ir_node* node, ir_node* dom);

void set_blocksize(int x) {
    blocksize = x;
}

static ir_node* (*bin_op_funcs[])(ir_node *, ir_node *, ir_node *) = {
    new_r_Mul,
    new_r_Add,
    new_r_Sub
};

static ir_node *resolve_operator(cfb_t *cfb) {
    // Choose random operator
    int randomFuncIdx = rand() % (sizeof(bin_op_funcs) / sizeof(bin_op_funcs[0]));
    // Create dummy operands
    ir_node* left_node = new_Dummy(mode_Is);
    ir_node* right_node = new_Dummy(mode_Is);
    cfb_add_temporary(cfb, left_node, TEMPORARY_NUMBER);
    cfb_add_temporary(cfb, right_node, TEMPORARY_NUMBER);
    // Return operation node
    ir_node *op_node = bin_op_funcs[randomFuncIdx](cfb->irb, left_node, right_node);
    assert(get_irn_n(op_node, -1) == cfb->irb);
    return op_node;
}

static ir_node *resolve_immediate_move(cfb_t *cfb) {
    (void)cfb;
    ir_node *random_const = new_Const(new_tarval_from_long(rand(), mode_Is));
    return random_const;
}

static ir_node *resolve_alloc(cfb_t *cfb) {
    if (is_dominated(current_temporary->node, cfb->mem)) {
        //printf("Can not resolve memory read, as cfb->mem dominates\n");
        return NULL;
    }


    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_node *size = new_Const(new_tarval_from_long(10, mode_Iu));
    ir_node *alloc = new_r_Alloc(cfb->irb, mem_dummy, size, 8);
	ir_node *alloc_ptr = new_Proj(alloc, mode_P, pn_Alloc_res);
	ir_node *alloc_mem = new_Proj(alloc, mode_M, pn_Alloc_M);
    exchange(cfb->mem, alloc_mem);
    cfb->mem = mem_dummy;

    return alloc_ptr;
}

// TODO: Use actualy memory read
static ir_node *resolve_memory_read(cfb_t *cfb) {
    if (is_dominated(current_temporary->node, cfb->mem)) {
        //printf("Can not resolve memory read, as cfb->mem dominates\n");
        return NULL;
    }

    ir_node *dummy_ptr = new_Dummy(mode_P);
    cfb_add_temporary(cfb, dummy_ptr, TEMPORARY_POINTER);    

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_type *int_type = new_type_primitive(mode_Is);
    ir_node *load = new_r_Load(cfb->irb, mem_dummy, dummy_ptr, mode_Is, int_type, cons_none);
	ir_node *load_result = new_Proj(load, mode_Is, pn_Load_res);
	ir_node *load_mem    = new_Proj(load, mode_M, pn_Load_M);

    exchange(cfb->mem, load_mem);
    cfb->mem = mem_dummy;
	return load_result;
}

static void seed_store(cfb_t *cfb, ir_node *node) {
    if (is_dominated(node, cfb->mem)) {
        //printf("Can not resolve memory read, as cfb->mem dominates\n");
        return;
    }

    ir_node *dummy_ptr = new_Dummy(mode_P);
    cfb_add_temporary(cfb, dummy_ptr, TEMPORARY_POINTER);

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_type *int_type = new_type_primitive(mode_Is);
    ir_node *store = new_r_Store(cfb->irb, mem_dummy, dummy_ptr, node, int_type, cons_none);
	ir_node *store_mem = new_Proj(store, mode_M, pn_Store_M);
    exchange(cfb->mem, store_mem);
    cfb->mem = mem_dummy;

}

static ir_node *resolve_pointer(cfb_t *cfb) {
    ir_node *frame = get_irg_frame(get_irn_irg(cfb->irb));
    return frame;
    //ir_node *conv = new_r_Conv(cfb->irb, frame, mode_Is);
    //return conv;
}

static ir_node *get_cf_op(ir_node *n)
{
	while (!is_cfop(n) && !is_fragile_op(n) && !is_Bad(n)) {
		n = skip_Tuple(n);
		n = skip_Proj(n);
	}
	return n;
}

static ir_node *resolve_phi(cfb_t *cfb) {
    //printf("resolve phi\n");
    ir_mode *mode = get_irn_mode(current_temporary->node);
    assert((mode == mode_P) ^ (current_temporary->type == TEMPORARY_NUMBER));
    assert((mode == mode_Is) ^ (current_temporary->type == TEMPORARY_POINTER));
    if (cfb->n_predecessors > 0) {
    
        ir_node *ins[cfb->n_predecessors];
        int pred_count = 0;

        for (int i = 0, n = get_Block_n_cfgpreds(cfb->irb); i < n; ++i) {
            ir_node *irpred  = get_nodes_block(get_cf_op(get_Block_cfgpred(cfb->irb, i)));
            cfb_t *pred = NULL;

            cfb_for_each_predecessor(cfb, pred_it) {
                cfb_t *it_pred = cfb_get_predecessor(cfb, pred_it);
                if (it_pred->irb == irpred) {
                    pred = it_pred;
                    break;
                }
            }
            assert(pred != NULL);

            ins[pred_count] = new_Dummy(mode);
            cfb_add_temporary(pred, ins[pred_count], current_temporary->type);
            pred_count++;
        }

        ir_node *new_node = new_r_Phi(cfb->irb, cfb->n_predecessors, ins, mode);
        if (0) printf(
            "Resolve phi %ld with type %s\n",
            get_irn_node_nr(new_node),
            current_temporary->type == TEMPORARY_NUMBER ? "number" : "pointer"
        );
        return new_node;
    } else {
        return NULL;
    }
}

static int is_dominated_core(ir_node* node, ir_node* dom) {
    if (0) printf(
        "is_dominated_core(%s(%ld), %s(%ld))\n", 
        get_irn_opname(node), get_irn_node_nr(node),
        get_irn_opname(dom), get_irn_node_nr(dom)
    );
    if (irn_visited(node)) {
        return 0;
    } else {
        mark_irn_visited(node);
    }

    ir_node* dom_block = current_cfb->irb;
    assert(node != dom);

    for (int i = 0, n = get_irn_arity(node); i < n; ++i) {
        ir_node *pred_node  = get_irn_n(node, i);
        ir_node *pred_block = get_irn_n(pred_node, -1);

        if (pred_node == dom) {
            return 1;
        }

        if (get_irn_opcode(pred_node) == iro_Phi) {
            continue;
        }

        if (pred_block != dom_block) {
            //continue;
        }

        if (is_dominated_core(pred_node, dom)) {
            return 1;
        }

    }
    return 0;
}

static int is_dominated(ir_node* node, ir_node* dom) {
    inc_irg_visited(get_irn_irg(node));
    int res =  is_dominated_core(node, dom);
    if (0) printf("\n");
    return res;
}

static ir_node *resolve_existing_temporary(cfb_t *cfb) {
    ir_node *repl[100];
    int repl_count = 0;

    cfb_for_each_temp(cfb, temporary) {
        if (
             temporary->resolved &&
             temporary->type == current_temporary->type &&
             !is_dominated(temporary->node, current_temporary->node)
        ) {
            repl[repl_count++] = temporary->node;
        }
        if (repl_count == 100) {
            break;
        }
    }

    if (repl_count > 0) {
        int repl_index = rand() % repl_count;
        return repl[repl_index];
    } else {
        return NULL;
    }
}

static ir_node *resolve_postpone(cfb_t *cfb) {
    (void)cfb;
    return NULL;
}

static ir_node * resolve_cmp(cfb_t *cfb) {
    int random_relation = rand() % (ir_relation_greater_equal - ir_relation_false - 1) + 1;
    ir_node* temp1 = new_Dummy(mode_Is);
    ir_node* temp2 = new_Dummy(mode_Is);
    ir_node* cmp = new_r_Cmp(cfb->irb, temp1, temp2, random_relation);
    /*
    ;//printf("Created cmp %ld for block %ld\n",
        get_irn_node_nr(cmp), get_irn_node_nr(cfb->irb)
    );
    */
    cfb_add_temporary(cfb, temp1, TEMPORARY_NUMBER);
    cfb_add_temporary(cfb, temp2, TEMPORARY_NUMBER);
    cfb->n_nodes += 2;
    return cmp;
}

// Percentage
double probabilites[6][2] = {
    { 65, 25 }, // Operator
    { 5, 5 },   // Memory read
    { 5, 15 },  // Immediate move
    { 5, 20 },  // Phi function
    { 10, 25 },  // Existing temporary
    { 10, 10 }  // Postpone
};

ir_node* (*resolve_funcs[6])(cfb_t*) = {
    resolve_operator,
    resolve_memory_read,
    resolve_immediate_move,
    resolve_phi,
    resolve_existing_temporary,
    resolve_postpone
};

double interpolation_prefix_sum[6];

static void resolve_temp(cfb_t *cfb, temporary_t *temporary) {
                    current_temporary = temporary;

    /*printf("Resolve temp %s %ld in block %ld\n",
        get_irn_opname(temporary->node), get_irn_node_nr(temporary->node), get_irn_node_nr(temporary->node)
    );
    */
    set_current_ir_graph(get_irn_irg(cfb->irb));
    set_cur_block(cfb->irb);
    ir_node *new_node = NULL;

    while (new_node == NULL) {
        switch (temporary->type) {
        case TEMPORARY_POINTER: {
            int choice = rand() % 2;
            if (choice == 0) {
                new_node = resolve_alloc(cfb);
            } else if (choice == 1) {
                new_node = resolve_phi(cfb);
            } else {
                new_node = resolve_existing_temporary(cfb);
            }
            break;
        }
        case TEMPORARY_BOOLEAN: {
            new_node = resolve_cmp(cfb);
            break;
        }
        case TEMPORARY_NUMBER: {
            double factor = ((double)cfb->n_nodes) / ((double)blocksize);
            factor = factor > 1.0 ? 1.0 : factor; 
            get_interpolation_prefix_sum_table(6, probabilites, interpolation_prefix_sum, factor);
            double random = get_random_percentage();
            
            int resolved = 0;
            //for (int i = 0; i < 6; ++i ) printf("%f\t", interpolation_prefix_sum[i]);
            for (int i = 0; i < 6 && !resolved; ++i) {
                if (interpolation_prefix_sum[i] > random) {
                    //printf("%d : %f >? %f\n", i, interpolation_prefix_sum[i], random);
                    new_node = resolve_funcs[i](cfb);
                    //printf("New node %ld\n", get_irn_node_nr(new_node));
                    resolved = 1;
                }
            }
            assert(resolved);
            break;
        }
        default:
            assert(0 && "TEMPORARY kind not handled");
        }
    }

    assert(get_irn_opcode(new_node) != iro_Dummy);
    //printf("Reroute %ld %ld\n", get_irn_node_nr(temporary->node), get_irn_node_nr(new_node));
    //edges_reroute(temporary->node, new_node);

    /*
    while ( head != head->next) {
        ir_edge_t *edge = list_entry(head->next, ir_edge_t, list);
        assert(edge->pos >= -1);
        set_edge(edge->src, edge->pos, to);
    }*/

    //edges_node_deleted(temporary->node);

    exchange(temporary->node, new_node);
    assert(get_irn_opcode(new_node) != iro_Dummy);
    temporary->node = new_node;
    stats_register_op(get_irn_opcode(new_node));

    temporary->resolved = 1;

    if (temporary->type == TEMPORARY_NUMBER && rand() % 8 == 1) {
        seed_store(cfb, new_node);
    }
    //printf("Removing dummy\n");
}

static void resolve_cfb_temporaries(cfb_t *cfb) {
    current_cfb = cfb;
    assert(get_Block_n_cfgpreds(cfb->irb) == cfb->n_predecessors);
    if (
        cfb->visited >= get_visit_counter() &&
        cfb->n_temporaries == 0
    ) {
        return;
    }
    cfb->visited = get_visit_counter();

    if (cfb->visited >= get_visit_counter()) {
        ;//printf("Head: %p - Next: %p - Prev: %p\n", &cfb->temporaries, cfb->temporaries.next, cfb->temporaries.prev);
        cfb_for_each_temp(cfb, temporary) {
            ;//printf("%s %s\n", get_irn_opname(temporary->node), temporary->resolved ? "resolved" : "");
        }
        ;//printf("\n");
    }

    ;//printf("resolve irb %ld\n", get_irn_node_nr(cfb->irb));

    // Resolve local temporaries
    
    set_cur_block(cfb->irb);

resolve:
    cfb_for_each_temp(cfb, temporary) {
        if (!temporary->resolved && temporary->type == TEMPORARY_POINTER) {
            resolve_temp(cfb, temporary);
            //_list_del(&temporary->head);
            cfb->n_temporaries -= 1;
            goto resolve;
        }
    }

    cfb_for_each_temp(cfb, temporary) {
        if (!temporary->resolved) {
            resolve_temp(cfb, temporary);
            //_list_del(&temporary->head);
            cfb->n_temporaries -= 1;
            goto resolve;
        }
    }

    cfb_for_each_predecessor(cfb, pred_it) {
        cfb_t *pred = cfb_get_predecessor(cfb, pred_it);
        resolve_cfb_temporaries(pred);
    }

    resolve_cfb_temporaries(cfb);
    assert(cfb->n_temporaries == 0);
    ;//printf("end resolve irb %ld\n", get_irn_node_nr(cfb->irb));

}

void resolve_cfg_temporaries(cfg_t *cfg) {
    //edges_activate(get_current_ir_graph());
    for (int i = 0; i < cfg->n_blocks; ++i) {
        cfb_t *cfb = cfg->blocks[i];
        if (cfb->n_successors == 0) {
            inc_visit_counter();
            resolve_cfb_temporaries(cfb);
        }
    }
}

static struct ir_node* skip_id(ir_node *n) {
    if (get_irn_opcode(n) == iro_Id) {
        return skip_id(get_irn_n(n, 0));
    } else {
        return n;
    }
}

static void resolve_mem_graph_walker_post(cfb_t *cfb) {
    if (0) printf("Visiting cfb %s(%ld)\n",
        get_irn_opname(cfb->irb),
        get_irn_node_nr(cfb->irb)
    );
    if (cfb->last_mem == NULL) return;
    set_cur_block(cfb->irb);
    ir_node *store = get_store();
    if (0) printf("In %s(%ld):\n\tlast_mem = %s(%ld)\n\t     mem = %s(%ld)\n",
        get_irn_opname(get_irn_n(cfb->last_mem, -1)),
        get_irn_node_nr(get_irn_n(cfb->last_mem, -1)),
        get_irn_opname(skip_id(cfb->last_mem)),
        get_irn_node_nr(skip_id(cfb->last_mem)),
        get_irn_opname(skip_id(cfb->mem)),
        get_irn_node_nr(skip_id(cfb->mem))
    );
    exchange(cfb->mem, store);
    set_store(cfb->last_mem);
    cfb->last_mem = NULL;
}

void resolve_mem_graph(cfg_t *cfg) {
    //edges_activate(get_current_ir_graph());
    for (int i = 0; i < cfg->n_blocks; ++i) {
        cfb_t *cfb = cfg->blocks[i];
        if (cfb->n_successors == 0) {
            cfb_walk_predecessors(cfb, NULL, resolve_mem_graph_walker_post, NULL);
        }
    }
}
