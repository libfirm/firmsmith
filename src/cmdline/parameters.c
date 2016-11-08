#include "parameters.h"

parameters_t fs_params = {
    .prog = {
        .seed = 0,
        .strid = "main",
        .has_stats = false,
        .has_cycles = true,
        .n_funcs = 1
    },
    .func = {
        .max_calls = 2
    },
    .cfg = {
        .has_loops = true,
        .n_blocks = 5
    },
    .cfb = {
        .n_nodes = 10,
        .has_memory_ops = true,
        .has_func_calls = 1
    }
};
