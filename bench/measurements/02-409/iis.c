/* The reading behind 02-409: the IIS and the feasibility relaxation on
 * generated infeasible LPs.
 *
 * The generator is 02-235's, LPs only, as 02-396's algos.c. Each model
 * the dual simplex finds infeasible is put to jaos_iis and to
 * jaos_feasrelax under its three scopes. Properties:
 *   P1 jaos_iis returns a subsystem
 *   P2 jaos_iis_model's model of it is infeasible
 *   P3 without any one of its member sides, it is feasible
 *   P4 adding every move jaos_feasrelax returns to its bound gives a
 *      feasible model; over the columns alone, a refusal that no move of
 *      the boxes helps must match an infeasible elastic LP (P5)
 *   P5 the total move is within 1e-6 of (1 + |total|) of the optimum of an
 *      elastic LP built here, with a slack of cost 1 on every finite side
 *      that may move
 *
 * Usage: iis RUNS SEED
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

static int64_t broke[6];

static void fail(int p, int64_t idx, const char *what)
{
    broke[p]++;
    if (broke[p] <= 8)
        printf("P%d m%" PRId64 " %s\n", p, idx, what);
}

static int solve_status(jaos_model *m)
{
    if (jaos_solve(m) != JAOS_OK)
        return -1;
    return (int)jaos_status_of(m);
}

static jaos_model *moved(const gen *g, const double *row_move,
                         const double *col_move)
{
    gen h = *g;
    for (int64_t j = 0; j < h.nc; j++)
        h.cost[j] = 0.0;
    for (int64_t i = 0; i < h.nr; i++) {
        if (row_move != nullptr && row_move[i] < 0.0) h.rl[i] += row_move[i];
        if (row_move != nullptr && row_move[i] > 0.0) h.ru[i] += row_move[i];
    }
    for (int64_t j = 0; j < h.nc; j++) {
        if (col_move != nullptr && col_move[j] < 0.0) h.cl[j] += col_move[j];
        if (col_move != nullptr && col_move[j] > 0.0) h.cu[j] += col_move[j];
    }
    return load(&h);
}

static double elastic(const gen *g, bool rows, bool cols)
{
    int64_t n = g->nc, nz = 0;
    int64_t nr = g->nr + (cols ? 2 * g->nc : 0);
    static double cost[4 * MAXC + 4 * MAXR], cl[4 * MAXC + 4 * MAXR],
        cu[4 * MAXC + 4 * MAXR], rl[MAXR + 2 * MAXC], ru[MAXR + 2 * MAXC],
        av[MAXC * MAXR + 8 * (MAXC + MAXR)];
    static int64_t ap[4 * MAXC + 4 * MAXR + 1], ai[MAXC * MAXR + 8 * (MAXC + MAXR)];
    for (int64_t j = 0; j < g->nc; j++) {
        cost[j] = 0.0;
        cl[j] = cols ? -INFINITY : g->cl[j];
        cu[j] = cols ? INFINITY : g->cu[j];
        ap[j] = nz;
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++) {
            ai[nz] = g->ai[p];
            av[nz++] = g->av[p];
        }
        if (cols) {
            ai[nz] = g->nr + 2 * j;
            av[nz++] = 1.0;
            ai[nz] = g->nr + 2 * j + 1;
            av[nz++] = 1.0;
        }
    }
    for (int64_t i = 0; i < g->nr; i++) {
        rl[i] = g->rl[i];
        ru[i] = g->ru[i];
    }
    if (cols)
        for (int64_t j = 0; j < g->nc; j++) {
            rl[g->nr + 2 * j] = g->cl[j];
            ru[g->nr + 2 * j] = INFINITY;
            rl[g->nr + 2 * j + 1] = -INFINITY;
            ru[g->nr + 2 * j + 1] = g->cu[j];
        }
    for (int64_t i = 0; i < nr; i++) {
        const bool is_col_row = i >= g->nr;
        if (is_col_row ? !cols : !rows)
            continue;
        if (isfinite(rl[i])) {
            cost[n] = 1.0; cl[n] = 0.0; cu[n] = INFINITY; ap[n] = nz;
            ai[nz] = i; av[nz++] = 1.0; n++;
        }
        if (isfinite(ru[i])) {
            cost[n] = 1.0; cl[n] = 0.0; cu[n] = INFINITY; ap[n] = nz;
            ai[nz] = i; av[nz++] = -1.0; n++;
        }
    }
    ap[n] = nz;
    jaos_model *m = nullptr;
    double obj = NAN;
    if (jaos_model_new(&m) == JAOS_OK &&
        jaos_load_lp(m, n, nr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, nz,
                     ap, ai, av) == JAOS_OK &&
        jaos_solve(m) == JAOS_OK && jaos_status_of(m) == JAOS_SOLVE_OPTIMAL)
        (void)jaos_objective(m, &obj);
    jaos_model_free(m);
    return obj;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: iis RUNS SEED\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    int64_t infeasible = 0, members = 0, drops = 0, relaxes = 0;
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr || jaos_solve(m) != JAOS_OK)
            return 1;
        if (jaos_status_of(m) != JAOS_SOLVE_INFEASIBLE) {
            jaos_model_free(m);
            continue;
        }
        infeasible++;
        char what[200];
        jaos_iis_side rs[MAXR], cs[MAXC];
        jaos_iis_report rep;
        if (jaos_iis(m, rs, cs, &rep) != JAOS_OK) {
            snprintf(what, sizeof what, "iis: %s", jaos_model_error(m));
            fail(1, r, what);
        } else {
            members += rep.members;
            jaos_model *sub = nullptr;
            if (jaos_iis_model(m, rs, cs, &sub) != JAOS_OK)
                return 1;
            const int s = solve_status(sub);
            if (s != JAOS_SOLVE_INFEASIBLE) {
                snprintf(what, sizeof what, "the subsystem solves %d", s);
                fail(2, r, what);
            }
            jaos_model_free(sub);
            for (int64_t k = 0; k < g.nr + g.nc; k++) {
                jaos_iis_side *side = k < g.nr ? &rs[k] : &cs[k - g.nr];
                for (int bit = 1; bit <= 2; bit <<= 1) {
                    if (!(*side & bit))
                        continue;
                    const jaos_iis_side keep = *side;
                    *side = (jaos_iis_side)(*side & ~bit);
                    jaos_model *less = nullptr;
                    if (jaos_iis_model(m, rs, cs, &less) != JAOS_OK)
                        return 1;
                    drops++;
                    const int t = solve_status(less);
                    if (t != JAOS_SOLVE_OPTIMAL) {
                        snprintf(what, sizeof what, "without the %s side of %s %"
                                 PRId64 " it solves %d", bit == 1 ? "lower" : "upper",
                                 k < g.nr ? "row" : "column",
                                 k < g.nr ? k : k - g.nr, t);
                        fail(3, r, what);
                    }
                    jaos_model_free(less);
                    *side = keep;
                }
            }
        }
        for (int sc = 1; sc <= 3; sc++) {
            double rmove[MAXR], cmove[MAXC];
            jaos_relax_report rr;
            const jaos_status fs = jaos_feasrelax(m, (jaos_relax_scope)sc, rmove,
                                                  cmove, &rr);
            if (fs != JAOS_OK) {
                if (sc == 2 && rr.status == JAOS_SOLVE_INFEASIBLE) {
                    if (!isnan(elastic(&g, false, true))) {
                        snprintf(what, sizeof what, "scope 2: no box, but the "
                                 "elastic LP over the columns is feasible");
                        fail(5, r, what);
                    }
                    continue;
                }
                snprintf(what, sizeof what, "feasrelax scope %d: %s", sc,
                         jaos_model_error(m));
                fail(4, r, what);
                continue;
            }
            relaxes++;
            jaos_model *mv = moved(&g, sc & 1 ? rmove : nullptr,
                                   sc & 2 ? cmove : nullptr);
            const int t = solve_status(mv);
            jaos_model_free(mv);
            if (t != JAOS_SOLVE_OPTIMAL) {
                snprintf(what, sizeof what, "scope %d: the moved model solves %d",
                         sc, t);
                fail(4, r, what);
            }
            const double best = elastic(&g, sc & 1, sc & 2);
            if (!(fabs(best - rr.total) <= 1e-6 * (1.0 + fabs(best)))) {
                snprintf(what, sizeof what, "scope %d: total %.12g, the elastic "
                         "LP's %.12g", sc, rr.total, best);
                fail(5, r, what);
            }
        }
        jaos_model_free(m);
    }
    printf("infeasible %" PRId64 " of %" PRId64 ", members %" PRId64
           ", drops %" PRId64 ", relaxations %" PRId64 "\n", infeasible, runs,
           members, drops, relaxes);
    printf("P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64 " P5=%"
           PRId64 " broken total %" PRId64 "\n", broke[1], broke[2], broke[3],
           broke[4], broke[5],
           broke[1] + broke[2] + broke[3] + broke[4] + broke[5]);
    return 0;
}
