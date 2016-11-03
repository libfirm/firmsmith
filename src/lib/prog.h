#ifndef PROG_H
#define PROG_H

#include <libfirm/adt/array.h>
#include "func.h"

typedef struct prog_t {
    func_t **funcs;
} prog_t;

prog_t *new_random_prog(void);

#endif
