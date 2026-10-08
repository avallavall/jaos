/* SPDX-License-Identifier: Apache-2.0 */
#include "../../../src/relax.c"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

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

static double coef(void)
{
    static const double frac[4] = {1.0, 0.5, 0.25, 0.125};
    double v = (double)pick(-3, 3);
    if (pick(0, 9) == 0)
        v *= frac[pick(1, 3)];
    return v;
}

static jaos_model *build(int64_t nr, int64_t nc, const bool *isint,
                         const double *a, const double *b)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    double *c = calloc((size_t)nc, sizeof *c);
    double *cl = calloc((size_t)nc, sizeof *cl);
    double *cu = calloc((size_t)nc, sizeof *cu);
    int64_t *s = calloc((size_t)nc + 1, sizeof *s);
    int64_t *ix = calloc((size_t)(nr * nc) + 1, sizeof *ix);
    double *v = calloc((size_t)(nr * nc) + 1, sizeof *v);
    int64_t nz = 0;
    for (int64_t j = 0; j < nc; j++) {
        s[j] = nz;
        for (int64_t i = 0; i < nr; i++)
            if (a[i * nc + j] != 0.0) {
                ix[nz] = i;
                v[nz++] = a[i * nc + j];
            }
    }
    s[nc] = nz;
    jaos_status rc = jaos_load_lp(m, nc, nr, JAOS_MINIMIZE, 0.0, c, cl, cu,
                                  b, b, nz, s, ix, v);
    for (int64_t j = 0; rc == JAOS_OK && j < nc; j++)
        if (isint[j])
            rc = jaos_set_col_integer(m, j, true);
    free(c); free(cl); free(cu); free(s); free(ix); free(v);
    if (rc != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

static double now(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static void small(int64_t count, bool half)
{
    int64_t proved = 0, skipped = 0, work = 0;
    int64_t found[2][3] = {{0, 0, 0}, {0, 0, 0}};
    int64_t far[2] = {0, 0};
    for (int64_t k = 0; k < count; k++) {
        const int64_t nr = pick(1, 8), nc = pick(2, 10);
        bool isint[10];
        double z[10], a[80], b[8];
        int64_t ni = 0;
        for (int64_t j = 0; j < nc; j++) {
            isint[j] = pick(0, 1) == 1;
            ni += isint[j];
            z[j] = isint[j] ? (double)pick(-6, 6) : (double)pick(-24, 24) / 8.0;
        }
        if (ni == 0) {
            isint[0] = true;
            z[0] = (double)pick(-6, 6);
        }
        for (int64_t i = 0; i < nr * nc; i++)
            a[i] = coef();
        if (half)
            for (int64_t j = 0; j < nc; j++)
                if (isint[j]) {
                    z[j] += 0.5;
                    break;
                }
        for (int64_t i = 0; i < nr; i++) {
            b[i] = 0.0;
            for (int64_t j = 0; j < nc; j++)
                b[i] += a[i * nc + j] * z[j];
        }
        jaos_model *m = build(nr, nc, isint, a, b);
        if (m == nullptr) {
            skipped++;
            continue;
        }
        int64_t w = 0;
        const bool p = rx_lattice_empty(m, &w);
        proved += p;
        work += w;
        for (int64_t j = 0; j < nc; j++)
            if (jaos_set_col_bounds(m, j, isint[j] ? -20.0 : -INFINITY,
                                    isint[j] ? 20.0 : INFINITY) != JAOS_OK)
                return;
        if (jaos_set_work_limit(m, 100000000) != JAOS_OK ||
            jaos_solve(m) != JAOS_OK)
            return;
        const jaos_solve_status st = jaos_status_of(m);
        const int f = st == JAOS_SOLVE_OPTIMAL ? 0
                    : st == JAOS_SOLVE_INFEASIBLE ? 1 : 2;
        found[p][f]++;
        if (!p && f == 1) {
            for (int64_t j = 0; j < nc; j++)
                if (isint[j] &&
                    jaos_set_col_bounds(m, j, -1e6, 1e6) != JAOS_OK)
                    return;
            if (jaos_solve(m) != JAOS_OK)
                return;
            far[jaos_status_of(m) == JAOS_SOLVE_OPTIMAL ? 0 : 1]++;
        }
        jaos_model_free(m);
    }
    printf("%s models %" PRId64 " proved %" PRId64 " skipped %" PRId64
           " work %" PRId64 "\n", half ? "half" : "plant", count, proved,
           skipped, work);
    printf("  box [-20,20] point/none/limit: unproved %" PRId64 "/%" PRId64
           "/%" PRId64 ", proved %" PRId64 "/%" PRId64 "/%" PRId64 "\n",
           found[0][0], found[0][1], found[0][2], found[1][0], found[1][1],
           found[1][2]);
    printf("  unproved with none in [-20,20], box [-1e6,1e6] point/none: %"
           PRId64 "/%" PRId64 "\n", far[0], far[1]);
}

static void dense(int64_t nr, int64_t nc, bool odd)
{
    bool *isint = calloc((size_t)nc, sizeof *isint);
    double *a = calloc((size_t)(nr * nc), sizeof *a);
    double *b = calloc((size_t)nr, sizeof *b);
    double *z = calloc((size_t)nc, sizeof *z);
    for (int64_t j = 0; j < nc; j++) {
        isint[j] = true;
        z[j] = (double)pick(-6, 6);
    }
    for (int64_t i = 0; i < nr * nc; i++)
        a[i] = (double)pick(-3, 3);
    if (odd)
        for (int64_t j = 0; j < nc; j++)
            a[(nr - 1) * nc + j] *= 2.0;
    for (int64_t i = 0; i < nr; i++)
        for (int64_t j = 0; j < nc; j++)
            b[i] += a[i * nc + j] * z[j];
    if (odd)
        b[nr - 1] += 1.0;
    jaos_model *m = build(nr, nc, isint, a, b);
    int64_t w = 0;
    const double t0 = now();
    const bool p = m != nullptr && rx_lattice_empty(m, &w);
    printf("dense %" PRId64 "x%" PRId64 " %s proved %d work %" PRId64
           " seconds %.3f\n", nr, nc, odd ? "odd" : "plant", (int)p, w,
           now() - t0);
    jaos_model_free(m);
    free(isint); free(a); free(b); free(z);
}

int main(int argc, char **argv)
{
    const int64_t count = argc > 1 ? strtoll(argv[1], nullptr, 10) : 2000;
    s_state = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
    small(count, false);
    small(count, true);
    dense(32, 127, false);
    dense(32, 127, true);
    dense(16, 255, true);
    dense(8, 511, true);
    return 0;
}
