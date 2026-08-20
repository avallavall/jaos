/* The case that refutes `ps_bound_scale` in clause 1's window.
 *
 * C — the wrong answer the first version of D160 published.
 *
 *     min x0 + x1  s.t.  R: -1e12 <= x0 + x1 <= 0
 *                        x0 in [1e-3, 1],  x1 in [0, 1]
 *
 *   Nothing is ever removed, so `row_traffic[i]` is 0 and `rg.traffic` is
 *   2.001. The model is infeasible by exactly 1e-3.
 *
 *   `ps_bound_scale(-1e12, 0)` is 1e12, so the window becomes 1.78e-3 and
 *   swallows it — and the window comes entirely from the row's LOWER bound
 *   while the test that uses it is on the UPPER side. `ps_bound_scale`'s own
 *   comment says it is the window a comparison between two BOUNDS uses;
 *   clause 1 compares a computed activity against one bound, so the helper
 *   was outside its contract.
 *
 *   Worse than a missed refusal: the row is then handed to FORCING, which
 *   pins its columns and deletes it, so the simplex never sees it either.
 *
 * D — the same shape at a scale where nothing can swallow it, as the control.
 *
 * E — the same-scale reject case for D160's own model: the shortfall raised
 *   from 1e-10 to 1.0 against a window of 1.78e-6. It pins the window from
 *   the tight side, which an accept test cannot.
 *
 * Found by `numerics-reviewer`.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

static void two_col(const char *label, double rlo, double rhi,
                    double x0lo, double x0hi, double x1lo, double x1hi)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {x0lo, x1lo};
    const double cu[] = {x0hi, x1hi};
    const double rl[] = {rlo};
    const double ru[] = {rhi};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    if (jaos_solve(m) != JAOS_OK) { printf("%s SOLVE\n", label); return; }
    const int st = (int)jaos_status_of(m);
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    printf("%-12s status=%d obj=%.17g\n", label, st, obj);
    jaos_model_free(m);
}

/* D160's own model with the shortfall as a parameter. */
static void act(const char *label, double g)
{
    const double c[]  = {0.0, 1.0, 1.0};
    const double cl[] = {1.0, g,    0.0};
    const double cu[] = {1.0, 10.0, 1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v) != JAOS_OK) { printf("%s LOAD\n", label); return; }
    if (jaos_solve(m) != JAOS_OK) { printf("%s SOLVE\n", label); return; }
    const int st = (int)jaos_status_of(m);
    double obj = 0.0, x[3] = {0, 0, 0};
    (void)jaos_objective(m, &obj);
    (void)jaos_solution(m, x, NULL, NULL, NULL);
    printf("%-12s status=%d obj=%.17g x={%g, %.17g, %g}\n",
           label, st, obj, x[0], x[1], x[2]);
    jaos_model_free(m);
}

int main(void)
{
    two_col("C-bound1e12", -1e12, 0.0, 1e-3, 1.0, 0.0, 1.0);
    two_col("D-control",   -1.0,  0.0, 1e-3, 1.0, 0.0, 1.0);
    act("E-reject-1.0", 1.0);
    act("F-accept-1e-10", 1e-10);
    return 0;
}
