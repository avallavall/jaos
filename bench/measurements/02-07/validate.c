/* Calibrating the counter before its number is believed. A model with a known
 * count of removable rows and columns, built so JAOS's five live families
 * cannot remove any of it first.
 *
 *   r0:  x0 + 2x1              <= 10
 *   r1: 2x0 + 4x1              <= 20     = 2 * r0
 *   r2: 3x0 + 6x1              <= 30     = 3 * r0
 *   r3:            x2 + 2x3 + x4 <=  5
 *   r4:           3x2 + 6x3 + x4 <= 12
 *   all x in [0,100], costs 1, 1, 1, 2, 1
 *
 * r0, r1, r2 are three mutually parallel rows, so two are removable.
 * Columns x2 = (1,3) and x3 = (2,6) are parallel with lambda = 2, and the
 * costs match it (c_x3 = 2 = 2 * c_x2), so one column is removable.
 * r3 = (1,2,1) and r4 = (3,6,1) are NOT proportional, which is why x4 is
 * there: with only two rows a parallel column pair forces a parallel row pair
 * too, and the two counts could not be told apart.
 * x4 = (1,1) is parallel to neither x2 nor x3.
 *
 * Every row's activity range is far wider than its bounds, so the redundant
 * and forcing rules cannot fire; every column has degree 2 and a nonzero
 * cost, so the singleton rules cannot either.
 *
 * EXPECTED: remrow=2/2/2/2 remcol=1/1/1/1
 */
#include <math.h>
#include <stdio.h>
#include "jaos.h"

int main(void)
{
    const double c[]  = {1.0, 1.0, 1.0, 2.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0, 100.0, 100.0};
    const double rl[] = {-INFINITY, -INFINITY, -INFINITY, -INFINITY, -INFINITY};
    const double ru[] = {10.0, 20.0, 30.0, 5.0, 12.0};
    /* CSC. x0:(r0,r1,r2) x1:(r0,r1,r2) x2:(r3,r4) x3:(r3,r4) x4:(r3,r4) */
    const int64_t s[]  = {0, 3, 6, 8, 10, 12};
    const int64_t ix[] = {0,1,2,  0,1,2,  3,4,  3,4,  3,4};
    const double  v[]  = {1,2,3,  2,4,6,  1,3,  2,6,  1,1};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_load_lp(m, 5, 5, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     12, s, ix, v) != JAOS_OK) {
        fprintf(stderr, "load failed\n");
        return 2;
    }
    if (jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "solve failed: %s\n", jaos_model_error(m));
        return 2;
    }
    fprintf(stderr, "EXPECTED remrow=2 remcol=1\n");
    jaos_model_free(m);
    return 0;
}
