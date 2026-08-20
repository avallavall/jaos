/* |published objective - the checker's long double objective of the same
 * point|, one line per instance, over every .mps in a directory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include "jaos.h"

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    DIR *d = opendir(argv[1]);
    if (!d) return 1;
    struct dirent *e;
    static char names[512][128];
    int n = 0;
    while ((e = readdir(d)) && n < 512) {
        size_t L = strlen(e->d_name);
        if (L > 4 && strcmp(e->d_name + L - 4, ".mps") == 0)
            snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    }
    closedir(d);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[128]; snprintf(t, sizeof t, "%s", names[i]);
                snprintf(names[i], sizeof names[0], "%s", names[j]);
                snprintf(names[j], sizeof names[0], "%s", t);
            }
    for (int i = 0; i < n; i++) {
        char path[65600];
        snprintf(path, sizeof path, "%s/%s", argv[1], names[i]);
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) continue;
        if (jaos_read_mps(m, path) != JAOS_OK) { jaos_model_free(m); continue; }
        if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m); continue;
        }
        double obj = 0.0;
        (void)jaos_objective(m, &obj);
        int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = calloc((size_t)nc, sizeof *x), *y = calloc((size_t)nr, sizeof *y);
        (void)jaos_solution(m, x, NULL, y, NULL);
        jaos_check_report r;
        if (jaos_check_solution(m, x, y, 1e-9, &r) == JAOS_OK)
            printf("%s %.17g %.17g\n", names[i], obj, r.primal_objective);
        free(x); free(y);
        jaos_model_free(m);
    }
    return 0;
}
