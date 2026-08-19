/* The model the width probe must detect, and the control beside it.
 *
 * Case A destroys the row's width and case B is the same shape at a magnitude
 * that cannot: if the probe reports a loss on both, or on neither, it is not
 * measuring what it claims.
 *
 *   A:  min x1  s.t.  R: 1e17*x0 + x1 in [1, 2],  x0 in [1,1],  x1 in [0,10]
 *
 * x0 is fixed, so the row keeps 1 - 1e17 and 2 - 1e17. ulp(1e17) is 16, so
 * those are the same double and the row reaches the simplex as an equality.
 *
 *   B:  the same with 1e3 in place of 1e17. 1 - 1e3 and 2 - 1e3 are distinct.
 *
 * Built against the JAOS_DIAG probe build; the probe's own stderr line is the
 * observable, not this program's output.
 */
#include <stdio.h>
#include "jaos.h"

static int one(const char *label, double big)
{
    const double c[]  = {0.0, 1.0};
    const double cl[] = {1.0, 0.0};
    const double cu[] = {1.0, 10.0};
    const double rl[] = {1.0};
    const double ru[] = {2.0};
    const int64_t as[] = {0, 1, 2};
    const int64_t ai[] = {0, 0};
    const double av[]  = {big, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 1;
    if (jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av) != JAOS_OK) {
        jaos_model_free(m);
        return 1;
    }
    const jaos_status st = jaos_solve(m);
    printf("%s  a=%g  status=%d  solve=%d\n", label, big, (int)st,
           (int)jaos_status_of(m));
    jaos_model_free(m);
    return 0;
}

int main(void)
{
    /* Case A first, so the two DIAG-WIDTH lines appear in this order. */
    if (one("A destroys", 1e17)) return 2;
    if (one("B control ", 1e3))  return 2;
    return 0;
}
