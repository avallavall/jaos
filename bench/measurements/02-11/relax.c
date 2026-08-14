/* What the cost-0 singleton column family does to the row it leaves behind.
 *
 * On grow7, grow15 and grow22 this is the ONLY family that fires, 20 times
 * each, and it is therefore the whole of the difference between the presolved
 * and the un-presolved solve. Two of the three get 8x worse and one gets 2x
 * better, so the question is not whether the family is correct -- it is -- but
 * what the relaxation does to the model the simplex then sees.
 *
 * Removing a cost-0 column that lives in one row means that row's bounds must
 * widen by everything that column could have contributed, or the rest of the
 * row could be driven somewhere the column can no longer complete. So the
 * relaxation is forced. What is not forced is what it leaves: a row relaxed
 * to (-inf, +inf) constrains nothing at all, and 20 of those is 20 constraints
 * the dual simplex no longer has to steer by.
 *
 * Prints one line per record: the row, its bounds before, what the column
 * could contribute, and the bounds after.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

static void show(double v)
{
    if (isinf(v)) printf("%8s", v > 0 ? "+inf" : "-inf");
    else          printf("%8.3g", v);
}

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) { printf("read failed\n"); return 2; }
        jm_presolve p;
        jm_presolve_init(&p);
        if (jm_presolve_run(m, &p, NULL) != JAOS_OK) { printf("presolve failed\n"); return 2; }

        printf("\n=== %s ===\n", argv[a]);
        printf("%6s %6s | %8s %8s | %8s %8s | %8s %8s | %s\n",
               "row", "col", "rlo", "rhi", "cmin", "cmax", "newlo", "newhi",
               "verdict");
        long long freed = 0, halffreed = 0, kept = 0, n = 0;
        for (int64_t r = 0; r < p.arena_len; r++) {
            const jm_presolve_rec *rec = &p.arena[r];
            if (rec->tag != JM_PS_SINGLETON_COL) continue;
            n++;
            const double c1 = rec->coef * rec->lo, c2 = rec->coef * rec->hi;
            const double cmin = c1 < c2 ? c1 : c2;
            const double cmax = c1 > c2 ? c1 : c2;
            const double nlo = isfinite(rec->row_lo) ? rec->row_lo - cmax
                                                     : -INFINITY;
            const double nhi = isfinite(rec->row_hi) ? rec->row_hi - cmin
                                                     : INFINITY;
            const int lo_gone = !isfinite(nlo), hi_gone = !isfinite(nhi);
            if (lo_gone && hi_gone)      freed++;
            else if (lo_gone || hi_gone) halffreed++;
            else                         kept++;

            if (n <= 8) {
                printf("%6lld %6lld |", (long long)rec->index,
                       (long long)rec->index2);
                show(rec->row_lo); show(rec->row_hi); printf(" |");
                show(cmin); show(cmax); printf(" |");
                show(nlo); show(nhi); printf(" | %s\n",
                    (lo_gone && hi_gone) ? "row constrains NOTHING"
                  : (lo_gone || hi_gone) ? "one side lost"
                                         : "both sides finite");
            }
        }
        printf("  %lld records: %lld left the row unconstrained, "
               "%lld lost one side, %lld kept both\n",
               n, freed, halffreed, kept);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    return 0;
}
