#include <libfirm/firm.h>
#include <stdlib.h>
#include <assert.h>

#include "cfb.h"
#include "resolve.h"
#include "utils.h"
#include "random.h"
#include "statistics.h"

int blocksize = 20;

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

// TODO: Use actualy memory read
static ir_node *resolve_memory_read(cfb_t *cfb) {
    return resolve_immediate_move(cfb);
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
    if (cfb->n_predecessors == 1) {
        return resolve_immediate_move(cfb);
    } else if (cfb->n_predecessors > 0) {
    
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

            ins[pred_count] = new_Dummy(mode_Is);
            cfb_add_temporary(pred, ins[pred_count], TEMPORARY_NUMBER);
            pred_count++;
        }

        return new_r_Phi(cfb->irb, cfb->n_predecessors, ins, mode_Is);
    } else {
        return resolve_immediate_move(cfb);
    }
}

static cfb_t *current_cfb = NULL;
static temporary_t *current_temporary = NULL;

static int is_dominated(ir_node* node, ir_node* dom) {
    ir_node* dom_block = current_cfb->irb;
    assert(node != dom);

    for (int i = 0, n = get_irn_arity(node); i < n; ++i) {
        ir_node *pred_node  = get_irn_n(node, i);
        ir_node *pred_block = get_irn_n(pred_node, -1);

        if (pred_node == dom) {
            return 1;
        }

        if (pred_block != dom_block) {
            continue;
        }

        if (is_dominated(pred_node, dom)) {
            return 1;
        }

    }
    return 0;
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
    }

    if (repl_count > 0) {
        int repl_index = rand() % repl_count;
        return repl[repl_index];
    } else {
        return resolve_immediate_move(cfb);
    }
}

static ir_node *resolve_postpone(cfb_t *cfb) {
    return resolve_immediate_move(cfb);
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
    /*printf("Resolve temp %s %ld in block %ld\n",
        get_irn_opname(temporary->node), get_irn_node_nr(temporary->node), get_irn_node_nr(temporary->node)
    );
    */
    set_current_ir_graph(get_irn_irg(cfb->irb));
    set_cur_block(cfb->irb);
    ir_node *new_node;

    switch (temporary->type) {
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
                current_temporary = temporary;
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

    cfb_for_each_temp(cfb, temporary) {
        if (!temporary->resolved) {
            resolve_temp(cfb, temporary);
            //_list_del(&temporary->head);
            cfb->n_temporaries -= 1;
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
