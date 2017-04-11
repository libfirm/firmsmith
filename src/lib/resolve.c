#include <libfirm/firm.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include "../cmdline/parameters.h"
#include "func.h"
#include "cfb.h"
#include "resolve.h"
#include "utils.h"
#include "random.h"
#include "statistics.h"
#include "types.h"

static ir_node* adopt_operator(void);
static ir_node* adopt_load(void);
static ir_node* adopt_const(void);
static ir_node* adopt_phi(void);
static ir_node* adopt_existing(void);
static ir_node* adopt_fcall(void);

// Resolution context
static prog_t *current_prog = NULL;
static func_t *current_func = NULL;
static cfg_t *current_cfg = NULL;
static cfb_t *current_cfb = NULL;
static temp_t *current_temp = NULL;

// Sliding probablilites

typedef struct sliding_prob_t {
    double start;
    double end;
} sliding_prob_t;

typedef ir_node* (*adopt_func_t)(void);

typedef struct resolver_t {
    adopt_func_t func;
    sliding_prob_t prob;
} resolver_t;

typedef struct kind_resolver_t {
    int n_resolvers;
    resolver_t **resolvers;
    double *ips_table;
} kind_resolver_t;

int n_kind_resolver;
static kind_resolver_t **kind_resolver_arr = NULL;


// Declare operations to be used
typedef ir_node* (*func_bin_op_t)(ir_node *, ir_node *, ir_node *);

static func_bin_op_t bin_op_funcs[] = {
    new_r_Mul,
    new_r_Add
};


// FUNCTIONS

/**
  * Allocates and initializes new kind resolver
  * @param n_funcs Number of resolver provided
  **/
static kind_resolver_t *new_kind_resolver(int n_resolvers) {
    kind_resolver_t *kind_resolver = malloc(sizeof(kind_resolver_t));
    kind_resolver->n_resolvers = n_resolvers;
    kind_resolver->resolvers = calloc(n_resolvers, sizeof(resolver_t*));
    assert(kind_resolver->resolvers != NULL);
    kind_resolver->ips_table = calloc(n_resolvers, sizeof(double));
    assert(kind_resolver->ips_table != NULL);
    return kind_resolver;
}

/**
  * Allocates and initializes new resolver
  **/
static resolver_t *new_resolver(adopt_func_t func, double start, double end) {
    resolver_t *resolver = malloc(sizeof(resolver_t));
    resolver->func  = func;
    resolver->prob.start = start;
    resolver->prob.end    = end;
    return resolver;
}

static void update_ips_table(kind_resolver_t *kind_resolver) {
    int n_nodes   = current_cfb == NULL ? 1 : current_cfb->n_nodes;
    int max_nodes = fs_params.cfb.n_nodes;
    double factor = n_nodes >= max_nodes ?
        1.0f : ((double)n_nodes) / ((double)max_nodes);
    double prefix_sum = 0.0;
    for (int i = 0; i < kind_resolver->n_resolvers; ++i) {
        resolver_t *resolver = kind_resolver->resolvers[i];
        double diff  = (resolver->prob.end  - resolver->prob.start);
        double intpl = resolver->prob.start + diff * factor;
        prefix_sum += intpl;
        kind_resolver->ips_table[i] = prefix_sum;
    }
}

/**
  * Update memory path
  **/
static void update_memory(ir_node *mem_dummy, ir_node *mem_result) {
    exchange(current_cfb->mem, mem_result);
    current_cfb->mem = mem_dummy;
}

/**
  * Walker function for domination check
  * @return 0 if node is dominated otherwise 1
  **/
static int is_dominated_core(ir_node* node, ir_node* dom) {
    if (irn_visited(node)) {
        return 0;
    } else {
        mark_irn_visited(node);
    }

    assert(node != dom);
    // Walk over all predecessor nodes, but the block node (index -1)
    for (int i = 0, n = get_irn_arity(node); i < n; ++i) {
        ir_node *pred_node  = get_irn_n(node, i);
        //ir_node *pred_block = get_irn_n(pred_node, -1);
        // We encountered the dominator node, therefore we found a path between
        // `node` and `dom`, which implies that `dom` is defined before `node` is.
        if (pred_node == dom) {
            return 1;
        }
        // Use-before-def does not apply to block boundary passing paths 
        if (get_irn_opcode(pred_node) == iro_Phi) {
            continue;
        }
        // Call walker function on predecessor nodes
        if (is_dominated_core(pred_node, dom)) {
            return 1;
        }
    }
    return 0;
}

/**
  * Checks whether a predecessor path between node node and dom exists
  * within block boundaries.
  * @return 0 if node is dominated otherwise 1
  **/
static int is_dominated(ir_node* node, ir_node* dom) {
    inc_irg_visited(get_irn_irg(node));
    int res =  is_dominated_core(node, dom);
    return res;
}

static func_bin_op_t get_random_bin_op(void) {
    int idx = rand() % (sizeof(bin_op_funcs) / sizeof(bin_op_funcs[0]));
    return bin_op_funcs[idx];
}

/**
  * Adopt operator as dummy replacement
  **/
static ir_node *adopt_operator(void) {
    ir_type *type = current_temp->type;
    ir_mode* mode       = get_irn_mode(current_temp->node);
    ir_node* left_node  = new_Dummy(mode);
    ir_node* right_node = new_Dummy(mode);
    cfb_add_temporary(current_cfb, left_node, type);
    cfb_add_temporary(current_cfb, right_node, type);
    // Return operation node
    func_bin_op_t func = get_random_bin_op();
    ir_node *op_node = func(current_cfb->irb, left_node, right_node);
    return op_node;
}

static ir_node *adopt_conv(void) {
    ir_type *new_type = NULL;
    do {
        new_type = get_random_prim_type();
    } while (new_type == current_temp->type);
    ir_node *dummy    = new_Dummy(get_type_mode(new_type));
    ir_node *conv     = new_Conv(dummy, get_irn_mode(current_temp->node));
    cfb_add_temporary(current_cfb, dummy, new_type);
    return conv;
}

/**
  * Adopt constant as dummy replacement
  **/
static ir_node *adopt_const(void) {
    assert(is_Primitive_type(current_temp->type));
    ir_mode *mode = get_type_mode(current_temp->type);
    ir_tarval *tv = mode_is_float(mode) ?
        new_tarval_from_long_double((double)rand(), mode) :
        new_tarval_from_long(rand(), mode);
    ir_node *random_const = new_Const(tv);
    return random_const;
}

/**
  * Adopts pointer projected from allocation as temporary.
  * Adds no further unresolved tempoaries.
  **/
static ir_node *adopt_alloc(void) {
    // Dertmine type size
    assert(is_Pointer_type(current_temp->type));
    ir_type *pointee_type = get_pointer_points_to_type(current_temp->type);
    int n_bytes = -1;
    if (is_Pointer_type(pointee_type) || is_Primitive_type(pointee_type)) {
        ir_mode *mode = get_type_mode(pointee_type);
        n_bytes       = get_mode_size_bytes(mode);
    } else if (is_Struct_type(pointee_type) || is_Union_type(pointee_type)) {
        n_bytes       = get_type_size(pointee_type);
    }
    assert(n_bytes > 0);

    ir_node *size    = new_Const(new_tarval_from_long(n_bytes, mode_Iu));
    // Alloc
    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_node *alloc = new_r_Alloc(current_cfb->irb, mem_dummy, size, 8);
	ir_node *alloc_ptr = new_Proj(alloc, mode_P, pn_Alloc_res);
	ir_node *alloc_mem = new_Proj(alloc, mode_M, pn_Alloc_M);
    update_memory(mem_dummy, alloc_mem);
    return alloc_ptr;
}

static ir_node *adopt_member(void) {
    ir_type *pointee_type = get_pointer_points_to_type(current_temp->type);
    // Find fitting entity
    ir_entity *ent = get_associated_entity(pointee_type);
    if (ent == NULL) {
        return NULL;
    }

    char type_name[256];
    ir_print_type(type_name, 256, pointee_type);

    //printf("Found entity %s for type %s\n", get_entity_ident(ent), type_name);
    ir_node *dummy  = new_Dummy(mode_P);
    cfb_add_temporary(
        current_cfb, dummy,
        new_type_pointer(get_entity_owner(ent))
    );
    ir_node *member = new_Member(dummy, ent);
    return member;
}

/**
  * Adopt loading from memory as dummy replacement
  **/
static ir_node *adopt_load(void) {
    if (fs_params.cfb.has_memory_ops == false) {
        return NULL;
    }

    assert(is_Primitive_type(current_temp->type));

    ir_type *ref_type  = new_type_pointer(current_temp->type);
    ir_node *dummy_ptr = new_Dummy(mode_P);
    cfb_add_temporary(current_cfb, dummy_ptr, ref_type);

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_type *type      = current_temp->type;
    ir_mode *mode      = get_type_mode(type);
    ir_node *load      = new_r_Load(current_cfb->irb, mem_dummy, dummy_ptr, mode, type, cons_none);

	ir_node *load_result = new_Proj(load, mode, pn_Load_res);
	ir_node *load_mem    = new_Proj(load, mode_M, pn_Load_M);

    update_memory(mem_dummy, load_mem);
	return load_result;
}

static ir_node *get_cf_op(ir_node *n)
{
	while (!is_cfop(n) && !is_fragile_op(n) && !is_Bad(n)) {
		n = skip_Tuple(n);
		n = skip_Proj(n);
	}
	return n;
}

/**
  * Adopt Phi node as dummy replacement
  **/
static ir_node *adopt_phi(void) {
    ir_mode *node_mode = get_irn_mode(current_temp->node);
    // We can only put a phi node, if not at starting block
    if (current_cfb->n_predecessors > 0) {
        // Create cfb array with matching order
        int n_blocks = get_Block_n_cfgpreds(current_cfb->irb);
        cfb_t *ordered_cfb[n_blocks];
        assert(n_blocks == current_cfb->n_predecessors);
        for (int i = 0; i < n_blocks; ++i) {
            ir_node *jmp      = get_cf_op(get_Block_cfgpred(current_cfb->irb, i));
            ir_node *ir_pred  = get_nodes_block(jmp);
            cfb_for_each_predecessor(current_cfb, pred_it) {
                cfb_t *it_pred = cfb_get_predecessor(current_cfb, pred_it);
                if (it_pred->irb == ir_pred) {
                    ordered_cfb[i] = it_pred;
                    break;
                }
            }
            assert(ordered_cfb[i] != NULL);
        }
        // Create phi inputs
        ir_node *ins[n_blocks];
        for (int i = 0; i < n_blocks; ++i) {
            ins[i] = new_Dummy(get_irn_mode(current_temp->node));
            cfb_add_temporary(ordered_cfb[i], ins[i], current_temp->type);
        }
        ir_node *new_node = new_r_Phi(current_cfb->irb, n_blocks, ins, node_mode);
        return new_node;
    } else {
        return NULL;
    }
}

/**
  * Adopt existing node as dummy replacement
  **/
static ir_node *adopt_existing(void) {
    ir_node **repl = NEW_ARR_F(ir_node*, 0);
    // Iterate over all temporaries and find fitting candidates
    cfb_for_each_temp(current_cfb, temporary) {
        if (
             temporary->resolved &&
             temporary->type == current_temp->type &&
             !is_dominated(temporary->node, current_temp->node)
        ) {
            ARR_APP1(ir_node*, repl, temporary->node);
        }
    }
    // Return random candidate, if any
    int repl_length = ARR_LEN(repl);
    if (repl_length > 0) {
        int repl_index = rand() % repl_length;
        return repl[repl_index];
    } else {
        return NULL;
    }
}

/**
  * Adopt function call as dummy replacement
  **/
static ir_node *adopt_fcall(void) {
    // Are function calls disabled?
    if (fs_params.cfb.has_func_calls == false) {
        return NULL;
    }

    // We currently do not support resolving compound temporaries
    // with function calls
    if (is_compound_type(current_temp->type)) {
        return NULL;
    }

    // Are we exceeding the maxium number of function calls?
    int n_calls = ARR_LEN(current_func->calls);
    if (n_calls >= fs_params.func.max_calls) {
        return NULL;
    }

    func_t *func = prog_get_random_func(current_prog);
    // Is there even a function to call? We do not allow calling main()
    if (func == NULL) {
        return NULL;
    }

    // If cycles are prohibited, check whether introducing a function call
    // would introduce a cycle 
    if (!fs_params.prog.has_cycles && func_is_dominated(func, current_func)) {
        return NULL;
    }

    // Add parameters as temporaries
    ir_graph *irg   = func->irg;
    ir_entity *ent  = get_irg_entity(irg);
    ir_type *proto  = get_entity_type(ent);

    // Check whether return type fits temporary type
    int n_res = get_method_n_ress(proto);
    assert(n_res == 1);
    ir_type *ret_type = get_method_res_type(proto, 0);
    if (ret_type != current_temp->type) {
        return NULL;
    }

    // Now all good, do function call
    func_add_call(current_func, func);

    int n_params = get_method_n_params(proto);
    ir_node *params[n_params];
    ir_type *param_type = NULL;
    for (int i = 0; i < n_params; ++i) {
        param_type = get_method_param_type(proto, i);
        params[i]  = new_Dummy(get_type_mode(param_type));
        cfb_add_temporary(current_cfb, params[i], param_type);
    }

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_node *call = new_Call(
        mem_dummy, new_Address(ent),
        func->n_params, params, proto
    );
	ir_node *call_mem = new_Proj(call, mode_M, pn_Call_M);
    exchange(current_cfb->mem, call_mem);
    current_cfb->mem = mem_dummy;
	ir_node *tuple  = new_Proj(call, mode_T, pn_Call_T_result);
	ir_node *result = new_Proj(tuple, get_type_mode(ret_type), 0);
    return result;
}

/**
  * Return random relation for Cmp node
  * @return Compare relation
  **/
static ir_relation get_random_relation(void) {
    return rand() % (ir_relation_greater_equal - ir_relation_false - 1) + 1;
}

/**
  * Return compare node
  * @return Node to replace dummy
  **/
static ir_node* adopt_cmp(void) {
    assert(is_Primitive_type(current_temp->type));
    assert(get_type_mode(current_temp->type) == mode_b);
    ir_type* type   = get_int_type();
    ir_mode* mode   = get_type_mode(type);
    ir_node* dummy1 = new_Dummy(mode);
    ir_node* dummy2 = new_Dummy(mode);
    ir_node* cmp    = new_r_Cmp(current_cfb->irb, dummy1, dummy2, get_random_relation());
    cfb_add_temporary(current_cfb, dummy1, type);
    cfb_add_temporary(current_cfb, dummy2, type);
    return cmp;
}

/**
  * Add a store node to the current CF block.
  **/
static void seed_store(ir_node *node) {
    if (is_dominated(node, current_cfb->mem)) {
        // Consuming given node in a Store,
        // would result in loop in memory path
        return;
    }

    // TODO: Allow to store pointers to compounds
    if (is_Primitive_type(current_temp->type) && get_type_mode(current_temp->type) == mode_b) {
        return;
    }

    ir_type *ref_type  = new_type_pointer(current_temp->type);
    ir_node *dummy_ptr = new_Dummy(mode_P);
    cfb_add_temporary(current_cfb, dummy_ptr, ref_type);

    ir_node *mem_dummy = new_Dummy(mode_M);
    ir_node *store     = new_r_Store(
        current_cfb->irb, mem_dummy, dummy_ptr,
        node, current_temp->type, cons_none);
	ir_node *store_mem = new_Proj(store, mode_M, pn_Store_M);
    update_memory(mem_dummy, store_mem);

    if (is_Pointer_type(current_temp->type)) {
        //printf("Storing pointer: %ld\n", get_irn_node_nr(store));
    }

}

/**
  * Resolve temporary using different techniques depending on the associated
  * type.
  * @param temporary The temporary to be resolved
  **/
static void resolve_temp(temp_t *temporary) {
    current_temp = temporary;

    // We try different techniques for resolving temporaries.
    // If the chosen method cannot be applied, we randomly
    // choose methods until one, which can be applied is found.
    ir_node *new_node = NULL;
    ir_type *type = temporary->type;

    kind_resolver_t *kind_resolver;
    if (is_Primitive_type(type)) {
        if (get_type_mode(type) == mode_b) {
            new_node = adopt_cmp();
            goto replace;
        } else {
            kind_resolver = kind_resolver_arr[1];
        }
    } else {
        kind_resolver = kind_resolver_arr[0];
    }

    update_ips_table(kind_resolver);

    while (new_node == NULL) {
        double random = get_random_percentage();
        int resolved = 0;
        //for (int i = 0; i < 6; ++i ) printf("%f\t", interpolation_prefix_sum[i]);
        for (int i = 0; i < kind_resolver->n_resolvers && !resolved; ++i) {
            //printf("%d : %f >? %f\n", i, kind_resolver->ips_table[i], random);
            if (kind_resolver->ips_table[i] > random) {
                new_node = kind_resolver->resolvers[i]->func();
                if (new_node != NULL) assert(get_irn_opcode(new_node) != iro_Dummy);
                //printf("New node %ld\n", get_irn_node_nr(new_node));
                resolved = 1;
            }
        }
        assert(resolved);
    }
replace:
    assert(get_irn_opcode(new_node) != iro_Dummy);
    exchange(temporary->node, new_node);
    assert(get_irn_opcode(new_node) != iro_Dummy);
    temporary->node = new_node;
    stats_register_op(get_irn_opcode(new_node));

    temporary->resolved = 1;

    if (rand() % 8 == 1) {
        seed_store(new_node);
    }
}

/**
  * Resolve temporaries inside CF block.
  * @param cfb CF block
  **/ 
static void resolve_cfb(cfb_t *cfb) {
    // If we have a loop, we might already have visited this CF block,
    // but as long as we have temporaries to resolve, we need to
    // process it. Newly added unresolved temporaries originate from
    // loops in the CF.
    // Even though there might be temporaries, we cannot already abort
    // before having invoked the resolution of tempoaries in predecessor
    // block, since blocks with only one successor have initially no
    // temporaries at all.
resolve_cfb_start:
    if (cfb->n_temporaries == 0 && cfb->visited >= get_visit_counter()) {
        return;
    }

    // Mark as visited
    cfb->visited = get_visit_counter();

    // Start context for cfb
    current_cfb = cfb;
    set_cur_block(cfb->irb);

    // Resolve all temporaries
    cfb_for_each_temp(cfb, temp) {
        if (!temp->resolved) {
            resolve_temp(temp);
            cfb->n_temporaries -= 1;
        }
    }

    // Visit all predecessor CF blocks
    cfb_for_each_predecessor(cfb, pred_it) {
        cfb_t *pred = cfb_get_predecessor(cfb, pred_it);
        resolve_cfb(pred);
    }

    goto resolve_cfb_start;
}

/**
  * Resolve tempoaries inside CF graph.
  * 
  * The parameters from the prototype associated with the CF graph
  * are added as resolved temporaries, so that they can be used
  * as a replacement for unresolved ones.
  * We then start resolving temporaries in leaves of the CF graph.
  *
  * @param cfg CF graph
  **/
static void resolve_cfg(cfg_t *cfg) {
    current_cfg = cfg;

    ir_entity *entity = get_irg_entity(current_func->irg);
    ir_type *proto    = get_entity_type(entity);
    ir_node *args     = get_irg_args(current_func->irg);

    // TODO: Add support for compound function parameters 

    // Iterate over function parameters and add argument projections
    // to the start control block
    cfb_t *start = cfg_get_start(cfg);
    for (size_t i = 0; i < get_method_n_params(proto); ++i) {
        // Create projection for parameter with corresponding mode
        ir_type *type = get_method_param_type(proto, i);
        assert(is_Primitive_type(type));
        ir_mode *mode = get_type_mode(type);
        ir_node *proj = new_Proj(args, mode, i);
        // Add resolved temporary
        temp_t *temp  = new_temporary(proj, type);
        temp->resolved = 1;
        list_add_tail(&temp->head, &start->temporaries);
    }

    // Start resolving the temporaries by starting at the blocks with
    // no successors and push the unresolved temporaries upwards inside the graph
    for (int i = 0; i < cfg->n_blocks; ++i) {
        cfb_t *cfb = cfg->blocks[i];
        if (cfb->n_successors == 0) {
            inc_visit_counter();
            resolve_cfb(cfb);
        }
    }
}

/**
  * Resolve the temporaries inside the provided function
  * @param func Function
  **/
static void resolve_func(func_t *func) {
    current_func = func;
    set_current_ir_graph(func->irg);
    resolve_cfg(func->cfg);
}

/**
  * Walker for resolve_mem_graph()
  * @param cfb CF block currently walked over
  **/ 
static void resolve_mem_graph_walker_post(cfb_t *cfb) {
    if (cfb->last_mem == NULL) return;
    set_cur_block(cfb->irb);
    ir_node *store = get_store();
    if (is_Phi(store)) {
      /* Documentation on website (Endless Loops) says that we must keep-alive
       * potentially endless loop twice: The PhiM and the block.
       * A PhiM is kept alive implicitly by construction, but the block must
       * be handled by the frontend. */
      keep_alive(cfb->irb);
    }
    exchange(cfb->mem, store);
    set_store(cfb->last_mem);
    cfb->last_mem = NULL;
}

/**
  * Walks over the CF graph and makes calls to get_store()
  * in order to trigger the resolution of memory dependencies.
  * @param cfg CF graph with memory to be resolved
  **/
static void resolve_mem_graph(cfg_t *cfg) {
    //edges_activate(get_current_ir_graph());
    for (int i = 0; i < cfg->n_blocks; ++i) {
        cfb_t *cfb = cfg->blocks[i];
        if (cfb->n_successors == 0) {
            cfb_walk_predecessors(cfb, NULL, resolve_mem_graph_walker_post, NULL);
        }
    }
}

/**
  * Resolve the temporaries inside the program
  * @param prog Program to be resolved
  **/
void resolve_prog(prog_t *prog) {
    current_prog = prog;
    for (size_t i = 0; i < ARR_LEN(prog->funcs); ++i) {
        resolve_func(prog->funcs[i]);
        resolve_mem_graph(prog->funcs[i]->cfg);
    }
    //print_func_call_graph(prog->funcs[0]);
}

/**
  * Initialize resolve module
  **/
void initialize_resolve(void) {
    int i;
    // Create resolver for pointer
    kind_resolver_t *pointer_resolver = new_kind_resolver(4);
    i = 0;
    pointer_resolver->resolvers[i++] = new_resolver(adopt_phi,      10, 10);
    pointer_resolver->resolvers[i++] = new_resolver(adopt_alloc,    20, 20);
    pointer_resolver->resolvers[i++] = new_resolver(adopt_member,   40, 40);
    pointer_resolver->resolvers[i++] = new_resolver(adopt_existing, 99, 99);
    assert(i == pointer_resolver->n_resolvers);

    // Create resolver for primitive
    kind_resolver_t *prim_resolver = new_kind_resolver(7);
    i = 0;
    double p = 100 / prim_resolver->n_resolvers;
    prim_resolver->resolvers[i++] = new_resolver(adopt_const,    p, p);
    prim_resolver->resolvers[i++] = new_resolver(adopt_operator, p, p);
    prim_resolver->resolvers[i++] = new_resolver(adopt_phi,      p, p);
    prim_resolver->resolvers[i++] = new_resolver(adopt_existing, p, p);
    prim_resolver->resolvers[i++] = new_resolver(adopt_load,     p, p);
    prim_resolver->resolvers[i++] = new_resolver(adopt_conv,     p, p);
    prim_resolver->resolvers[i++] = new_resolver(adopt_fcall,    100, 100);
    assert(i == prim_resolver->n_resolvers);

    n_kind_resolver = 2;
    kind_resolver_arr = calloc(n_kind_resolver, sizeof(kind_resolver_t*));
    kind_resolver_arr[0] = pointer_resolver;
    kind_resolver_arr[1] = prim_resolver;
}

/**
  * Clean up data allocated by resolve module
  **/
void finish_resolve(void) {
    // TODO: clean up
}
