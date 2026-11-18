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

enum { NMAX = 12 };

typedef struct {
    int64_t nr, nc;
    bool isint[NMAX];
    double a[NMAX * NMAX], lo[NMAX], hi[NMAX];
} spec;

static double now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static jaos_model *build(const spec *g)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    double c[NMAX] = {0}, cl[NMAX], cu[NMAX], v[NMAX * NMAX];
    int64_t s[NMAX + 1], ix[NMAX * NMAX], nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        cl[j] = -INFINITY;
        cu[j] = INFINITY;
        s[j] = nz;
        for (int64_t i = 0; i < g->nr; i++)
            if (g->a[i * g->nc + j] != 0.0) {
                ix[nz] = i;
                v[nz++] = g->a[i * g->nc + j];
            }
    }
    s[g->nc] = nz;
    jaos_status rc = jaos_load_lp(m, g->nc, g->nr, JAOS_MINIMIZE, 0.0, c, cl,
                                  cu, g->lo, g->hi, nz, s, ix, v);
    for (int64_t j = 0; rc == JAOS_OK && j < g->nc; j++)
        if (g->isint[j])
            rc = jaos_set_col_integer(m, j, true);
    if (rc != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

static void print_spec(const char *tag, const spec *g)
{
    printf("%s %" PRId64 " %" PRId64, tag, g->nr, g->nc);
    for (int64_t j = 0; j < g->nc; j++)
        printf(" %d", (int)g->isint[j]);
    for (int64_t i = 0; i < g->nr; i++) {
        printf(" |");
        for (int64_t j = 0; j < g->nc; j++)
            printf(" %.17g", g->a[i * g->nc + j]);
        printf(" : %.17g %.17g", g->lo[i], g->hi[i]);
    }
    printf("\n");
}

static double coef(int64_t big)
{
    static const double frac[4] = {1.0, 0.5, 0.25, 0.125};
    double v = (double)pick(1, big) * (pick(0, 1) ? 1.0 : -1.0);
    if (pick(0, 9) == 0)
        v *= frac[pick(1, 3)];
    return v;
}

static void gen_mixed(spec *g, bool shift, int64_t big)
{
    static const double slack[8] = {0, 0, 0.125, 0.25, 0.5, 0.75, 1, 2};
    double z[NMAX];
    g->nc = pick(2, 8);
    g->nr = pick(1, 8);
    int64_t ni = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = pick(0, 3) != 0;
        ni += g->isint[j];
        z[j] = g->isint[j] ? (double)pick(-6, 6) : (double)pick(-48, 48) / 8.0;
    }
    if (ni == 0) {
        g->isint[0] = true;
        z[0] = (double)pick(-6, 6);
    }
    if (shift)
        for (int64_t j = 0; j < g->nc; j++)
            if (g->isint[j]) {
                z[j] += 0.5;
                break;
            }
    for (int64_t i = 0; i < g->nr; i++) {
        bool any = false;
        double dot = 0.0;
        for (int64_t j = 0; j < g->nc; j++) {
            double v = pick(0, 9) < 3 ? 0.0 : coef(big);
            if (j == g->nc - 1 && !any && v == 0.0)
                v = 1.0;
            any = any || v != 0.0;
            g->a[i * g->nc + j] = v;
            dot += v * z[j];
        }
        const int64_t kind = pick(0, 9);
        g->lo[i] = kind <= 5 || kind == 6 || kind == 7
                       ? dot - (kind <= 2 ? 0.0 : slack[pick(0, 7)]) : -INFINITY;
        g->hi[i] = kind <= 2 ? dot
                 : kind <= 5 || kind >= 8 ? dot + slack[pick(0, 7)] : INFINITY;
        if (kind <= 2)
            g->lo[i] = g->hi[i] = dot;
    }
}

static void gen_pugh(spec *g, int64_t big)
{
    double z[NMAX];
    g->nc = pick(2, 3);
    g->nr = g->nc + pick(0, 1);
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = true;
        z[j] = (double)pick(-3, 3) + (double)pick(0, 7) / 8.0;
    }
    for (int64_t i = 0; i < g->nr; i++) {
        double dot = 0.0;
        bool any = false;
        for (int64_t j = 0; j < g->nc; j++) {
            double v = (double)pick(-big, big);
            if (j == g->nc - 1 && !any && v == 0.0)
                v = 1.0;
            any = any || v != 0.0;
            g->a[i * g->nc + j] = v;
            dot += v * z[j];
        }
        g->lo[i] = dot - (double)pick(0, 12) / 8.0;
        g->hi[i] = dot + (double)pick(0, 12) / 8.0;
    }
}

static int mip_point(jaos_model *m, const spec *g, double box)
{
    for (int64_t j = 0; j < g->nc; j++)
        if (jaos_set_col_bounds(m, j, g->isint[j] ? -box : -INFINITY,
                                g->isint[j] ? box : INFINITY) != JAOS_OK)
            return -2;
    if (jaos_set_work_limit(m, 100000000) != JAOS_OK || jaos_solve(m) != JAOS_OK)
        return -2;
    const jaos_solve_status st = jaos_status_of(m);
    return st == JAOS_SOLVE_OPTIMAL ? 1 : st == JAOS_SOLVE_INFEASIBLE ? 0 : -1;
}

static int brute(const spec *g)
{
    int64_t lo[NMAX], hi[NMAX], count = 1;
    jaos_model *m = build(g);
    if (m == nullptr)
        return -2;
    for (int64_t j = 0; j < g->nc; j++)
        if (jaos_set_col_integer(m, j, false) != JAOS_OK)
            return -2;
    for (int64_t j = 0; j < g->nc; j++) {
        double r[2];
        for (int k = 0; k < 2; k++) {
            double c[NMAX] = {0};
            c[j] = k == 0 ? 1.0 : -1.0;
            for (int64_t t = 0; t < g->nc; t++)
                if (jaos_set_col_cost(m, t, c[t]) != JAOS_OK)
                    return -2;
            if (jaos_solve(m) != JAOS_OK)
                return -2;
            const jaos_solve_status st = jaos_status_of(m);
            if (st == JAOS_SOLVE_INFEASIBLE) {
                jaos_model_free(m);
                return 0;
            }
            if (st != JAOS_SOLVE_OPTIMAL) {
                jaos_model_free(m);
                return -1;
            }
            if (jaos_objective(m, &r[k]) != JAOS_OK)
                return -2;
        }
        lo[j] = (int64_t)floor(r[0] - 1e-6);
        hi[j] = (int64_t)ceil(-r[1] + 1e-6);
        count *= hi[j] - lo[j] + 1;
        if (count > 10000000) {
            jaos_model_free(m);
            return -1;
        }
    }
    jaos_model_free(m);
    int64_t x[NMAX];
    for (int64_t j = 0; j < g->nc; j++)
        x[j] = lo[j];
    for (;;) {
        bool ok = true;
        for (int64_t i = 0; i < g->nr && ok; i++) {
            int64_t dot = 0;
            for (int64_t j = 0; j < g->nc; j++)
                dot += (int64_t)(8.0 * g->a[i * g->nc + j]) * x[j];
            ok = dot >= (int64_t)(8.0 * g->lo[i]) &&
                 dot <= (int64_t)(8.0 * g->hi[i]);
        }
        if (ok)
            return 1;
        int64_t j = 0;
        while (j < g->nc && x[j] == hi[j]) {
            x[j] = lo[j];
            j++;
        }
        if (j == g->nc)
            return 0;
        x[j]++;
    }
}

static int verdict(const spec *g, int64_t budget, int64_t *work, bool *old)
{
    jaos_model *m = build(g);
    if (m == nullptr)
        return -9;
    int64_t w = 0;
    *old = rx_lattice_empty(m, &w) || rx_range_empty(m, &w) >= 0;
    const int v = rx_rows_empty(m, work, budget);
    jaos_model_free(m);
    return v;
}

static void family(const char *name, int64_t count, int64_t budget,
                   int which, int64_t big)
{
    int64_t n[3] = {0, 0, 0}, old = 0, only_new = 0, only_old = 0;
    int64_t check[3][4] = {{0}};
    int64_t work = 0, top = 0;
    const double t0 = now();
    for (int64_t k = 0; k < count; k++) {
        spec g;
        if (which == 0)
            gen_mixed(&g, false, big);
        else if (which == 1)
            gen_mixed(&g, true, big);
        else
            gen_pugh(&g, big);
        int64_t w = 0;
        bool o = false;
        const int v = verdict(&g, budget, &w, &o);
        if (v < -1)
            continue;
        n[v + 1]++;
        work += w;
        top = w > top ? w : top;
        old += o;
        only_new += v == RX_NONE && !o;
        only_old += v != RX_NONE && o;
        if (which == 0 && v == RX_NONE)
            print_spec("REFUSED-PLANT", &g);
        if (which == 1) {
            jaos_model *m = build(&g);
            int f = mip_point(m, &g, 64.0);
            if (f == 0 && v != RX_NONE) {
                const int far = mip_point(m, &g, 1e5);
                f = far == 1 ? 2 : far;
            }
            jaos_model_free(m);
            check[v + 1][f < 0 ? 3 : f]++;
            if (v == RX_NONE && f >= 1)
                print_spec("REFUSED-WITH-POINT", &g);
            if (v == RX_POINT && f == 0)
                print_spec("POINT-NOT-FOUND", &g);
        }
        if (which == 2) {
            const int b = brute(&g);
            check[v + 1][b < 0 ? 3 : b]++;
            if ((v == RX_NONE && b == 1) || (v == RX_POINT && b == 0))
                print_spec("DISAGREE", &g);
        }
    }
    printf("%s models %" PRId64 " none %" PRId64 " point %" PRId64
           " open %" PRId64 " | old proofs %" PRId64 ", new only %" PRId64
           ", old only %" PRId64 " | work %" PRId64 " most %" PRId64
           " seconds %.3f\n",
           name, n[0] + n[1] + n[2], n[2], n[1], n[0], old, only_new,
           only_old, work, top, now() - t0);
    if (which == 1)
        for (int v = 2; v >= 0; v--)
            printf("  %-5s MIP: none in [-64,64] %" PRId64 ", point in it %"
                   PRId64 ", point in [-1e5,1e5] %" PRId64 ", stopped %"
                   PRId64 "\n", v == 2 ? "none" : v == 1 ? "point" : "open",
                   check[v][0], check[v][1], check[v][2], check[v][3]);
    if (which == 2)
        for (int v = 2; v >= 0; v--)
            printf("  %-5s brute force: no point %" PRId64 ", point %" PRId64
                   ", unbounded or too wide %" PRId64 "\n",
                   v == 2 ? "none" : v == 1 ? "point" : "open", check[v][0],
                   check[v][1], check[v][3]);
}

static void dense(int64_t nr, int64_t nc, int64_t budget)
{
    double *a = calloc((size_t)(nr * nc), sizeof *a);
    double *lo = calloc((size_t)nr, sizeof *lo);
    double *hi = calloc((size_t)nr, sizeof *hi);
    double *z = calloc((size_t)nc, sizeof *z);
    double *c = calloc((size_t)nc, sizeof *c);
    double *cl = calloc((size_t)nc, sizeof *cl);
    double *cu = calloc((size_t)nc, sizeof *cu);
    int64_t *s = calloc((size_t)nc + 1, sizeof *s);
    int64_t *ix = calloc((size_t)(nr * nc), sizeof *ix);
    double *v = calloc((size_t)(nr * nc), sizeof *v);
    for (int64_t j = 0; j < nc; j++)
        z[j] = (double)pick(-6, 6);
    for (int64_t i = 0; i < nr; i++) {
        double dot = 0.0;
        for (int64_t j = 0; j < nc; j++) {
            a[i * nc + j] = (double)pick(-3, 3);
            dot += a[i * nc + j] * z[j];
        }
        lo[i] = dot - 0.25;
        hi[i] = dot + 0.25;
    }
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
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) == JAOS_OK &&
        jaos_load_lp(m, nc, nr, JAOS_MINIMIZE, 0.0, c, cl, cu, lo, hi, nz, s,
                     ix, v) == JAOS_OK) {
        bool ok = true;
        for (int64_t j = 0; ok && j < nc; j++)
            ok = jaos_set_col_integer(m, j, true) == JAOS_OK;
        int64_t w = 0;
        const double t0 = now();
        const int r = ok ? rx_rows_empty(m, &w, budget) : -9;
        printf("dense %" PRId64 "x%" PRId64 " verdict %s work %" PRId64
               " seconds %.3f\n", nr, nc,
               r == RX_NONE ? "none" : r == RX_POINT ? "point" : "open", w,
               now() - t0);
    }
    jaos_model_free(m);
    free(a); free(lo); free(hi); free(z); free(c); free(cl); free(cu);
    free(s); free(ix); free(v);
}

int main(int argc, char **argv)
{
    const int64_t count = argc > 1 ? strtoll(argv[1], nullptr, 10) : 2000;
    s_state = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
    const int64_t budget = argc > 3 ? strtoll(argv[3], nullptr, 10)
                                    : RELAX_SHADOW_WORK;
    family("plant", count, budget, 0, 5);
    family("plant-wide", count, budget, 0, 15);
    family("half", count, budget, 1, 5);
    family("pugh", count, budget, 2, 15);
    family("pugh-wide", count / 4, budget, 2, 60);
    dense(2, 1023, budget);
    dense(8, 255, budget);
    dense(8, 15, budget);
    dense(16, 15, budget);
    dense(32, 63, budget);
    return 0;
}
