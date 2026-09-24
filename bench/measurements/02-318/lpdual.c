#include "jaos_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    jaos_model *m;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    const char *p = argv[1];
    jaos_status st = strstr(p, ".qplib") ? jaos_read_qplib(m, p) : jaos_read_mps(m, p);
    if (st != JAOS_OK) { printf("read %d\n", st); return 1; }
    const int64_t n = m->num_col;
    double *x = calloc(n, sizeof *x);
    FILE *f = fopen(argv[2], "r");
    for (int64_t j = 0; j < n; j++)
        if (fscanf(f, "%lf", &x[j]) != 1) { printf("short x\n"); return 1; }
    fclose(f);
    long double *qx = calloc(n, sizeof *qx);
    for (int64_t j = 0; j < n; j++)
        if (m->col_quad) qx[j] += (long double)m->col_quad[j] * x[j];
    for (int64_t j = 0; m->q_start && j < n; j++)
        for (int64_t q = m->q_start[j]; q < m->q_start[j + 1]; q++) {
            int64_t i = m->q_index[q];
            qx[i] += (long double)m->q_value[q] * x[j];
            qx[j] += (long double)m->q_value[q] * x[i];
        }
    jaos_model *lp;
    if (jaos_model_copy(m, &lp) != JAOS_OK) return 1;
    for (int64_t j = 0; j < n; j++) {
        if (jaos_set_col_quadratic(lp, j, 0.0) != JAOS_OK) return 1;
        if (jaos_set_col_cost(lp, j, (double)(m->col_cost[j] + qx[j])) != JAOS_OK) return 1;
    }
    if (m->q_nz > 0 && jaos_set_quadratic(lp, 0, nullptr, nullptr, nullptr) != JAOS_OK) { printf("clear q failed\n"); return 1; }
    st = jaos_solve(lp);
    printf("lp solve %d status %d obj %.17g iters %lld work %lld\n", st, lp->solve_status, lp->objective,
           (long long)jaos_iterations(lp), (long long)jaos_work_units(lp));
    long double gx = 0;
    for (int64_t j = 0; j < n; j++) gx += (long double)(m->col_cost[j] + qx[j]) * x[j];
    printf("g'x at the push point %.17Lg\n", gx);
    jaos_check_report ck;
    jaos_check_solution(m, x, lp->sol_dual, 1e-7, &ck);
    printf("check (x, y_lp): primal %d dual %d col %.3g row %.3g rowrel %.3g dualviol %.3g gap %.3g rsub %.3g pobj %.17g\n",
           ck.primal_feasible, ck.dual_feasible, ck.max_col_violation, ck.max_row_violation,
           ck.max_row_violation_relative, ck.max_dual_violation, ck.objective_gap,
           ck.relative_suboptimality, ck.primal_objective);
    return 0;
}
