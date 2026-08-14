#include <stdio.h>
#include <math.h>
#include "jaos.h"

/* Two fixed columns and one equality row they cannot satisfy.
 * row0:  1e9*x0 + 1e9*x1 == 2e9 + GAP,  x0 = x1 = 1 fixed.
 * True activity is 2e9, so the row is violated by GAP for any GAP != 0. */
static void go(double gap)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1.0, 1.0};
    const double cu[] = {1.0, 1.0};
    const double rl[] = {2e9 + gap}, ru[] = {2e9 + gap};
    const int64_t as[] = {0, 1, 2};
    const int64_t ai[] = {0, 0};
    const double av[] = {1e9, 1e9};

    jaos_model *m = NULL;
    (void)jaos_model_new(&m);
    (void)jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                       2, as, ai, av);
    (void)jaos_solve(m);
    double obj = 0.0, x[2], a[1], y[1];
    if (jaos_objective(m, &obj) == JAOS_OK) {
        (void)jaos_solution(m, x, a, y, NULL);
        jaos_check_report rep;
        (void)jaos_check_solution(m, x, y, 1e-7, &rep);
        printf("  gap=%-10g -> OPTIMAL act=%.17g need=%.17g "
               "checker primal_ok=%d rowviol=%.3g\n",
               gap, a[0], 2e9 + gap, (int)rep.primal_feasible,
               rep.max_row_violation);
    } else {
        printf("  gap=%-10g -> status=%d (2=INFEASIBLE)\n",
               gap, (int)jaos_status_of(m));
    }
    jaos_model_free(m);
}

int main(void)
{
    go(0.5);
    go(1.5);
    go(2.5);     /* just past 1e-9 * traffic(2e9) = 2.0 */
    return 0;
}
