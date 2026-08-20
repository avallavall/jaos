/* What the clamp costs, and the case the assert must not abort on.
 *
 * Built twice, from the tree before the clamp and from the tree after it, and
 * diffed. Reports the checker's own two violations, because the trade the
 * clamp makes is between them: the midpoint splits the residue across the
 * column bound and the row, and the clamp puts all of it on the row.
 *
 * `primal_feasible` is an ABSOLUTE test at CHECK_TOL, so a row residual that
 * doubles can cross it where the split did not. That is what case R measures.
 *
 * Case I is an inverted column box, which include/jaos.h says is legal and is
 * to be reported infeasible rather than refused.
 */
#include <stdio.h>
#include <math.h>
#include "jaos.h"

#define CHECK_TOL 1e-6

static void one_a(const char *label, double rl0, double clo, double chi,
                  double a)
{
    const double c[]  = {1.0};
    const double cl[] = {clo}, cu[] = {chi};
    const double rl[] = {rl0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {a};

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) { printf("%s ALLOC\n", label); return; }
    if (jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v) != JAOS_OK) {
        printf("%-10s LOAD REFUSED\n", label); jaos_model_free(m); return;
    }
    if (jaos_solve(m) != JAOS_OK) {
        printf("%-10s SOLVE ERROR\n", label); jaos_model_free(m); return;
    }
    const int st = (int)jaos_status_of(m);
    if (st != JAOS_SOLVE_OPTIMAL) {
        printf("%-10s status=%d (not optimal, no point to check)\n", label, st);
        jaos_model_free(m);
        return;
    }
    double x[1] = {0}, y[1] = {0};
    (void)jaos_solution(m, x, NULL, y, NULL);
    jaos_check_report rep;
    if (jaos_check_solution(m, x, y, CHECK_TOL, &rep) != JAOS_OK) {
        printf("%-10s CHECK ERROR\n", label); jaos_model_free(m); return;
    }
    printf("%-10s x0=%.17g col_viol=%.6g row_viol=%.6g primal_ok=%d "
           "max_dual_viol=%.6g\n",
           label, x[0], rep.max_col_violation, rep.max_row_violation,
           (int)rep.primal_feasible, rep.max_dual_violation);
    jaos_model_free(m);
}

static void one(const char *label, double rl0, double clo, double chi)
{
    one_a(label, rl0, clo, chi, 1.0);
}

int main(void)
{
    /* R: the residue trade. 1.5e-6 is inside the 1.78e-6 window at this
     *    scale, so the interval collapses; the midpoint splits the residue
     *    and the clamp does not. */
    one("R-trade",   1e9 + 1.5e-6, 0.0, 1e9);
    /* The repro model, whose residue is small enough that neither side
     * crosses CHECK_TOL either way. */
    one("A-repro",   1e9 + 5e-7,   0.0, 1e9);
    /* I: an inverted column box. jaos.h says this is legal and INFEASIBLE. */
    one("I-inverted", 0.0, 1e9, 1e9 - 5e-7);

    /* The two above both have a = 1, which is the ONE case where the residue
     * crosses from the column to the row at 1:1. The column violation is in
     * x units and the row violation in a*x units, so with a gap g in x units
     * the midpoint splits it g/2 on the column and |a|*g/2 on the row, while
     * the clamp puts 0 and |a|*g. The worst of the two therefore changes by
     * 2|a| / max(1, |a|): it doubles at |a| >= 1 and SHRINKS below |a| = 0.5.
     *
     * Same 1.5e-6 gap in x units at two coefficients, to measure that rather
     * than assert it (`numerics-reviewer`). */
    one_a("a=4",    4e9 + 6e-6,   0.0, 1e9, 4.0);
    one_a("a=0.25", 2.5e8 + 3.75e-7, 0.0, 1e9, 0.25);
    return 0;
}
