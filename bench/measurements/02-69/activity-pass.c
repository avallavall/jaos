/* Two models the review turned up, and the second is a test that could not
 * fail.
 *
 * A — the SAME defect at the activity pass, which this change does not touch.
 *
 *     min x1 + x2  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
 *     x0 in [1,1]            fixed: cur_ru = 0, row_traffic = 1e9
 *     x1 in [1e-10,10] cost 1
 *     x2 in [0,1]      cost 1     <- cost != 0, so R is NEVER frozen
 *
 *   The frozen-row model with one cost changed from 0 to 1. The activity pass
 *   uses `ps_row_tol(&rg)`, which is 8*eps*rg.traffic and knows nothing about
 *   the 1e9 subtracted from `cur_ru`, so it refuses a model the reference
 *   build solves. Its window is shared with FORCING and REDUNDANT, and
 *   widening the forcing window is what cost 02-04 a campaign, so the repair
 *   there is a separate change with its own measurement.
 *
 * B — the model that actually guards the frozen-row widening.
 *
 *     min x1  s.t.  R: 1e9*x0 + x1 == 1e9 + 100
 *     x0 in [1,1]          fixed: cur_rl = cur_ru = 100, row_traffic = 1e9
 *     x1 in [0,3] cost 0, degree 1 -> relaxes R to [97,100], freezes it,
 *                                     and leaves it EMPTY
 *
 *   Genuinely infeasible, every product exact. The row is emptied, so the
 *   simplex has no column left to refuse it with and the frozen-row test is
 *   the last word. Raise PRESOLVE_ROUND_ULPS far enough and this flips to
 *   OPTIMAL — which is what a guard on a widening has to be able to do.
 *
 *   The first version of the negative test used a model whose row keeps a
 *   live column, so the simplex refused it at PRIMAL_TOL whatever the window
 *   did. It read the pipeline, not the window, and could not fail.
 *
 * Build three ways and diff: normally, with -DJAOS_NO_PRESOLVE (the oracle),
 * and with -DJAOS_PRESOLVE_ROUND_ULPS_VALUE=1e12.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

static void model_a(void)
{
    const double c[]  = {0.0, 1.0, 1.0};
    const double cl[] = {1.0, 1e-10, 0.0};
    const double cu[] = {1.0, 10.0,  1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("A ALLOC\n"); return; }
    if (jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v) != JAOS_OK) { printf("A LOAD\n"); return; }
    if (jaos_solve(m) != JAOS_OK) { printf("A SOLVE\n"); return; }
    printf("A-activity-pass  status=%d   (1 optimal, 2 infeasible)\n",
           (int)jaos_status_of(m));
    jaos_model_free(m);
}

static void model_b(void)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1.0, 0.0};
    const double cu[] = {1.0, 3.0};
    const double rl[] = {1e9 + 100.0};
    const double ru[] = {1e9 + 100.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1e9, 1.0};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("B ALLOC\n"); return; }
    if (jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v) != JAOS_OK) { printf("B LOAD\n"); return; }
    if (jaos_solve(m) != JAOS_OK) { printf("B SOLVE\n"); return; }
    printf("B-emptied-row    status=%d   (1 optimal, 2 infeasible)\n",
           (int)jaos_status_of(m));
    jaos_model_free(m);
}

int main(void)
{
    model_a();
    model_b();
    return 0;
}
