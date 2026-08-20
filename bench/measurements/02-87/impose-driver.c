/* Solves each instance in turn, announcing which one, so the IMPOSE records
 * the patched presolve emits can be attributed to it.
 *
 * `bench/run` would do the solving, but it forks per instance under `-j` and
 * emits no marker, so a row index could not be told apart from the same row
 * index in another model. Pooling them overstates the collision this
 * measurement is counting.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "jaos.h"

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        char name[128];
        const char *b = strrchr(argv[i], '/');
        snprintf(name, sizeof name, "%s", b ? b + 1 : argv[i]);
        char *dot = strrchr(name, '.');
        if (dot != NULL && strcmp(dot, ".mps") == 0) *dot = '\0';

        char mark[256];
        const int n = snprintf(mark, sizeof mark, "INSTANCE %s\n", name);
        if (n > 0)
            (void)!write(2, mark, (size_t)n);

        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            jaos_model_free(m);
            continue;
        }
        const jaos_status rc = jaos_solve(m);
        char out[256];
        const int k = snprintf(out, sizeof out, "RESULT %s %s\n", name,
                               rc == JAOS_OK
                                   ? jaos_solve_status_str(jaos_status_of(m))
                                   : "call_failed");
        if (k > 0)
            (void)!write(2, out, (size_t)k);
        jaos_model_free(m);
    }
    return 0;
}
