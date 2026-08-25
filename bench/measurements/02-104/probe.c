/* Does Bland's rule ever arm in the primal on the netlib set?
 *
 * The rule landed as a finiteness argument, not as a performance change, and
 * `make primal` came back with every count identical to D191's. Identical
 * counts are what a dead branch looks like AND what a live branch that never
 * changed an outcome looks like. This tells the two apart: it solves with
 * `cfg.force_primal` at `JAOS_LOG_DETAIL` and counts the arming lines.
 *
 * **Phase 2 and the dual print the same sentence**, so they are counted
 * together as `other`. Only phase 1 names itself, and phase 1's detector is
 * the half that is new; phase 2's detector predates this change and only its
 * leaving-variable half is new. So `phase1` is the number this exists for.
 *
 * It mirrors `bench/primal.c`: dual solve, clear the basis, work limit at 10x
 * the dual's units, primal solve. Without that limit the eight instances that
 * overrun there run to the 200x iteration cap here instead.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, which is
 * not public API, the same exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include <string.h>
#include "jaos.h"
#include "jaos_internal.h"

static int n_p1, n_other;

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    if (strstr(line, "switching to Bland's rule") == nullptr)
        return;
    if (strstr(line, "primal phase 1") != nullptr) n_p1++;
    else                                           n_other++;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>...\n"); return 2; }
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

        n_p1 = n_other = 0;
        if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
        m->cfg.force_primal = true;
        (void)jaos_solve(m);

        printf("%-14s primal=%-22s iters=%-7lld bland: phase1=%d other=%d\n",
               argv[i], jaos_solve_status_str(jaos_status_of(m)),
               (long long)jaos_iterations(m), n_p1, n_other);
        fflush(stdout);
        jaos_model_free(m);
    }
    return 0;
}
