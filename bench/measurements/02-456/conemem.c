/* A conic branch and bound under a work limit. Prints its status, nodes and
 * work units, and the growth of the heap in use over the solve, read through
 * glibc's mallinfo2 at every node by the progress callback.
 *
 * Usage: conemem FILE.mps WORK_LIMIT
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
static size_t in_use(void)
{
    const struct mallinfo2 mi = mallinfo2();
    return mi.uordblks + mi.hblkhd;
}
static size_t peak;
static jaos_callback_action watch(const jaos_progress *p, void *user)
{
    (void)p; (void)user;
    const size_t u = in_use();
    if (u > peak) peak = u;
    return JAOS_CALLBACK_CONTINUE;
}
int main(int argc, char **argv)
{
    jaos_model *m = nullptr;
    if (argc != 3 || jaos_model_new(&m) != JAOS_OK ||
        jaos_read_mps(m, argv[1]) != JAOS_OK)
        return 1;
    const size_t base = in_use();
    peak = base;
    if (jaos_set_work_limit(m, atoll(argv[2])) != JAOS_OK ||
        jaos_set_progress_callback(m, watch, nullptr) != JAOS_OK || jaos_solve(m) != JAOS_OK)
        return 1;
    jaos_mip_report r = {0};
    (void)jaos_mip_result(m, &r);
    printf("status %s nodes %lld work %lld heap growth %zu bytes\n", jaos_solve_status_str(jaos_status_of(m)),
           (long long)r.nodes, (long long)jaos_work_units(m), peak - base);
    jaos_model_free(m);
    return 0;
}
