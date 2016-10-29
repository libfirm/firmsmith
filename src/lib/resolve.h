#ifndef RESOLVE_H
#define RESOLVE_H

#include "cfg.h"

void resolve_cfg_temporaries(cfg_t *cfg);
void set_blocksize(int x);
void resolve_mem_graph(cfg_t *cfg);

#endif
