#ifndef RANDOM_H
#define RANDOM_H

#include <stdlib.h>

double get_random_percentage(void);
void get_interpolation_prefix_sum_table(int n, double probs[][2], double result[], double factor);

#endif
