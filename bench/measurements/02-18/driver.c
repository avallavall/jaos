/* S1c driver: load the constructed model, verify it parsed to the intended
 * doubles, solve, and report what the postsolve published for S against its
 * own bound, plus the independent checker's verdict on the whole point.
 *
 * Usage: driver model.mps V b L
 * It reads. It never ships. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: driver model.mps V b L\n"); return 2; }
    const double V = strtod(argv[2], NULL);
    const double b = strtod(argv[3], NULL);
    const double L = strtod(argv[4], NULL);

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) { printf("READ FAILED\n"); return 2; }

    /* The file must have parsed to the exact doubles the generator chose,
     * or every number below is about a different model. */
    if (m->num_row != 1) { printf("PARSE: num_row=%lld\n", (long long)m->num_row); return 1; }
    if (m->a_value[m->a_start[0]] != 1.0) { printf("PARSE: S coef %.17g\n", m->a_value[m->a_start[0]]); return 1; }
    if (m->a_value[m->a_start[1]] != V) { printf("PARSE: V got %.17g want %.17g\n", m->a_value[m->a_start[1]], V); return 1; }
    if (m->row_lower[0] != b || m->row_upper[0] != b) { printf("PARSE: b got %.17g/%.17g want %.17g\n", m->row_lower[0], m->row_upper[0], b); return 1; }
    if (m->col_lower[0] != L) { printf("PARSE: L got %.17g want %.17g\n", m->col_lower[0], L); return 1; }
    printf("parse ok: cols=%lld V, b, L exact\n", (long long)m->num_col);

    if (jaos_solve(m) != JAOS_OK) { printf("SOLVE FAILED: %s\n", m->err); return 2; }
    printf("status=%d presolve=%lld/%lld->%lld/%lld\n",
           (int)m->solve_status,
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->presolve_num_row, (long long)m->presolve_num_col);

    const double S = m->sol_col[0];
    printf("S_pub=%.17g\n", S);
    printf("L=%.17g\n", L);
    printf("S_minus_L=%.6e\n", S - L);

    jaos_check_report rep;
    if (jaos_check_solution(m, m->sol_col, m->sol_dual, 1e-6, &rep) != JAOS_OK) {
        printf("CHECK CALL FAILED\n"); return 2;
    }
    printf("col_viol=%.6e row_viol=%.6e dual_viol=%.6e obj=%.17g\n",
           rep.max_col_violation, rep.max_row_violation,
           rep.max_dual_violation, rep.primal_objective);
    jaos_model_free(m);
    return 0;
}
