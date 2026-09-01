/* Solve instances with asserts enabled, in one method or both.
 *
 * The unit suites never reach five of the ten asserts D232 adds: the primal
 * phase 1 runs only under `cfg.force_primal`, which no test sets, and
 * presolve's FORCING and singleton-column families need a real model. This
 * is the smallest thing that reaches both.
 *
 * Not a gate tool. Linked against the `build/dev/` objects, which carry no
 * `-DNDEBUG`, by `run-assert-controls.sh` and `census-forcing.sh` beside it.
 * Run from the repository root:
 *   ./probe afiro sc50a                  both methods, bench/instances/
 *   ./probe --dual-only bench/instances-kennington/cre-a.mps
 * An argument containing a slash is a path; anything else is a name under
 * `bench/instances/`. Every line is flushed, so the last instance printed
 * before an abort is the one that fired the assert.
 */
#include "jaos.h"
#include "jaos_internal.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int first = 1;
    int methods = 2;
    if (argc > 1 && strcmp(argv[1], "--dual-only") == 0) {
        methods = 1;
        first = 2;
    }
    for (int i = first; i < argc; i++) {
        for (int primal = 0; primal < methods; primal++) {
            char path[512];
            if (strchr(argv[i], '/') != nullptr)
                snprintf(path, sizeof path, "%s", argv[i]);
            else
                snprintf(path, sizeof path, "bench/instances/%s.mps", argv[i]);
            jaos_model *m = nullptr;
            if (jaos_model_new(&m) != JAOS_OK)
                return 2;
            if (jaos_read_mps(m, path) != JAOS_OK) {
                printf("SKIP    %-14s cannot read %s\n", argv[i], path);
                fflush(stdout);
                jaos_model_free(m);
                break;
            }
            printf("solving %-14s %-6s\n", argv[i],
                   primal ? "primal" : "dual");
            fflush(stdout);
            m->cfg.force_primal = primal != 0;
            jaos_status st = jaos_solve(m);
            printf("done    %-14s %-6s st=%d status=%s\n", argv[i],
                   primal ? "primal" : "dual", (int)st,
                   jaos_solve_status_str(jaos_status_of(m)));
            fflush(stdout);
            jaos_model_free(m);
        }
    }
    return 0;
}
