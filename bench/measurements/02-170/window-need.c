/* Does any instance still NEED the relative row window?
 *
 * `docs/tolerances.md` argues that the complementary-slackness test on a
 * row must scale by the row's own traffic, because a row activity is a sum
 * whose terms cancel. Its worked example was `finnis` row 3, which carried
 * 4.0e10 of traffic and whose recomputed activity therefore missed its
 * bound by more than an absolute 1e-6 window allows. D261 took that
 * traffic away. So the question is a measurement: with `s` forced to 1,
 * how many rows on the standard set would be reported as violations that
 * the relative window admits?
 *
 * The test is `src/check.c`'s, restated over the public answer:
 *   |y_i| <= tol                          -> no condition
 *   y_i > 0  requires act_i <= lo_i + tol*s
 *   y_i < 0  requires act_i >= hi_i - tol*s
 * with `act` RECOMPUTED from the published x, which is what makes the
 * cancellation real, and `s = max(1, sum_j |a_ij x_j|)` or 1.
 *
 *   window-need <instance-dir> [tol]
 */
#define _POSIX_C_SOURCE 200809L
#include "jaos.h"
#include "jaos_internal.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp(const void *a, const void *b) { return strcmp(a, b); }

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    const double tol = argc > 2 ? strtod(argv[2], NULL) : 1e-6;
    DIR *dp = opendir(argv[1]);
    if (!dp) return 2;
    static char names[512][256];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(dp)) && n < 512)
        if (strstr(e->d_name, ".mps"))
            snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    closedir(dp);
    qsort(names, (size_t)n, sizeof names[0], cmp);

    int dirty = 0, seen = 0;
    double worst_ratio = 0.0;
    char worst_name[256] = "none";
    long long worst_row = -1;
    printf("# rows the ABSOLUTE window would report and the relative one admits,\n"
           "# at tol=%g. `need` is (distance to the bound) / tol, so a row above\n"
           "# 1 is one the absolute window refuses.\n", tol);
    for (int t = 0; t < n; t++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", argv[1], names[t]);
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, path) != JAOS_OK || jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }
        seen++;
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = calloc((size_t)nc, sizeof *x);
        double *y = calloc((size_t)(nr ? nr : 1), sizeof *y);
        double *act = calloc((size_t)(nr ? nr : 1), sizeof *act);
        double *traffic = calloc((size_t)(nr ? nr : 1), sizeof *traffic);
        if (!x || !y || !act || !traffic) return 2;
        if (jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK) return 2;
        /* Recomputed from x, the way check.c does. */
        for (int64_t j = 0; j < nc; j++)
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                act[m->a_index[k]] += m->a_value[k] * x[j];
                traffic[m->a_index[k]] += fabs(m->a_value[k] * x[j]);
            }
        int here = 0;
        for (int64_t i = 0; i < nr; i++) {
            if (fabs(y[i]) <= tol)
                continue;
            double lo = 0.0, hi = 0.0;
            if (jaos_row_bounds(m, i, &lo, &hi) != JAOS_OK) return 2;
            const double s = traffic[i] > 1.0 ? traffic[i] : 1.0;
            double dist;
            if (y[i] > 0.0) dist = act[i] - lo;   /* must be <= tol*s */
            else            dist = hi - act[i];   /* must be >= -tol*s */
            if (!isfinite(dist)) continue;        /* points at an infinite bound */
            if (dist < 0.0) dist = 0.0;
            const double need = dist / tol;       /* > 1 means absolute refuses */
            if (need <= 1.0) continue;
            here++;
            if (need / s > 0.0 && need > worst_ratio) {
                worst_ratio = need;
                snprintf(worst_name, sizeof worst_name, "%s", names[t]);
                worst_row = (long long)i;
            }
            if (here <= 3)
                printf("%-14s row %-6lld y=%-12.6g dist=%-12.6g traffic=%-12.6g "
                       "need=%.4g  admitted-by-relative=%s\n",
                       names[t], (long long)i, y[i], dist, traffic[i], need,
                       dist <= tol * s ? "yes" : "NO");
        }
        if (here) dirty++;
        free(x); free(y); free(act); free(traffic);
        jaos_model_free(m);
    }
    printf("-- %d optimal instances, %d with at least one row the absolute "
           "window refuses; worst need %.4g on %s row %lld\n",
           seen, dirty, worst_ratio, worst_name, worst_row);
    return 0;
}
