/* Ranging over a whole instance set (D258): every instance is solved, the
 * three ranging calls are made on its published basis, and each answer is
 * checked for what jaos.h promises -- the call succeeds, no value is NaN,
 * every current number lies inside its own interval, every nonbasic cost
 * range has the reduced cost's exact end, and the two bounds of a row or
 * column never cross inside their ranges. The count of refusals (a basis
 * the unscaled refactorization calls singular) is the reading the entry
 * needs; a refusal is honest and a wrong interval is not, so the second
 * is what the checks are for.
 *
 *   ranging-population <instance-dir>
 */
#define _POSIX_C_SOURCE 200809L
#include "jaos.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int cmp(const void *a, const void *b) { return strcmp(a, b); }

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    DIR *dp = opendir(argv[1]);
    if (!dp) return 2;
    static char names[512][128];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(dp)) && n < 512)
        if (strstr(e->d_name, ".mps"))
            snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    closedir(dp);
    qsort(names, (size_t)n, sizeof names[0], cmp);

    int solved = 0, ranged = 0, refused = 0, wrong = 0;
    double worst_gap = 0.0;
    for (int t = 0; t < n; t++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", argv[1], names[t]);
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, path) != JAOS_OK || jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            printf("%-14s not optimal, skipped\n", names[t]);
            jaos_model_free(m);
            continue;
        }
        solved++;
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *clo = malloc((size_t)nc * sizeof *clo), *chi = malloc((size_t)nc * sizeof *chi);
        double *rll = malloc((size_t)nr * sizeof *rll), *rlh = malloc((size_t)nr * sizeof *rlh);
        double *rul = malloc((size_t)nr * sizeof *rul), *ruh = malloc((size_t)nr * sizeof *ruh);
        double *bll = malloc((size_t)nc * sizeof *bll), *blh = malloc((size_t)nc * sizeof *blh);
        double *bul = malloc((size_t)nc * sizeof *bul), *buh = malloc((size_t)nc * sizeof *buh);
        double *x = malloc((size_t)nc * sizeof *x), *d = malloc((size_t)nc * sizeof *d);
        double *act = malloc((size_t)nr * sizeof *act), *y = malloc((size_t)nr * sizeof *y);
        jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
        jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
        if (!clo || !chi || !rll || !rlh || !rul || !ruh || !bll || !blh ||
            !bul || !buh || !x || !d || !act || !y || !cs || !rs) return 2;

        const double t0 = now();
        jaos_status s1 = jaos_cost_ranging(m, clo, chi);
        jaos_status s2 = jaos_rhs_ranging(m, rll, rlh, rul, ruh);
        jaos_status s3 = jaos_bound_ranging(m, bll, blh, bul, buh);
        const double secs = now() - t0;
        if (s1 != JAOS_OK || s2 != JAOS_OK || s3 != JAOS_OK) {
            refused++;
            printf("%-14s REFUSED (%d %d %d): %s\n", names[t], (int)s1, (int)s2,
                   (int)s3, jaos_model_error(m));
        } else {
            ranged++;
            (void)jaos_solution(m, x, act, y, d);
            (void)jaos_basis(m, cs, rs);
            int bad = 0;
            double gap = 0.0;
            for (int64_t j = 0; j < nc; j++) {
                double c, l, u;
                (void)jaos_col_cost(m, j, &c);
                (void)jaos_col_bounds(m, j, &l, &u);
                if (isnan(clo[j]) || isnan(chi[j]) || !(clo[j] <= c && c <= chi[j])) bad++;
                if (isnan(bll[j]) || isnan(blh[j]) || isnan(bul[j]) || isnan(buh[j])) bad++;
                if (isfinite(l) && !(bll[j] <= l && l <= blh[j])) bad++;
                if (isfinite(u) && !(bul[j] <= u && u <= buh[j])) bad++;
                if (blh[j] > buh[j] || bll[j] > bul[j]) bad++;
                /* The exact end a nonbasic's reduced cost names. */
                if (cs[j] == JAOS_BASIS_AT_LOWER && l != u && isfinite(clo[j])) {
                    const double g = fabs((c - d[j]) - clo[j]);
                    if (g > gap) gap = g;
                }
            }
            for (int64_t i = 0; i < nr; i++) {
                double l, u;
                (void)jaos_row_bounds(m, i, &l, &u);
                if (isnan(rll[i]) || isnan(rlh[i]) || isnan(rul[i]) || isnan(ruh[i])) bad++;
                if (isfinite(l) && !(rll[i] <= l && l <= rlh[i])) bad++;
                if (isfinite(u) && !(rul[i] <= u && u <= ruh[i])) bad++;
                if (rlh[i] > ruh[i] || rll[i] > rul[i]) bad++;
            }
            if (gap > worst_gap) worst_gap = gap;
            if (bad) wrong++;
            printf("%-14s rows=%lld cols=%lld ranged in %.3fs  checks failed=%d  nonbasic-end gap=%.3g\n",
                   names[t], (long long)nr, (long long)nc, secs, bad, gap);
        }
        free(clo); free(chi); free(rll); free(rlh); free(rul); free(ruh);
        free(bll); free(blh); free(bul); free(buh); free(x); free(d);
        free(act); free(y); free(cs); free(rs);
        jaos_model_free(m);
    }
    printf("-- %d instances: %d solved, %d ranged, %d refused, %d with a failed check; worst nonbasic-end gap %.3g\n",
           n, solved, ranged, refused, wrong, worst_gap);
    return wrong ? 1 : 0;
}
