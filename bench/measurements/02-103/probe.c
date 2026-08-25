/* Why does the primal fail on sc50a? */
#include "jaos.h"
#include "jaos_internal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static void logline(void *u, jaos_log_level l, const char *line)
{
    (void)u; (void)l;
    printf("    %s\n", line);
}

int main(int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "sc50a";
    char path[512];
    snprintf(path, sizeof path, "bench/instances/%s.mps", name);

    for (int primal = 0; primal < 2; primal++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) return 1;
        if (jaos_read_mps(m, path) != JAOS_OK) {
            printf("cannot read %s\n", path);
            return 1;
        }
        printf("== %s : %s\n", name, primal ? "PRIMAL" : "dual");
        (void)jaos_set_log_callback(m, logline, nullptr);
        (void)jaos_set_log_level(m, JAOS_LOG_DETAIL);
        m->cfg.force_primal = primal != 0;
        jaos_status st = jaos_solve(m);
        double obj = 0.0;
        (void)jaos_objective(m, &obj);
        printf("    -> st=%d status=%s obj=%.17g iters=%lld\n"
               "    -> err=[%s]\n",
               (int)st, jaos_solve_status_str(jaos_status_of(m)), obj,
               (long long)jaos_iterations(m), jaos_model_error(m));
        jaos_model_free(m);
    }
    return 0;
}
