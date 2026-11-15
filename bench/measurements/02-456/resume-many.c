/* Stops at many points of a warm-started simplex solve.
 *
 * Each FILE.mps is an LP written with the basis its solve starts from
 * (FILE.bas beside it). The harness solves it from that basis for the
 * reference, then, for k = 1 to STOPS, a fresh copy under a work limit of
 * k / (STOPS + 1) of the reference's work units, lifts the limit and solves
 * on. Every stop must resume to the reference's status, iterations and work
 * units. Prints one line a broken stop and the counts.
 *
 * Usage: resume-many STOPS FILE.mps [FILE.mps ...]
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
    const int k_stops = atoi(argv[1]);
    long stops = 0, broke = 0, files_broke = 0;
    for (int a = 2; a < argc; a++) {
        const char *path = argv[a];
        jaos_model *ref = load(path);
        if (jaos_solve(ref) != JAOS_OK)
            return 1;
        const int64_t w = jaos_work_units(ref), it = jaos_iterations(ref);
        int fb = 0;
        for (int k = 1; k <= k_stops; k++) {
            const int64_t limit = w * k / (k_stops + 1);
            if (limit <= 0)
                continue;
            jaos_model *m = load(path);
            if (jaos_set_work_limit(m, limit) != JAOS_OK || jaos_solve(m) != JAOS_OK)
                return 1;
            const jaos_solve_status stopped = jaos_status_of(m);
            if (stopped != JAOS_SOLVE_WORK_LIMIT) {
                jaos_model_free(m);
                continue;
            }
            stops++;
            if (jaos_set_work_limit(m, 0) != JAOS_OK || jaos_solve(m) != JAOS_OK)
                return 1;
            const bool same = jaos_status_of(m) == jaos_status_of(ref) &&
                              jaos_iterations(m) == it && jaos_work_units(m) == w;
            if (!same) {
                broke++;
                fb++;
                printf("%s limit %lld straight %s %lld it %lld work, resumed %s %lld it %lld work: BROKE\n",
                       strrchr(path, '/') + 1, (long long)limit,
                       jaos_solve_status_str(jaos_status_of(ref)), (long long)it, (long long)w,
                       jaos_solve_status_str(jaos_status_of(m)),
                       (long long)jaos_iterations(m), (long long)jaos_work_units(m));
            }
            jaos_model_free(m);
        }
        files_broke += fb > 0;
        jaos_model_free(ref);
    }
    printf("files %d stops %ld broken stops %ld files with a broken stop %ld\n",
           argc - 2, stops, broke, files_broke);
    return 0;
}
