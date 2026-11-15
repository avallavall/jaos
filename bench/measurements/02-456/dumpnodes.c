/* Runs a branch and bound under a work limit, so that a library built with
 * dumpnodes.patch writes the node LPs it solves.
 *
 * Usage: dumpnodes FILE.mps WORK_LIMIT
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    jaos_model *m = nullptr;
    if (argc != 3 || jaos_model_new(&m) != JAOS_OK ||
        jaos_read_mps(m, argv[1]) != JAOS_OK ||
        jaos_set_work_limit(m, atoll(argv[2])) != JAOS_OK ||
        jaos_solve(m) != JAOS_OK)
        return 1;
    printf("%s: %s after %lld work units\n", argv[1],
           jaos_solve_status_str(jaos_status_of(m)),
           (long long)jaos_work_units(m));
    jaos_model_free(m);
    return 0;
}
