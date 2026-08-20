/* TODO §1's second half: a collapsed record leaves a dual bound no record owns.
 *
 *   min x0 + x1 + x2
 *   r0: x0 >= 5
 *   r1: x0 <= 5 - g          two singleton rows on x0, contradictory by g
 *   r2: x1 + x2 >= 3
 *   x0 in [0, 10]
 *
 * TODO records this at g = 1e-13 with max_dual_violation = 1. The fold's
 * window at scale 5 is 8 * DBL_EPSILON * 5 = 8.88e-15, so 1e-13 is eleven
 * times too wide: both builds refuse the model as INFEASIBLE and the shape
 * never reaches the collapse. It is re-taken here at a g that does.
 *
 * Built before and after the clamp and diffed. The observable is
 * max_dual_violation.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

#define CHECK_TOL 1e-6

static void one(const char *label, double g)
{
    /* x0, x1, x2 */
    const double c[]  = {1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 10.0};
    /* r0: x0 >= 5 ; r1: x0 <= 5 - g ; r2: x1 + x2 >= 3 */
    const double rl[] = {5.0, -INFINITY, 3.0};
    const double ru[] = {INFINITY, 5.0 - g, INFINITY};
    const int64_t s[]  = {0, 2, 3, 4};
    const int64_t ix[] = {0, 1, 2, 2};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v) != JAOS_OK) {
        printf("%-12s LOAD REFUSED\n", label); jaos_model_free(m); return;
    }
    if (jaos_solve(m) != JAOS_OK) {
        printf("%-12s SOLVE ERROR\n", label); jaos_model_free(m); return;
    }
    const int st = (int)jaos_status_of(m);
    if (st != JAOS_SOLVE_OPTIMAL) {
        printf("%-12s g=%-10g status=%d (not optimal)\n", label, g, st);
        jaos_model_free(m);
        return;
    }
    double x[3] = {0}, y[3] = {0};
    (void)jaos_solution(m, x, NULL, y, NULL);
    jaos_check_report rep;
    if (jaos_check_solution(m, x, y, CHECK_TOL, &rep) != JAOS_OK) {
        printf("%-12s CHECK ERROR\n", label); jaos_model_free(m); return;
    }
    printf("%-12s g=%-10g x0=%.17g max_dual_viol=%.6g dual_ok=%d "
           "col_viol=%.6g row_viol=%.6g\n",
           label, g, x[0], rep.max_dual_violation, (int)rep.dual_feasible,
           rep.max_col_violation, rep.max_row_violation);
    jaos_model_free(m);
}

int main(void)
{
    one("TODOs-1e-13", 1e-13);   /* the model as TODO records it */
    one("inside-4e-15", 4e-15);  /* inside the window, so it collapses */
    one("inside-1e-15", 1e-15);
    return 0;
}
