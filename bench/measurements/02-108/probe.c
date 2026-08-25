/* The corrected primal/dual iteration split.
 *
 * D194's version read phase 1's count from a log line printed only on success,
 * so a phase 1 that ran and did not finish read as a phase 1 that never ran.
 * This reads the DIAG line `patch.py` adds after every exit from
 * `run_primal_phase1`, and prints `ran=0` only when phase 1 genuinely did not
 * run — which is the case the solve skips because the point is already primal
 * feasible.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include "jaos.h"
#include "jaos_internal.h"

static long long p1_iters, primal_iters, total_iters;
static int p1_ran, p1_feasible, saw_summary;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    long long a = 0, b = 0;
    int f = 0, st = 0, o = 0;
    if (sscanf(line, "DIAG phase1 ran %lld iterations feasible=%d st=%d out=%d",
               &a, &f, &st, &o) == 4) {
        p1_ran = 1; p1_iters = a; p1_feasible = f;
        return;
    }
    const char *p = strstr(line, " after ");
    const char *q = strstr(line, " primal iterations");
    if (p == nullptr || q == nullptr) return;
    if (sscanf(p, " after %lld iterations", &a) != 1) return;
    const char *r = q;
    while (r > line && (r[-1] == ' ' || (r[-1] >= '0' && r[-1] <= '9'))) r--;
    if (sscanf(r, "%lld", &b) != 1) return;
    total_iters = a; primal_iters = b; saw_summary = 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>...\n"); return 2; }
    int seen = 0, skipped = 0, ran_unfinished = 0, ran_finished = 0;
    printf("%-13s %-20s %8s %8s %8s %8s %5s %s\n",
           "instance", "status", "total", "primal", "phase1", "dual",
           "feas", "phase1");
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
        p1_iters = primal_iters = total_iters = 0;
        p1_ran = p1_feasible = saw_summary = 0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);

        const char *b = strrchr(argv[i], '/'); b = b ? b + 1 : argv[i];
        if (saw_summary) {
            seen++;
            const char *tag;
            if (!p1_ran)          { tag = "skipped";   skipped++; }
            else if (p1_feasible) { tag = "finished";  ran_finished++; }
            else                  { tag = "UNFINISHED"; ran_unfinished++; }
            printf("%-13s %-20s %8lld %8lld %8lld %8lld %5d %s\n",
                   b, jaos_solve_status_str(jaos_status_of(m)),
                   total_iters, primal_iters, p1_iters,
                   total_iters - primal_iters, p1_feasible, tag);
        } else {
            printf("%-13s %-20s   (no summary line)\n",
                   b, jaos_solve_status_str(jaos_status_of(m)));
        }
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("\nparsed %d of %d\n", seen, argc - 1);
    printf("  phase 1 SKIPPED (point already primal feasible): %d\n", skipped);
    printf("  phase 1 ran and finished                       : %d\n", ran_finished);
    printf("  phase 1 ran and did NOT finish                 : %d\n", ran_unfinished);
    if (seen == 0) { fprintf(stderr, "nothing parsed; not a measurement\n"); return 3; }
    return 0;
}
