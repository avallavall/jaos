/* Counter: implied free column singletons, split by row sense and by the
 * dual sign condition (TODO.md §1a).
 *
 * Detection is byte-for-byte the 02-10 counter (implied_free.c): column j
 * live with exactly one entry a_ij in row i, and the box the row implies on
 * x_j sits inside the column's own box. That part must reproduce 02-10's
 * totals exactly — netlib 3321 hits over 3315 distinct rows, maros-r7 984,
 * truss 0 — or this port is wrong and nothing it prints should be read.
 *
 * What is new is the classification. Eliminating an implied free singleton
 * forces the row's multiplier: d_j = c_j - y_i * a_ij = 0, so
 * y_i = c_j / a_ij, in minimize-canonical space (sigma = -1 under
 * JAOS_MAXIMIZE, exactly as src/check.c judges the published duals). An
 * inequality row restricts the sign: y_i > 0 needs a finite row lower bound
 * to bind against, y_i < 0 a finite upper (src/check.c, sign_condition). A
 * hit whose forced multiplier points at an infinite row end cannot be
 * eliminated soundly and a sign-respecting family must decline it. A zero
 * cost forces y_i = 0, which every row sense admits.
 *
 * Columns: rows cols hits rows_hit eq in ok dec ok_rows ok_nz c0 rng
 *   hits/rows_hit  all implied-free singleton hits / distinct rows (02-10)
 *   eq/in          hits in equality rows / in everything else
 *   ok/dec         inequality hits the sign admits / declines
 *   ok_rows/ok_nz  distinct sign-ok inequality rows, and their nonzeros
 *   c0/rng         of ok: zero-cost hits, and hits in two-sided range rows
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
    printf("%-16s %6s %6s %6s %8s %5s %5s %5s %5s %7s %7s %4s %4s\n",
           "instance", "rows", "cols", "hits", "rows_hit", "eq", "in",
           "ok", "dec", "ok_rows", "ok_nz", "c0", "rng");
    for (int a = 1; a < argc; a++) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) { printf("%s read failed\n", argv[a]); continue; }
        const int64_t nr = m->num_row, nc = m->num_col;
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

        /* Row-wise mirror, built in index order. */
        int64_t *deg = calloc((size_t)nr + 1, sizeof *deg);
        double  *lo_sum = calloc((size_t)nr + 1, sizeof *lo_sum);
        double  *hi_sum = calloc((size_t)nr + 1, sizeof *hi_sum);
        int64_t *lo_inf = calloc((size_t)nr + 1, sizeof *lo_inf);
        int64_t *hi_inf = calloc((size_t)nr + 1, sizeof *hi_inf);
        bool    *hitrow = calloc((size_t)nr + 1, sizeof *hitrow);
        bool    *okrow  = calloc((size_t)nr + 1, sizeof *okrow);
        if (!deg || !lo_sum || !hi_sum || !lo_inf || !hi_inf || !hitrow || !okrow)
            return 2;

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

        long long hits = 0, rows_hit = 0;
        long long eq = 0, in = 0, ok = 0, dec = 0;
        long long okr = 0, oknz = 0, c0 = 0, rng = 0;
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
            if (!hitrow[i]) { hitrow[i] = true; rows_hit++; }

            if (isfinite(rl) && isfinite(ru) && rl == ru) { eq++; continue; }

            in++;
            const double ce = sigma * m->col_cost[j];
            bool sign_ok;
            if (ce == 0.0)             sign_ok = true;
            else if (ce / v > 0.0)     sign_ok = isfinite(rl);
            else                       sign_ok = isfinite(ru);
            if (!sign_ok) { dec++; continue; }

            ok++;
            if (ce == 0.0) c0++;
            if (isfinite(rl) && isfinite(ru)) rng++;
            if (!okrow[i]) { okrow[i] = true; okr++; oknz += deg[i]; }
        }

        const char *nm = argv[a];
        for (const char *s = argv[a]; *s; s++) if (*s == '/') nm = s + 1;
        printf("%-16s %6lld %6lld %6lld %8lld %5lld %5lld %5lld %5lld %7lld %7lld %4lld %4lld\n",
               nm, (long long)nr, (long long)nc, hits, rows_hit,
               eq, in, ok, dec, okr, oknz, c0, rng);

        free(deg); free(lo_sum); free(hi_sum); free(lo_inf); free(hi_inf);
        free(hitrow); free(okrow);
        jaos_model_free(m);
    }
    return 0;
}
