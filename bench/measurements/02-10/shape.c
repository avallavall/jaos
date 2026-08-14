/* What shape is a model that JAOS's eight families cannot touch and HiGHS
 * reduces by 31% of its rows?
 *
 * This counts structure only. It is a question about the MODEL, asked of the
 * model, and it names candidate families rather than assuming one. The four
 * counted here are the ones a presolve of this size normally carries beyond
 * JAOS's eight:
 *
 *   doubleton equality  a == row with exactly 2 entries. One variable is
 *                       substituted out and the row disappears; this is the
 *                       single most common reduction after the singletons,
 *                       and it removes nonzeros wherever the substituted
 *                       column appeared.
 *   equality rows by degree, generally -- substitution scales with them.
 *   free columns        no finite bound either side. A free column in an
 *                       equality row is substitutable whatever its degree.
 *   implied-free        a column whose own bounds are looser than what its
 *                       rows already force. JAOS refused bound tightening
 *                       (D97), so it cannot see these at all.
 *
 * Built against src/ because the public header does not expose row degrees.
 * It reads; it changes nothing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 1;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) {
            printf("%s: could not read\n", argv[a]);
            jaos_model_free(m);
            continue;
        }
        const int64_t nr = m->num_row, nc = m->num_col;
        int64_t *deg = calloc((size_t)nr, sizeof *deg);
        if (!deg) return 1;
        for (int64_t j = 0; j < nc; j++)
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                deg[m->a_index[k]]++;

        int64_t eq = 0, eq2 = 0, eq3 = 0, eqbig = 0, d2 = 0;
        for (int64_t i = 0; i < nr; i++) {
            const int is_eq = (m->row_lower[i] == m->row_upper[i]) &&
                              isfinite(m->row_lower[i]);
            if (deg[i] == 2) d2++;
            if (!is_eq) continue;
            eq++;
            if (deg[i] == 2) eq2++;
            else if (deg[i] == 3) eq3++;
            else if (deg[i] > 3) eqbig++;
        }

        int64_t freecol = 0, halffree = 0, boxed = 0, fixed = 0;
        for (int64_t j = 0; j < nc; j++) {
            const int lo = isfinite(m->col_lower[j]);
            const int hi = isfinite(m->col_upper[j]);
            if (!lo && !hi) freecol++;
            else if (lo && hi && m->col_lower[j] == m->col_upper[j]) fixed++;
            else if (lo && hi) boxed++;
            else halffree++;
        }

        printf("%-12s rows=%-6ld cols=%-6ld\n", argv[a], (long)nr, (long)nc);
        printf("  equality rows      %ld  (%.1f%% of rows)\n",
               (long)eq, 100.0 * (double)eq / (double)nr);
        printf("    of degree 2      %ld   <- doubleton equations\n", (long)eq2);
        printf("    of degree 3      %ld\n", (long)eq3);
        printf("    of degree 4+     %ld\n", (long)eqbig);
        printf("  rows of degree 2   %ld  (any sense)\n", (long)d2);
        printf("  columns free       %ld\n", (long)freecol);
        printf("  columns half-bound %ld\n", (long)halffree);
        printf("  columns boxed      %ld\n", (long)boxed);
        printf("  columns fixed      %ld\n\n", (long)fixed);
        free(deg);
        jaos_model_free(m);
    }
    return 0;
}
