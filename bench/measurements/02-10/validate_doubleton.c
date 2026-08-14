/* Calibration for diag_doubleton.inc. An instrument that finds nothing is
 * worth nothing until it has been shown able to find something.
 *
 * Six columns, four rows, and an answer worked out by hand:
 *
 *   min  x0 + x1 + x2 + x3 + x4 + x5
 *   r0:  x0 + x1                     == 5     doubleton equality
 *   r1:            x2 + x3           == 7     doubleton equality
 *   r2:  x0 + x1 + x2 + x3 + x4 + x5 >= 1     degree 6, not an equality
 *   r3:                      x4 + x5 == 3     doubleton equality, x4 free
 *
 *   x0..x3, x5 in [0, 100];  x4 in (-inf, +inf)
 *
 * Expected: eqrows=3 dbl=3 dblfree=1 subnz=6
 *
 * Every column has degree 2, so no singleton-column family can fire. No row
 * has degree 1, so no singleton row can. Every cost is nonzero, so the cost-0
 * families cannot. Every equality row's activity range is far wider than its
 * bound, so nothing is forcing or redundant, and r2's lower bound of 1 sits
 * strictly inside its range. So the five live families remove nothing and the
 * counter sees the model whole -- which is what makes the hand answer the
 * right one to compare against.
 *
 * dblfree is 1 and not 3: only r3 has an endpoint whose own box is infinite
 * on both sides. That is the distinction the whole reading turns on, so the
 * calibration has to separate it rather than confirm a total.
 *
 * subnz is 6: two entries for the endpoint eliminated from each of the three
 * rows. On r0 and r1 both endpoints have degree 2 and the tie goes to the
 * lower index; on r3 the free endpoint is taken because that is the
 * substitution actually available.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

int main(void)
{
    const double c[]  = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, -INFINITY, 0.0};
    const double cu[] = {100.0, 100.0, 100.0, 100.0, INFINITY, 100.0};
    const double rl[] = {5.0, 7.0, 1.0, 3.0};
    const double ru[] = {5.0, 7.0, INFINITY, 3.0};

    /* Column-wise. col j occupies s[j] .. s[j+1]-1.
     * x0: r0,r2   x1: r0,r2   x2: r1,r2   x3: r1,r2   x4: r2,r3   x5: r2,r3 */
    const int64_t s[]  = {0, 2, 4, 6, 8, 10, 12};
    const int64_t ix[] = {0, 2,  0, 2,  1, 2,  1, 2,  2, 3,  2, 3};
    const double v[]   = {1, 1,  1, 1,  1, 1,  1, 1,  1, 1,  1, 1};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_load_lp(m, 6, 4, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     12, s, ix, v) != JAOS_OK) {
        fprintf(stderr, "load failed: %s\n", jaos_model_error(m));
        return 2;
    }
    fprintf(stderr, "EXPECT eqrows=3 dbl=3 dblfree=1 subnz=6\n");
    if (jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "solve failed: %s\n", jaos_model_error(m));
        return 2;
    }
    fprintf(stderr, "status=%s\n", jaos_solve_status_str(jaos_status_of(m)));
    jaos_model_free(m);
    return 0;
}
