/* The reading behind 02-410: models on the edge of feasibility.
 *
 * The generator is 02-235's, LPs only, as 02-396's algos.c. For each
 * model the dual simplex finds infeasible, jaos_feasrelax's moves under
 * each of its three scopes are added to the bounds, which leaves a model
 * feasible only on the edge: often a single point, its sides carrying the
 * rounding of the moves. Each such model is solved by the five LP
 * algorithms. Properties, for each:
 *   P1 the solve ends optimal
 *   P2 the checker passes its point and duals at 1e-7
 *
 * Usage: edge RUNS SEED
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


static const char *const NAME[5] = {"dual", "primal", "barrier", "pdlp",
                                    "concurrent"};
static int64_t broke[5][3];

static void fail(int a, int p, int64_t idx, int scope, const char *what)
{
    broke[a][p]++;
    if (broke[a][p] <= 6)
        printf("%s P%d m%" PRId64 " scope %d %s\n", NAME[a], p, idx, scope, what);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: edge RUNS SEED\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    int64_t edges = 0;
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
        for (int sc = 1; sc <= 3; sc++) {
            double rmove[MAXR], cmove[MAXC];
            jaos_relax_report rr;
            if (jaos_feasrelax(m, (jaos_relax_scope)sc, rmove, cmove, &rr) !=
                JAOS_OK)
                continue;
            edges++;
            for (int a = 0; a < 5; a++) {
                jaos_model *e = moved(&g, sc & 1 ? rmove : nullptr,
                                      sc & 2 ? cmove : nullptr);
                char what[200];
                if (e == nullptr ||
                    jaos_set_algorithm(e, (jaos_algorithm)a) != JAOS_OK)
                    return 1;
                if (jaos_solve(e) != JAOS_OK ||
                    jaos_status_of(e) != JAOS_SOLVE_OPTIMAL) {
                    snprintf(what, sizeof what, "status %s: %s",
                             jaos_solve_status_str(jaos_status_of(e)),
                             jaos_model_error(e));
                    fail(a, 1, r, sc, what);
                } else {
                    double x[MAXC], y[MAXR];
                    jaos_check_report ck;
                    if (jaos_solution(e, x, nullptr, y, nullptr) != JAOS_OK ||
                        jaos_check_solution(e, x, y, 1e-7, &ck) != JAOS_OK ||
                        !ck.primal_feasible || !ck.dual_feasible) {
                        snprintf(what, sizeof what, "the checker refuses the "
                                 "optimum: rows %.3g", ck.max_row_violation);
                        fail(a, 2, r, sc, what);
                    }
                }
                jaos_model_free(e);
            }
        }
        jaos_model_free(m);
    }
    printf("edge models %" PRId64 "\n", edges);
    int64_t total = 0;
    for (int a = 0; a < 5; a++) {
        printf("%-10s P1=%" PRId64 " P2=%" PRId64 "\n", NAME[a], broke[a][1],
               broke[a][2]);
        total += broke[a][1] + broke[a][2];
    }
    printf("broken total %" PRId64 "\n", total);
    return 0;
}
