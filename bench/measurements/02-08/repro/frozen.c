/* The frozen-row defect, from TODO.md's standing debts.
 *
 *   min x0  s.t.  x0 + x1 = 100,  x0 in [4,4],  x1 in [0,3]
 *
 * There is no feasible point: x0 is pinned at 4, so x1 would have to be 96
 * against a box of [0,3]. x1 has cost 0 and degree 1, so the cost-0 singleton
 * column family relaxes row0 and freezes it, and nothing revisits a frozen
 * row afterwards.
 *
 * Expected without presolve: INFEASIBLE.
 * Reported with presolve under -DNDEBUG: OPTIMAL, x1 = 96, col violation 93.
 */
#include <math.h>
#include <stdio.h>
#include "jaos.h"

int main(void)
{
    const double c[]  = {1.0, 0.0};
    const double cl[] = {4.0, 0.0};
    const double cu[] = {4.0, 3.0};
    const double rl[] = {100.0};
    const double ru[] = {100.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double  v[]  = {1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v) != JAOS_OK) {
        fprintf(stderr, "load failed\n");
        return 2;
    }
    const jaos_status st = jaos_solve(m);
    printf("solve=%s status=%s\n", jaos_status_str(st),
           jaos_solve_status_str(jaos_status_of(m)));

    if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL) {
        double x[2], y[1];
        if (jaos_solution(m, x, NULL, y, NULL) == JAOS_OK) {
            printf("x0=%.17g x1=%.17g   (x1's own box is [0,3])\n", x[0], x[1]);
            jaos_check_report r;
            if (jaos_check_solution(m, x, y, 1e-6, &r) == JAOS_OK)
                printf("checker: primal_feasible=%d maxcol=%.17g maxrow=%.17g\n",
                       (int)r.primal_feasible, r.max_col_violation,
                       r.max_row_violation);
        }
    }
    jaos_model_free(m);
    return 0;
}
