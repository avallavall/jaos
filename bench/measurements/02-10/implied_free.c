/* Counter 1: implied free column singletons.
 *
 * Column j is live with exactly one matrix entry a_ij, in row i. The row
 * implies a box on x_j once the other terms are accounted for:
 *
 *   a_ij * x_j  in  [ rl_i - maxact_-j ,  ru_i - minact_-j ]
 *
 * divided by a_ij, with the ends swapped when a_ij < 0. The column is
 * IMPLIED FREE when that box sits inside the column's own box -- its own
 * bounds can never bind, so it can be substituted out exactly, with no bound
 * to transfer anywhere. That is what separates this from bound tightening,
 * which D97 refused: nothing is narrowed and nothing is published.
 *
 * The prediction under test, from a literature scout: maros-r7 reads 984
 * distinct rows, which is exactly what HiGHS removes from it, and truss reads
 * 0. If those two numbers come out, the mechanism is identified. If they do
 * not, the explanation is wrong and nothing else in that report should be
 * acted on.
 *
 * Reports the cost split too, because the family JAOS already has fires only
 * at cost 0 (src/presolve.c) and the whole question is what lies outside it.
 *
 * It reads. It changes nothing. Never built into the shipping library.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "jaos.h"
#include "jaos_internal.h"

int main(int argc, char **argv)
{
    printf("%-12s %6s %6s | %6s %7s %8s | %6s %6s\n",
           "instance", "rows", "cols", "hits", "rows_hit", "nz_saved",
           "cost0", "costNZ");
    for (int a = 1; a < argc; a++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) { printf("%s read failed\n", argv[a]); continue; }
        const int64_t nr = m->num_row, nc = m->num_col;

        /* Row-wise mirror, built in index order. */
        int64_t *deg = calloc((size_t)nr + 1, sizeof *deg);
        double  *lo_sum = calloc((size_t)nr + 1, sizeof *lo_sum);
        double  *hi_sum = calloc((size_t)nr + 1, sizeof *hi_sum);
        int64_t *lo_inf = calloc((size_t)nr + 1, sizeof *lo_inf);
        int64_t *hi_inf = calloc((size_t)nr + 1, sizeof *hi_inf);
        bool    *hitrow = calloc((size_t)nr + 1, sizeof *hitrow);
        if (!deg || !lo_sum || !hi_sum || !lo_inf || !hi_inf || !hitrow) return 2;

        /* Full activity range per row, with the infinities counted rather
         * than propagated, so a single term can be removed again below. */
        for (int64_t j = 0; j < nc; j++) {
            const double cl = m->col_lower[j], cu = m->col_upper[j];
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                const int64_t i = m->a_index[k];
                const double v = m->a_value[k];
                deg[i]++;
                const double t1 = v * cl, t2 = v * cu;
                const double tmin = (v > 0) ? t1 : t2;
                const double tmax = (v > 0) ? t2 : t1;
                if (isfinite(tmin)) lo_sum[i] += tmin; else lo_inf[i]++;
                if (isfinite(tmax)) hi_sum[i] += tmax; else hi_inf[i]++;
            }
        }

        long long hits = 0, rows_hit = 0, nz_saved = 0, c0 = 0, cnz = 0;
        for (int64_t j = 0; j < nc; j++) {
            if (m->a_start[j + 1] - m->a_start[j] != 1) continue;
            const int64_t k = m->a_start[j];
            const int64_t i = m->a_index[k];
            const double v = m->a_value[k];
            if (v == 0.0) continue;
            const double cl = m->col_lower[j], cu = m->col_upper[j];

            /* Activity of row i EXCLUDING this column's own term. */
            const double t1 = v * cl, t2 = v * cu;
            const double tmin = (v > 0) ? t1 : t2;
            const double tmax = (v > 0) ? t2 : t1;
            int64_t li = lo_inf[i], hi = hi_inf[i];
            double ls = lo_sum[i], hs = hi_sum[i];
            if (isfinite(tmin)) ls -= tmin; else li--;
            if (isfinite(tmax)) hs -= tmax; else hi--;
            const double minact = (li > 0) ? -HUGE_VAL : ls;
            const double maxact = (hi > 0) ?  HUGE_VAL : hs;

            const double rl = m->row_lower[i], ru = m->row_upper[i];
            /* rl - maxact and ru - minact, with the infinite cases explicit
             * so no NaN can be produced by inf - inf. */
            const double loside = (!isfinite(maxact) || !isfinite(rl))
                                  ? -HUGE_VAL : rl - maxact;
            const double upside = (!isfinite(minact) || !isfinite(ru))
                                  ?  HUGE_VAL : ru - minact;
            double ilo, iup;
            if (v > 0) { ilo = loside / v; iup = upside / v; }
            else       { ilo = upside / v; iup = loside / v; }

            const bool lo_ok = !isfinite(cl) ? true : (ilo >= cl);
            const bool up_ok = !isfinite(cu) ? true : (iup <= cu);
            if (!(lo_ok && up_ok)) continue;

            hits++;
            if (m->col_cost[j] == 0.0) c0++; else cnz++;
            if (!hitrow[i]) { hitrow[i] = true; rows_hit++; nz_saved += deg[i]; }
        }

        const char *nm = argv[a], *sl = argv[a];
        for (const char *s = argv[a]; *s; s++) if (*s == '/') sl = s + 1;
        nm = sl;
        printf("%-12s %6lld %6lld | %6lld %7lld %8lld | %6lld %6lld\n",
               nm, (long long)nr, (long long)nc,
               hits, rows_hit, nz_saved, c0, cnz);

        free(deg); free(lo_sum); free(hi_sum); free(lo_inf); free(hi_inf);
        free(hitrow);
        jaos_model_free(m);
    }
    return 0;
}
