#include "statistics.h"

void print_cfg_stats(cfg_t *cfg) {
    int n_branches = 0;
    int n_exists = 0;
    
    for (int i = 0; i < cfg->n_blocks; ++i) {
        cfb_t *cfb = cfg->blocks[i];
        n_branches += cfb->n_successors >= 2;
        n_exists   += cfb->n_successors == 0;
    }

    printf("Control flow graph statistics\n");
    printf("=============================\n");
    printf("Branches:    \t%d\n", n_branches);
    printf("Exits:       \t%d\n", n_exists);
    printf("Block total: \t%d\n", cfg->n_blocks);
    printf("\n");
}

unsigned opcodes[iro_last] = {0};

static void op_stats_init() {
    for (int i = 0; i < iro_last; ++i) {
        opcodes[i] = 0;
    }
}

static void op_stats_walker(ir_node *node, void *data) {
    ;//printf("Visiting %s\n", get_op_name(get_irn_op(node)));
    (void)data;
    opcodes[get_irn_opcode(node)] += 1;
}

void print_op_stats(cfg_t *cfg) {
    (void)cfg;
    //op_stats_init();
    //ir_graph *irg = get_current_ir_graph();
    //irg_walk_graph(irg, op_stats_walker, NULL, NULL);
    ;//printf("IR operation statistics\n");
    ;//printf("========================\n");
    for (int i = 0; i < iro_last; ++i) {
        if (opcodes[i] != 0) {
            //ir_op* op = ir_get_opcode(i);
            ;//printf("%s: %d\n", get_op_name(op), opcodes[i]);
        }
    }
    ;//printf("\n");
}

void stats_register_op(unsigned iro) {
    opcodes[iro] += 1;
}
