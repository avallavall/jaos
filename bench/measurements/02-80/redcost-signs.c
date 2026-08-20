/* Do the PUBLISHED reduced costs obey the sign conditions of the PUBLISHED
 * basis?  Nothing checks this: jaos_check_solution recomputes d from y and
 * never reads col_dual, and bench/run.c's digest covers x and y only.
 *
 *   MINIMIZE, at an optimum:  d_j >= 0 at a lower bound
 *                             d_j <= 0 at an upper bound
 *                             d_j == 0 basic or free
 *   MAXIMIZE flips every sign.  A fixed column (lo == hi) accepts any sign.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include "jaos.h"
#include "jaos_internal.h"

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    DIR *dp = opendir(argv[1]);
    if (!dp) return 1;
    struct dirent *e;
    static char names[512][128];
    int n = 0;
    while ((e = readdir(dp)) && n < 512) {
        size_t L = strlen(e->d_name);
        if (L > 4 && strcmp(e->d_name + L - 4, ".mps") == 0)
            snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    }
    closedir(dp);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[128]; snprintf(t, sizeof t, "%s", names[i]);
                snprintf(names[i], sizeof names[0], "%s", names[j]);
                snprintf(names[j], sizeof names[0], "%s", t);
            }
    double worst = 0.0;
    char wn[128] = "";
    int counted = 0, dirty = 0;
    for (int i = 0; i < n; i++) {
        char path[65600];
        snprintf(path, sizeof path, "%s/%s", argv[1], names[i]);
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) continue;
        if (jaos_read_mps(m, path) != JAOS_OK) { jaos_model_free(m); continue; }
        if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m); continue;
        }
        int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = calloc((size_t)nc, sizeof *x);
        double *y = calloc((size_t)nr, sizeof *y);
        double *dcol = calloc((size_t)nc, sizeof *dcol);
        jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
        jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
        (void)jaos_solution(m, x, NULL, y, dcol);
        (void)jaos_basis(m, cs, rs);
        double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double bad = 0.0;
        for (int64_t j = 0; j < nc; j++) {
            double lo = 0.0, hi = 0.0;
            (void)jaos_col_bounds(m, j, &lo, &hi);
            if (lo == hi) continue;              /* fixed: any sign */
            double d = sigma * dcol[j];
            double v = 0.0;
            if (cs[j] == JAOS_BASIS_AT_LOWER)      v = d < 0.0 ? -d : 0.0;
            else if (cs[j] == JAOS_BASIS_AT_UPPER) v = d > 0.0 ?  d : 0.0;
            else                                   v = fabs(d);
            if (v > bad) bad = v;
        }
        counted++;
        if (bad > 1e-7) { dirty++;
            printf("%-14s worst published-reduced-cost sign breach = %.6g\n",
                   names[i], bad); }
        if (bad > worst) { worst = bad; snprintf(wn, sizeof wn, "%s", names[i]); }
        free(x); free(y); free(dcol); free(cs); free(rs);
        jaos_model_free(m);
    }
    printf("-- %d instances, %d above the 1e-7 dual tolerance. worst %.6g on %s\n",
           counted, dirty, worst, wn);
    return 0;
}
