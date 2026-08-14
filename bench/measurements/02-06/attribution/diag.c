/* Throwaway diagnostic driver for the dual half of the postsolve defect
 * (D100). It was TODO.md's section 1 when this ran; that section is gone now
 * that the defect is closed, so the pointer is to the decision entry, which
 * does not move.
 * Solves one MPS, prints the checker's terms; presolve.c's JAOS_DIAG hook
 * prints one line per JM_PS_SINGLETON_ROW record in replay order.
 * Never built into the repo tree. */
#include <stdio.h>
#include <stdlib.h>
#include "jaos.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: diag <model.mps> [tol]\n");
        return 2;
    }
    const double tol = (argc > 2) ? atof(argv[2]) : 1e-7;

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) {
        fprintf(stderr, "read failed: %s\n", jaos_model_error(m));
        return 2;
    }
    const int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    fprintf(stderr, "== %s nr=%lld nc=%lld tol=%g\n", argv[1],
            (long long)nr, (long long)nc, tol);

    if (jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "solve failed: %s\n", jaos_model_error(m));
        return 2;
    }
    double *x = malloc((size_t)nc * sizeof *x);
    double *d = malloc((size_t)nc * sizeof *d);
    double *y = malloc((size_t)nr * sizeof *y);
    double *act = malloc((size_t)nr * sizeof *act);
    if (!x || !d || !y || !act)
        return 2;
    if (jaos_solution(m, x, act, y, d) != JAOS_OK)
        return 2;

    jaos_check_report r;
    if (jaos_check_solution(m, x, y, tol, &r) != JAOS_OK)
        return 2;

    fprintf(stderr,
            "CHECK status=%s maxdual=%.17g maxrow=%.17g maxcol=%.17g "
            "primal_feasible=%d dual_feasible=%d\n",
            jaos_solve_status_str(jaos_status_of(m)),
            r.max_dual_violation, r.max_row_violation, r.max_col_violation,
            (int)r.primal_feasible, (int)r.dual_feasible);

    free(x); free(d); free(y); free(act);
    jaos_model_free(m);
    return 0;
}
