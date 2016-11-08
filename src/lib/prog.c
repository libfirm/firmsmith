#include <libfirm/adt/array.h>

#include "../cmdline/parameters.h"
#include "prog.h"

prog_t *new_random_prog(void) {
    int n_funcs = fs_params.prog.n_funcs;
    prog_t *prog = malloc(sizeof(prog_t));
    prog->funcs = NEW_ARR_F(func_t*, n_funcs);
    for (int i = 0; i < n_funcs; ++i) {
        func_t *func = new_random_func(1, 1);
        if (i == 0) {
            func->name = "_main";
        }
        prog->funcs[i] = func;
    }
    return prog;
}

/**
  * Returns random function associated with program,
  * which is not the main function.
  **/
func_t *prog_get_random_func(prog_t* prog) {
    func_t *func = NULL;
    if (ARR_LEN(prog->funcs) > 1) {
        int index = (rand() % (ARR_LEN(prog->funcs) - 1)) + 1;
        func = prog->funcs[index];
        assert(func);
    }
    return func;
}
