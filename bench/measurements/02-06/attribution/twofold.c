/* The minimum case for F2: two singleton rows folding into one column, where
 * the one replayed first is not the one that produced the bound x0 rests on.
 *
 *   min  x0 + x1
 *   r0:  x0        >= 2       singleton, tightens x0's lower to 2
 *   r1:  x0        <= 10      singleton, tightens x0's upper to 10
 *   r2:  x0 + x1   <= 100     survives, keeps x0 from becoming an empty column
 *   x0, x1 in [0, inf)
 *
 * By hand: x0 = 2, x1 = 0, objective 2. r0's activity rests on its own lower
 * bound, so r0 is the row owed the multiplier (y_r0 = 1). r1's activity is 2
 * against (-inf, 10], strictly interior, so y_r1 must be exactly 0.
 * r0 is pushed first, so it replays LAST. */
#include <math.h>
#include <stdio.h>
#include "jaos.h"

int main(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 10.0, 100.0};
    const int64_t s[] = {0, 3, 4};
    const int64_t ix[] = {0, 1, 2, 2};
    const double v[] = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v) != JAOS_OK) {
        fprintf(stderr, "load failed\n");
        return 2;
    }
    if (jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "solve failed: %s\n", jaos_model_error(m));
        return 2;
    }
    double x[2], y[3], dj[2];
    if (jaos_solution(m, x, NULL, y, dj) != JAOS_OK)
        return 2;
    double obj = 0.0;
    if (jaos_objective(m, &obj) != JAOS_OK)
        return 2;

    jaos_check_report r;
    if (jaos_check_solution(m, x, y, 1e-9, &r) != JAOS_OK)
        return 2;

    printf("status=%s obj=%.17g x0=%.17g x1=%.17g\n",
           jaos_solve_status_str(jaos_status_of(m)), obj, x[0], x[1]);
    printf("y0=%.17g y1=%.17g y2=%.17g d0=%.17g d1=%.17g\n",
           y[0], y[1], y[2], dj[0], dj[1]);
    printf("dual_feasible=%d primal_feasible=%d maxdual=%.17g\n",
           (int)r.dual_feasible, (int)r.primal_feasible,
           r.max_dual_violation);
    jaos_model_free(m);
    return 0;
}
