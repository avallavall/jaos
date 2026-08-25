/* Counts `refresh`'s repair sweeps during a forced-primal solve, by phase.
 *
 * Reads the DIAG line `patch.py` adds in a worktree. Against an unpatched
 * library it prints zeros everywhere, which is why the runner refuses a build
 * whose totals are all zero: that is what "the patch did not apply" looks
 * like and it is indistinguishable from a real result.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "jaos.h"
#include "jaos_internal.h"

static int sweeps_p1, sweeps_other, shifted_p1, shifted_other;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    const char *p = strstr(line, "DIAG sweep ");
    if (p == nullptr)
        return;
    int rep = 0, pend = 0, p1 = 0;
    long long nsh = 0;
    if (sscanf(p, "DIAG sweep repaired=%d pending=%d in_phase1=%d shifted=%lld",
               &rep, &pend, &p1, &nsh) != 4)
        return;
    if (p1) { sweeps_p1++;    shifted_p1    += (int)nsh; }
    else    { sweeps_other++; shifted_other += (int)nsh; }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>...\n"); return 2; }
    int tot_p1 = 0, tot_other = 0;
    for (int i = 1; i < argc; i++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            printf("%-14s READ FAILED\n", argv[i]);
            jaos_model_free(m); continue;
        }
        m->cfg.force_primal = false;
        (void)jaos_solve(m);
        jaos_clear_basis(m);
        const int64_t work_d = jaos_work_units(m);
        if (jaos_set_work_limit(m, 10 * (work_d + 1)) != JAOS_OK) return 2;

        sweeps_p1 = sweeps_other = shifted_p1 = shifted_other = 0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);

        if (sweeps_p1 || sweeps_other)
            printf("%-14s primal=%-22s sweeps: phase1=%d (shifted %d)  "
                   "other=%d (shifted %d)\n",
                   argv[i], jaos_solve_status_str(jaos_status_of(m)),
                   sweeps_p1, shifted_p1, sweeps_other, shifted_other);
        tot_p1 += sweeps_p1; tot_other += sweeps_other;
        fflush(stdout);
        jaos_model_free(m);
    }
    printf("TOTAL phase1 sweeps %d, other sweeps %d over %d instance(s)\n",
           tot_p1, tot_other, argc - 1);
    if (tot_p1 == 0 && tot_other == 0) {
        fprintf(stderr,
                "no DIAG line was seen at all. Either the patch did not apply "
                "or this is an unpatched library; a zero from those is not a "
                "measurement\n");
        return 3;
    }
    return 0;
}
