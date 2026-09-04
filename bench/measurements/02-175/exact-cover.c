/* What does exact evaluation say that the checker's own arithmetic does
 * not, and what does it cost?
 *
 * For every gate instance: solve, take the published point, and evaluate
 * it twice. Once through jaos_check_solution, which sums in long double
 * uncompensated (D268), and once through jm_exact_evaluate, which does
 * not round at all. Then print both, and the difference in ulps.
 *
 * The question this exists to answer is whether the checker's numbers are
 * ever wrong enough to matter. D262 is the case where they were: the
 * checker's objective was measurably out on `finnis` because long double
 * holds 64 bits and a binary64 product needs 106. D262 has the figure.
 * That was found by accident. This asks the whole population.
 *
 * The seconds printed are a development number and do not belong in any
 * record; they are here because the cost is half the question.
 *
 * Not a gate tool. Run from the repository root through its script.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* How many representable doubles lie between a and b. Both finite. */
static double ulps_between(double a, double b)
{
    if (a == b)
        return 0.0;
    if (!isfinite(a) || !isfinite(b))
        return INFINITY;
    /* The gap at the larger magnitude; enough for a report. */
    const double mag = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    if (mag == 0.0)
        return 0.0;
    const double ulp = nextafter(mag, INFINITY) - mag;
    return fabs(a - b) / ulp;
}

static const char *base(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

int main(int argc, char **argv)
{
    printf("%-14s %-9s %8s %8s %10s %10s %10s %8s\n",
           "instance", "status", "rows", "terms", "obj_ulps", "row_chk",
           "row_exact", "secs");

    int moved_obj = 0, moved_row = 0, refused = 0, done = 0;
    for (int i = 1; i < argc; i++) {
        const char *name = base(argv[i]);
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) {
            printf("%-14s new failed\n", name);
            continue;
        }
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            printf("%-14s read failed: %s\n", name, jaos_model_error(m));
            jaos_model_free(m);
            continue;
        }
        if (jaos_solve(m) != JAOS_OK || jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            printf("%-14s %-9s (skipped: no optimum to evaluate)\n", name,
                   jaos_solve_status_str(jaos_status_of(m)));
            jaos_model_free(m);
            continue;
        }

        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x);
        double *y = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *y);
        if (x == nullptr || y == nullptr) {
            printf("%-14s out of memory\n", name);
            free(x);
            free(y);
            jaos_model_free(m);
            continue;
        }
        if (jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK) {
            printf("%-14s solution failed\n", name);
            free(x);
            free(y);
            jaos_model_free(m);
            continue;
        }

        jaos_check_report rep;
        memset(&rep, 0, sizeof rep);
        if (jaos_check_solution(m, x, y, 1e-7, &rep) != JAOS_OK) {
            printf("%-14s check failed\n", name);
            free(x);
            free(y);
            jaos_model_free(m);
            continue;
        }

        jm_exact_point p;
        const clock_t t0 = clock();
        const bool ok = jm_exact_evaluate(m, x, &p);
        const double secs = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;

        if (!ok) {
            printf("%-14s %-9s %8lld %8s %10s %10s %10s %8.2f  "
                   "REFUSED: out of limbs\n",
                   name, "optimal", (long long)nr, "-", "-", "-", "-", secs);
            refused++;
        } else {
            const double ou = ulps_between(rep.primal_objective, p.objective);
            printf("%-14s %-9s %8lld %8lld %10.3g %10.3g %10.3g %8.2f",
                   name, "optimal", (long long)nr, (long long)p.terms, ou,
                   rep.max_row_violation, p.row_violation, secs);
            if (ou > 0.0) {
                printf("  objective moves %.3g -> %.3g",
                       rep.primal_objective, p.objective);
                moved_obj++;
            }
            if (rep.max_row_violation != p.row_violation)
                moved_row++;
            printf("\n");
            done++;
        }
        fflush(stdout);

        free(x);
        free(y);
        jaos_model_free(m);
    }

    printf("\n%d evaluated exactly, %d refused for limbs\n", done, refused);
    printf("%d objectives differ from the checker's, %d worst-row "
           "violations differ\n", moved_obj, moved_row);
    return 0;
}
