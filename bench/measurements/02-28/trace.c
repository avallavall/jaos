/* Solve one instance with the summary log on, and print what the checker
 * says about the answer. TODO.md section 4c: pilotnov publishes a feasible
 * but suboptimal point as optimal under D118's refused candidate, and the
 * first question is whether the SOLVE went wrong or the model did.
 *
 * Reads. Changes nothing. Built against whichever tree it sits in, so the
 * same source runs on the parent and on the candidate.
 *
 * Usage: trace <file.mps> [reference-objective]
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "jaos.h"

static void logline(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    printf("  log: %s\n", line);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: trace <file.mps> [ref]\n"); return 2; }

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) {
        fprintf(stderr, "read failed: %s\n", argv[1]);
        return 2;
    }
    if (jaos_set_log_callback(m, logline, nullptr) != JAOS_OK) return 2;
    if (jaos_set_log_level(m, JAOS_LOG_SUMMARY) != JAOS_OK) return 2;

    const int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    printf("instance %s  rows=%lld cols=%lld\n", argv[1],
           (long long)nr, (long long)nc);

    const jaos_status st = jaos_solve(m);
    printf("solve status=%d  verdict=%d (%s)\n", (int)st,
           (int)jaos_status_of(m), jaos_solve_status_str(jaos_status_of(m)));

    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    printf("objective %.17g\n", obj);
    if (argc > 2) {
        const double ref = atof(argv[2]);
        printf("reference %.17g   relative gap %.6g\n", ref,
               fabs(obj - ref) / (fabs(ref) > 1.0 ? fabs(ref) : 1.0));
    }

    double *x = malloc((size_t)nc * sizeof *x);
    double *y = malloc((size_t)nr * sizeof *y);
    double *d = malloc((size_t)nc * sizeof *d);
    if (!x || !y || !d) return 2;
    if (jaos_solution(m, x, nullptr, y, d) == JAOS_OK) {
        jaos_check_report r;
        if (jaos_check_solution(m, x, y, 1e-7, &r) == JAOS_OK) {
            printf("checker primal=%d dual=%d  col=%.6g row=%.6g "
                   "dualviol=%.6g gap=%.6g gap+=%.6g gap-=%.6g\n",
                   (int)r.primal_feasible, (int)r.dual_feasible,
                   r.max_col_violation, r.max_row_violation,
                   r.max_dual_violation, r.objective_gap,
                   r.gap_positive, r.gap_negative);
        }
    }

    /* How many of the published statuses are basic, against the promise. */
    jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
    jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
    if (cs && rs && jaos_basis(m, cs, rs) == JAOS_OK) {
        int64_t basic = 0;
        for (int64_t j = 0; j < nc; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
        for (int64_t i = 0; i < nr; i++) basic += (rs[i] == JAOS_BASIS_BASIC);
        printf("basic %lld against num_row %lld\n", (long long)basic,
               (long long)nr);
    }

    free(x); free(y); free(d); free(cs); free(rs);
    jaos_model_free(m);
    return 0;
}
