#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "convert.h"
#include "cfg.h"
#include "cfb.h"
#include "func.h"

static void add_cfb_pred_jmp(cfb_t *cfb, ir_node* jmp) {
    if (!cfb->irb) {
        cfb->irb = new_immBlock();
         ;//printf("cfb with index %d has node nr %ld\n", cfb->index, get_irn_node_nr(cfb->irb));
    }
    add_immBlock_pred(cfb->irb, jmp);
}

static void convert_cfb(cfb_t *cfb) {
    if (cfb->visited >= get_visit_counter()) {
        return;
    }
    cfb->visited = get_visit_counter();
    cfb->mem = cfb->last_mem = new_Dummy(mode_M);

    assert(cfb->n_successors >= 0 && cfb->n_successors <= 2);
    set_cur_block(cfb->irb);

    if (cfb->n_successors == 2) {
        /* We have a branch in the control flow */   
        ir_node *dummy = new_Dummy(mode_b);
        ir_node *cnd = new_Cond(dummy);

        cfb_add_temporary(cfb, dummy, TEMPORARY_BOOLEAN);

        int succ_count = 0;
        cfb_for_each_successor(cfb, succ_iterator) {
            cfb_t *succ = cfb_get_successor(cfb, succ_iterator);
            add_cfb_pred_jmp(
                succ,
                new_Proj(cnd, mode_X, succ_count == 0 ? pn_Cond_false : pn_Cond_true)
            );
            /*
            printf("Add %s projection as predecessor %ld to block %ld\n", succ_count == 0 ? "true" : "false",
                get_irn_node_nr(cfb->irb), get_irn_node_nr(succ->irb)
            );
            */
            convert_cfb(succ);
            succ_count += 1;
        }
        assert(succ_count == 2);

    } else if (cfb->n_successors == 1) {
        
        cfb_for_each_successor(cfb, succ_iterator) {
            cfb_t *succ = cfb_get_successor(cfb, succ_iterator);
            add_cfb_pred_jmp(succ, new_Jmp());
            convert_cfb(succ);
        }
    } else {
        /* No successors, we have an exit */
        ir_graph *irg   = get_current_ir_graph();
        ir_entity *ent  = get_irg_entity(irg);
        ir_type *proto  = get_entity_type(ent);

        int n_res = get_method_n_ress(proto);
        ir_node *results[n_res];

        for (int i = 0; i < n_res; ++i) {
            ir_type *type = get_method_res_type(proto, i);
            ir_node *dummy = new_Dummy(get_type_mode(type));
            cfb_add_temporary(cfb, dummy, TEMPORARY_NUMBER);
            results[i] = dummy;
        }

        ir_node *res = new_Return(cfb->mem, 1, results);
        ir_node *end_block = get_irg_end_block(irg);
        add_immBlock_pred(end_block, res);
    }

}


static void mature_cfb(cfb_t *cfb) {
    ;//printf("mature cfb with index %d has node nr %ld\n", cfb->index, get_irn_node_nr(cfb->irb));
    mature_immBlock(cfb->irb);
}

static void convert_cfg(cfg_t *cfg) {
    // Construction
    ir_graph *irg = get_current_ir_graph();
    ir_node *irb = get_r_cur_block(irg);

    cfb_t *start = cfg_get_start(cfg);
    start->irb = irb;

    inc_visit_counter();
    convert_cfb(start);
}

void convert_func(func_t *func) {
    // Construct Int Type 
    ir_type *int_type = new_type_primitive(mode_Is);
    // Construct method type
    ir_type *proto = new_type_method(func->n_params, func->n_res, false, cc_cdecl_set, mtp_no_property);
    for (int i = 0; i < func->n_params; ++i) {
        set_method_param_type(proto, i, int_type);
    }
    for (int i = 0; i < func->n_res; ++i) {
        set_method_res_type(proto, i, int_type);
    }

    // Create graph
    ir_entity *ent = new_entity(get_glob_type(), new_id_from_str("_main"), proto);
    ir_graph *irg = new_ir_graph(ent, 2); // 1 local variable

    // Store old irg, and switch to new irg
    //ir_graph *old_irg = get_current_ir_graph(); 
    func->irg = irg;
    set_current_ir_graph(irg);

    // Convert control flow graph
    convert_cfg(func->cfg);
    cfb_walk_successors(cfg_get_start(func->cfg), NULL, mature_cfb, NULL);

    // Reset old irg
    //mature_immBlock(get_r_cur_block(irg));
}

void finalize_convert(cfg_t *cfg) {
    cfb_t *start = cfg_get_start(cfg);
    irg_finalize_cons(get_irn_irg(start->irb));
}
