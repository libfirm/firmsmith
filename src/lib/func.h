#ifndef FUNC_H
#define FUNC_H

#include "cfg.h"

typedef struct func_t {
    char *name;
    cfg_t *cfg;
    ir_graph *irg;
    int n_params;
    int n_res;
} func_t;

func_t *new_random_func(int n_params, int n_res);
void destroy_func(func_t *func);
void set_cfg_size(int n);
#endif
