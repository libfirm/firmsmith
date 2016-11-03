#ifndef CONVERT_H
#define CONVERT_H

#include <libfirm/firm.h>

#include "prog.h"
#include "func.h"

void convert_func(func_t* func);
void convert_prog(prog_t* prog);
void finalize_convert(prog_t *prog);
#endif
