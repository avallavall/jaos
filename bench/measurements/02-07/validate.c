/* Calibrating the counter before its number is believed. A model with a known
 * count of removable rows and columns, built so JAOS's live presolve families
 * cannot remove any of it first.
 *
 *   r0:  x0 + 2x1                          <= 10
 *   r1: 2x0 + 4x1                          <= 20     = 2 * r0
 *   r2: 3x0 + 6x1                          <= 30     = 3 * r0
 *   r3:            x2 + 2x3 + x4 + x5      <=  5
 *   r4:           3x2 + 6x3 + x4           <= 12
 *   r5:                 x5 - x6 + x7 + x8  <=  6
 *   r6:                2x6 + x8 + x9       >=  1
 *   r7:                 x7 + x9            in [1, 8]
 *   all x in [0,100], costs 1, 1, 1, 2, 1, 1, -1, 1, 1, 1
 *
 * r0, r1, r2 are three mutually parallel rows, so two are removable.
 * Columns x2 = (1,3) and x3 = (2,6) are parallel with lambda = 2, and the
 * costs match it (c_x3 = 2 = 2 * c_x2), so one column is removable.
 * r3 = (1,2,1,1) and r4 = (3,6,1) are NOT proportional, which is why x4 is
 * there: with only two rows a parallel column pair forces a parallel row pair
 * too, and the two counts could not be told apart.
 * x4 = (1,1) is parallel to neither x2 nor x3, and x8 = (1,1) over {r5,r6}
 * is not parallel to x6 = (-1,2) over the same support.
 *
 * The dual-fixing arm's known answer is seven, and each case is one rule:
 *
 *   x0..x4  candidates at LOWER: every entry positive in a row bounded only
 *           above, every cost >= 0.  (5)
 *   x5      candidate at LOWER, same shape, spanning r3 and r5.  (+1)
 *   x6      candidate at UPPER: cost -1, and raising it only loosens its
 *           rows -- a < 0 in the <=-row r5, a > 0 in the >=-row r6.  (+1)
 *   x7      NOT a candidate: r7 is bounded on both sides, which disqualifies
 *           the column outright. This is the exact shape the counter's first
 *           version got wrong when it called 421615 Kennington columns
 *           fixable, so it is the case this model must refuse.  (+0)
 *   x8      NOT a candidate: senses mixed -- a > 0 in the <=-row r5 kills
 *           the upper direction, a > 0 in the >=-row r6 kills the lower.  (+0)
 *   x9      NOT a candidate: r7 again.  (+0)
 *
 * Every row's bounds sit strictly inside its activity range, so the
 * redundant and forcing rules cannot fire; every row and every column has
 * degree at least two and every cost is nonzero, so the singleton rules
 * cannot either. The counter's own liverows/livecols line proves that held:
 * it must read 8 and 10.
 *
 * EXPECTED: liverows=8 livecols=10 remrow=2/2/2/2 remcol=1/1/1/1 dualfix=7
 */
#include <math.h>
#include <stdio.h>
#include "jaos.h"

int main(void)
{
    const double c[]  = {1.0, 1.0, 1.0, 2.0, 1.0, 1.0, -1.0, 1.0, 1.0, 1.0};
    const double cl[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const double cu[] = {100.0, 100.0, 100.0, 100.0, 100.0,
                         100.0, 100.0, 100.0, 100.0, 100.0};
    const double rl[] = {-INFINITY, -INFINITY, -INFINITY, -INFINITY,
                         -INFINITY, -INFINITY, 1.0, 1.0};
    const double ru[] = {10.0, 20.0, 30.0, 5.0, 12.0, 6.0, INFINITY, 8.0};
    /* CSC. x0:(r0,r1,r2) x1:(r0,r1,r2) x2:(r3,r4) x3:(r3,r4) x4:(r3,r4)
     * x5:(r3,r5) x6:(r5,r6) x7:(r5,r7) x8:(r5,r6) x9:(r6,r7) */
    const int64_t s[]  = {0, 3, 6, 8, 10, 12, 14, 16, 18, 20, 22};
    const int64_t ix[] = {0,1,2,  0,1,2,  3,4,  3,4,  3,4,
                          3,5,  5,6,  5,7,  5,6,  6,7};
    const double  v[]  = {1,2,3,  2,4,6,  1,3,  2,6,  1,1,
                          1,1,  -1,2,  1,1,  1,1,  1,1};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_load_lp(m, 10, 8, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     22, s, ix, v) != JAOS_OK) {
        fprintf(stderr, "load failed\n");
        return 2;
    }
    if (jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "solve failed: %s\n", jaos_model_error(m));
        return 2;
    }
    fprintf(stderr, "EXPECTED liverows=8 livecols=10 remrow=2 remcol=1 "
            "dualfix=7\n");
    jaos_model_free(m);
    return 0;
}
