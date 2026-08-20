/* D162's model at several slacks, with the objective printed.
 * Two claims to check:
 *   - at slack 0 the objective is determined (the reviewer says exactly 1e-7)
 *   - slack 5e-6 is genuinely infeasible and must be refused
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

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
                     nz, as, ai, av) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    if (jaos_solve(m) != JAOS_OK) { printf("%s ERR\n", label); return; }
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    printf("  %-22s %-11s obj=%.17g\n", label,
           jaos_solve_status_str(jaos_status_of(m)), obj);
    jaos_model_free(m);
}

int main(void)
{
    g("slack=0 (feasible)",  0.0);
    g("slack=1e-8",          1e-8);
    g("slack=1e-7",          1e-7);
    g("slack=5e-6",          5e-6);
    g("slack=1e-5",          1e-5);
    g("slack=1e-2 (control)",1e-2);
    return 0;
}
