#include "random.h"

double get_random_percentage(void) {
    return (double)rand()/(((double)RAND_MAX)/100.0);
}

void get_interpolation_prefix_sum_table(int n, double probs[][2], double result[], double factor) {
    double prefix_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        prefix_sum += probs[i][0] + (probs[i][1] - probs[i][0]) * factor;
        result[i] = prefix_sum;
    }
}
