#include <stdlib.h>
#include <assert.h>

#include "func.h"
#include "cfg.h"

#define FUNC_NAME_LEN 25

static int func_counter = 0;
static int cfg_size = 10;

static func_t* new_func(void) {
    func_t* func = malloc(sizeof(func_t));
    assert(func != NULL);
    return func;
}

void destroy_func(func_t *func) {
    destroy_cfg(func->cfg);
    func->cfg = NULL;
    free(func);
}

func_t* new_random_func(void) {
    func_t* func = new_func();

    // create unique name for function
    func->name = malloc(FUNC_NAME_LEN);
    snprintf(func->name, FUNC_NAME_LEN, "r_func_%d", func_counter);
    func_counter++;

    // expand functiion graph
    func->cfg = new_cfg();
    for (int i = 0; i < cfg_size; ++i) {
        cfg_expand(func->cfg);
    }

    return func;
}

void set_cfg_size(int n) {
    if (n > MAX_CF_BLOCKS) {
        fprintf(stderr, "Warning: CFG size set to maxiumum %d\n", MAX_CF_BLOCKS);
        cfg_size = MAX_CF_BLOCKS;
    } else {
        cfg_size = n;
    }
}
