#include <libfirm/firm.h>
#include <stdlib.h>
#include <assert.h>

#include "func.h"
#include "cfb.h"
#include "resolve.h"
#include "utils.h"
#include "random.h"
#include "statistics.h"

static ir_node* resolve_operator();
static ir_node* resolve_memory_read();
static ir_node* resolve_immediate_move();
static ir_node* resolve_phi();
static ir_node* resolve_existing_temporary();
static ir_node* resolve_postpone();

// Options
int blocksize = 20;

// Resolution context
static func_t *current_func = NULL;
static cfg_t *current_cfg = NULL;
static cfb_t *current_cfb = NULL;
static temporary_t *current_temp = NULL;

// Sliding probablilites
double probabilites[6][2] = {
    { 65, 25 },  // Operator
    { 5, 5 },    // Memory read
    { 5, 15 },   // Immediate move
    { 5, 20 },   // Phi function
    { 10, 25 },  // Existing temporary
    { 10, 10 }   // Postpone
};

// Resolution methods
ir_node* (*resolve_funcs[6])() = {
    resolve_operator,
    resolve_memory_read,
    resolve_immediate_move,
    resolve_phi,
    resolve_existing_temporary,
    resolve_postpone
};

// Binary ir operations
static ir_node* (*bin_op_funcs[])(ir_node *, ir_node *, ir_node *) = {
    new_r_Mul,
    new_r_Add,
    new_r_Sub
};

// FUNCTIONS

static int is_dominated(ir_node* node, ir_node* dom);

void set_blocksize(int x) {
    blocksize = x;
}

static ir_node *resolve_operator() {
    // Choose random operator
    int randomFuncIdx = rand() % (sizeof(bin_op_funcs) / sizeof(bin_op_funcs[0]));
    // Create dummy operands
    ir_mode* mode = get_irn_mode(current_temp->node);
    ir_node* left_node = new_Dummy(mode);
    ir_node* right_node = new_Dummy(mode);
    cfb_add_temporary(current_cfb, left_node, TEMPORARY_NUMBER);
    cfb_add_temporary(current_cfb, right_node, TEMPORARY_NUMBER);
    // Return operation node
    ir_node *op_node = bin_op_funcs[randomFuncIdx](current_cfb->irb, left_node, right_node);
    assert(get_irn_n(op_node, -1) == current_cfb->irb);
    return op_node;
}

static ir_node *resolve_immediate_move() {
    ir_node *random_const = new_Const(new_tarval_from_long(rand(), mode_Is));
    return random_const;
}

static ir_node *resolve_alloc() {
    if (is_dominated(current_temp->node, current_cfb->mem)) {
        //printf("Can not resolve memory read, as cfb->mem dominates\n");
        return NULL;
    }

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_node *size = new_Const(new_tarval_from_long(10, mode_Iu));
    ir_node *alloc = new_r_Alloc(current_cfb->irb, mem_dummy, size, 8);
	ir_node *alloc_ptr = new_Proj(alloc, mode_P, pn_Alloc_res);
	ir_node *alloc_mem = new_Proj(alloc, mode_M, pn_Alloc_M);
    exchange(current_cfb->mem, alloc_mem);
    current_cfb->mem = mem_dummy;

    return alloc_ptr;
}

// TODO: Use actualy memory read
static ir_node *resolve_memory_read() {
    if (is_dominated(current_temp->node, current_cfb->mem)) {
        //printf("Can not resolve memory read, as cfb->mem dominates\n");
        return NULL;
    }

    ir_node *dummy_ptr = new_Dummy(mode_P);
    cfb_add_temporary(current_cfb, dummy_ptr, TEMPORARY_POINTER);    

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_type *int_type = new_type_primitive(mode_Is);
    ir_node *load = new_r_Load(current_cfb->irb, mem_dummy, dummy_ptr, mode_Is, int_type, cons_none);
	ir_node *load_result = new_Proj(load, mode_Is, pn_Load_res);
	ir_node *load_mem    = new_Proj(load, mode_M, pn_Load_M);

    exchange(current_cfb->mem, load_mem);
    current_cfb->mem = mem_dummy;
	return load_result;
}

static void seed_store(ir_node *node) {
    if (is_dominated(node, current_cfb->mem)) {
        //printf("Can not resolve memory read, as cfb->mem dominates\n");
        return;
    }

    ir_node *dummy_ptr = new_Dummy(mode_P);
    cfb_add_temporary(current_cfb, dummy_ptr, TEMPORARY_POINTER);

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_type *int_type = new_type_primitive(mode_Is);
    ir_node *store = new_r_Store(current_cfb->irb, mem_dummy, dummy_ptr, node, int_type, cons_none);
	ir_node *store_mem = new_Proj(store, mode_M, pn_Store_M);
    exchange(current_cfb->mem, store_mem);
    current_cfb->mem = mem_dummy;

}

static ir_node *resolve_pointer() {
    ir_node *frame = get_irg_frame(get_irn_irg(current_cfb->irb));
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

static ir_node *resolve_phi() {
    //printf("resolve phi\n");
    ir_mode *mode = get_irn_mode(current_temp->node);
    assert((mode == mode_P) ^ (current_temp->type == TEMPORARY_NUMBER));
    assert((mode == mode_Is) ^ (current_temp->type == TEMPORARY_POINTER));
    if (current_cfb->n_predecessors > 0) {
    
        ir_node *ins[current_cfb->n_predecessors];
        int pred_count = 0;

        for (int i = 0, n = get_Block_n_cfgpreds(current_cfb->irb); i < n; ++i) {
            ir_node *irpred  = get_nodes_block(get_cf_op(get_Block_cfgpred(current_cfb->irb, i)));
            cfb_t *pred = NULL;

            cfb_for_each_predecessor(current_cfb, pred_it) {
                cfb_t *it_pred = cfb_get_predecessor(current_cfb, pred_it);
                if (it_pred->irb == irpred) {
                    pred = it_pred;
                    break;
                }
            }
            assert(pred != NULL);

            ins[pred_count] = new_Dummy(mode);
            cfb_add_temporary(pred, ins[pred_count], current_temp->type);
            pred_count++;
        }

        ir_node *new_node = new_r_Phi(current_cfb->irb, current_cfb->n_predecessors, ins, mode);
        if (0) printf(
            "Resolve phi %ld with type %s\n",
            get_irn_node_nr(new_node),
            current_temp->type == TEMPORARY_NUMBER ? "number" : "pointer"
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

static ir_node *resolve_existing_temporary() {
    ir_node *repl[100];
    int repl_count = 0;

    cfb_for_each_temp(current_cfb, temporary) {
        if (
             temporary->resolved &&
             temporary->type == current_temp->type &&
             !is_dominated(temporary->node, current_temp->node)
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

static ir_node *resolve_postpone() {
    return NULL;
}

static ir_node * resolve_cmp() {
    int random_relation = rand() % (ir_relation_greater_equal - ir_relation_false - 1) + 1;
    ir_node* temp1 = new_Dummy(mode_Is);
    ir_node* temp2 = new_Dummy(mode_Is);
    ir_node* cmp = new_r_Cmp(current_cfb->irb, temp1, temp2, random_relation);
    /*
    ;//printf("Created cmp %ld for block %ld\n",
        get_irn_node_nr(cmp), get_irn_node_nr(cfb->irb)
    );
    */
    cfb_add_temporary(current_cfb, temp1, TEMPORARY_NUMBER);
    cfb_add_temporary(current_cfb, temp2, TEMPORARY_NUMBER);
    current_cfb->n_nodes += 2;
    return cmp;
}

double interpolation_prefix_sum[6];

static void resolve_temp(temporary_t *temporary) {
    current_temp = temporary;

    /*printf("Resolve temp %s %ld in block %ld\n",
        get_irn_opname(temporary->node), get_irn_node_nr(temporary->node), get_irn_node_nr(temporary->node)
    );
    */
    set_current_ir_graph(get_irn_irg(current_cfb->irb));
    set_cur_block(current_cfb->irb);
    ir_node *new_node = NULL;

    while (new_node == NULL) {
        switch (temporary->type) {
        case TEMPORARY_POINTER: {
            int choice = rand() % 2;
            if (choice == 0) {
                new_node = resolve_alloc();
            } else if (choice == 1) {
                new_node = resolve_phi();
            } else {
                new_node = resolve_existing_temporary();
            }
            break;
        }
        case TEMPORARY_BOOLEAN: {
            new_node = resolve_cmp();
            break;
        }
        case TEMPORARY_NUMBER: {
            double factor = ((double)current_cfb->n_nodes) / ((double)blocksize);
            factor = factor > 1.0 ? 1.0 : factor; 
            get_interpolation_prefix_sum_table(6, probabilites, interpolation_prefix_sum, factor);
            double random = get_random_percentage();
            
            int resolved = 0;
            //for (int i = 0; i < 6; ++i ) printf("%f\t", interpolation_prefix_sum[i]);
            for (int i = 0; i < 6 && !resolved; ++i) {
                if (interpolation_prefix_sum[i] > random) {
                    //printf("%d : %f >? %f\n", i, interpolation_prefix_sum[i], random);
                    new_node = resolve_funcs[i](current_cfb);
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
        seed_store(new_node);
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
            resolve_temp(temporary);
            //_list_del(&temporary->head);
            cfb->n_temporaries -= 1;
            goto resolve;
        }
    }

    cfb_for_each_temp(cfb, temporary) {
        if (!temporary->resolved) {
            resolve_temp(temporary);
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

static void resolve_cfg_temporaries(cfg_t *cfg) {
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

static void resolve_cfg(cfg_t *cfg) {
    current_cfg = cfg;
    ir_type *proto = get_entity_type(get_irg_entity(current_func->irg));
    assert(is_Method_type(proto));
    ir_node *args = get_irg_args(current_func->irg);
    cfb_t *start = cfg_get_start(cfg);
    for (size_t i = 0; i < get_method_n_params(proto); ++i) {
        ir_type *type = get_method_param_type(proto, i);
        ir_node *node = new_Proj(args, get_type_mode(type), i);
        temporary_t *temp = new_temporary(node, TEMPORARY_NUMBER);
        temp->resolved = 1;
        list_add_tail(&temp->head, &start->temporaries);
    }
    resolve_cfg_temporaries(cfg);
}

void resolve_func(func_t *func) {
    current_func = func;
    set_current_ir_graph(func->irg);
    resolve_cfg(func->cfg);
}

void resolve_prog(prog_t *prog) {
    for (size_t i = 0; i < ARR_LEN(prog->funcs); ++i) {
        resolve_func(prog->funcs[i]);
        resolve_mem_graph(prog->funcs[i]->cfg);
    }
}
