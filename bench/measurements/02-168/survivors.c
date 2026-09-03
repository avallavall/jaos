/* The models the retirement does NOT clean up, from the family search at
 * 200000. Solved at JAOS_LOG_DETAIL so the retirement's own line says how
 * many it moved and why it left the rest.
 *   survivors
 */
#include "jaos.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INF (jaos_infinity())
static const char *nm[] = {"BASIC", "AT_LOWER", "AT_UPPER", "FREE"};

static void logger(void *ud, jaos_log_level lv, const char *msg)
{
    (void)ud; (void)lv;
    if (strstr(msg, "lent bounds")) printf("   log: %s\n", msg);
}

static void run(const char *name, int64_t nc, int64_t nr, const double *c,
                const double *cl, const double *cu, const double *rl,
                const double *ru, int64_t nz, const int64_t *as,
                const int64_t *ai, const double *av)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return;
    if (jaos_set_log_callback(m, logger, NULL) != JAOS_OK ||
        jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK ||
        jaos_load_lp(m, nc, nr, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av) != JAOS_OK) {
        printf("%s: load failed\n", name); jaos_model_free(m); return;
    }
    printf("%s:\n", name);
    if (jaos_solve(m) != JAOS_OK) { printf("  solve failed\n"); jaos_model_free(m); return; }
    printf("  %s", jaos_solve_status_str(jaos_status_of(m)));
    if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) { printf("\n"); jaos_model_free(m); return; }
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    double *x = calloc((size_t)nc, sizeof *x), *d = calloc((size_t)nc, sizeof *d);
    double *act = calloc((size_t)nr, sizeof *act), *y = calloc((size_t)nr, sizeof *y);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
    (void)jaos_solution(m, x, act, y, d);
    (void)jaos_basis(m, cs, rs);
    printf("  obj=%.17g\n", obj);
    for (int64_t j = 0; j < nc; j++)
        printf("   col %lld %-9s x=%.17g d=%.17g box=[%g,%g] cost=%g\n",
               (long long)j, nm[cs[j]], x[j], d[j], cl[j], cu[j], c[j]);
    jaos_check_report rep;
    if (jaos_check_solution(m, x, y, 1e-7, &rep) == JAOS_OK)
        printf("  check: primal %d dual %d  col %.3g row %.3g dual %.3g gap %.3g\n",
               (int)rep.primal_feasible, (int)rep.dual_feasible,
               rep.max_col_violation, rep.max_row_violation,
               rep.max_dual_violation, rep.objective_gap);
    free(x); free(d); free(act); free(y); free(cs); free(rs);
    jaos_model_free(m);
}

int main(void)
{
    {   /* survivor 1: nc=6 nr=4 */
        const double c[] = {1, 2, 0, 0, -1, -2};
        const double cl[] = {0, 0, 0, 0, 0, 0};
        const double cu[] = {INF, INF, INF, INF, INF, 4};
        const double rl[] = {-INF, -INF, -INF, -INF}, ru[] = {2, 5, 0, 4};
        const int64_t as[] = {0, 4, 8, 11, 14, 18, 21};
        const int64_t ai[] = {0,1,2,3, 0,1,2,3, 1,2,3, 0,1,2, 0,1,2,3, 0,1,2};
        const double av[] = {-2,-1,1,-2, -2,-1,-2,-2, 1,2,-2, 2,2,-1, 2,-2,-1,2, 1,-2,2};
        run("survivor1", 6, 4, c, cl, cu, rl, ru, 21, as, ai, av);
    }
    {   /* survivor 2: nc=5 nr=4 */
        const double c[] = {2, 0, 1, -2, -1};
        const double cl[] = {0, 0, 0, 0, 0};
        const double cu[] = {INF, 1, INF, INF, INF};
        const double rl[] = {-INF, -INF, -INF, -INF}, ru[] = {0, 5, 3, 6};
        const int64_t as[] = {0, 2, 5, 9, 13, 16};
        const int64_t ai[] = {0,3, 0,1,3, 0,1,2,3, 0,1,2,3, 0,1,3};
        const double av[] = {2,1, 1,2,2, -2,-2,-1,1, -1,-2,1,2, 2,-2,-1};
        run("survivor2", 5, 4, c, cl, cu, rl, ru, 16, as, ai, av);
    }
    {   /* survivor 3: nc=4 nr=3, default build only */
        const double c[] = {2, -1, -2, -2};
        const double cl[] = {0, 0, 0, 0};
        const double cu[] = {INF, 6, INF, INF};
        const double rl[] = {-INF, -INF, -INF}, ru[] = {1, 8, 10};
        const int64_t as[] = {0, 3, 6, 9, 12};
        const int64_t ai[] = {0,1,2, 0,1,2, 0,1,2, 0,1,2};
        const double av[] = {-2,-2,2, -2,-1,-2, 2,-1,-2, 1,-1,2};
        run("survivor3", 4, 3, c, cl, cu, rl, ru, 12, as, ai, av);
    }
    return 0;
}
