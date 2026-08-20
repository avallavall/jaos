/* Does a row that imposed bounds on two of its columns end with two of them
 * RESTED AT?
 *
 * That is the configuration §8d says breaks the rank argument, and it is
 * strictly rarer than the one 02-87 counted: a row can impose on two columns
 * and have neither of them end up there.
 *
 * The patched presolve fills `jm_diag_recs`; this reads it after the solve
 * and compares each imposed bound against the published `x`, which is in the
 * same original column indices.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jaos.h"

struct jm_diag_rec { long long row, col; double val; int eq; };
extern struct jm_diag_rec *jm_diag_recs;
extern long long jm_diag_n, jm_diag_cap, jm_diag_lost;

/* Two thresholds, because a count that turns on one constant is a fit to
 * that constant. The tight one is the solver's own PRIMAL_TOL, the loose one
 * is the gate's CHECK_TOL. */
static const double TOLS[2] = { 1e-7, 1e-6 };

static int rests(double x, double bound, double tol)
{
    const double scale = fabs(x) > 1.0 ? fabs(x) : 1.0;
    return fabs(x - bound) <= tol * scale;
}

int main(int argc, char **argv)
{
    printf("# rows imposing on 2+ columns, and how many of those rows end\n");
    printf("# with 2+ of the imposed columns RESTING at the imposed bound.\n");
    printf("# tight = 1e-7 (PRIMAL_TOL), loose = 1e-6 (CHECK_TOL).\n");
    printf("# %-12s %8s %8s %9s %9s %9s %9s %s\n",
           "instance", "imposed", "rows2+", "rest2+t", "rest2+l",
           "eqrest_t", "eqrest_l", "status");

    long long g_imp = 0, g_r2 = 0, g_rest[2] = {0, 0}, g_eq[2] = {0, 0};

    for (int i = 1; i < argc; i++) {
        char name[128];
        const char *b = strrchr(argv[i], '/');
        snprintf(name, sizeof name, "%s", b ? b + 1 : argv[i]);
        char *dot = strrchr(name, '.');
        if (dot != NULL && strcmp(dot, ".mps") == 0) *dot = '\0';

        jm_diag_n = 0;          /* the array is reused across instances */
        jm_diag_lost = 0;

        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) { jaos_model_free(m); continue; }
        const jaos_status rc = jaos_solve(m);
        const jaos_solve_status ss = jaos_status_of(m);

        const int64_t nc = jaos_num_col(m);
        double *x = calloc((size_t)nc, sizeof *x);
        int ok = (rc == JAOS_OK && ss == JAOS_SOLVE_OPTIMAL && x != NULL &&
                  jaos_solution(m, x, NULL, NULL, NULL) == JAOS_OK);

        /* Group the records by row. Rows are sparse, so a sort would be
         * tidier; a linear scan per distinct row is enough at these sizes and
         * has nothing to get wrong. */
        long long n = jm_diag_n, rows2 = 0, rest2[2] = {0, 0}, eq2[2] = {0, 0};
        char *seen = calloc((size_t)(n > 0 ? n : 1), 1);
        if (seen == NULL) return 2;
        for (long long a = 0; a < n; a++) {
            if (seen[a]) continue;
            const long long row = jm_diag_recs[a].row;
            /* distinct columns of this row that were imposed on, and how
             * many of them rest at the imposed value */
            long long cols = 0, at[2] = {0, 0};
            for (long long c = a; c < n; c++) {
                if (jm_diag_recs[c].row != row) continue;
                seen[c] = 1;
                bool dup = false;
                for (long long e = a; e < c; e++)
                    if (jm_diag_recs[e].row == row &&
                        jm_diag_recs[e].col == jm_diag_recs[c].col) {
                        dup = true; break;
                    }
                if (!dup) cols++;
                if (!ok) continue;
                const long long j = jm_diag_recs[c].col;
                if (j < 0 || j >= nc) continue;
                for (int t = 0; t < 2; t++)
                    if (rests(x[j], jm_diag_recs[c].val, TOLS[t]))
                        at[t]++;
            }
            if (cols > 1) {
                rows2++;
                for (int t = 0; t < 2; t++)
                    if (at[t] > 1) {
                        rest2[t]++;
                        if (jm_diag_recs[a].eq) eq2[t]++;
                    }
            }
        }
        free(seen);

        if (n > 0)
            printf("%-14s %8lld %8lld %9lld %9lld %9lld %9lld %s%s\n",
                   name, n, rows2, rest2[0], rest2[1], eq2[0], eq2[1],
                   rc == JAOS_OK ? jaos_solve_status_str(ss) : "call_failed",
                   jm_diag_lost ? " LOST-RECORDS" : "");
        g_imp += n; g_r2 += rows2;
        for (int t = 0; t < 2; t++) { g_rest[t] += rest2[t]; g_eq[t] += eq2[t]; }

        free(x);
        jaos_model_free(m);
        fflush(stdout);
    }

    printf("\nimposed bounds: %lld\n", g_imp);
    printf("rows imposing on 2+ columns:            %lld\n", g_r2);
    printf("of those, 2+ actually rest there (1e-7): %lld  = %.2f%%\n",
           g_rest[0], g_r2 ? 100.0 * (double)g_rest[0] / (double)g_r2 : 0.0);
    printf("of those, 2+ actually rest there (1e-6): %lld  = %.2f%%\n",
           g_rest[1], g_r2 ? 100.0 * (double)g_rest[1] / (double)g_r2 : 0.0);
    printf("and the row is an equality (1e-7 / 1e-6): %lld / %lld\n",
           g_eq[0], g_eq[1]);
    return 0;
}
