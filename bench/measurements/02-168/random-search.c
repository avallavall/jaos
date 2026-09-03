/* A search for a SMALL model whose solve leaves a column resting on a bound
 * it lent. The hand-built shapes in model-search.c never do: the dual
 * simplex moves the column off its loan on every one of them. This walks a
 * deterministic family of small models instead.
 *
 * Two readings per model, both of them about the PUBLISHED answer:
 *   - a nonbasic status naming a bound the model does not have;
 *   - a published value at the magnitude of the lent bound, which a BASIC
 *     status hides -- the model's own numbers are all below eleven.
 * The second is what a degenerate blocker would leave behind.
 *
 *   random-search [count]
 */
#include "jaos.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fired;
static void logger(void *ud, jaos_log_level lv, const char *msg)
{
    (void)ud; (void)lv;
    if (strstr(msg, "lent bounds retired")) fired = 1;
}

/* xorshift64: one seed in, one stream out, no library randomness. */
static uint64_t rng(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    return *s = x;
}
static int64_t pick(uint64_t *s, int64_t n) { return (int64_t)(rng(s) % (uint64_t)n); }

#define INF (jaos_infinity())
#define MAXC 6
#define MAXR 4
#define HUGE_X 1e9      /* a tenth of ARTIFICIAL_BOUND; the models reach 11 */

int main(int argc, char **argv)
{
    const long count = argc > 1 ? strtol(argv[1], NULL, 10) : 200000;
    uint64_t seed = 0x9E3779B97F4A7C15ull;
    long hits = 0, optimal = 0, bad_status = 0, huge_value = 0, shown = 0;

    for (long t = 0; t < count; t++) {
        const int64_t nc = 2 + pick(&seed, MAXC - 1);
        const int64_t nr = 1 + pick(&seed, MAXR);
        double c[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
        int64_t as[MAXC + 1], ai[MAXC * MAXR];
        double av[MAXC * MAXR];
        int64_t nz = 0;
        for (int64_t j = 0; j < nc; j++) {
            c[j] = (double)(pick(&seed, 5) - 2);
            cl[j] = 0.0;
            /* Half the columns keep an open top, which is what earns a loan
             * when the cost is negative. */
            cu[j] = pick(&seed, 2) ? INF : (double)(1 + pick(&seed, 9));
            as[j] = nz;
            for (int64_t i = 0; i < nr; i++) {
                const double a = (double)(pick(&seed, 5) - 2);
                if (a == 0.0) continue;
                ai[nz] = i; av[nz] = a; nz++;
            }
        }
        as[nc] = nz;
        for (int64_t i = 0; i < nr; i++) {
            rl[i] = -INF;
            ru[i] = (double)pick(&seed, 11);
        }

        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        fired = 0;
        if (jaos_set_log_callback(m, logger, NULL) != JAOS_OK ||
            jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK ||
            jaos_add_rows(m, nr, rl, ru, 0, nullptr, nullptr, nullptr) != JAOS_OK ||
            jaos_add_cols(m, nc, c, cl, cu, nz, as, ai, av) != JAOS_OK ||
            jaos_solve(m) != JAOS_OK) {
            jaos_model_free(m);
            continue;
        }
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) { jaos_model_free(m); continue; }
        optimal++;
        if (fired) hits++;

        double x[MAXC], d[MAXC], act[MAXR], y[MAXR];
        jaos_basis_status cs[MAXC], rs[MAXR];
        if (jaos_solution(m, x, act, y, d) != JAOS_OK ||
            jaos_basis(m, cs, rs) != JAOS_OK) { jaos_model_free(m); continue; }
        int bs = 0, hv = 0;
        for (int64_t j = 0; j < nc; j++) {
            if ((cs[j] == JAOS_BASIS_AT_LOWER && cl[j] <= -INF) ||
                (cs[j] == JAOS_BASIS_AT_UPPER && cu[j] >= INF)) bs = 1;
            if (fabs(x[j]) >= HUGE_X) hv = 1;
        }
        if (bs) bad_status++;
        if (hv) huge_value++;
        if ((bs || hv) && shown < 5) {
            shown++;
            printf("case %ld: nc=%lld nr=%lld bad_status=%d huge_value=%d\n",
                   shown, (long long)nc, (long long)nr, bs, hv);
            printf("  cost   ="); for (int64_t j = 0; j < nc; j++) printf(" %g", c[j]); printf("\n");
            printf("  upper  ="); for (int64_t j = 0; j < nc; j++) printf(" %g", cu[j]); printf("\n");
            printf("  rowub  ="); for (int64_t i = 0; i < nr; i++) printf(" %g", ru[i]); printf("\n");
            printf("  a_start="); for (int64_t j = 0; j <= nc; j++) printf(" %lld", (long long)as[j]); printf("\n");
            printf("  a_index="); for (int64_t k = 0; k < nz; k++) printf(" %lld", (long long)ai[k]); printf("\n");
            printf("  a_value="); for (int64_t k = 0; k < nz; k++) printf(" %g", av[k]); printf("\n");
            printf("  x      ="); for (int64_t j = 0; j < nc; j++) printf(" %.17g/%d", x[j], (int)cs[j]); printf("\n");
            double obj = 0.0;
            if (jaos_objective(m, &obj) == JAOS_OK) printf("  objective %.17g\n", obj);
        }
        jaos_model_free(m);
    }
    printf("-- %ld models, %ld optimal, %ld reached the retirement; "
           "%ld published a status on a bound the model lacks, "
           "%ld published a value at or past %g\n",
           count, optimal, hits, bad_status, huge_value, HUGE_X);
    return 0;
}
