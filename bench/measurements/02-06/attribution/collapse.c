/* Independent re-run of the reviewer's P1: the midpoint-collapse branch.
 *
 *   min  x0 + x1 + x2
 *   r0:  x0            >= 5              tightens x0's lower to 5
 *   r1:  x0            <= 5 - 1e-13      new_lo > new_hi inside btol, so the
 *                                        interval collapses to a midpoint that
 *                                        is no row's implied bound
 *   r2:  x1 + x2       >= 3              survives, keeps the model off the
 *                                        SOLVED path
 *   x0 in [0,10], x1 and x2 in [0,inf), all cost 1.
 *
 * The claim under test: after the collapse, r0's record still carries .lo = 5
 * while x0 is published at the midpoint, so r0 compares unequal and declines.
 * If d0's sign points at the lower side, nothing pays and the reduced cost is
 * left on a column strictly inside its own box. */
#include <math.h>
#include <stdio.h>
#include "jaos.h"

int main(void)
{
    const double c[]  = {1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {10.0, INFINITY, INFINITY};
    const double rl[] = {5.0, -INFINITY, 3.0};
    const double ru[] = {INFINITY, 5.0 - 1e-13, INFINITY};
    const int64_t s[]  = {0, 2, 3, 4};
    const int64_t ix[] = {0, 1, 2, 2};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v) != JAOS_OK) {
        fprintf(stderr, "load failed\n");
        return 2;
    }
    if (jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "solve failed: %s\n", jaos_model_error(m));
        return 2;
    }
    double x[3], y[3], dj[3];
    if (jaos_solution(m, x, NULL, y, dj) != JAOS_OK)
        return 2;

    jaos_check_report r;
    if (jaos_check_solution(m, x, y, 1e-6, &r) != JAOS_OK)
        return 2;

    printf("status=%s\n", jaos_solve_status_str(jaos_status_of(m)));
    printf("x  = %.17g %.17g %.17g\n", x[0], x[1], x[2]);
    printf("y  = %.17g %.17g %.17g\n", y[0], y[1], y[2]);
    printf("dual_feasible=%d primal_feasible=%d maxdual=%.17g maxcol=%.17g\n",
           (int)r.dual_feasible, (int)r.primal_feasible,
           r.max_dual_violation, r.max_col_violation);
    jaos_model_free(m);
    return 0;
}
