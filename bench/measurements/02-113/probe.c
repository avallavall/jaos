/* Which of phase 1's four refusals are reached at all, over the standard set.
 *
 * Two of them gained a D20 gate and the campaign came back byte-identical, so
 * the gate never fired. Counting tells "unreachable" apart from "fired and
 * agreed" — the second would have cost an extra refresh and moved the units.
 *
 * **The positive control is inside the table.** `tinypivot` is not gated and
 * `pilot87` is known to end there, so that column must be non-zero or the
 * probe is blind and the zeros mean nothing.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include "jaos.h"
#include "jaos_internal.h"

enum { N_SITE = 5 };
static const char *const SITE[N_SITE] = {
    "q-retry", "q-refuse", "r-retry", "r-refuse", "tinypivot"
};
static int hit[N_SITE], tot[N_SITE];
static int retry_tp, tot_retry_tp;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    const char *p = strstr(line, "DIAG hit ");
    if (p == nullptr)
        return;
    p += 9;
    if (strncmp(p, "tinypivot-retry", 15) == 0) { retry_tp++; return; }
    for (int i = 0; i < N_SITE; i++)
        if (strcmp(p, SITE[i]) == 0) { hit[i]++; return; }
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
        m->cfg.force_primal = false;
        (void)jaos_solve(m);
        jaos_clear_basis(m);
        const int64_t wd = jaos_work_units(m);
        if (jaos_set_work_limit(m, 10 * (wd + 1)) != JAOS_OK) return 2;
        memset(hit, 0, sizeof hit);
        retry_tp = 0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);

        int any = retry_tp;
        for (int k = 0; k < N_SITE; k++) { tot[k] += hit[k]; any += hit[k]; }
        tot_retry_tp += retry_tp;
        if (any) {
            const char *b = strrchr(argv[i], '/'); b = b ? b + 1 : argv[i];
            printf("%-13s", b);
            for (int k = 0; k < N_SITE; k++)
                if (hit[k]) printf("  %s=%d", SITE[k], hit[k]);
            if (retry_tp) printf("  tinypivot-retry=%d", retry_tp);
            printf("\n");
        }
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("\nTOTAL over %d instances\n", argc - 1);
    for (int k = 0; k < N_SITE; k++)
        printf("  %-18s %d\n", SITE[k], tot[k]);
    printf("  %-18s %d\n", "tinypivot-retry", tot_retry_tp);
    if (tot[4] == 0 && tot_retry_tp == 0) {
        fprintf(stderr, "the two UNGATED sites were never reached either, so "
                        "this probe has no positive control and its zeros "
                        "mean nothing\n");
        return 3;
    }
    return 0;
}
