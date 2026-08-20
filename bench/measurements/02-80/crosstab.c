/* Cross-tabulate two symptoms of the published basis, per instance:
 *
 *   (a) the count promise: exactly num_row of the statuses must be BASIC
 *   (b) a column published BASIC whose published reduced cost is not zero
 *
 * If every instance firing (b) also fails (a), (b) is a second symptom of the
 * known defect and not a new one.
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
    int both = 0, count_only = 0, redcost_only = 0, clean = 0, total = 0;
    printf("%-14s %8s %8s   %s\n", "instance", "off", "d!=0", "");
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
        double *dc = calloc((size_t)nc, sizeof *dc);
        jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
        jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
        (void)jaos_solution(m, NULL, NULL, NULL, dc);
        (void)jaos_basis(m, cs, rs);
        int64_t nb = 0;
        for (int64_t j = 0; j < nc; j++) nb += cs[j] == JAOS_BASIS_BASIC;
        for (int64_t r = 0; r < nr; r++) nb += rs[r] == JAOS_BASIS_BASIC;
        long long off = (long long)(nb - nr);
        int fire = 0;
        for (int64_t j = 0; j < nc; j++)
            if (cs[j] == JAOS_BASIS_BASIC && fabs(dc[j]) > 1e-7) fire++;
        total++;
        if (off != 0 && fire) both++;
        else if (off != 0) count_only++;
        else if (fire) redcost_only++;
        else clean++;
        if (off != 0 || fire)
            printf("%-14s %8lld %8d   %s\n", names[i], off, fire,
                   (off != 0 && fire) ? "both"
                   : (fire ? "REDCOST ONLY -- new" : "count only"));
        free(dc); free(cs); free(rs);
        jaos_model_free(m);
    }
    printf("\n-- %d optimal solves: both %d, count only %d, "
           "REDCOST ONLY %d, clean %d\n",
           total, both, count_only, redcost_only, clean);
    return 0;
}
