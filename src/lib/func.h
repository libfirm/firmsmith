#ifndef FUNC_H
#define FUNC_H

#include "cfg.h"

typedef struct func_t {
    char *name;
    cfg_t *cfg;
    ir_graph *irg;
    int n_params;
    int n_res;
    int visited;
    struct func_t **calls;
} func_t;

func_t *new_random_func(int n_params, int n_res);
void destroy_func(func_t *func);
void set_cfg_size(int n);
int func_is_dominated(func_t* func, func_t* dom);
void func_add_call(func_t *func, func_t *callee);
#endif
