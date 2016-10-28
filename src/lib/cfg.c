#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "cfg.h"
#include "cfb.h"


void cfg_register_bb(cfg_t *cfg, int index, cfb_t* block) {
    block->index = index;
    cfg->blocks[index] = block;
}

cfg_t *new_cfg(void) {
    cfg_t *cfg = malloc(sizeof(cfg_t));
    assert(cfg != NULL);
    memset(cfg, 0, sizeof(cfg_t));
    /* Create new start and end block */
    cfb_t *start = new_cfb();
    cfb_t *end = new_cfb();
    cfg_register_bb(cfg, CF_GRAPH_START, start);
    cfg_register_bb(cfg, CF_GRAPH_END, end);
    cfg->n_blocks = 2;
    /* Connect start and end block */
    cfb_add_succ(start, end);

    return cfg;
}

void destroy_cfg(cfg_t *cfg) {
    for (int i = 0; i < cfg->n_blocks; ++i) {
        free(cfg->blocks[i]);
        cfg->blocks[i] = NULL;
    }
}

static cfb_t* cfg_random_block(cfg_t *cfg) {
    int r = rand() % cfg->n_blocks;
    return cfg->blocks[r];
}

void cfg_expand(cfg_t *cfg) {
    cfb_t *start_block = cfg_get_start(cfg);
    cfb_t *end_block = cfg_get_end(cfg);
    //cfb_t *end_block = NULL;
    int cont = 1;
    while (cont) {
        cfb_t *random_block = cfg_random_block(cfg);
        /* Choose random tranformation */
        int trans_nr = rand() % 4;
        /*
        cfg_print(cfg);
        for (int i = 0; i < cfg->n_blocks; ++i) 
            healthy_predecessors(cfg->blocks[i]);
        cfg_print(cfg);
        */

        switch (trans_nr) {
        /* Transformation: T1 */    
        case 0: {
            if (random_block->n_successors == 1 && random_block != start_block) {
                int has_loop = 0;
                cfb_for_each_successor(random_block, succ_it) {
                    cfb_t *succ = cfb_get_successor(random_block, succ_it);
                    if (succ == random_block)
                        has_loop = 1;
                }
                if (!has_loop) {
                    ;//printf("T1\n");
                    
                    cfb_transform_T1(random_block);
                    cont = 0;
                }
            }
            break;
        }
        /* Transformation: T2a */
        case 1: {
            if (random_block != end_block && random_block->n_successors > 0) {
                cfb_t *new_block = cfb_transform_T2a(random_block);
                cfg_register_bb(cfg, cfg->n_blocks++, new_block);
                cont = 0;
            }
            break;
        }
        /* Transformation: T2b */
        case 2: {
            if (random_block != end_block && random_block->n_successors > 0 && random_block->n_successors <= 2) {
                cfb_t *new_block = cfb_transform_T2b(random_block);
                cfg_register_bb(cfg, cfg->n_blocks++, new_block);
                cont = 0;
            }
            break;
        }
        /* Transformation: T2c */
        case 3: {
            if (random_block != end_block && random_block->n_successors < 2) {
                cfb_t *new_block = cfb_transform_T2c(random_block);
                cfg_register_bb(cfg, cfg->n_blocks++, new_block);
                cont = 0;
            }
            break;
        }
        }

        //cfg_print(cfg);
        /*
        for (int i = 0; i < cfg->n_blocks; ++i) 
            healthy_predecessors(cfg->blocks[i]);*/
    }
}

cfb_t* cfg_get_start(cfg_t *cfg) {
    return cfg->blocks[CF_GRAPH_START];
}

cfb_t* cfg_get_end(cfg_t *cfg) {
    return cfg->blocks[CF_GRAPH_END];
}

void cfg_print(cfg_t *cfg) {
    cfb_print(cfg_get_start(cfg));
}
