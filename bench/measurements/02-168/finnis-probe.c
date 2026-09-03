/* finnis alone, with the retirement's own log line, and the four columns
 * D258's population run found published on a lent bound.
 *   finnis-probe <path-to-finnis.mps>
 */
#include "jaos.h"
#include <stdio.h>
#include <stdlib.h>

static void logger(void *ud, jaos_log_level lv, const char *msg)
{
    (void)ud; (void)lv;
    printf("  log: %s\n", msg);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_set_log_callback(m, logger, NULL) != JAOS_OK) return 2;
    if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) { printf("read: %s\n", jaos_model_error(m)); return 2; }
    if (jaos_solve(m) != JAOS_OK) return 2;
    printf("status %s\n", jaos_solve_status_str(jaos_status_of(m)));
    double obj = 0.0;
    if (jaos_objective(m, &obj) != JAOS_OK) return 2;
    printf("objective %.17g  work %lld  iters %lld\n", obj,
           (long long)jaos_work_units(m), (long long)jaos_iterations(m));
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = malloc((size_t)nc * sizeof *x), *d = malloc((size_t)nc * sizeof *d);
    double *act = malloc((size_t)nr * sizeof *act), *y = malloc((size_t)nr * sizeof *y);
    jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
    jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
    if (!x || !d || !act || !y || !cs || !rs) return 2;
    if (jaos_solution(m, x, act, y, d) != JAOS_OK) return 2;
    if (jaos_basis(m, cs, rs) != JAOS_OK) return 2;
    static const char *nm[] = {"BASIC", "AT_LOWER", "AT_UPPER", "FREE"};
    const int64_t look[] = {6, 12, 13, 14};
    for (int t = 0; t < 4; t++) {
        const int64_t j = look[t];
        double lo = 0.0, up = 0.0;
        if (jaos_col_bounds(m, j, &lo, &up) != JAOS_OK) return 2;
        printf("col %-4lld %-9s x=%.17g d=%.17g box=[%g, %g]\n",
               (long long)j, nm[cs[j]], x[j], d[j], lo, up);
    }
    int64_t basics = 0;
    for (int64_t j = 0; j < nc; j++) if (cs[j] == JAOS_BASIS_BASIC) basics++;
    for (int64_t i = 0; i < nr; i++) if (rs[i] == JAOS_BASIS_BASIC) basics++;
    printf("basics %lld of num_row %lld\n", (long long)basics, (long long)nr);
    jaos_check_report rep;
    const jaos_status ck = jaos_check_solution(m, x, y, 1e-7, &rep);
    printf("check %s  col %.6g  row %.6g  dual %.6g  gap %.6g\n",
           jaos_status_str(ck), rep.max_col_violation, rep.max_row_violation,
           rep.max_dual_violation, rep.objective_gap);
    jaos_model_free(m);
    return 0;
}
