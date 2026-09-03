/* Small models built through the API, looking for one whose solve leaves a
 * column resting on a bound it lent. On the tree with the retirement the
 * DETAIL log says so; on HEAD the same model publishes the bad status.
 *   model-search
 */
#include "jaos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void logger(void *ud, jaos_log_level lv, const char *msg)
{
    (void)ud; (void)lv;
    if (strstr(msg, "lent bounds retired")) printf("      %s\n", msg);
}

static void report(const char *name, jaos_model *m)
{
    printf("  %-30s %s", name, jaos_solve_status_str(jaos_status_of(m)));
    if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) { printf("\n"); return; }
    double obj = 0.0;
    if (jaos_objective(m, &obj) != JAOS_OK) { printf(" (no objective)\n"); return; }
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)nc, sizeof *x), *d = calloc((size_t)nc, sizeof *d);
    double *act = calloc((size_t)(nr ? nr : 1), sizeof *act);
    double *y = calloc((size_t)(nr ? nr : 1), sizeof *y);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)(nr ? nr : 1), sizeof *rs);
    const double INF = jaos_infinity();
    int bad = 0;
    if (jaos_solution(m, x, act, y, d) == JAOS_OK &&
        jaos_basis(m, cs, rs) == JAOS_OK) {
        for (int64_t j = 0; j < nc; j++) {
            double lo = 0.0, up = 0.0;
            if (jaos_col_bounds(m, j, &lo, &up) != JAOS_OK) continue;
            if ((cs[j] == JAOS_BASIS_AT_LOWER && lo <= -INF) ||
                (cs[j] == JAOS_BASIS_AT_UPPER && up >= INF))
                bad++;
        }
        printf("  obj=%-14.10g bad=%d  ", obj, bad);
        for (int64_t j = 0; j < nc; j++)
            printf("x%lld=%g/%d ", (long long)j, x[j], (int)cs[j]);
    }
    printf("\n");
    free(x); free(d); free(act); free(y); free(cs); free(rs);
}

/* Column-wise build. Every array is given; the API takes no nulls. */
static jaos_model *build(int64_t nc, int64_t nr, const double *c,
                         const double *cl, const double *cu,
                         const double *rl, const double *ru,
                         int64_t nz, const int64_t *as, const int64_t *ai,
                         const double *av)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return NULL;
    if (jaos_set_log_callback(m, logger, NULL) != JAOS_OK ||
        jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK ||
        jaos_add_rows(m, nr, rl, ru, 0, nullptr, nullptr, nullptr) != JAOS_OK ||
        jaos_add_cols(m, nc, c, cl, cu, nz, as, ai, av) != JAOS_OK) {
        printf("  build failed: %s\n", jaos_model_error(m));
        jaos_model_free(m);
        return NULL;
    }
    return m;
}

#define INF (jaos_infinity())

/* min -x0 - x1  s.t.  x0 + x1 <= cap; both [0, inf). */
static void two_flat(double cap)
{
    const double c[] = {-1.0, -1.0}, cl[] = {0.0, 0.0}, cu[] = {INF, INF};
    const double rl[] = {-INF}, ru[] = {cap};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    jaos_model *m = build(2, 1, c, cl, cu, rl, ru, 2, as, ai, av);
    if (!m) return;
    if (jaos_solve(m) == JAOS_OK) {
        char nm[64]; snprintf(nm, sizeof nm, "two_flat(cap=%g)", cap);
        report(nm, m);
    }
    jaos_model_free(m);
}

/* min -x0 - x1 - x2  s.t.  x0 + x1 <= 5, x0 + x2 <= 5; all [0, inf). */
static void two_rows(double c2)
{
    const double c[] = {-1.0, -1.0, c2}, cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INF, INF, INF};
    const double rl[] = {-INF, -INF}, ru[] = {5.0, 5.0};
    const int64_t as[] = {0, 2, 3, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = build(3, 2, c, cl, cu, rl, ru, 4, as, ai, av);
    if (!m) return;
    if (jaos_solve(m) == JAOS_OK) {
        char nm[64]; snprintf(nm, sizeof nm, "two_rows(c2=%g)", c2);
        report(nm, m);
    }
    jaos_model_free(m);
}

/* min -x0 - x1  s.t.  x0 + x1 = cap, x1 <= w; x0 [0, inf), x1 [0, w]. */
static void equality(double cap, double w)
{
    const double c[] = {-1.0, -1.0}, cl[] = {0.0, 0.0}, cu[] = {INF, w};
    const double rl[] = {cap}, ru[] = {cap};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    jaos_model *m = build(2, 1, c, cl, cu, rl, ru, 2, as, ai, av);
    if (!m) return;
    if (jaos_solve(m) == JAOS_OK) {
        char nm[64]; snprintf(nm, sizeof nm, "equality(cap=%g,w=%g)", cap, w);
        report(nm, m);
    }
    jaos_model_free(m);
}

/* min -x0 - x1 + 0*x2  s.t.  x0 + x1 <= 5, x2 - x0 <= 0; x2 free above. */
static void trailing(double c2)
{
    const double c[] = {-1.0, -1.0, c2}, cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INF, INF, INF};
    const double rl[] = {-INF, -INF}, ru[] = {5.0, 0.0};
    const int64_t as[] = {0, 2, 3, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, -1.0, 1.0, 1.0};
    jaos_model *m = build(3, 2, c, cl, cu, rl, ru, 4, as, ai, av);
    if (!m) return;
    if (jaos_solve(m) == JAOS_OK) {
        char nm[64]; snprintf(nm, sizeof nm, "trailing(c2=%g)", c2);
        report(nm, m);
    }
    jaos_model_free(m);
}

int main(void)
{
    two_flat(5.0);
    two_flat(0.0);
    two_rows(0.0);
    two_rows(-1.0);
    two_rows(1.0);
    equality(5.0, 2.0);
    equality(5.0, 10.0);
    trailing(-1.0);
    trailing(-0.5);
    return 0;
}
