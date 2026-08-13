/* The sweep's canary: a model built to cascade one link per round.
 *
 * A sweep of N settings that measures one binary N times reports a flat line
 * and a flat line looks like a result (D82: a five-point sweep once read
 * exactly 1.0000x at every setting because `make` could not see a `CFLAGS`
 * change). The defence is a reading that MUST move between the lowest and
 * the highest setting, checked before any row of the sweep is believed.
 *
 * The model:
 *
 *     row 0:      x_0 = 1
 *     row k:      -x_{k-1} + x_k = 0      for k = 1 .. n-1
 *     every column in [0, 100], cost 1
 *
 * Row 0 is a singleton, so it folds into x_0's own bounds and the column
 * pass fixes x_0. That drops row 1 to degree 1, which the NEXT round folds
 * into x_1, and so on: exactly one link per round, by construction. The
 * activity-range families cannot short-circuit it — every row k has a range
 * of [-100, 100] against bounds of [0, 0], so it is neither forcing nor
 * redundant, and neither end of the range implies anything tighter than the
 * bounds the columns already carry.
 *
 * So the number of columns presolve fixes is the round cap, capped by the
 * chain length. At JM_PRESOLVE_ROUNDS = 1 it is 1; at 128 it is 128. A run
 * where those two agree is a run where the constant did not reach the
 * binary, and the sweep that follows it is measuring nothing.
 *
 * Reads jm_presolve_run directly rather than going through jaos_solve: the
 * counters are internal by design (D64, D-13) and the tests already reach
 * them the same way.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <stdio.h>
#include <stdlib.h>

#define CHAIN 200

int main(void)
{
    static double c[CHAIN], cl[CHAIN], cu[CHAIN];
    static double rl[CHAIN], ru[CHAIN];
    static int64_t s[CHAIN + 1];
    static int64_t ix[2 * CHAIN];
    static double v[2 * CHAIN];

    /* Column j owns its entry in row j (coefficient +1 for j = 0, and for
     * every j >= 1 it is row j's own +1) plus, for j < CHAIN-1, the -1 that
     * row j+1 carries on it. */
    int64_t nz = 0;
    for (int64_t j = 0; j < CHAIN; j++) {
        c[j] = 1.0;
        cl[j] = 0.0;
        cu[j] = 100.0;
        s[j] = nz;
        ix[nz] = j;
        v[nz] = 1.0;
        nz++;
        if (j < CHAIN - 1) {
            ix[nz] = j + 1;
            v[nz] = -1.0;
            nz++;
        }
    }
    s[CHAIN] = nz;

    rl[0] = 1.0;
    ru[0] = 1.0;
    for (int64_t i = 1; i < CHAIN; i++) {
        rl[i] = 0.0;
        ru[i] = 0.0;
    }

    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_load_lp(m, CHAIN, CHAIN, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, s, ix, v) != JAOS_OK)
        return 2;

    jm_presolve p;
    jm_presolve_init(&p);
    if (jm_presolve_run(m, &p, NULL) != JAOS_OK)
        return 2;

    printf("canary_rounds chain=%d rounds=%lld fixed_col=%lld "
           "singleton_row=%lld outcome=%d\n",
           CHAIN, (long long)p.counts.rounds, (long long)p.counts.fixed_col,
           (long long)p.counts.singleton_row, (int)p.outcome);

    jm_presolve_free(&p);
    jaos_model_free(m);

    /* The epsilon's own canary, and it has to be a different model: the
     * chain above is decided by exact arithmetic and would read the same at
     * every epsilon, which is precisely the flat line a sweep must not be
     * allowed to report.
     *
     *     min x0 + x1
     *     row 0:  x0 >= 5 + 1e-8       x0 in [0, 5]
     *     row 1:  x0 + x1 >= 6         x1 in [0, 10]
     *
     * Row 0 is a singleton, so it folds into a lower bound on x0 of
     * 5 + 1e-8, meeting x0's own upper bound of 5. That is the boundary
     * PRESOLVE_TIGHTEN_EPS decides: the fold conflicts by 1e-8 against a
     * window of eps times 5, so it is INFEASIBLE below eps = 2e-9 and a
     * collapse to a fixed column — an OPTIMAL solve — above it. The flip
     * sits between 1e-9 and 1e-8, inside the grid the sweep runs.
     *
     * `outcome` reads 2 (JM_PRESOLVE_INFEASIBLE) at the tight end and 4
     * (JM_PRESOLVE_SOLVED) at the loose end. Two readings that agree are a
     * constant that did not reach the binary, and the sweep beneath them is
     * measuring one binary N times. */
    {
        const double c2[]  = {1.0, 1.0};
        const double cl2[] = {0.0, 0.0}, cu2[] = {5.0, 10.0};
        const double rl2[] = {5.0 + 1e-8, 6.0};
        const double ru2[] = {1e30, 1e30};
        const int64_t s2[]  = {0, 2, 3};
        const int64_t ix2[] = {0, 1, 1};
        const double v2[]   = {1.0, 1.0, 1.0};
        jaos_model *m2 = NULL;
        if (jaos_model_new(&m2) != JAOS_OK)
            return 2;
        if (jaos_load_lp(m2, 2, 2, JAOS_MINIMIZE, 0.0, c2, cl2, cu2, rl2, ru2,
                         3, s2, ix2, v2) != JAOS_OK)
            return 2;
        jm_presolve p2;
        jm_presolve_init(&p2);
        if (jm_presolve_run(m2, &p2, NULL) != JAOS_OK)
            return 2;
        printf("canary_eps outcome=%d fixed_col=%lld singleton_row=%lld\n",
               (int)p2.outcome, (long long)p2.counts.fixed_col,
               (long long)p2.counts.singleton_row);
        jm_presolve_free(&p2);
        jaos_model_free(m2);
    }
    return 0;
}
