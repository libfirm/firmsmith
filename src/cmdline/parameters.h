#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <stdbool.h>

typedef struct prog_parameters_t {
    int seed;
    const char* strid;
    bool has_stats;
    bool has_cycles;
    int n_funcs;
} prog_parameters_t;

typedef struct func_parameters_t {
    int max_calls;
} func_parameters_t;

typedef struct cfg_parameters_t {
    int n_blocks;
    bool has_loops;
} cfg_parameters_t;

typedef struct cfb_parameters_t {
    int n_nodes;
    bool has_memory_ops;
    bool has_func_calls;
} cfb_parameters_t;

typedef struct parameters_t {
    prog_parameters_t prog;
    func_parameters_t func;
    cfg_parameters_t cfg;
    cfb_parameters_t cfb;
} parameters_t;

extern parameters_t fs_params;

#endif
