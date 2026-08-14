/* The canary for PRESOLVE_ROUND_ULPS's sweep.
 *
 * Four models, one grid. Each is a singleton row folding past a column's own
 * upper bound of 1e9, by the conflict named. The window is
 * ULPS * DBL_EPSILON * 1e9 = ULPS * 2.22e-7, so:
 *
 *   conflict   refused at ULPS         accepted at ULPS
 *   3e-7       1                       2 4 8 16 64 256
 *   1e-6       1 2 4                   8 16 64 256
 *   1e-5       1 2 4 8 16              64 256
 *   3e-5       1 2 4 8 16 64           256
 *
 * Every adjacent pair of settings in the grid produces a different line, so
 * a setting that does not reach the binary shows up as a repeated line rather
 * than as nothing at all. That is the whole job: D82's sweep read exactly
 * 1.0000x at five settings because one binary was measured six times, and the
 * table looked perfect.
 *
 * Prints one character per model: R refused, A accepted.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

static char verdict(double conflict)
{
    const double c[] = {1.0}, cl[] = {0.0}, cu[] = {1e9};
    const double rl[] = {1e9 + conflict}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};
    jaos_model *m = NULL;
    (void)jaos_model_new(&m);
    (void)jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                       1, as, ai, av);
    (void)jaos_solve(m);
    const int st = (int)jaos_status_of(m);
    jaos_model_free(m);
    return st == JAOS_SOLVE_INFEASIBLE ? 'R' : 'A';
}

int main(void)
{
    printf("canary %c%c%c%c\n", verdict(3e-7), verdict(1e-6),
           verdict(1e-5), verdict(3e-5));
    return 0;
}
