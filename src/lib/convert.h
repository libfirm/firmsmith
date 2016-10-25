#ifndef CONVERT_H
#define CONVERT_H

#include <libfirm/firm.h>

#include "func.h"

void convert_func(func_t* func);
void finalize_convert(cfg_t *cfg);
#endif
