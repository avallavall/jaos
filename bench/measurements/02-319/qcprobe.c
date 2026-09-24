#include "jaos_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    jaos_model *m;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_read_qplib(m, argv[1]) != JAOS_OK) return 1;
    const double tol = 1e-7;
    if (jaos_solve(m) != JAOS_OK) return 1;
    printf("status %d err %s\n", m->solve_status, m->err);
    const int64_t n = m->num_col, nr = m->num_row;
    const double *x = m->sol_col, *y = m->sol_dual;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    long double *act = calloc(nr, sizeof *act), *traf = calloc(nr, sizeof *traf);
    for (int64_t j = 0; j < n; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            act[m->a_index[k]] += (long double)m->a_value[k] * x[j];
            traf[m->a_index[k]] += fabsl((long double)m->a_value[k] * x[j]);
        }
    long double *g = calloc(n, sizeof *g);
    for (int64_t i = 0; m->rq_start && i < nr; i++)
        for (int64_t p = m->rq_start[i]; p < m->rq_start[i + 1]; p++) {
            const int64_t r = m->rq_i[p], c = m->rq_j[p];
            const double v = m->rq_v[p];
            long double t = (r == c ? 0.5L : 1.0L) * v * x[r] * x[c];
            act[i] += t;
            traf[i] += fabsl(t);
            g[r] += (long double)y[i] * v * x[c];
            if (r != c) g[c] += (long double)y[i] * v * x[r];
        }
    int64_t bad_rows = 0;
    double worst_row = 0;
    for (int64_t i = 0; i < nr; i++) {
        const double w = sigma * y[i];
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        const double sc = fmax(1.0, (double)traf[i]);
        const bool at_lo = isfinite(lo) && act[i] <= lo + tol * sc;
        const bool at_hi = isfinite(hi) && act[i] >= hi - tol * sc;
        double viol = 0;
        if (fabs(w) > tol) {
            if (w > 0 && !at_lo) viol = w;
            if (w < 0 && !at_hi) viol = -w;
        }
        if (viol > 0) {
            bad_rows++;
            if (viol > worst_row) worst_row = viol;
            if (bad_rows <= 12)
                printf("row %lld quad %d y %.3e act %.12g lo %g hi %g slack %.3e traffic %.3e\n", (long long)i,
                       m->rq_start ? (int)(m->rq_start[i + 1] - m->rq_start[i]) : 0, y[i], (double)act[i], lo, hi,
                       w > 0 ? (double)(act[i] - lo) : (double)(hi - act[i]), (double)traf[i]);
        }
    }
    int64_t bad_cols = 0;
    double worst_col = 0;
    for (int64_t j = 0; j < n; j++) {
        long double d = m->col_cost[j] - g[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            d -= (long double)m->a_value[k] * y[m->a_index[k]];
        const double w = sigma * (double)d;
        const double lo = m->col_lower[j], hi = m->col_upper[j];
        const bool at_lo = isfinite(lo) && x[j] <= lo + tol * fmax(1, fabs(x[j]));
        const bool at_hi = isfinite(hi) && x[j] >= hi - tol * fmax(1, fabs(x[j]));
        double viol = 0;
        if (fabs(w) > tol) {
            if (w > 0 && !at_lo) viol = w;
            if (w < 0 && !at_hi) viol = -w;
        }
        if (viol > 0) {
            bad_cols++;
            if (viol > worst_col) worst_col = viol;
            if (bad_cols <= 6)
                printf("col %lld w %.3e x %.6g lo %g hi %g\n", (long long)j, w, x[j], lo, hi);
        }
    }
    printf("rows refused %lld (worst %.3e), columns refused %lld (worst %.3e)\n", (long long)bad_rows, worst_row, (long long)bad_cols, worst_col);
    int64_t q = 0, act_q = 0; double miny = 1e300, maxy = 0;
    for (int64_t i = 0; m->rq_start && i < nr; i++) {
        if (m->rq_start[i + 1] == m->rq_start[i]) continue;
        q++;
        const double hi = m->row_upper[i];
        if (isfinite(hi) && hi - act[i] < 1e-7 * fmax(1.0, (double)traf[i])) act_q++;
        if (fabs(y[i]) > 0 && fabs(y[i]) < miny) miny = fabs(y[i]);
        if (fabs(y[i]) > maxy) maxy = fabs(y[i]);
    }
    printf("quadratic rows %lld, at their side %lld, |y| from %.3e to %.3e\n", (long long)q, (long long)act_q, miny, maxy);
    return 0;
}
