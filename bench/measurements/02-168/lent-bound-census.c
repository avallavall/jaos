/* Every column or row published nonbasic on a bound the model does not
 * have (D19's lent bound, TODO's lent-bound status item). One line per
 * instance that has any, with each offender's published value and reduced
 * cost, and one total per set.
 *
 *   lent-bound-census <instance-dir>
 */
#define _POSIX_C_SOURCE 200809L
#include "jaos.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp(const void *a, const void *b) { return strcmp(a, b); }

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
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

    const double INF = jaos_infinity();
    int solved = 0, dirty = 0, offenders = 0;
    double worst_d = 0.0;
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
        solved++;
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = malloc((size_t)nc * sizeof *x), *d = malloc((size_t)nc * sizeof *d);
        double *act = malloc((size_t)nr * sizeof *act), *y = malloc((size_t)nr * sizeof *y);
        jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
        jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
        if (!x || !d || !act || !y || !cs || !rs) return 2;
        if (jaos_solution(m, x, act, y, d) != JAOS_OK ||
            jaos_basis(m, cs, rs) != JAOS_OK) {
            printf("%-14s could not read its answer\n", names[t]);
            goto next;
        }
        int here = 0;
        for (int64_t j = 0; j < nc; j++) {
            double lo = 0.0, up = 0.0;
            if (jaos_col_bounds(m, j, &lo, &up) != JAOS_OK) return 2;
            const bool bad = (cs[j] == JAOS_BASIS_AT_LOWER && lo <= -INF) ||
                             (cs[j] == JAOS_BASIS_AT_UPPER && up >= INF);
            if (!bad) continue;
            if (here == 0) printf("%-14s\n", names[t]);
            here++;
            if (fabs(d[j]) > worst_d) worst_d = fabs(d[j]);
            printf("    col %-8lld %s  x=%.17g  d=%.17g  box=[%g, %g]\n",
                   (long long)j,
                   cs[j] == JAOS_BASIS_AT_LOWER ? "AT_LOWER" : "AT_UPPER",
                   x[j], d[j], lo, up);
        }
        for (int64_t i = 0; i < nr; i++) {
            double lo = 0.0, up = 0.0;
            if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK) return 2;
            const bool bad = (rs[i] == JAOS_BASIS_AT_LOWER && lo <= -INF) ||
                             (rs[i] == JAOS_BASIS_AT_UPPER && up >= INF);
            if (!bad) continue;
            if (here == 0) printf("%-14s\n", names[t]);
            here++;
            if (fabs(y[i]) > worst_d) worst_d = fabs(y[i]);
            printf("    row %-8lld %s  act=%.17g  y=%.17g  box=[%g, %g]\n",
                   (long long)i,
                   rs[i] == JAOS_BASIS_AT_LOWER ? "AT_LOWER" : "AT_UPPER",
                   act[i], y[i], lo, up);
        }
        if (here) { dirty++; offenders += here; }
next:
        free(x); free(d); free(act); free(y); free(cs); free(rs);
        jaos_model_free(m);
    }
    printf("-- %d instances: %d optimal, %d with a lent-bound status, "
           "%d offenders, worst |d| %.3g\n", n, solved, dirty, offenders, worst_d);
    return 0;
}
