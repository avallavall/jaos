/* Can the entering column overshoot its OWN opposite bound?
 *
 * min -x - 0.5y  s.t.  x + y <= 10,  x + 2y <= 12
 * x in [0, 1], y in [0, 10].
 *
 * The true optimum is x = 1, y = 5.5 at -4.25 (x + 2y = 12 binds).
 * From the origin the primal prices x first (|d| = 1 against 0.5) and moves it
 * up. Nothing basic blocks before x reaches 10 on the first row, but x's own
 * upper bound is 1. A ratio test that never looks at u_q - l_q walks past it.
 */
#include "jaos.h"
#include "jaos_internal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static char g_last[512];
static void logline(void *u, jaos_log_level l, const char *line)
{
    (void)u; (void)l;
    snprintf(g_last, sizeof g_last, "%s", line);
}

static jaos_model *build(void)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) return nullptr;
    const double c[]  = {-1.0, -0.5};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 10.0};
    const double rl[] = {-INFINITY, -INFINITY};
    const double ru[] = {10.0, 12.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};
    if (jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av) != JAOS_OK) return nullptr;
    return m;
}

static void run(const char *what, bool primal)
{
    jaos_model *m = build();
    /* Both columns on their lower bounds, both logicals basic: the origin.
     * Row activities are 0, inside both upper bounds, so it is primal
     * feasible — and both reduced costs point the wrong way at a lower
     * bound, so there is real primal work. */
    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    if (jaos_set_basis(m, cs, rs) != JAOS_OK) printf("  set_basis REFUSED\n");
    (void)jaos_set_log_callback(m, logline, nullptr);
    (void)jaos_set_log_level(m, JAOS_LOG_SUMMARY);
    m->cfg.force_primal = primal;
    g_last[0] = '\0';
    jaos_status st = jaos_solve(m);

    double obj = 0.0, x[2] = {0, 0}, y[2] = {0, 0};
    (void)jaos_objective(m, &obj);
    (void)jaos_solution(m, x, nullptr, y, nullptr);
    printf("== %s\n   st=%d status=%s obj=%.17g x=(%.17g, %.17g)\n",
           what, (int)st, jaos_solve_status_str(jaos_status_of(m)), obj,
           x[0], x[1]);
    printf("   x within [0,1]? %s      y within [0,10]? %s\n",
           (x[0] >= -1e-9 && x[0] <= 1.0 + 1e-9) ? "yes" : "*** NO ***",
           (x[1] >= -1e-9 && x[1] <= 10.0 + 1e-9) ? "yes" : "*** NO ***");
    if (st == JAOS_OK) {
        jaos_check_report rep;
        if (jaos_check_solution(m, x, y, 1e-6, &rep) == JAOS_OK)
            printf("   checker: primal=%s dual=%s\n",
                   rep.primal_feasible ? "ok" : "*** REFUSED ***",
                   rep.dual_feasible ? "ok" : "*** REFUSED ***");
    } else {
        printf("   err=%s\n", jaos_model_error(m));
    }
    printf("   %s\n", g_last);
    jaos_model_free(m);
}

int main(void)
{
    run("dual   (reference)", false);
    run("primal (under test)", true);
    return 0;
}
