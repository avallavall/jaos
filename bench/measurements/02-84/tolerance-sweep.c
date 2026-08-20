/* Where does `pilot` lose its 2.31e-05, and can a caller's tolerance get it
 * back?
 *
 * D173 measured the gap and said nothing about its cause. The two cheapest
 * questions come before any instrumented build, and both are asked through
 * the public interface alone:
 *
 *   - does the gap survive `-DJAOS_NO_PRESOLVE`? That splits presolve from
 *     the simplex, and the reference build is the oracle every presolve entry
 *     in this repository is judged against;
 *   - does either tolerance a caller owns close it? `jaos_set_dual_tolerance`
 *     and `jaos_set_primal_tolerance` are the only two, by D64.
 *
 * The published objective is the correctly rounded exact one (D173), so a
 * plain double difference against the reference is the whole gap and no
 * accumulator is needed here.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jaos.h"

static void one(const char *path, double ref, const char *what,
                double ptol, double dtol)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) exit(2);
    if (jaos_read_mps(m, path) != JAOS_OK) exit(2);
    if (ptol > 0.0 && jaos_set_primal_tolerance(m, ptol) != JAOS_OK) exit(2);
    if (dtol > 0.0 && jaos_set_dual_tolerance(m, dtol) != JAOS_OK) exit(2);

    const jaos_status rc = jaos_solve(m);
    const jaos_solve_status ss = jaos_status_of(m);
    double obj = NAN;
    (void)jaos_objective(m, &obj);

    /* The checker's own verdict on the same point, so a setting that closes
     * the gap by publishing an infeasible point cannot look like a win. */
    jaos_check_report rep = {0};
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)nc, sizeof *x);
    double *y = calloc((size_t)nr, sizeof *y);
    if (x != NULL && y != NULL && ss == JAOS_SOLVE_OPTIMAL) {
        (void)jaos_solution(m, x, NULL, y, NULL);
        (void)jaos_check_solution(m, x, y, 1e-6, &rep);
    }

    printf("%-22s ptol=%-8.0e dtol=%-8.0e %-10s obj=%.17g gap=%-12.4g "
           "iters=%-7lld work=%-9lld row=%-10.4g dual=%-10.4g gappos=%-10.4g "
           "cert=%s\n",
           what, ptol, dtol,
           rc == JAOS_OK ? jaos_solve_status_str(ss) : "ERROR",
           obj, obj - ref,
           (long long)jaos_iterations(m), (long long)jaos_work_units(m),
           rep.max_row_violation, rep.max_dual_violation,
           rep.gap_positive, rep.gap_certified ? "yes" : "no");
    fflush(stdout);

    free(x); free(y);
    jaos_model_free(m);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <instance.mps> <reference> [build tag]\n",
                argv[0]);
        return 1;
    }
    const char *path = argv[1];
    const double ref = strtod(argv[2], NULL);
    const char *tag = (argc > 3) ? argv[3] : "build";

    char what[64];
    snprintf(what, sizeof what, "%s default", tag);
    one(path, ref, what, 0.0, 0.0);

    /* One tolerance at a time, so a move is attributable. Both live in
     * scaled space (docs/tolerances.md) and both default to 1e-7. */
    static const double sweep[] = { 1e-5, 1e-6, 1e-8, 1e-9, 1e-10, 1e-11,
                                    1e-12, 1e-13 };
    for (unsigned i = 0; i < sizeof sweep / sizeof sweep[0]; i++) {
        snprintf(what, sizeof what, "%s dual-only", tag);
        one(path, ref, what, 0.0, sweep[i]);
    }
    for (unsigned i = 0; i < sizeof sweep / sizeof sweep[0]; i++) {
        snprintf(what, sizeof what, "%s primal-only", tag);
        one(path, ref, what, sweep[i], 0.0);
    }
    /* Both together, at the two ends, because the dual simplex reads them in
     * the same iteration and one alone may be held back by the other. */
    snprintf(what, sizeof what, "%s both tight", tag);
    one(path, ref, what, 1e-11, 1e-11);
    snprintf(what, sizeof what, "%s both loose", tag);
    one(path, ref, what, 1e-5, 1e-5);
    return 0;
}
