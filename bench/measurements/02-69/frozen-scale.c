/* The pair that separates the two windows.
 *
 *   min x1  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
 *                 x0 in [1,1]      fixed: subtracts 1e9, so row_traffic = 1e9
 *                 x2 in [0,1] cost 0, degree 1: relaxes R and FREEZES it
 *                 x1 in [g, 10] cost 1: the surviving column
 *
 * After the fixed column `cur_ru = 0` and `cur_rl = -inf`, so the SHIPPED
 * window is `8 * DBL_EPSILON * max(1, 0) = 1.78e-15` — the row's own bounds
 * say nothing about the 1e9 that was subtracted to reach them. The window
 * that covers the traffic is `8 * DBL_EPSILON * 1e9 = 1.78e-6`.
 *
 * The model is infeasible by exactly g. So:
 *
 *   g = 1e-10   below the traffic window, above the bound-scale one.
 *               1e-10 is about a thousandth of one ulp of 1e9, so it is an
 *               infeasibility the arithmetic cannot represent: the reference
 *               build, which computes the activity directly, calls it
 *               FEASIBLE. Refusing it is the mirror-image catastrophe.
 *   g = 1e-4    840 ulps of 1e9, so it is real and both windows refuse it.
 *
 * Built against the normal build and -DJAOS_NO_PRESOLVE, which is the oracle.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

static void one(double g)
{
    /* x0, x1, x2 */
    const double c[]  = {0.0, 1.0, 0.0};
    const double cl[] = {1.0, g,    0.0};
    const double cu[] = {1.0, 10.0, 1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("g=%-8g ALLOC\n", g); return; }
    if (jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v) != JAOS_OK) {
        printf("g=%-8g LOAD REFUSED\n", g); jaos_model_free(m); return;
    }
    if (jaos_solve(m) != JAOS_OK) {
        printf("g=%-8g SOLVE ERROR\n", g); jaos_model_free(m); return;
    }
    printf("g=%-8g status=%-2d  (1 optimal, 2 infeasible)\n",
           g, (int)jaos_status_of(m));
    jaos_model_free(m);
}

int main(void)
{
    one(1e-10);   /* below the traffic window: must NOT be refused */
    one(1e-4);    /* above it: must be refused */
    return 0;
}
