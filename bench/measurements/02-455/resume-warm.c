/* A stop inside the settling re-entry of a warm-started dual solve.
 *
 * Each file is a node LP of tests/data/mip_noinc_propagate.mps, written
 * with the basis the tree started it from (FILE.bas beside FILE.mps). The
 * harness solves it from that basis for the reference, then a fresh copy
 * under LIMIT work units, which stops it inside the re-entry that settles
 * its optimum, lifts the limit and solves on. The resumed solve must end
 * on the reference's status, iterations and work units.
 *
 * Usage: resume-warm LIMIT FILE.mps [LIMIT FILE.mps ...]
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static jaos_model *load(const char *path)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK || jaos_read_mps(m, path) != JAOS_OK)
        exit(1);
    char basis[1024];
    snprintf(basis, sizeof basis, "%.*s.bas", (int)(strlen(path) - 4), path);
    jaos_basis_status *cs = malloc((size_t)jaos_num_col(m) * sizeof *cs);
    jaos_basis_status *rs = malloc((size_t)jaos_num_row(m) * sizeof *rs);
    if (cs == nullptr || rs == nullptr ||
        jaos_read_mps_basis(m, basis, cs, rs) != JAOS_OK ||
        jaos_set_basis(m, cs, rs) != JAOS_OK)
        exit(1);
    free(cs);
    free(rs);
    return m;
}

int main(int argc, char **argv)
{
    int broke = 0;
    for (int k = 1; k + 1 < argc; k += 2) {
        const int64_t limit = atoll(argv[k]);
        const char *path = argv[k + 1];
        jaos_model *ref = load(path);
        if (jaos_solve(ref) != JAOS_OK)
            return 1;
        jaos_model *m = load(path);
        if (jaos_set_work_limit(m, limit) != JAOS_OK || jaos_solve(m) != JAOS_OK)
            return 1;
        const jaos_solve_status stopped = jaos_status_of(m);
        if (jaos_set_work_limit(m, 0) != JAOS_OK || jaos_solve(m) != JAOS_OK)
            return 1;
        const bool same = jaos_status_of(m) == jaos_status_of(ref) &&
                          jaos_iterations(m) == jaos_iterations(ref) &&
                          jaos_work_units(m) == jaos_work_units(ref);
        broke += !same;
        printf("%-12s limit %-6lld stopped %-17s straight %s %lld iterations "
               "%lld work, resumed %s %lld iterations %lld work: %s\n",
               strrchr(path, '/') != nullptr ? strrchr(path, '/') + 1 : path,
               (long long)limit, jaos_solve_status_str(stopped),
               jaos_solve_status_str(jaos_status_of(ref)),
               (long long)jaos_iterations(ref), (long long)jaos_work_units(ref),
               jaos_solve_status_str(jaos_status_of(m)),
               (long long)jaos_iterations(m), (long long)jaos_work_units(m),
               same ? "same" : "BROKE");
        jaos_model_free(m);
        jaos_model_free(ref);
    }
    printf("broken %d\n", broke);
    return 0;
}
