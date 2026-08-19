/* Driver for the termination trajectory dump: degen2 cold (control), then
 * hostile shift 1 — the same construction as 02-54's probe, one instance,
 * one shift. The instrumented library prints TRAJ lines to stderr; this
 * driver only brackets them with PHASE markers and reports the objectives.
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <degen2.mps>\n", argv[0]);
        return 2;
    }
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 1;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK)
        return 1;
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);

    fprintf(stderr, "PHASE cold\n");
    if (jaos_solve(m) != JAOS_OK)
        return 1;
    double ref = 0.0;
    (void)jaos_objective(m, &ref);
    fprintf(stderr, "PHASE cold-done status=%d obj=%.17g iters=%lld\n",
            (int)jaos_status_of(m), ref, (long long)jaos_iterations(m));

    if (jaos_read_mps(m, argv[1]) != JAOS_OK)
        return 1;
    jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
    jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
    if (!cs || !rs)
        return 1;
    for (int64_t j = 0; j < nc; j++)
        cs[j] = JAOS_BASIS_AT_LOWER;
    for (int64_t i = 0; i < nr; i++)
        rs[i] = JAOS_BASIS_AT_LOWER;
    const int s = 1;
    int64_t placed = 0;
    for (int64_t k = 0; placed < nr && k < nc; k++) {
        const int64_t j = (k * (int64_t)s * 7 + s) % nc;
        if (cs[j] != JAOS_BASIS_BASIC) {
            cs[j] = JAOS_BASIS_BASIC;
            placed++;
        }
    }
    for (int64_t i = 0; placed < nr && i < nr; i++) {
        rs[i] = JAOS_BASIS_BASIC;
        placed++;
    }
    if (jaos_set_basis(m, cs, rs) != JAOS_OK)
        return 1;

    fprintf(stderr, "PHASE hostile\n");
    if (jaos_solve(m) != JAOS_OK)
        return 1;
    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    fprintf(stderr, "PHASE hostile-done status=%d obj=%.17g ref=%.17g "
                    "iters=%lld\n",
            (int)jaos_status_of(m), obj, ref, (long long)jaos_iterations(m));

    free(cs); free(rs);
    jaos_model_free(m);
    return 0;
}
