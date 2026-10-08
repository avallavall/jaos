/* SPDX-License-Identifier: Apache-2.0 */
#include "../../../src/relax.c"

#include <inttypes.h>
#include <stdio.h>

static uint64_t s_state;

static uint64_t rnd(void)
{
    s_state = s_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return s_state >> 33;
}

static int64_t pick(int64_t lo, int64_t hi)
{
    return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1));
}

static double value(void)
{
    static const double frac[4] = {1.0, 0.5, 0.25, 0.125};
    return (double)pick(-12, 12) * frac[pick(0, 3)];
}

int main(int argc, char **argv)
{
    const int64_t count = argc > 1 ? strtoll(argv[1], nullptr, 10) : 1000;
    s_state = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
    for (int64_t k = 0; k < count; k++) {
        const int64_t nc = pick(1, 4);
        double a[4], lo, hi;
        for (int64_t j = 0; j < nc; j++) {
            a[j] = value();
            if (a[j] == 0.0)
                a[j] = 1.0;
        }
        lo = value();
        hi = lo + (double)pick(0, 8) * 0.125;
        if (!(lo < hi))
            hi = lo + 0.125;
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK)
            return 1;
        double c[4] = {0}, cl[4], cu[4];
        int64_t st[5], ix[4];
        for (int64_t j = 0; j < nc; j++) {
            cl[j] = -10.0;
            cu[j] = 10.0;
            st[j] = j;
            ix[j] = 0;
        }
        st[nc] = nc;
        if (jaos_load_lp(m, nc, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, &lo, &hi,
                         nc, st, ix, a) != JAOS_OK)
            return 1;
        for (int64_t j = 0; j < nc; j++)
            if (jaos_set_col_integer(m, j, true) != JAOS_OK)
                return 1;
        int64_t work = 0;
        const int64_t row = rx_range_empty(m, &work);
        printf("%d %.17g %.17g", row == 0 ? 1 : 0, lo, hi);
        for (int64_t j = 0; j < nc; j++)
            printf(" %.17g", a[j]);
        printf("\n");
        jaos_model_free(m);
    }
    return 0;
}
