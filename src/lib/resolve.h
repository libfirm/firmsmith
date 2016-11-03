#ifndef RESOLVE_H
#define RESOLVE_H

#include "cfg.h"
#include "prog.h"

void resolve_func(func_t *func);
void resolve_prog(prog_t *prog);
void set_blocksize(int x);
void resolve_mem_graph(cfg_t *cfg);

#endif
