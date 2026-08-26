/* How much margin phase 1's hand-over has, over the standard set.
 *
 * Phase 1 declares feasibility from the `xb` it carried; `run_primal` refreshes
 * and re-checks against `primal_tol` exactly. Reports both readings and the
 * ratio to the bar, so "comfortable" and "one ulp from a JAOS defect" are told
 * apart by a number rather than by reading the code.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include "jaos.h"
#include "jaos_internal.h"

static double before, after, tol;
static int seen;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    const char *p = strstr(line, "DIAG handover ");
    if (p == nullptr)
        return;
    if (sscanf(p, "DIAG handover before=%lf after=%lf tol=%lf",
               &before, &after, &tol) == 3)
        seen = 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>...\n"); return 2; }
    int n = 0;
    double worst = 0.0; const char *worst_at = "-";
    printf("%-13s %14s %14s %14s %10s\n",
           "instance", "carried xb", "refreshed xb", "primal_tol",
           "after/tol");
    for (int i = 1; i < argc; i++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            printf("%-13s READ FAILED\n", argv[i]); jaos_model_free(m); continue;
        }
        m->cfg.force_primal = false;
        (void)jaos_solve(m);
        jaos_clear_basis(m);
        const int64_t wd = jaos_work_units(m);
        if (jaos_set_work_limit(m, 10 * (wd + 1)) != JAOS_OK) return 2;
        seen = 0; before = after = tol = 0.0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);
        if (seen) {
            n++;
            const char *b = strrchr(argv[i], '/'); b = b ? b + 1 : argv[i];
            const double r = tol > 0.0 ? after / tol : 0.0;
            if (r > worst) { worst = r; worst_at = b; }
            printf("%-13s %14.6g %14.6g %14.6g %10.4f\n",
                   b, before, after, tol, r);
        }
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("\nreached the hand-over on %d of %d instances\n", n, argc - 1);
    printf("worst refreshed violation as a fraction of primal_tol: %.6f",
           worst);
    if (worst > 0.0) printf("  on %s", worst_at);
    printf("\n");
    if (n == 0) {
        fprintf(stderr, "no hand-over was observed at all; not a measurement\n");
        return 3;
    }
    return 0;
}
