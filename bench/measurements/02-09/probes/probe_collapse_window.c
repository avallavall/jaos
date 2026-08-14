#include <stdio.h>
#include <math.h>
#include "jaos.h"
/* min x1  s.t.  row0: 1*x1 >= RL (singleton row),  x1 in [0, CU], RL > CU.
 * No feasible point exists.  presolve's collapse branch accepts it whenever
 * RL - CU <= PRESOLVE_TIGHTEN_EPS * max(|RL|,|CU|,1). */
static void go(double rl0, double cu1)
{
    const double c[] = {1.0}, cl[] = {0.0}, cu[] = {cu1};
    const double rl[] = {rl0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};
    jaos_model *m = NULL;
    (void)jaos_model_new(&m);
    (void)jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                       1, as, ai, av);
    (void)jaos_solve(m);
    double obj = 0.0, x[1], a[1], y[1];
    if (jaos_objective(m, &obj) == JAOS_OK) {
        (void)jaos_solution(m, x, a, y, NULL);
        jaos_check_report rep;
        (void)jaos_check_solution(m, x, y, 1e-7, &rep);
        printf("  rl=%.17g cu=%.17g -> OPTIMAL x1=%.17g (cu exceeded by %.3g)"
               " checker primal_ok=%d colviol=%.3g\n", rl0, cu1, x[0],
               x[0] - cu1, (int)rep.primal_feasible, rep.max_col_violation);
    } else {
        printf("  rl=%.17g cu=%.17g -> status=%d (2=INFEASIBLE)\n",
               rl0, cu1, (int)jaos_status_of(m));
    }
    jaos_model_free(m);
}
int main(void)
{
    go(1000000.0005, 1000000.0);
    go(1e9 + 0.4, 1e9);
    go(1.0000000005, 1.0);
    return 0;
}
