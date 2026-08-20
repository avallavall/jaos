/* The models the fold probe must detect, and the control beside them.
 *
 * `min x0 s.t. x0 >= rl0, x0 in [0, 1e9]` — one row, one column, so the row is
 * a singleton and the fold decides x0 outright.
 *
 *   A  rl0 = 1e9 + 5e-7   four ulps of 1e9, inside the eight-ulp window: the
 *                         interval collapses and the midpoint is published
 *                         above the column's own upper bound. This is the
 *                         shape tests/test_presolve.c's
 *                         test_a_fold_onto_the_box_at_scale_still_collapses
 *                         documents at 2.4e-7.
 *   B  rl0 = 1e9 + 0.4    past the window: refused as INFEASIBLE, no collapse.
 *   C  rl0 = 1e9 - 1.0    comfortably inside the box: no collapse at all.
 *
 * The probe's own stderr line is the observable. A must report collapse>=1 and
 * out_orig>=1; B and C must report collapse=0.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

static void one(const char *label, double rl0)
{
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {1e9};
    const double rl[] = {rl0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v) != JAOS_OK) {
        printf("%s LOAD\n", label); jaos_model_free(m); return;
    }
    if (jaos_solve(m) != JAOS_OK) {
        printf("%s SOLVE\n", label); jaos_model_free(m); return;
    }
    double x[1] = {0};
    (void)jaos_solution(m, x, NULL, NULL, NULL);
    printf("%-12s rl0=%.17g status=%d x0=%.17g  over_upper=%.6g\n",
           label, rl0, (int)jaos_status_of(m), x[0],
           x[0] > 1e9 ? x[0] - 1e9 : 0.0);
    jaos_model_free(m);
}

int main(void)
{
    one("A-collapse", 1e9 + 5e-7);
    one("B-refused",  1e9 + 0.4);
    one("C-inside",   1e9 - 1.0);
    return 0;
}
