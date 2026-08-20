/* The 27 firing columns, split by whether the published value rests on one of
 * the column's own bounds.
 *
 *   ON a bound   -> the STATUS is what is wrong; AT_LOWER/AT_UPPER would make
 *                   the published reduced cost dual feasible.
 *   strictly IN  -> the status is right and the published duals disagree with
 *                   the basis, which is a different thing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

int main(int argc, char **argv)
{
    int on_lo = 0, on_up = 0, inside = 0;
    for (int i = 1; i < argc; i++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 1;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) return 1;
        if (jaos_solve(m) != JAOS_OK) return 1;
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) continue;
        int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = calloc((size_t)nc, sizeof *x);
        double *dc = calloc((size_t)nc, sizeof *dc);
        jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
        jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
        (void)jaos_solution(m, x, NULL, NULL, dc);
        (void)jaos_basis(m, cs, rs);
        double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        int a = 0, b = 0, c = 0;
        double worst_in = 0.0;
        for (int64_t j = 0; j < nc; j++) {
            if (cs[j] != JAOS_BASIS_BASIC || fabs(dc[j]) <= 1e-7) continue;
            double lo = 0.0, hi = 0.0;
            (void)jaos_col_bounds(m, j, &lo, &hi);
            double d = sigma * dc[j];
            double tol = 1e-9 * (1.0 + fabs(x[j]));
            if (isfinite(lo) && fabs(x[j] - lo) <= tol) {
                /* dual feasible AT_LOWER means d >= 0 */
                a++; if (d >= 0.0) on_lo++;
            } else if (isfinite(hi) && fabs(x[j] - hi) <= tol) {
                b++; if (d <= 0.0) on_up++;
            } else {
                c++; inside++;
                if (fabs(d) > worst_in) worst_in = fabs(d);
            }
        }
        printf("%-30s on a lower bound %2d, on an upper bound %2d, "
               "strictly inside %2d (worst |d| inside %.6g)\n",
               argv[i], a, b, c, worst_in);
        free(x); free(dc); free(cs); free(rs);
        jaos_model_free(m);
    }
    printf("-- of the columns on a bound, %d at lower and %d at upper would be "
           "DUAL FEASIBLE with the right status; %d are strictly inside\n",
           on_lo, on_up, inside);
    return 0;
}
