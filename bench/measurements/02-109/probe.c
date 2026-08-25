/* How much of the shared iteration cap each method finds already spent.
 *
 * The three loops each compute `ITER_SANITY_FACTOR * (nrow + ncol + 1)` and
 * each tests the CUMULATIVE `s->iters` against it. So phase 2's real allowance
 * is the cap minus phase 1's spend, and the dual's re-entry is third in the
 * queue behind both.
 *
 * Reports, per instance: the cap, what each method found already spent, the
 * final iteration count, and the headroom left at the end. `jaos_solve`'s
 * return value is checked, because ignoring it is what made D194's probe print
 * a stale status for `pilot87`.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include "jaos.h"
#include "jaos_internal.h"

static long long cap, spent_p1, spent_p2, spent_dual, total;
static int saw_p1, saw_p2, saw_dual;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    long long c = 0, s = 0, a = 0;
    char which[16];
    if (sscanf(line, "DIAG cap %15s cap=%lld spent=%lld", which, &c, &s) == 3) {
        cap = c;
        if      (!strcmp(which, "phase1"))  { saw_p1 = 1;   spent_p1 = s; }
        else if (!strcmp(which, "primal2")) { saw_p2 = 1;   spent_p2 = s; }
        else if (!strcmp(which, "dual"))    { saw_dual = 1; spent_dual = s; }
        return;
    }
    const char *p = strstr(line, " after ");
    if (p && strstr(line, " primal iterations") &&
        sscanf(p, " after %lld iterations", &a) == 1)
        total = a;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>...\n"); return 2; }
    int seen = 0, p2_squeezed = 0, worst_pct_at = -1;
    double worst_pct = 0.0;
    printf("%-13s %10s %10s %10s %10s %10s %6s\n",
           "instance", "cap", "p1 spent", "p2 spent", "dual spent",
           "total", "p1%");
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
        cap = spent_p1 = spent_p2 = spent_dual = total = 0;
        saw_p1 = saw_p2 = saw_dual = 0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        const jaos_status st = jaos_solve(m);

        const char *b = strrchr(argv[i], '/'); b = b ? b + 1 : argv[i];
        if (cap == 0) {
            printf("%-13s (no DIAG cap line)\n", b);
        } else {
            seen++;
            const double pct = 100.0 * (double)spent_p2 / (double)cap;
            if (pct > worst_pct) { worst_pct = pct; worst_pct_at = i; }
            if (saw_p2 && spent_p2 > 0) p2_squeezed++;
            printf("%-13s %10lld %10lld %10lld %10lld %10lld %5.1f%s\n",
                   b, cap, saw_p1 ? spent_p1 : -1, saw_p2 ? spent_p2 : -1,
                   saw_dual ? spent_dual : -1, total, pct,
                   st != JAOS_OK ? "  (solve returned an error)" : "");
        }
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("\nparsed %d of %d\n", seen, argc - 1);
    printf("  phase 2 started with the cap already partly spent : %d\n",
           p2_squeezed);
    printf("  largest share of the cap spent before phase 2     : %.2f%%",
           worst_pct);
    if (worst_pct_at > 0) printf("  on %s", argv[worst_pct_at]);
    printf("\n");
    if (seen == 0) {
        fprintf(stderr, "no DIAG cap line seen; not a measurement\n");
        return 3;
    }
    return 0;
}
