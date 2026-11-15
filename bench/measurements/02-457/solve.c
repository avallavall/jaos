/* Solves each file with the defaults `make miplib` uses and prints its
 * status, nodes, iterations and work units.
 *
 * Usage: solve FILE.mps [FILE.mps ...]
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK || jaos_read_mps(m, argv[a]) != JAOS_OK)
            return 1;
        const jaos_status st = jaos_solve(m);
        jaos_mip_report r = {0};
        (void)jaos_mip_result(m, &r);
        const char *b = strrchr(argv[a], '/');
        printf("%s rc=%d status=%s nodes=%lld iters=%lld work=%lld\n",
               b != nullptr ? b + 1 : argv[a], (int)st,
               jaos_solve_status_str(jaos_status_of(m)), (long long)r.nodes,
               (long long)jaos_iterations(m), (long long)jaos_work_units(m));
        jaos_model_free(m);
    }
    return 0;
}
