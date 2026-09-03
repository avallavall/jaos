/* The two small models the family search found (02-168), each solved and
 * each published basis checked against the boxes the model declares. Built
 * against a given src/ tree, so HEAD and the candidate can be compared:
 * HEAD publishes a nonbasic status on a bound the model does not have, the
 * candidate does not.
 *   pinned-models
 */
#include "jaos.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define INF (jaos_infinity())

static const char *nm[] = {"BASIC", "AT_LOWER", "AT_UPPER", "FREE"};

static void run(const char *name, int64_t nc, int64_t nr, const double *c,
                const double *cl, const double *cu, const double *rl,
                const double *ru, int64_t nz, const int64_t *as,
                const int64_t *ai, const double *av)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return;
    if (jaos_load_lp(m, nc, nr, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av) != JAOS_OK ||
        jaos_solve(m) != JAOS_OK) {
        printf("%s: load or solve failed: %s\n", name, jaos_model_error(m));
        jaos_model_free(m);
        return;
    }
    printf("%s: %s", name, jaos_solve_status_str(jaos_status_of(m)));
    if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) { printf("\n"); jaos_model_free(m); return; }
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    double *x = calloc((size_t)nc, sizeof *x), *d = calloc((size_t)nc, sizeof *d);
    double *act = calloc((size_t)nr, sizeof *act), *y = calloc((size_t)nr, sizeof *y);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
    (void)jaos_solution(m, x, act, y, d);
    (void)jaos_basis(m, cs, rs);
    int bad = 0, basics = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] == JAOS_BASIS_BASIC) basics++;
        if ((cs[j] == JAOS_BASIS_AT_LOWER && cl[j] <= -INF) ||
            (cs[j] == JAOS_BASIS_AT_UPPER && cu[j] >= INF)) bad++;
    }
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i] == JAOS_BASIS_BASIC) basics++;
        if ((rs[i] == JAOS_BASIS_AT_LOWER && rl[i] <= -INF) ||
            (rs[i] == JAOS_BASIS_AT_UPPER && ru[i] >= INF)) bad++;
    }
    printf("  obj=%.17g  basics=%d/%lld  bad=%d\n", obj, basics, (long long)nr, bad);
    for (int64_t j = 0; j < nc; j++)
        printf("   col %lld %-9s x=%.17g d=%.17g box=[%g,%g]\n",
               (long long)j, nm[cs[j]], x[j], d[j], cl[j], cu[j]);
    for (int64_t i = 0; i < nr; i++)
        printf("   row %lld %-9s act=%.17g y=%.17g box=[%g,%g]\n",
               (long long)i, nm[rs[i]], act[i], y[i], rl[i], ru[i]);
    free(x); free(d); free(act); free(y); free(cs); free(rs);
    jaos_model_free(m);
}

int main(void)
{
    /* hit 5 of the family search: 3 columns, 4 rows, and column 2 carries
     * the only negative cost with no upper bound, so it is the one the
     * solve lends a bound to. */
    {
        const double c[] = {1.0, 0.0, -1.0};
        const double cl[] = {0.0, 0.0, 0.0};
        const double cu[] = {INF, INF, INF};
        const double rl[] = {-INF, -INF, -INF, -INF};
        const double ru[] = {1.0, 3.0, 1.0, 4.0};
        const int64_t as[] = {0, 3, 5, 9};
        const int64_t ai[] = {0, 1, 2, 1, 2, 0, 1, 2, 3};
        const double av[] = {-2.0, 2.0, 1.0, 1.0, -2.0, 2.0, -2.0, -1.0, -1.0};
        run("hit5", 3, 4, c, cl, cu, rl, ru, 9, as, ai, av);
    }
    /* hit 2: same size, different shape. */
    {
        const double c[] = {0.0, 1.0, -1.0};
        const double cl[] = {0.0, 0.0, 0.0};
        const double cu[] = {INF, INF, INF};
        const double rl[] = {-INF, -INF, -INF, -INF};
        const double ru[] = {5.0, 5.0, 0.0, 5.0};
        const int64_t as[] = {0, 2, 5, 9};
        const int64_t ai[] = {0, 2, 0, 1, 2, 0, 1, 2, 3};
        const double av[] = {1.0, -1.0, -2.0, 2.0, 1.0, 2.0, -2.0, -2.0, -1.0};
        run("hit2", 3, 4, c, cl, cu, rl, ru, 9, as, ai, av);
    }
    return 0;
}
