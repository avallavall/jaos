/* How many doubleton equalities survive presolve, measured on the model
 * presolve actually publishes rather than on the model as loaded.
 *
 * `bench/measurements/02-10/`'s first reading counted the model the reader
 * hands over: 6504 rows on netlib, 7.53%. That is an upper bound and it says
 * nothing about what is LEFT once the five live families have run to a fixed
 * point, which is the only count that decides whether a sixth family is worth
 * building. This is that count. It follows 02-07's convention exactly, is
 * called at the same exit, and prints one line per instance.
 *
 * A doubleton equality is a row `a*x_p + b*x_q == c` with exactly two entries.
 * One of the two columns is substituted out and the row disappears with it.
 *
 * Four numbers, and the distinctions between them are the whole point:
 *
 *   dbl       doubleton equality rows surviving. The population.
 *   dblfree   of those, how many have an endpoint whose own box is (-inf,
 *             +inf). Those substitute with no bound handling at all: the
 *             substituted variable had nothing to violate. Every other
 *             doubleton needs the substituted column's bounds transferred
 *             onto the survivor, which is bound tightening, and D97 refused
 *             every design of that. So `dbl - dblfree` is the part that
 *             cannot be built until D97's premise changes, and reporting one
 *             number for both would hide that.
 *   subnz     entries carried by the smaller endpoint, summed. An upper bound
 *             on the nonzeros one non-interacting pass would remove.
 *   eqrows    equality rows surviving, for scale.
 *
 * `subnz` is an upper bound twice over and the README says so rather than
 * leaving it to be discovered. A substitution SUBSTITUTES: the eliminated
 * column's terms are added into the survivor's rows, so it creates fill
 * wherever the survivor was not already present, and this counts none of
 * that. And two doubletons sharing a column interact, so the sum
 * double-counts. Whether the family pays is a measurement of an
 * implementation, not of this number; this number says whether to write one.
 *
 * Diagnostic only. Never compiled into a shipping build. */

/* Self-sufficient on purpose. The patch drops this include high in
 * presolve.c, above where that file reaches its own math and allocation
 * headers, so relying on them is how the first build failed. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void diag_doubleton(const jaos_model *r)
{
    const int64_t nr = r->num_row, nc = r->num_col;
    const int64_t nz = (nc > 0) ? r->a_start[nc] : 0;

    if (nr <= 0 || nc <= 0) {
        fprintf(stderr, "DOUBLETON liverows=0 livecols=0 "
                "eqrows=0 dbl=0 dblfree=0 subnz=0\n");
        return;
    }

    /* Row degrees, and the two column indices of every degree-2 row. Built
     * from the column-wise matrix, in index order, so nothing here depends on
     * an address or on an iteration order the model does not fix. */
    int64_t *deg = jm_calloc_array((size_t)nr + 1, sizeof *deg);
    int64_t *c0  = jm_calloc_array((size_t)nr + 1, sizeof *c0);
    int64_t *c1  = jm_calloc_array((size_t)nr + 1, sizeof *c1);
    int64_t *cdeg = jm_calloc_array((size_t)nc + 1, sizeof *cdeg);
    if (!deg || !c0 || !c1 || !cdeg) {
        fprintf(stderr, "DOUBLETON alloc-failed\n");
        goto out;
    }
    for (int64_t i = 0; i < nr; i++)
        c0[i] = c1[i] = -1;

    for (int64_t j = 0; j < nc; j++) {
        cdeg[j] = r->a_start[j + 1] - r->a_start[j];
        for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++) {
            const int64_t i = r->a_index[k];
            if (deg[i] == 0)      c0[i] = j;
            else if (deg[i] == 1) c1[i] = j;
            deg[i]++;
        }
    }

    long long eqrows = 0, dbl = 0, dblfree = 0, subnz = 0, nlr = 0, nlc = 0;
    for (int64_t i = 0; i < nr; i++) {
        if (deg[i] > 0) nlr++;
        const bool is_eq = (r->row_lower[i] == r->row_upper[i]) &&
                           isfinite(r->row_lower[i]);
        if (!is_eq) continue;
        eqrows++;
        if (deg[i] != 2) continue;
        dbl++;

        const int64_t p = c0[i], q = c1[i];
        const bool pfree = !isfinite(r->col_lower[p]) &&
                           !isfinite(r->col_upper[p]);
        const bool qfree = !isfinite(r->col_lower[q]) &&
                           !isfinite(r->col_upper[q]);
        if (pfree || qfree)
            dblfree++;

        /* The cheaper endpoint to eliminate is the one with fewer entries.
         * A free endpoint is taken when there is one, because that is the
         * substitution that is actually available. */
        int64_t sub;
        if (pfree && !qfree)      sub = p;
        else if (qfree && !pfree) sub = q;
        else                      sub = (cdeg[p] <= cdeg[q]) ? p : q;
        subnz += cdeg[sub];
    }
    for (int64_t j = 0; j < nc; j++)
        if (cdeg[j] > 0) nlc++;

    fprintf(stderr, "DOUBLETON liverows=%lld livecols=%lld eqrows=%lld "
            "dbl=%lld dblfree=%lld subnz=%lld totalnz=%lld\n",
            nlr, nlc, eqrows, dbl, dblfree, subnz, (long long)nz);

out:
    free(deg); free(c0); free(c1); free(cdeg);
}
