#ifndef FUNC_H
#define FUNC_H

#include "cfg.h"

typedef struct func_t {
    char* name;
    cfg_t* cfg;
} func_t;

func_t *new_random_func(void);
void destroy_func(func_t *func);
void set_cfg_size(int n);
#endif
