/* The chained model, and what the ANSWER is rather than what the status is.
 *
 * 02-73's CHAIN reads INFEASIBLE where the reference build reads OPTIMAL at
 * 1.1920928955078125e-07. Carrying the error weight stops the refusal. The
 * question this asks is whether that is a repair or a worse defect: a false
 * INFEASIBLE is loud, and an OPTIMAL whose published point violates a row is
 * silent, which is the failure this file's own comments call the mirror image.
 *
 *   row S:  x1 + (256 y_s fixed at 2^-25) == 1e9      x1 in [1e9-1, 1e9+1]
 *   row R:  x1 + w1 + w2 == 1e9 - 63*2^-23            w1, w2 in [0, 2^-23]
 *
 * Feasible exactly at x1 = 1e9 - 2^-17, every y_s = 2^-25, w1 = 2^-23, w2 = 0.
 *
 * It prints the published point and recomputes both rows from it in the
 * highest precision available here, so the residual is the model's and not the
 * solver's own accumulator. CHECK_TOL is 1e-6 absolute.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

#define KS 256
#define SMALL ldexp(1.0, -25)
#define WCAP  ldexp(1.0, -23)

int main(void)
{
    enum { NC = KS + 3, NNZ = KS + 4 };
    static double c[NC], cl[NC], cu[NC], av[NNZ], x[NC];
    static int64_t as[NC + 1], ai[NNZ];
    const double target = 1e9 - 63.0 * WCAP;
    const double rl[] = { 1e9, target }, ru[] = { 1e9, target };
    int64_t nz = 0;

    for (int64_t j = 0; j < NC; j++) {
        as[j] = nz; c[j] = 0.0;
        if (j == 0) {
            cl[j] = 1e9 - 1.0; cu[j] = 1e9 + 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
            ai[nz] = 1; av[nz++] = 1.0;
        } else if (j < KS + 1) {
            cl[j] = cu[j] = SMALL;
            ai[nz] = 0; av[nz++] = 1.0;
        } else {
            c[j] = 1.0; cl[j] = 0.0; cu[j] = WCAP;
            ai[nz] = 1; av[nz++] = 1.0;
        }
    }
    as[NC] = nz;

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av) != JAOS_OK) { printf("LOAD\n"); return 1; }
    if (jaos_solve(m) != JAOS_OK) { printf("SOLVE\n"); return 1; }

    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    (void)jaos_solution(m, x, NULL, NULL, NULL);
    printf("status=%s obj=%.17g\n",
           jaos_solve_status_str(jaos_status_of(m)), obj);
    if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) { jaos_model_free(m); return 0; }

    printf("   x1=%.17g  w1=%.17g  w2=%.17g\n", x[0], x[KS + 1], x[KS + 2]);

    /* Both rows recomputed from the published point. The smalls are summed
     * first and at their own magnitude, so this sum is exact where a
     * column-order one is not -- that is the whole point of the model. */
    double smalls = 0.0;
    for (int64_t j = 1; j < KS + 1; j++)
        smalls += x[j];
    const double actS = smalls + x[0];
    const double actR = x[0] + x[KS + 1] + x[KS + 2];
    printf("   row S activity %.17g  against %.17g  residual %.4g\n",
           actS, 1e9, actS - 1e9);
    printf("   row R activity %.17g  against %.17g  residual %.4g\n",
           actR, target, actR - target);
    printf("   CHECK_TOL is 1e-6 absolute\n");
    jaos_model_free(m);
    return 0;
}
