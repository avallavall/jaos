/* Counts primal bound flips and what each does to the quantity its phase owns.
 *
 * Phase 2's invariant is primal feasibility, so its predicate is the worst
 * declared-bound violation. Phase 1 minimises the SUM and deliberately lets a
 * basic go further out, so its predicate is the total. Reported separately,
 * because the first version of this probe reported the worst for both and
 * every one of its 113 firings was an innocent phase-1 move.
 *
 * Reads the DIAG line `patch.py` adds at both flip sites. Against an unpatched
 * library it sees nothing, and the runner refuses that: a zero from a probe
 * that never saw a line is not a measurement.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

static int flips[2], invented[2], t_grew[2], w_grew_past[2];
static double worst_t_growth[2], big_delta[2];
static const char *cur;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    const char *p = strstr(line, "DIAG flip ");
    if (p == nullptr)
        return;
    int p1 = 0, inv = 0;
    double delta = 0, wb = 0, wa = 0, tb = 0, ta = 0, tol = 0;
    if (sscanf(p, "DIAG flip in_phase1=%d delta=%lf invented=%d "
                  "worst %lf %lf total %lf %lf tol=%lf",
               &p1, &delta, &inv, &wb, &wa, &tb, &ta, &tol) != 8)
        return;
    const int k = p1 ? 1 : 0;
    flips[k]++;
    if (inv) invented[k]++;
    if (fabs(delta) > big_delta[k]) big_delta[k] = fabs(delta);

    if (ta > tb) {
        t_grew[k]++;
        if (ta - tb > worst_t_growth[k]) worst_t_growth[k] = ta - tb;
        printf("  %-12s %s TOTAL GREW %.6g -> %.6g  delta=%.6g invented=%d\n",
               cur, p1 ? "phase1" : "phase2", tb, ta, delta, inv);
    }
    /* phase 2's own invariant: the point was feasible and must stay so */
    if (!p1 && wa > tol && wa > wb) {
        w_grew_past[k]++;
        printf("  %-12s phase2 FEASIBILITY BROKEN %.6g -> %.6g (tol %.6g) "
               "delta=%.6g invented=%d\n", cur, wb, wa, tol, delta, inv);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>...\n"); return 2; }
    for (int i = 1; i < argc; i++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            printf("%-13s READ FAILED\n", argv[i]); jaos_model_free(m); continue;
        }
        const char *b = strrchr(argv[i], '/');
        cur = b ? b + 1 : argv[i];
        m->cfg.force_primal = false;
        (void)jaos_solve(m);
        jaos_clear_basis(m);
        const int64_t wd = jaos_work_units(m);
        if (jaos_set_work_limit(m, 10 * (wd + 1)) != JAOS_OK) return 2;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("\n%-34s %10s %10s\n", "", "phase 2", "phase 1");
    printf("%-34s %10d %10d\n", "flips", flips[0], flips[1]);
    printf("%-34s %10d %10d\n", "origin was an invented bound",
           invented[0], invented[1]);
    printf("%-34s %10d %10d\n", "the phase's own measure GREW",
           t_grew[0], t_grew[1]);
    printf("%-34s %10.6g %10.6g\n", "largest growth in one flip",
           worst_t_growth[0], worst_t_growth[1]);
    printf("%-34s %10.6g %10.6g\n", "largest |delta|",
           big_delta[0], big_delta[1]);
    printf("%-34s %10d %10s\n", "phase-2 feasibility broken",
           w_grew_past[0], "n/a");
    if (flips[0] + flips[1] == 0) {
        fprintf(stderr, "no DIAG line was seen at all; either the patch did "
                        "not apply or this is an unpatched library, and a "
                        "zero from those is not a measurement\n");
        return 3;
    }
    return 0;
}
