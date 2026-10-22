/* The reading behind 02-421: what a ranging interval promises.
 *
 * The generator is 02-235's, LPs only, as 02-411's scaled.c. Inside the
 * interval that jaos_cost_ranging, jaos_rhs_ranging or jaos_bound_ranging
 * gives a number, the published basis stays optimal, so the optimal value
 * is linear in that number. For each optimal model the harness picks three
 * columns' costs, three rows' sides and three columns' bounds, sets each
 * to two points strictly inside its interval, solves both models afresh,
 * and checks:
 *   P1 each solve ends optimal
 *   P2 the three values (the current number's and the two new ones) lie on
 *      one line, within 1e-7 of their size
 *
 * Usage: rangecheck RUNS SEED
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state;

static uint64_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static int64_t ri(int64_t lo, int64_t hi)
{
    return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1));
}

static double ru(void)
{
    return (double)(rnd() >> 11) / 9007199254740992.0;
}

#define MAXC 24
#define MAXR 16

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool maximise;
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->offset = (double)ri(-3, 3);
    const bool broken = ri(0, 9) < 6;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = broken && ri(0, 3) == 0 ? INFINITY : (double)ri(1, 10);
        z[j] = ri(0, isfinite(g->cu[j]) ? (int64_t)g->cu[j] : 10);
        g->ap[j] = g->nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (ri(0, 9) >= 5)
                continue;
            int64_t a = ri(-3, 3);
            if (a == 0)
                a = 1;
            g->ai[g->nz] = i;
            g->av[g->nz] = (double)a;
            g->nz++;
            act[i] += (double)a * (double)z[j];
        }
    }
    g->ap[g->nc] = g->nz;
    for (int64_t i = 0; i < g->nr; i++) {
        const int64_t kind = ri(0, 9);
        const double shift = broken ? (double)ri(-40, 40) : 0.0;
        if (kind < 3) {
            g->rl[i] = g->ru[i] = act[i] + shift;
        } else if (kind < 6) {
            g->rl[i] = -INFINITY;
            g->ru[i] = act[i] + (double)ri(0, 3) + shift;
        } else if (kind < 9) {
            g->rl[i] = act[i] - (double)ri(0, 3) + shift;
            g->ru[i] = INFINITY;
        } else {
            g->rl[i] = act[i] - (double)ri(0, 2) + shift;
            g->ru[i] = act[i] + (double)ri(0, 2) + shift;
        }
    }
}

static jaos_model *load(const gen *g)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    if (jaos_load_lp(m, g->nc, g->nr,
                     g->maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE, g->offset,
                     g->cost, g->cl, g->cu, g->rl, g->ru, g->nz, g->ap,
                     g->ai, g->av) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

enum { COST, ROW_LO, ROW_UP, COL_LO, COL_UP };
static const char *const KIND[5] = {"cost", "row lower", "row upper",
                                    "column lower", "column upper"};

static bool apply(gen *g, int kind, int64_t k, double t)
{
    switch (kind) {
    case COST: g->cost[k] = t; return true;
    case ROW_LO: g->rl[k] = t; return g->rl[k] <= g->ru[k];
    case ROW_UP: g->ru[k] = t; return g->rl[k] <= g->ru[k];
    case COL_LO: g->cl[k] = t; return g->cl[k] <= g->cu[k];
    default: g->cu[k] = t; return g->cl[k] <= g->cu[k];
    }
}

static double current(const gen *g, int kind, int64_t k)
{
    switch (kind) {
    case COST: return g->cost[k];
    case ROW_LO: return g->rl[k];
    case ROW_UP: return g->ru[k];
    case COL_LO: return g->cl[k];
    default: return g->cu[k];
    }
}

static int64_t broke[3], tried;

static void fail(int p, int64_t idx, const char *what)
{
    broke[p]++;
    if (broke[p] <= 8)
        printf("P%d m%" PRId64 " %s\n", p, idx, what);
}

static double inside(double lo, double hi, double now)
{
    const double a = isfinite(lo) ? lo : now - 10.0;
    const double b = isfinite(hi) ? hi : now + 10.0;
    return a + (0.1 + 0.8 * ru()) * (b - a);
}

static void probe(const gen *g, int64_t r, double obj, int kind, int64_t k,
                  double lo, double hi)
{
    const double now = current(g, kind, k);
    if (!isfinite(now) || !(hi - lo > 1e-6))
        return;
    const double t2 = inside(lo, hi, now), t3 = inside(lo, hi, now);
    if (fabs(t2 - now) < 1e-6 || fabs(t3 - now) < 1e-6)
        return;
    double o[2] = {0.0, 0.0};
    const double t[2] = {t2, t3};
    char what[300];
    for (int s = 0; s < 2; s++) {
        gen h = *g;
        if (!apply(&h, kind, k, t[s]))
            return;
        jaos_model *m = load(&h);
        if (m == nullptr || jaos_solve(m) != JAOS_OK)
            exit(1);
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            snprintf(what, sizeof what, "%s %" PRId64 " at %.9g in [%.9g, %.9g] "
                     "(now %.9g): %s", KIND[kind], k, t[s], lo, hi, now,
                     jaos_solve_status_str(jaos_status_of(m)));
            fail(1, r, what);
            jaos_model_free(m);
            return;
        }
        if (jaos_objective(m, &o[s]) != JAOS_OK)
            exit(1);
        jaos_model_free(m);
    }
    tried++;
    const double pred = obj + (o[0] - obj) * (t3 - now) / (t2 - now);
    const double size = 1.0 + fabs(obj) + fabs(o[0]) + fabs(o[1]);
    if (fabs(pred - o[1]) > 1e-7 * size) {
        snprintf(what, sizeof what, "%s %" PRId64 " in [%.9g, %.9g]: at %.9g "
                 "%.12g, at %.9g %.12g, at %.9g %.12g where a line gives %.12g",
                 KIND[kind], k, lo, hi, now, obj, t2, o[0], t3, o[1], pred);
        fail(2, r, what);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: rangecheck RUNS SEED\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    int64_t optimal = 0, refused = 0;
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr || jaos_solve(m) != JAOS_OK)
            return 1;
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }
        optimal++;
        double obj = 0.0;
        double clo[MAXC], chi[MAXC];
        double rll[MAXR], rlh[MAXR], rul[MAXR], ruh[MAXR];
        double bll[MAXC], blh[MAXC], bul[MAXC], buh[MAXC];
        if (jaos_objective(m, &obj) != JAOS_OK)
            return 1;
        if (jaos_cost_ranging(m, clo, chi) != JAOS_OK ||
            jaos_rhs_ranging(m, rll, rlh, rul, ruh) != JAOS_OK ||
            jaos_bound_ranging(m, bll, blh, bul, buh) != JAOS_OK) {
            refused++;
            jaos_model_free(m);
            continue;
        }
        jaos_model_free(m);
        for (int s = 0; s < 3; s++) {
            const int64_t j = ri(0, g.nc - 1), i = ri(0, g.nr - 1);
            const int64_t b = ri(0, g.nc - 1);
            probe(&g, r, obj, COST, j, clo[j], chi[j]);
            probe(&g, r, obj, ROW_LO, i, rll[i], rlh[i]);
            probe(&g, r, obj, ROW_UP, i, rul[i], ruh[i]);
            probe(&g, r, obj, COL_LO, b, bll[b], blh[b]);
            probe(&g, r, obj, COL_UP, b, bul[b], buh[b]);
        }
    }
    printf("models %" PRId64 " optimal %" PRId64 " ranging refused %" PRId64
           " intervals probed %" PRId64 "\n", runs, optimal, refused, tried);
    printf("P1=%" PRId64 " P2=%" PRId64 "\n", broke[1], broke[2]);
    printf("broken total %" PRId64 "\n", broke[1] + broke[2]);
    return 0;
}
