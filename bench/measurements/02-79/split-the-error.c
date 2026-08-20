#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "jaos.h"

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 1;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) return 1;
        if (jaos_solve(m) != JAOS_OK) return 1;
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) continue;
        int64_t nc = jaos_num_col(m);
        double *x = calloc((size_t)nc, sizeof *x);
        (void)jaos_solution(m, x, NULL, NULL, NULL);
        double naive = 0.0, s = 0.0, c = 0.0, absum = 0.0, maxt = 0.0;
        long double exact = 0.0L, exact_of_rounded = 0.0L;
        for (int64_t j = 0; j < nc; j++) {
            double cost = 0.0;
            (void)jaos_col_cost(m, j, &cost);
            double t = cost * x[j];
            naive += t;
            double u = s + t;
            c += (fabs(s) >= fabs(t)) ? ((s - u) + t) : ((t - u) + s);
            s = u;
            absum += fabs(t);
            if (fabs(t) > maxt) maxt = fabs(t);
            exact += (long double)cost * (long double)x[j];
            exact_of_rounded += (long double)t;
        }
        printf("%s  n=%lld\n"
               "  naive double        = %.17g\n"
               "  Neumaier double     = %.17g\n"
               "  long double of the ROUNDED products = %.17Lg\n"
               /* NOT exact: a binary64 product needs 106 bits and a long
                * double mantissa holds 64, so each term still rounds. On
                * finnis that is up to 2^-64 * 3.2e12 = 1.73e-07, which is the
                * floor this comparison stops at (D172). */
               "  long double, products rounded to 64 bits = %.17Lg\n"
               "  sum|term| = %.6g   max|term| = %.6g   n*eps*max|t| = %.3g\n",
               argv[i], (long long)nc, naive, s + c,
               exact_of_rounded, exact, absum, maxt,
               (double)nc * DBL_EPSILON * maxt);
        free(x);
        jaos_model_free(m);
    }
    return 0;
}
