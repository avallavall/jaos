/* Every instance of a set, by what its checker reports about rows and by
 * what the rows actually carry. `docs/tolerances.md` argues that a row's
 * window has to be relative and uses one instance as the worked example;
 * D261 took the traffic out of that instance, so this is what picks the
 * one that carries the example now.
 *
 * Per instance: the checker's worst absolute row residue and its relative
 * one, the traffic of the row the absolute worst sits in, the two halves
 * of the objective gap, and the objective traffic.
 *
 *   row-census <instance-dir>
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

    printf("# name  rowabs  rowrel  traffic-of-the-worst-row  gap  gappos  gapneg  objtraf\n");
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
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = calloc((size_t)nc, sizeof *x);
        double *act = calloc((size_t)(nr ? nr : 1), sizeof *act);
        double *y = calloc((size_t)(nr ? nr : 1), sizeof *y);
        double *traffic = calloc((size_t)(nr ? nr : 1), sizeof *traffic);
        if (!x || !act || !y || !traffic) return 2;
        if (jaos_solution(m, x, act, y, nullptr) != JAOS_OK) return 2;
        for (int64_t j = 0; j < nc; j++)
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                traffic[m->a_index[k]] += fabs(m->a_value[k] * x[j]);
        double objtraf = 0.0;
        for (int64_t j = 0; j < nc; j++) {
            double c = 0.0;
            if (jaos_col_cost(m, j, &c) != JAOS_OK) return 2;
            objtraf += fabs(c * x[j]);
        }
        jaos_check_report rep;
        if (jaos_check_solution(m, x, y, 1e-6, &rep) != JAOS_OK) return 2;
        /* The row the checker's absolute worst most likely sits in: the
         * one whose own recomputed activity misses its bound by the most,
         * recomputed here the way check.c does, from x. */
        int64_t worst = -1;
        double wv = -1.0;
        for (int64_t i = 0; i < nr; i++) {
            double lo = 0.0, up = 0.0;
            if (jaos_row_bounds(m, i, &lo, &up) != JAOS_OK) return 2;
            double v = 0.0;
            if (isfinite(lo) && lo - act[i] > v) v = lo - act[i];
            if (isfinite(up) && act[i] - up > v) v = act[i] - up;
            if (v > wv) { wv = v; worst = i; }
        }
        printf("%-14s %10.4g %10.4g %12.6g %10.4g %10.4g %10.4g %12.6g\n",
               names[t], rep.max_row_violation,
               rep.max_row_violation_relative,
               worst >= 0 ? traffic[worst] : 0.0,
               rep.objective_gap, rep.gap_positive, rep.gap_negative,
               objtraf);
        free(x); free(act); free(y); free(traffic);
        jaos_model_free(m);
    }
    return 0;
}
