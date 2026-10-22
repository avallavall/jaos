/* The probe behind 02-431's continuous disagreements: each model as it
 * is, then with every quadratic row's finite sides moved out by 1e-4 and
 * every cone's head (both heads of a rotated cone) replaced by a new
 * column fixed by a row at the old head plus 1e-4. Both solves print
 * their status and objective.
 *
 * Usage: coneout MODEL.mps ...
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include <math.h>
#include <stdio.h>

static const char *solve(jaos_model *m, double *obj)
{
    if (jaos_solve(m) != JAOS_OK)
        return "error";
    *obj = 0.0;
    (void)jaos_objective(m, obj);
    return jaos_solve_status_str(jaos_status_of(m));
}

static int shift(jaos_model *r, int64_t col, double eps, int64_t *h)
{
    const double zero = 0.0, inf = jaos_infinity();
    *h = jaos_num_col(r);
    const double ninf = -inf;
    if (jaos_add_cols(r, 1, &zero, &ninf, &inf, 0, nullptr, nullptr, nullptr) != JAOS_OK)
        return 1;
    const int64_t rs[2] = {0, 2}, ri[2] = {*h, col};
    const double rv[2] = {1.0, -1.0};
    return jaos_add_rows(r, 1, &eps, &eps, 2, rs, ri, rv) != JAOS_OK;
}

int main(int argc, char **argv)
{
    const double eps = 1e-4;
    for (int a = 1; a < argc; a++) {
        jaos_model *m = nullptr, *r = nullptr;
        if (jaos_model_new(&m) != JAOS_OK || jaos_read_mps(m, argv[a]) != JAOS_OK ||
            jaos_model_copy(m, &r) != JAOS_OK)
            return 1;
        for (int64_t i = 0; i < jaos_num_row(m); i++) {
            if (jaos_row_quadratic_nz(m, i) == 0)
                continue;
            double lo = 0.0, up = 0.0;
            if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK ||
                jaos_set_row_bounds(r, i, isfinite(lo) ? lo - eps : lo,
                                    isfinite(up) ? up + eps : up) != JAOS_OK)
                return 1;
        }
        const int64_t nk = jaos_num_cones(m);
        int64_t del[16];
        for (int64_t k = 0; k < nk; k++) {
            jaos_cone_type ty;
            int64_t n = 0, cols[64];
            if (jaos_cone(m, k, &ty, &n, cols) != JAOS_OK)
                return 1;
            const int64_t heads = ty == JAOS_CONE_ROTATED ? 2 : 1;
            for (int64_t q = 0; q < heads; q++)
                if (shift(r, cols[q], eps, &cols[q]))
                    return 1;
            if (jaos_add_cone(r, ty, n, cols) != JAOS_OK)
                return 1;
            del[k] = k;
        }
        if (nk > 0 && jaos_delete_cones(r, nk, del) != JAOS_OK)
            return 1;
        double o1 = 0.0, o2 = 0.0;
        const char *s1 = solve(m, &o1);
        const char *s2 = solve(r, &o2);
        printf("%-12s cones %lld | as is: %s %.10g | all moved out: %s %.10g\n",
               argv[a], (long long)nk, s1, o1, s2, o2);
        jaos_model_free(m);
        jaos_model_free(r);
    }
    return 0;
}
