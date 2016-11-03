#include "prog.h"

prog_t *new_random_prog(void) {
    prog_t *prog = malloc(sizeof(prog_t));
    prog->funcs = NEW_ARR_F(func_t*, 0);
    func_t *func_main = new_random_func(1, 1);
    func_main->name = "_main";
    ARR_APP1(func_t*, prog->funcs, func_main);
    return prog;
}
