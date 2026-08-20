/* Does the published objective lose terms the way the row activity did?
 *
 *   costs, in column order:  +1e16,  1 (k times),  -1e16
 *   every column fixed at 1, one non-binding row holding them all
 *
 * True objective is k. Summed in column order the running total is 1e16 while
 * the k ones arrive, each below half an ulp of it (ulp(1e16) = 2), so all k
 * are lost and the -1e16 brings the total to 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include "jaos.h"

#ifndef KSMALL
#define KSMALL 256
#endif
#define NC (KSMALL + 2)

int main(void)
{
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double rl[] = {-1e30}, ru[] = {1e30};

    for (int64_t j = 0; j < NC; j++) {
        as[j] = j;
        cl[j] = cu[j] = 1.0;
        ai[j] = 0; av[j] = 1.0;
        c[j] = (j == 0) ? 1e16 : (j == NC - 1) ? -1e16 : 1.0;
    }
    as[NC] = NC;

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     NC, as, ai, av) != JAOS_OK) { printf("LOAD\n"); return 1; }
    if (jaos_solve(m) != JAOS_OK) { printf("SOLVE ERR\n"); return 1; }
    jaos_solve_status st = jaos_status_of(m);
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    printf("k=%d  status=%s  published obj=%.17g  true=%d\n",
           KSMALL, jaos_solve_status_str(st), obj, KSMALL);
    if (st == JAOS_SOLVE_OPTIMAL) {
        double *x = calloc(NC, sizeof *x);
        double *y = calloc(1, sizeof *y);
        if (jaos_solution(m, x, NULL, y, NULL) == JAOS_OK) {
            jaos_check_report r;
            if (jaos_check_solution(m, x, y, 1e-9, &r) == JAOS_OK)
                printf("      checker primal_objective=%.17g  (long double)\n",
                       r.primal_objective);
        }
        free(x); free(y);
    }
    jaos_model_free(m);
    return 0;
}
