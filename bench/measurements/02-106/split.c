/* How much of a "forced primal" solve is actually the primal?
 *
 * `pilot4` reached OPTIMAL before D193 with 2598 primal iterations out of
 * 4148, and 2596 of those 2598 were phase 1. So its phase 2 took TWO
 * iterations and the dual's settling re-entry did the remaining 1550. The
 * campaign counts that instance as the primal agreeing with the dual.
 *
 * D188 left this open in one line — `reenter_after_settling` calls `run()`,
 * so a forced-primal solve can still finish with dual iterations — and nobody
 * has counted it. This does.
 *
 * The solver's own end-of-solve summary carries both numbers, so no patch is
 * needed and this runs against the shipping library.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include "jaos.h"
#include "jaos_internal.h"

static long long p1_iters, primal_iters, total_iters;
static int saw_summary;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    long long a = 0, b = 0;
    if (sscanf(line, "phase 1 reached a feasible point in %lld iterations",
               &a) == 1) {
        p1_iters = a;
        return;
    }
    const char *p = strstr(line, " after ");
    const char *q = strstr(line, " primal iterations");
    if (p == nullptr || q == nullptr)
        return;
    if (sscanf(p, " after %lld iterations", &a) != 1)
        return;
    /* walk back from " primal iterations" to the number in front of it */
    const char *r = q;
    while (r > line && (r[-1] == ' ' || (r[-1] >= '0' && r[-1] <= '9')))
        r--;
    if (sscanf(r, "%lld", &b) != 1)
        return;
    total_iters = a; primal_iters = b; saw_summary = 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: split <mps>...\n"); return 2; }
    int seen = 0;
    printf("%-13s %-20s %8s %8s %8s %8s %6s\n",
           "instance", "status", "total", "primal", "phase1", "dual", "p2%%");
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

        p1_iters = primal_iters = total_iters = 0; saw_summary = 0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);

        const char *base = strrchr(argv[i], '/');
        base = base ? base + 1 : argv[i];
        if (saw_summary) {
            seen++;
            const long long p2 = primal_iters - p1_iters;
            const long long dual = total_iters - primal_iters;
            printf("%-13s %-20s %8lld %8lld %8lld %8lld %5.1f\n",
                   base, jaos_solve_status_str(jaos_status_of(m)),
                   total_iters, primal_iters, p1_iters, dual,
                   total_iters ? 100.0 * (double)p2 / (double)total_iters : 0.0);
        } else {
            printf("%-13s %-20s   (no summary line)\n",
                   base, jaos_solve_status_str(jaos_status_of(m)));
        }
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("parsed %d of %d\n", seen, argc - 1);
    if (seen == 0) {
        fprintf(stderr, "no summary line was parsed at all; a zero from that "
                        "is not a measurement\n");
        return 3;
    }
    return 0;
}
