#include "jaos_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rest_range(const jaos_model *r, const int64_t *rs, const int64_t *rc,
                       const double *rv, int64_t i, int64_t skip,
                       double *lo, double *hi)
{
    double l = 0.0, h = 0.0;
    for (int64_t k = rs[i]; k < rs[i + 1]; k++) {
        int64_t c = rc[k];
        if (c == skip) continue;
        double a = rv[k], cl = r->col_lower[c], cu = r->col_upper[c];
        if (a > 0) { l += isfinite(cl) ? a * cl : -HUGE_VAL; h += isfinite(cu) ? a * cu : HUGE_VAL; }
        else       { l += isfinite(cu) ? a * cu : -HUGE_VAL; h += isfinite(cl) ? a * cl : HUGE_VAL; }
    }
    *lo = l; *hi = h;
}

int main(int argc, char **argv)
{
    printf("%-12s %6s %6s %6s %6s %6s %6s %6s %8s\n", "name", "rows", "cols", "eq", "dbl", "dblIF", "genIF", "genIF4", "fill4");
    for (int f = 1; f < argc; f++) {
        jaos_model *m;
        if (jaos_model_new(&m) != JAOS_OK || jaos_read_mps(m, argv[f]) != JAOS_OK) { printf("%s read failed\n", argv[f]); continue; }
        jm_presolve p; jm_presolve_init(&p); p.orig = m; jm_work w = {0};
        if (jm_presolve_run(m, &p, &w) != JAOS_OK) { printf("%s presolve failed\n", argv[f]); continue; }
        const jaos_model *r = (p.outcome == JM_PRESOLVE_REDUCED) ? &p.reduced : m;
        int64_t nr = r->num_row, nc = r->num_col;
        int64_t *rs = calloc(nr + 1, sizeof *rs), *rc = malloc((r->num_nz + 1) * sizeof *rc), *cur = malloc((nr + 1) * sizeof *cur);
        double *rv = malloc((r->num_nz + 1) * sizeof *rv);
        for (int64_t j = 0; j < nc; j++) for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++) rs[r->a_index[k] + 1]++;
        for (int64_t i = 0; i < nr; i++) rs[i + 1] += rs[i];
        for (int64_t i = 0; i < nr; i++) cur[i] = rs[i];
        for (int64_t j = 0; j < nc; j++) for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++) { int64_t i = r->a_index[k]; rc[cur[i]] = j; rv[cur[i]] = r->a_value[k]; cur[i]++; }
        int64_t eq = 0, dbl = 0, dblif = 0, genif = 0, genif4 = 0, fill4 = 0;
        for (int64_t i = 0; i < nr; i++) {
            if (!(isfinite(r->row_lower[i]) && r->row_lower[i] == r->row_upper[i])) continue;
            eq++;
            int64_t deg = rs[i + 1] - rs[i];
            if (deg == 2) dbl++;
            double b = r->row_lower[i];
            bool any = false, any4 = false; int64_t bestfill = -1;
            for (int64_t k = rs[i]; k < rs[i + 1]; k++) {
                int64_t j = rc[k]; double a = rv[k];
                if (r->col_integer && r->col_integer[j]) continue;
                int64_t cdeg = r->a_start[j + 1] - r->a_start[j];
                if (cdeg < 2) continue;
                double lo, hi; rest_range(r, rs, rc, rv, i, j, &lo, &hi);
                double il, ih;
                if (a > 0) { il = (b - hi) / a; ih = (b - lo) / a; } else { il = (b - lo) / a; ih = (b - hi) / a; }
                bool implied = (!isfinite(r->col_lower[j]) || il >= r->col_lower[j]) && (!isfinite(r->col_upper[j]) || ih <= r->col_upper[j]);
                if (!implied) continue;
                any = true;
                int64_t fill = (cdeg - 1) * (deg - 1);
                if (cdeg <= 4 && deg <= 4) { any4 = true; if (bestfill < 0 || fill < bestfill) bestfill = fill; }
            }
            if (any) { genif++; if (deg == 2) dblif++; }
            if (any4) { genif4++; fill4 += bestfill; }
        }
        const char *base = strrchr(argv[f], '/'); base = base ? base + 1 : argv[f];
        printf("%-12.12s %6lld %6lld %6lld %6lld %6lld %6lld %6lld %8lld\n", base, (long long)nr, (long long)nc, (long long)eq, (long long)dbl, (long long)dblif, (long long)genif, (long long)genif4, (long long)fill4);
        free(rs); free(rc); free(cur); free(rv);
        jm_presolve_free(&p); jaos_model_free(m);
    }
    return 0;
}
