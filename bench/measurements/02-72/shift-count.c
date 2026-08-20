/* Does the shift count change what presolve decides on the model built for it?
 *
 *   row R:  x0 + x1 + (256 smalls) + w1 + w2  ==  2^-17 + 1e-7
 *   row S:  x1 + z                            == -1e9
 *
 *   x0     fixed at +1e9
 *   x1     in [-1e9-1, -1e9+1], not fixed at load time
 *   smalls fixed at 2^-25, a quarter of an ulp of 1e9
 *   w1, w2 in [0, 2e-7], cost 1, so no family relaxes them
 *   z      fixed at 0, which is what delays x1 by one round
 *
 * Exact activity of the fixed part: 1e9 + 256*2^-25 - 1e9 = 2^-17, so the row
 * is met with w1 + w2 = 1e-7 and the model has an exactly representable
 * feasible point.
 *
 * Presolve removes x0 and all 256 smalls in round 1, while x1 is still free.
 * Each small is a quarter of an ulp of an accumulator of magnitude 1e9 and
 * rounds away. Round 2 folds row S, fixes x1 and subtracts it, and cur_rl
 * comes back to 2^-17 + 1e-7 where the truth is 1e-7.
 *
 * The error is 2^-17 = 7.63e-6 against a shipped window of
 * 8 * DBL_EPSILON * 2e9 = 3.55e-6, so clause 1 of the activity pass reads
 * max_act < cur_rl - itol and returns INFEASIBLE. With the 258 shifts counted
 * the window is 1.18e-4 and it does not.
 *
 * B is the control: the same shape 1e-2 away from feasible, which must be
 * refused whatever the count says.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

#ifndef KSMALL
#define KSMALL 256
#endif
#define NC (KSMALL + 5)
#define NNZ (KSMALL + 6)

static void g(const char *label, double slack)
{
    static double c[NC], cl[NC], cu[NC], av[NNZ];
    static int64_t as[NC + 1], ai[NNZ];
    const double t = ldexp(1.0, -25);
    const double T = (double)KSMALL * t + 1e-7 + slack;
    const double rl[] = { T, -1e9 };
    const double ru[] = { T, -1e9 };
    int64_t nz = 0;

    for (int64_t j = 0; j < NC; j++) {
        as[j] = nz;
        c[j] = 0.0;
        if (j == 0) {
            cl[j] = cu[j] = 1e9;
            ai[nz] = 0; av[nz++] = 1.0;
        } else if (j == 1) {
            cl[j] = -1e9 - 1.0; cu[j] = -1e9 + 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
            ai[nz] = 1; av[nz++] = 1.0;
        } else if (j < KSMALL + 2) {
            cl[j] = cu[j] = t;
            ai[nz] = 0; av[nz++] = 1.0;
        } else if (j < KSMALL + 4) {
            cl[j] = 0.0; cu[j] = 2e-7; c[j] = 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
        } else {
            cl[j] = cu[j] = 0.0;
            ai[nz] = 1; av[nz++] = 1.0;
        }
    }
    as[NC] = nz;

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av) != JAOS_OK) {
        printf("%s LOAD\n", label); return;
    }
    jm_presolve p;
    jm_presolve_init(&p);
    if (jm_presolve_run(m, &p, NULL) != JAOS_OK) {
        printf("%s RUN\n", label); return;
    }
    printf("%-22s presolve outcome=%-11s", label,
           p.outcome == JM_PRESOLVE_INFEASIBLE ? "INFEASIBLE" : "not refused");
    jm_presolve_free(&p);
    if (jaos_solve(m) != JAOS_OK) { printf(" solve=ERR\n"); return; }
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    printf(" solve=%-11s obj=%g\n",
           jaos_solve_status_str(jaos_status_of(m)), obj);
    jaos_model_free(m);
}

int main(void)
{
    g("A-feasible-exactly", 0.0);
    g("B-control-infeas", 1e-2);
    return 0;
}
