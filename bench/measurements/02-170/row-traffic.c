/* One instance's rows, by what they carry and by how far they miss their
 * bound. `docs/tolerances.md` uses three figures about `finnis` to argue
 * why a row's window is relative and not absolute, and D261 moved the
 * point those figures were read from. This is what reads them again.
 *
 * For each row: the traffic `sum_j |a_ij x_j|`, the activity, the bound it
 * is nearest, the absolute residue, and the residue in ulps of the
 * traffic. Prints the worst few and any row named on the command line.
 * Also prints `jaos_check_solution`'s own report for the whole model.
 *
 *   row-traffic <instance.mps> [row ...]
 */
#define _POSIX_C_SOURCE 200809L
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) {
        printf("read: %s\n", jaos_model_error(m));
        return 2;
    }
    if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
        printf("not optimal\n");
        return 2;
    }
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)nc, sizeof *x);
    double *act = calloc((size_t)nr, sizeof *act);
    double *y = calloc((size_t)nr, sizeof *y);
    double *traffic = calloc((size_t)nr, sizeof *traffic);
    if (!x || !act || !y || !traffic) return 2;
    if (jaos_solution(m, x, act, y, nullptr) != JAOS_OK) return 2;

    /* The traffic of each row, over the model as loaded. */
    for (int64_t j = 0; j < nc; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            traffic[m->a_index[k]] += fabs(m->a_value[k] * x[j]);

    double objtraf = 0.0;
    for (int64_t j = 0; j < nc; j++) {
        double c = 0.0;
        if (jaos_col_cost(m, j, &c) != JAOS_OK) return 2;
        objtraf += fabs(c * x[j]);
    }

    printf("%s: %lld rows, %lld cols\n", argv[1], (long long)nr, (long long)nc);
    printf("objective traffic sum|c_j x_j| = %.6g\n", objtraf);

    /* The worst absolute residue, and the worst relative one. */
    int64_t wa = -1, wr = -1;
    double wav = -1.0, wrv = -1.0;
    for (int64_t i = 0; i < nr; i++) {
        double lo = 0.0, up = 0.0;
        if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK) return 2;
        double v = 0.0;
        if (isfinite(lo) && lo - act[i] > v) v = lo - act[i];
        if (isfinite(up) && act[i] - up > v) v = act[i] - up;
        const double s = traffic[i] > 1.0 ? traffic[i] : 1.0;
        if (v > wav) { wav = v; wa = i; }
        if (v / s > wrv) { wrv = v / s; wr = i; }
    }
    printf("worst absolute residue: row %lld, %.6g (traffic %.6g)\n",
           (long long)wa, wav, wa >= 0 ? traffic[wa] : 0.0);
    printf("worst relative residue: row %lld, %.6g (traffic %.6g)\n",
           (long long)wr, wrv, wr >= 0 ? traffic[wr] : 0.0);

    for (int a = 2; a < argc; a++) {
        const int64_t i = strtoll(argv[a], NULL, 10);
        if (i < 0 || i >= nr) { printf("row %lld out of range\n", (long long)i); continue; }
        double lo = 0.0, up = 0.0;
        if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK) return 2;
        double v = 0.0;
        if (isfinite(lo) && lo - act[i] > v) v = lo - act[i];
        if (isfinite(up) && act[i] - up > v) v = act[i] - up;
        const double ulp = nextafter(traffic[i], HUGE_VAL) - traffic[i];
        printf("row %-5lld traffic=%.6g act=%.17g box=[%g, %g] residue=%.6g "
               "one-ulp-of-traffic=%.6g  y=%.6g\n",
               (long long)i, traffic[i], act[i], lo, up, v, ulp, y[i]);
    }

    jaos_check_report rep;
    if (jaos_check_solution(m, x, y, 1e-6, &rep) != JAOS_OK) return 2;
    printf("check: row=%.6g rowrel=%.6g col=%.6g dual=%.6g gap=%.6g "
           "gappos=%.6g gapneg=%.6g cert=%s\n",
           rep.max_row_violation, rep.max_row_violation_relative,
           rep.max_col_violation, rep.max_dual_violation, rep.objective_gap,
           rep.gap_positive, rep.gap_negative,
           rep.gap_certified ? "yes" : "no");
    jaos_model_free(m);
    free(x); free(act); free(y); free(traffic);
    return 0;
}
