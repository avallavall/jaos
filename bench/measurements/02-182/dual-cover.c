/* Every figure the checker's DUAL side publishes, at full precision, for
 * every gate instance.
 *
 * D270 converted the checker's primal walk from `long double` to a
 * compensated `double`. D277 finishes the file: the dual walk, the reduced
 * cost, `implied_bounds`'s two range sums, `certified_step`, and both
 * certificate checkers. The question this instrument exists to answer is
 * the one a gate run cannot answer on its own: which of the published
 * figures move, by how much, and does any VERDICT move with them.
 *
 * The dual half is the half that decides. A bound `implied_bounds` tightens
 * sets `sign_condition`'s window, and that window reaches `dual_feasible`.
 * So every boolean is printed beside every figure, and the comparison
 * script counts the booleans separately.
 *
 * There is no exact oracle for these figures the way `jm_exact_evaluate` is
 * one for the primal side (D267), so this is a before-and-after against
 * HEAD's own arithmetic and nothing more. It says what moved. It does not
 * say which of the two is closer to the truth, and the record must not
 * claim it does.
 *
 * The solve is untouched by this change, so the point being judged is the
 * same point in both halves. That is what makes the difference readable:
 * one input, two checkers.
 *
 * Printed at 17 significant digits, which round-trips a binary64 exactly,
 * so `diff` on the two output files is a bit-level comparison.
 *
 * The name printed is the basename, and it is NOT unique across the three
 * sets: `greenbea.mps` is a netlib optimum in `bench/instances` and a
 * different, infeasible model in `bench/instances-infeas`. The comparison
 * script joins the two halves by POSITION for that reason, and checks the
 * names agree at every position before it compares a figure.
 *
 * The seconds are a development number and belong in no baseline.
 *
 * Not a gate tool. Run from the repository root through its script.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *base(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* The tolerance the gate's own acceptance runner judges with. Not swept
 * here: the question is whether the arithmetic moved, and a second
 * tolerance would confound that with a second question. */
static const double CHECK_TOL = 1e-7;

int main(int argc, char **argv)
{
    int optimal = 0, infeas = 0, unbounded = 0, other = 0;

    for (int i = 1; i < argc; i++) {
        const char *name = base(argv[i]);
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) {
            printf("%-14s NEW_FAILED\n", name);
            continue;
        }
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            printf("%-14s READ_FAILED\n", name);
            jaos_model_free(m);
            continue;
        }
        if (jaos_solve(m) != JAOS_OK) {
            printf("%-14s SOLVE_FAILED\n", name);
            jaos_model_free(m);
            continue;
        }

        const jaos_solve_status st = jaos_status_of(m);
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);

        if (st == JAOS_SOLVE_OPTIMAL) {
            double *x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x);
            double *ra = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *ra);
            double *y = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *y);
            double *dj = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *dj);
            jaos_check_report r;
            if (x == nullptr || ra == nullptr || y == nullptr ||
                dj == nullptr ||
                jaos_solution(m, x, ra, y, dj) != JAOS_OK ||
                jaos_check_solution(m, x, y, CHECK_TOL, &r) != JAOS_OK) {
                printf("%-14s OPTIMAL_READ_FAILED\n", name);
            } else {
                optimal++;
                printf("%-14s OPTIMAL"
                       " pobj=%.17g dobj=%.17g gap=%.17g"
                       " dviol=%.17g gpos=%.17g gneg=%.17g"
                       " relsub=%.17g certsub=%.17g dropmax=%.17g"
                       " rviol=%.17g cviol=%.17g rrel=%.17g"
                       " dropped=%lld rays=%lld"
                       " pfeas=%d dfeas=%d gcert=%d\n",
                       name,
                       r.primal_objective, r.dual_objective,
                       r.objective_gap,
                       r.max_dual_violation, r.gap_positive, r.gap_negative,
                       r.relative_suboptimality, r.certified_suboptimality,
                       r.max_dropped_multiplier,
                       r.max_row_violation, r.max_col_violation,
                       r.max_row_violation_relative,
                       (long long)r.dropped_terms,
                       (long long)r.unquantified_rays,
                       (int)r.primal_feasible, (int)r.dual_feasible,
                       (int)r.gap_certified);
            }
            free(x);
            free(ra);
            free(y);
            free(dj);
        } else if (st == JAOS_SOLVE_INFEASIBLE) {
            double *ray = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *ray);
            jaos_certificate_report c;
            if (ray == nullptr ||
                jaos_certificate(m, ray) != JAOS_OK ||
                jaos_check_certificate(m, ray, CHECK_TOL, &c) != JAOS_OK) {
                printf("%-14s INFEASIBLE no-certificate\n", name);
            } else {
                infeas++;
                printf("%-14s INFEASIBLE"
                       " sup=%.17g inf=%.17g cgap=%.17g cert=%d\n",
                       name, c.sup_columns, c.inf_rows, c.gap,
                       (int)c.certified);
            }
            free(ray);
        } else if (st == JAOS_SOLVE_UNBOUNDED) {
            double *d = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *d);
            jaos_ray_report rr;
            if (d == nullptr ||
                jaos_unbounded_ray(m, d) != JAOS_OK ||
                jaos_check_ray(m, d, CHECK_TOL, &rr) != JAOS_OK) {
                printf("%-14s UNBOUNDED no-ray\n", name);
            } else {
                unbounded++;
                printf("%-14s UNBOUNDED"
                       " rate=%.17g cesc=%.17g resc=%.17g cert=%d\n",
                       name, rr.rate, rr.max_col_escape, rr.max_row_escape,
                       (int)rr.certified);
            }
            free(d);
        } else {
            other++;
            printf("%-14s %s (nothing to check)\n", name,
                   jaos_solve_status_str(st));
        }
        jaos_model_free(m);
        fflush(stdout);
    }

    printf("\n# summary: optimal=%d infeasible=%d unbounded=%d other=%d\n",
           optimal, infeas, unbounded, other);
    return 0;
}
