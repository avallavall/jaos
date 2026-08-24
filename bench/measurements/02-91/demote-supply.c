/* Is there a supply of demotable basic variables OUTSIDE the firing row?
 *
 * D141 refused the within-row rule with a count: of the firings that publish
 * a basis one member too long, 66 of 80 and 86 of 152 have no other basic
 * column of that row resting on its own bound, so no rule confined to the row
 * can close the residue. `TODO.md` then asks for "a rank argument WIDER than
 * the firing row", and the first question a wider design has to answer is
 * whether it has anything to work with at all.
 *
 * A candidate here is a basic variable whose PUBLISHED value rests exactly on
 * one of its OWN declared bounds. Demoting such a variable to AT_LOWER or
 * AT_UPPER is status-consistent by itself: the status claims the variable
 * rests on that bound, and it does. Whether the remaining set is still
 * nonsingular is the rank argument, and this program does not attempt it —
 * it counts the supply the argument would draw on.
 *
 * Three tiers, because "exactly" is the strong form and a design may not need
 * it: exact equality, within one ulp of the bound, and within 1e-9 relative.
 * A fixed variable (lo == hi) is excluded: it is nonbasic-eligible on both
 * sides and demoting it says nothing.
 *
 * Public API only. No instrumented build.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <dirent.h>
#include "jaos.h"

static int cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* How near `v` sits to `b`, in the three tiers. Returns 0 exact, 1 within an
 * ulp, 2 within 1e-9 relative, 3 not near. An infinite bound is never near. */
static int nearness(double v, double b)
{
    if (!isfinite(b)) return 3;
    if (v == b) return 0;
    const double d = fabs(v - b);
    if (d <= nextafter(fabs(b), HUGE_VAL) - fabs(b)) return 1;
    if (d <= 1e-9 * fmax(1.0, fabs(b))) return 2;
    return 3;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    DIR *dp = opendir(argv[1]);
    if (!dp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    static char names[512][128];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(dp)) != NULL && n < 512) {
        size_t L = strlen(e->d_name);
        if (L > 4 && strcmp(e->d_name + L - 4, ".mps") == 0)
            snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    }
    closedir(dp);
    qsort(names, (size_t)n, sizeof names[0], cmp);

    printf("# instance    nrow  ncol  nbasic  over  "
           "colExact colUlp colRel  rowExact rowUlp rowRel  "
           "supplyExact supplyUlp supplyRel  covered\n");
    int wrong = 0, coveredExact = 0, coveredUlp = 0, coveredRel = 0, solved = 0;
    for (int i = 0; i < n; i++) {
        char path[65600];
        snprintf(path, sizeof path, "%s/%s", argv[1], names[i]);
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) continue;
        if (jaos_read_mps(m, path) != JAOS_OK) { jaos_model_free(m); continue; }
        if (jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m); continue;
        }
        solved++;
        const int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
        double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof *x);
        double *act = calloc((size_t)(nr > 0 ? nr : 1), sizeof *act);
        jaos_basis_status *cs =
            calloc((size_t)(nc > 0 ? nc : 1), sizeof *cs);
        jaos_basis_status *rs =
            calloc((size_t)(nr > 0 ? nr : 1), sizeof *rs);
        if (!x || !act || !cs || !rs ||
            jaos_solution(m, x, act, NULL, NULL) != JAOS_OK ||
            jaos_basis(m, cs, rs) != JAOS_OK) {
            free(x); free(act); free(cs); free(rs);
            jaos_model_free(m);
            continue;
        }
        int64_t nbasic = 0;
        int64_t ce[3] = {0, 0, 0}, re[3] = {0, 0, 0};
        for (int64_t j = 0; j < nc; j++) {
            if (cs[j] != JAOS_BASIS_BASIC) continue;
            nbasic++;
            double lo = 0.0, hi = 0.0;
            if (jaos_col_bounds(m, j, &lo, &hi) != JAOS_OK) continue;
            if (lo == hi) continue;            /* fixed: says nothing */
            int t = nearness(x[j], lo);
            int u = nearness(x[j], hi);
            int best = t < u ? t : u;
            for (int k = best; k < 3; k++) ce[k]++;
        }
        for (int64_t r = 0; r < nr; r++) {
            if (rs[r] != JAOS_BASIS_BASIC) continue;
            nbasic++;
            double lo = 0.0, hi = 0.0;
            if (jaos_row_bounds(m, r, &lo, &hi) != JAOS_OK) continue;
            if (lo == hi) continue;
            int t = nearness(act[r], lo);
            int u = nearness(act[r], hi);
            int best = t < u ? t : u;
            for (int k = best; k < 3; k++) re[k]++;
        }
        const int64_t over = nbasic - nr;
        if (over > 0) {
            wrong++;
            coveredExact += (ce[0] + re[0]) >= over;
            coveredUlp   += (ce[1] + re[1]) >= over;
            coveredRel   += (ce[2] + re[2]) >= over;
            char nm[128];
            snprintf(nm, sizeof nm, "%s", names[i]);
            char *dot = strrchr(nm, '.');
            if (dot) *dot = '\0';
            printf("%-12s %5lld %5lld %7lld %5lld  "
                   "%8lld %6lld %6lld  %8lld %6lld %6lld  "
                   "%11lld %9lld %9lld  %s\n",
                   nm, (long long)nr, (long long)nc, (long long)nbasic,
                   (long long)over,
                   (long long)ce[0], (long long)ce[1], (long long)ce[2],
                   (long long)re[0], (long long)re[1], (long long)re[2],
                   (long long)(ce[0] + re[0]), (long long)(ce[1] + re[1]),
                   (long long)(ce[2] + re[2]),
                   (ce[0] + re[0]) >= over ? "yes" : "NO");
        }
        free(x); free(act); free(cs); free(rs);
        jaos_model_free(m);
    }
    printf("\n%s: %d solved, %d publish a basis one or more members too long\n",
           argv[1], solved, wrong);
    printf("  the over-count is covered by the model-wide supply on:\n");
    printf("    exact equality with a bound        %d of %d\n", coveredExact, wrong);
    printf("    within one ulp of a bound          %d of %d\n", coveredUlp, wrong);
    printf("    within 1e-9 relative of a bound    %d of %d\n", coveredRel, wrong);
    return 0;
}
