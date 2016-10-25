#ifndef CF_GRAPH_H
#define CF_GRAPH_H

#include "cfb.h"

#define MAX_CF_BLOCKS 1000

#define CF_GRAPH_START 0
#define CF_GRAPH_END 1

typedef struct cfg_t {
    int n_blocks;
    cfb_t *blocks[MAX_CF_BLOCKS];
} cfg_t;

cfg_t *new_cfg(void);
void destroy_cfg(cfg_t *cfg);
void cfg_expand(cfg_t *cfg);
void cfg_print(cfg_t *cfg);

cfb_t* cfg_get_start(cfg_t *cfg);
cfb_t* cfg_get_end(cfg_t *cfg);

void cfg_register_bb(cfg_t *cfg, int index, cfb_t* block);

#endif
