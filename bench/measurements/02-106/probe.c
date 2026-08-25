/* The tail of the solve log for one instance, forced primal.
 *
 * `pilot4` ends JAOS_SOLVE_NUMERICAL_ERROR with an empty `jaos_model_error`,
 * and every site in `run_primal` that sets that status is downstream of a
 * `jm_set_err`. So either the message is cleared after the fact or the status
 * comes from somewhere else. The log says which, and it says it the same way
 * on both sides of D193's guard.
 *
 * Prints every line at JAOS_LOG_DETAIL, so the caller can tail or grep it.
 *
 * Not a gate tool. Reads `jaos_internal.h` for `cfg.force_primal`, the same
 * exception `bench/primal.c` takes (D-13). */
#include <stdio.h>
#include "jaos.h"
#include "jaos_internal.h"

static void logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    printf("  %s\n", line);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: probe <mps>\n"); return 2; }
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) return 2;

    m->cfg.force_primal = false;
    (void)jaos_solve(m);
    printf("== dual: %s, %lld iters, %lld units\n",
           jaos_solve_status_str(jaos_status_of(m)),
           (long long)jaos_iterations(m), (long long)jaos_work_units(m));
    jaos_clear_basis(m);
    const int64_t wd = jaos_work_units(m);
    if (jaos_set_work_limit(m, 10 * (wd + 1)) != JAOS_OK) return 2;

    printf("== primal log follows\n");
    if (jaos_set_log_callback(m, logger, nullptr) != JAOS_OK) return 2;
    if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) return 2;
    m->cfg.force_primal = true;
    const jaos_status st = jaos_solve(m);
    const char *why = jaos_model_error(m);
    printf("== primal: %s, %lld iters, %lld units, jaos_status=%d\n",
           jaos_solve_status_str(jaos_status_of(m)),
           (long long)jaos_iterations(m), (long long)jaos_work_units(m),
           (int)st);
    printf("== jaos_model_error: [%s]\n", why ? why : "(null)");
    jaos_model_free(m);
    return 0;
}
