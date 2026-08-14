#include <stdio.h>
#include <math.h>
#include "jaos.h"

static void run(const char *tag)
{
    const double c[]  = {3.0, 2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};
    const int64_t as[] = {0, 2, 3};
    const int64_t ai[] = {0, 1, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = NULL;
    (void)jaos_model_new(&m);
    (void)jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                       3, as, ai, av);
    (void)jaos_solve(m);

    double obj = 0.0, x[2], y[2], d[2], a[2];
    (void)jaos_objective(m, &obj);
    (void)jaos_solution(m, x, a, y, d);
    printf("%-10s status=%d obj=%.17g x=[%g %g] act=[%g %g] dual=[%.17g %.17g] d=[%.17g %.17g]\n",
           tag, (int)jaos_status_of(m), obj, x[0], x[1], a[0], a[1],
           y[0], y[1], d[0], d[1]);

    jaos_check_report rep;
    (void)jaos_check_solution(m, x, y, 1e-7, &rep);
    printf("%-10s primal_ok=%d dual_ok=%d colviol=%g rowviol=%g dualviol=%g\n",
           tag, (int)rep.primal_feasible, (int)rep.dual_feasible,
           rep.max_col_violation, rep.max_row_violation,
           rep.max_dual_violation);
    jaos_model_free(m);
}

static void empty_col(const char *tag, double lo, double hi)
{
    const double c[]  = {0.0, 1.0};
    const double cl[] = {0.0, lo};
    const double cu[] = {10.0, hi};
    const double rl[] = {0.0}, ru[] = {10.0};
    const int64_t as[] = {0, 1, 1};
    const int64_t ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = NULL;
    (void)jaos_model_new(&m);
    (void)jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                       1, as, ai, av);
    (void)jaos_solve(m);
    double obj = 0.0, x[2];
    jaos_status st = jaos_objective(m, &obj);
    if (st == JAOS_OK) {
        (void)jaos_solution(m, x, NULL, NULL, NULL);
        printf("%-18s status=%d obj=%.17g x=[%g %g]\n", tag,
               (int)jaos_status_of(m), obj, x[0], x[1]);
    } else {
        printf("%-18s status=%d (no optimum available)\n", tag,
               (int)jaos_status_of(m));
    }
    jaos_model_free(m);
}

int main(void)
{
    run("maximise");
    empty_col("emptycol[0,5]", 0.0, 5.0);
    empty_col("emptycol[-inf,5]", -INFINITY, 5.0);
    return 0;
}
