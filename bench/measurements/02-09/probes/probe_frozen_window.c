/* The frozen-row test's window, at a scale where 1e-9 stops being small.
 *
 *   minimize x0
 *   r0:  x0 + x1 == 1e9 + 1
 *   x0 in [0.5, 0.5]  cost 1      (fixed column)
 *   x1 in [0, 1e9]    cost 0      (cost-0 bounded singleton -> relaxes r0)
 *
 * Infeasible by 0.5: x1 would have to be 1e9 + 0.5.
 *
 * The fixed column leaves cur_rl = cur_ru = 1e9 + 0.5. The singleton column
 * relaxes the row and freezes it, leaving cur_rl = 0.5 and no live column, so
 * the frozen-row test sees min_act = max_act = 0 and has to refuse on 0.5.
 * Its window is PRESOLVE_TIGHTEN_EPS * ps_bound_scale(0.5, 1e9 + 0.5), which
 * is 1.0000000005 -- larger than the violation it exists to catch.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

int main(void)
{
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.5, 0.0}, cu[] = {0.5, 1e9};
    const double rl[] = {1e9 + 1.0}, ru[] = {1e9 + 1.0};
    const int64_t as[] = {0, 1, 2};
    const int64_t ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = NULL;
    (void)jaos_model_new(&m);
    (void)jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                       2, as, ai, av);
    (void)jaos_solve(m);

    const int st = (int)jaos_status_of(m);
    double obj = 0.0, x[2], y[1];
    if (jaos_objective(m, &obj) == JAOS_OK) {
        (void)jaos_solution(m, x, NULL, y, NULL);
        jaos_check_report rep;
        (void)jaos_check_solution(m, x, y, 1e-7, &rep);
        printf("  status=%d OPTIMAL obj=%.17g x=[%.17g %.17g]\n", st, obj,
               x[0], x[1]);
        printf("  x1 exceeds its own upper bound by %.17g\n", x[1] - 1e9);
        printf("  checker primal_ok=%d colviol=%.3g rowviol=%.3g\n",
               (int)rep.primal_feasible, rep.max_col_violation,
               rep.max_row_violation);
    } else {
        printf("  status=%d (2=INFEASIBLE, which is the right answer)\n", st);
    }
    jaos_model_free(m);
    return 0;
}
