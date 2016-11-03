#include <stdlib.h>
#include <assert.h>

#include <libfirm/adt/array.h>
#include "func.h"
#include "cfg.h"

#define FUNC_NAME_LEN 25

static int func_counter = 0;
static int cfg_size = 10;

static int visit_counter = 0;

static func_t* new_func(void) {
    func_t* func = malloc(sizeof(func_t));
    func->calls = NEW_ARR_F(func_t*, 0);
    assert(func != NULL);
    return func;
}

void func_add_call(func_t *func, func_t *callee) {
    ARR_APP1(func_t*, func->calls, callee);
    //printf("%s ;; add call to %s ;; has %d calls\n", func->name, callee->name, ARR_LEN(func->calls));
}

static int func_is_dominated_core(func_t* func, func_t* dom) {
    if (func->visited >= visit_counter) {
        return 0;
    } else {
        func->visited = visit_counter;
    }

    if (func == dom) {
        return 1;
    }

    for (size_t i = 0; i < ARR_LEN(func->calls); ++i) {
        func_t *callee = func->calls[i];

        if (callee == dom) {
            return 1;
        }

        if (func_is_dominated_core(callee, dom)) {
            return 1;
        }
    }

    return 0;
}

int func_is_dominated(func_t* func, func_t* dom) {
    visit_counter++;
    return func_is_dominated_core(func, dom);
}

void destroy_func(func_t *func) {
    destroy_cfg(func->cfg);
    func->cfg = NULL;
    free(func);
}

func_t* new_random_func(int n_params, int n_res) {
    func_t* func = new_func();

    // create unique name for function
    func->name = malloc(FUNC_NAME_LEN);
    func->n_params = n_params;
    func->n_res = n_res;
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
