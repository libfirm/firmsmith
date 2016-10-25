#ifndef STATISTICS_H
#define STATISTICS_H

#include "cfg.h"

void print_cfg_stats(cfg_t *cfg);
void print_op_stats(cfg_t *cfg);
void stats_register_op(unsigned iro);
#endif
