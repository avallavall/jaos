/* The probe behind 02-452: for each model, its status and objective, then
 * for each cone and each quadratic row the largest t, at most 1, by which
 * it can be tightened over the rest of the model with the objective set
 * aside. A cone is tightened by taking t off its head (off both heads of a
 * rotated cone), a quadratic row by moving its finite side in by t. A
 * piece whose largest t is 0 has no point strictly inside. For each such
 * piece the probe's point says whether it lies at the cone's tip.
 *
 * Usage: interior MODEL.mps ...
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *solve(jaos_model *m, double *obj)
{
    (void)jaos_set_log_level(m, JAOS_LOG_OFF);
    if (jaos_solve(m) != JAOS_OK)
        return "error";
    *obj = 0.0;
    (void)jaos_objective(m, obj);
    return jaos_solve_status_str(jaos_status_of(m));
}

static int plain(jaos_model *p)
{
    const int64_t n = jaos_num_col(p);
    for (int64_t j = 0; j < n; j++) {
        double q = 0.0;
        if (jaos_set_col_cost(p, j, 0.0) != JAOS_OK)
            return 1;
        if (jaos_col_quadratic(p, j, &q) == JAOS_OK && q != 0.0 &&
            jaos_set_col_quadratic(p, j, 0.0) != JAOS_OK)
            return 1;
    }
    if (jaos_quadratic_nz(p) > 0 && jaos_set_quadratic(p, 0, nullptr, nullptr, nullptr) != JAOS_OK)
        return 1;
    if (jaos_set_objective_offset(p, 0.0) != JAOS_OK ||
        jaos_set_objective_sense(p, JAOS_MAXIMIZE) != JAOS_OK)
        return 1;
    const double one = 1.0, ninf = -jaos_infinity(), cap = 1.0;
    return jaos_add_cols(p, 1, &one, &ninf, &cap, 0, nullptr, nullptr, nullptr) != JAOS_OK;
}

static int shift_head(jaos_model *p, int64_t col, int64_t t, int64_t *h)
{
    const double zero = 0.0, inf = jaos_infinity(), ninf = -inf;
    *h = jaos_num_col(p);
    if (jaos_add_cols(p, 1, &zero, &ninf, &inf, 0, nullptr, nullptr, nullptr) != JAOS_OK)
        return 1;
    const int64_t rs[2] = {0, 3}, ri[3] = {*h, col, t};
    const double rv[3] = {1.0, -1.0, 1.0}, z = 0.0;
    return jaos_add_rows(p, 1, &z, &z, 3, rs, ri, rv) != JAOS_OK;
}

static void report(jaos_model *p, const char *what, const int64_t *cols,
                   int64_t heads, int64_t n)
{
    double t = 0.0;
    const char *s = solve(p, &t);
    printf(" %s=%s", what, s);
    if (strcmp(s, "optimal") != 0)
        return;
    printf(":%.3g", t);
    if (t > 1e-6 || cols == nullptr)
        return;
    double *x = malloc((size_t)jaos_num_col(p) * sizeof *x);
    if (x == nullptr || jaos_solution(p, x, nullptr, nullptr, nullptr) != JAOS_OK) {
        free(x);
        return;
    }
    double hs = 0.0, ms = 0.0;
    for (int64_t q = 0; q < n; q++) {
        if (q < heads)
            hs += fabs(x[cols[q]]);
        else
            ms += x[cols[q]] * x[cols[q]];
    }
    printf(hs + sqrt(ms) <= 1e-6 ? "(tip)" : "(ray %.3g)", hs);
    free(x);
}

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK || jaos_read_mps(m, argv[a]) != JAOS_OK)
            return 1;
        double o = 0.0;
        const char *s = solve(m, &o);
        printf("%-14s %s %.10g |", argv[a], s, o);
        const int64_t nk = jaos_num_cones(m);
        for (int64_t k = 0; k < nk; k++) {
            jaos_model *p = nullptr;
            jaos_cone_type ty;
            int64_t n = 0, cols[64], orig[64];
            if (jaos_model_copy(m, &p) != JAOS_OK || plain(p) ||
                jaos_cone(p, k, &ty, &n, cols) != JAOS_OK)
                return 1;
            memcpy(orig, cols, sizeof cols);
            const int64_t t = jaos_num_col(p) - 1;
            const int64_t heads = ty == JAOS_CONE_ROTATED ? 2 : 1;
            for (int64_t q = 0; q < heads; q++)
                if (shift_head(p, cols[q], t, &cols[q]))
                    return 1;
            const int64_t del = k;
            if (jaos_add_cone(p, ty, n, cols) != JAOS_OK ||
                jaos_delete_cones(p, 1, &del) != JAOS_OK)
                return 1;
            char what[32];
            snprintf(what, sizeof what, "K%lld", (long long)k + 1);
            report(p, what, orig, heads, n);
            jaos_model_free(p);
        }
        for (int64_t i = 0; i < jaos_num_row(m); i++) {
            if (jaos_row_quadratic_nz(m, i) == 0)
                continue;
            double lo = 0.0, up = 0.0;
            if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK)
                return 1;
            if (isfinite(lo) == isfinite(up))
                continue;
            jaos_model *p = nullptr;
            if (jaos_model_copy(m, &p) != JAOS_OK || plain(p) ||
                jaos_set_coefficient(p, i, jaos_num_col(p) - 1,
                                     isfinite(up) ? 1.0 : -1.0) != JAOS_OK)
                return 1;
            char what[32];
            snprintf(what, sizeof what, "R%lld", (long long)i + 1);
            report(p, what, nullptr, 0, 0);
            jaos_model_free(p);
        }
        printf("\n");
        jaos_model_free(m);
    }
    return 0;
}
