#ifndef RESOLVE_H
#define RESOLVE_H

#include "cfg.h"
#include "prog.h"

void initialize_resolve(void);
void finish_resolve(void);

void resolve_prog(prog_t *prog);

#endif
