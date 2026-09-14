/* The reading behind 02-231: the incumbent callback over generated MIPs.
 *
 * Every model plants an integer point inside the boxes and sets each row's
 * bounds around it, so every model has an integer point and the tree has
 * something to announce. Half the models run with the rounding heuristic,
 * the dive heuristic, the pump and the root cuts off and the tree diving,
 * so the incumbents come from the leaves and there are several of them. Each model is solved three
 * times: once recording every call, once with the callback stopping at
 * its first call, and once more recording every call again.
 *
 * Nine properties, read off the model's own arrays:
 *   P1 every announced point is feasible for the model as loaded, by
 *      jaos_check_solution at 1e-6, integrality included
 *   P2 the announced objective is the cost row at the announced point
 *   P3 each announcement is strictly better than the one before
 *   P4 the announced bound is on the right side of the announced objective
 *   P5 nodes never go backwards, and the first call's node is what
 *      jaos_mip_result reports as first_incumbent_node
 *   P6 the last announced objective is the published one, and the last
 *      announced integer columns are the published ones
 *   P7 there is a call exactly when the report has an incumbent
 *   P8 a STOP from the first call ends the solve INTERRUPTED, and
 *      jaos_mip_incumbent gives the announced point back exactly
 *   P9 the second full solve announces the same sequence, call for call
 *
 * Usage: incumbent RUNS SEED [DUMP_INDEX DUMP_PATH]
 * Prints one line per broken property and a summary; exits non-zero when
 * any property broke.
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

#define MAXC 16
#define MAXR 8
#define MAXCALLS 4096

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool isint[MAXC];
    bool maximise;
    bool plain;
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 16);
    g->nr = ri(3, 8);
    g->maximise = ri(0, 1) == 1;
    g->plain = ri(0, 1) == 1;
    g->offset = (double)ri(-3, 3);
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = ri(0, 9) < 6;
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = (double)ri(1, 10);
        z[j] = ri(0, (int64_t)g->cu[j]);
        g->ap[j] = g->nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (ri(0, 9) >= 6)
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
        if (kind < 3) {
            g->rl[i] = g->ru[i] = act[i];
        } else if (kind < 6) {
            g->rl[i] = -INFINITY;
            g->ru[i] = act[i] + (double)ri(0, 3);
        } else if (kind < 9) {
            g->rl[i] = act[i] - (double)ri(0, 3);
            g->ru[i] = INFINITY;
        } else {
            g->rl[i] = act[i] - (double)ri(0, 2);
            g->ru[i] = act[i] + (double)ri(0, 2);
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
    for (int64_t j = 0; j < g->nc; j++) {
        if (g->isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) {
            jaos_model_free(m);
            return nullptr;
        }
    }
    if (g->plain &&
        (jaos_set_mip_heuristics(m, false) != JAOS_OK ||
         jaos_set_mip_dive_heuristic(m, 0) != JAOS_OK ||
         jaos_set_mip_feaspump(m, 0) != JAOS_OK ||
         jaos_set_mip_cut_rounds(m, 0) != JAOS_OK ||
         jaos_set_mip_dive(m, true) != JAOS_OK)) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

typedef struct {
    int64_t node[MAXCALLS];
    double obj[MAXCALLS], bound[MAXCALLS];
    double x[MAXCALLS][MAXC];
    bool by_rounding[MAXCALLS];
    int64_t calls;
    int64_t stop_at;
    int64_t nc;
} record;

static jaos_callback_action on_incumbent(const jaos_incumbent *inc, void *user)
{
    record *r = user;
    if (r->calls < MAXCALLS) {
        r->node[r->calls] = inc->node;
        r->obj[r->calls] = inc->objective;
        r->bound[r->calls] = inc->bound;
        r->by_rounding[r->calls] = inc->by_rounding;
        for (int64_t j = 0; j < r->nc && j < MAXC; j++)
            r->x[r->calls][j] = inc->col_value[j];
    }
    r->calls++;
    return r->stop_at > 0 && r->calls >= r->stop_at ? JAOS_CALLBACK_STOP
                                                     : JAOS_CALLBACK_CONTINUE;
}

static int64_t broke[10];

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

static double cost_at(const gen *g, const double *x)
{
    double v = g->offset;
    for (int64_t j = 0; j < g->nc; j++)
        v += g->cost[j] * x[j];
    return v;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t dump = argc > 4 ? atoll(argv[3]) : -1;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, calls_total = 0, other = 0, multi = 0, most = 0;
    static record r1, r2, r3;
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        const double sigma = g.maximise ? -1.0 : 1.0;
        if (idx == dump) {
            jaos_model *d = load(&g);
            if (d == nullptr || jaos_write_mps(d, argv[4]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }

        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        memset(&r1, 0, sizeof r1);
        r1.nc = g.nc;
        if (jaos_set_incumbent_callback(m, on_incumbent, &r1) != JAOS_OK)
            abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        const jaos_solve_status st = jaos_status_of(m);
        jaos_mip_report rep;
        if (jaos_mip_result(m, &rep) != JAOS_OK) abort();
        if (st == JAOS_SOLVE_OPTIMAL) optimal++; else other++;
        calls_total += r1.calls;
        if (r1.calls >= 2) multi++;
        if (r1.calls > most) most = r1.calls;
        const int64_t n = r1.calls < MAXCALLS ? r1.calls : MAXCALLS;

        if ((r1.calls > 0) != rep.has_incumbent)
            fail(7, idx, "calls-vs-has_incumbent", (double)r1.calls,
                 rep.has_incumbent ? 1.0 : 0.0);
        for (int64_t k = 0; k < n; k++) {
            jaos_check_report cr;
            if (jaos_check_solution(m, r1.x[k], nullptr, 1e-6, &cr) != JAOS_OK)
                abort();
            if (!cr.primal_feasible || cr.max_integrality_violation > 1e-6)
                fail(1, idx, "infeasible-point", cr.max_row_violation,
                     cr.max_integrality_violation);
            const double c = cost_at(&g, r1.x[k]);
            if (fabs(c - r1.obj[k]) > 1e-9 * (1.0 + fabs(c)))
                fail(2, idx, "objective", r1.obj[k], c);
            if (k > 0 && !(sigma * r1.obj[k] < sigma * r1.obj[k - 1]))
                fail(3, idx, "not-better", r1.obj[k - 1], r1.obj[k]);
            if (sigma * r1.bound[k] > sigma * r1.obj[k] +
                                          1e-9 * (1.0 + fabs(r1.obj[k])))
                fail(4, idx, "bound-past-objective", r1.bound[k], r1.obj[k]);
            if (k > 0 && r1.node[k] < r1.node[k - 1])
                fail(5, idx, "node-backwards", (double)r1.node[k - 1],
                     (double)r1.node[k]);
        }
        if (n > 0 && r1.node[0] != rep.first_incumbent_node)
            fail(5, idx, "first_incumbent_node", (double)r1.node[0],
                 (double)rep.first_incumbent_node);
        if (st == JAOS_SOLVE_OPTIMAL && n > 0) {
            double obj, x[MAXC];
            if (jaos_objective(m, &obj) != JAOS_OK) abort();
            if (jaos_solution(m, x, nullptr, nullptr, nullptr) != JAOS_OK)
                abort();
            if (fabs(obj - r1.obj[n - 1]) > 1e-9 * (1.0 + fabs(obj)))
                fail(6, idx, "last-vs-published-objective", r1.obj[n - 1],
                     obj);
            for (int64_t j = 0; j < g.nc; j++)
                if (g.isint[j] &&
                    floor(r1.x[n - 1][j] + 0.5) != x[j])
                    fail(6, idx, "last-vs-published-integer", r1.x[n - 1][j],
                         x[j]);
        }
        jaos_model_free(m);

        if (n > 0) {
            m = load(&g);
            if (m == nullptr) abort();
            memset(&r2, 0, sizeof r2);
            r2.nc = g.nc;
            r2.stop_at = 1;
            if (jaos_set_incumbent_callback(m, on_incumbent, &r2) != JAOS_OK)
                abort();
            if (jaos_solve(m) != JAOS_OK) abort();
            if (jaos_status_of(m) != JAOS_SOLVE_INTERRUPTED)
                fail(8, idx, "stop-status", (double)jaos_status_of(m), 0.0);
            if (r2.calls != 1)
                fail(8, idx, "stop-calls", (double)r2.calls, 1.0);
            double x[MAXC], obj = 0.0;
            if (jaos_mip_incumbent(m, x, &obj) != JAOS_OK) {
                fail(8, idx, "stop-no-incumbent", 0.0, 0.0);
            } else {
                if (obj != r2.obj[0])
                    fail(8, idx, "stop-objective", obj, r2.obj[0]);
                for (int64_t j = 0; j < g.nc; j++)
                    if (x[j] != r2.x[0][j])
                        fail(8, idx, "stop-point", x[j], r2.x[0][j]);
            }
            jaos_model_free(m);
        }

        m = load(&g);
        if (m == nullptr) abort();
        memset(&r3, 0, sizeof r3);
        r3.nc = g.nc;
        if (jaos_set_incumbent_callback(m, on_incumbent, &r3) != JAOS_OK)
            abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        if (r3.calls != r1.calls) {
            fail(9, idx, "second-solve-calls", (double)r1.calls,
                 (double)r3.calls);
        } else {
            for (int64_t k = 0; k < n; k++) {
                bool same = r3.node[k] == r1.node[k] &&
                            r3.obj[k] == r1.obj[k] &&
                            r3.bound[k] == r1.bound[k] &&
                            r3.by_rounding[k] == r1.by_rounding[k];
                for (int64_t j = 0; same && j < g.nc; j++)
                    same = r3.x[k][j] == r1.x[k][j];
                if (!same) {
                    fail(9, idx, "second-solve-differs", (double)k,
                         r3.obj[k] - r1.obj[k]);
                    break;
                }
            }
        }
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int p = 1; p <= 9; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " other %" PRId64
           " calls %" PRId64 " models-with-2+ %" PRId64 " most %" PRId64
           "\n", runs, optimal, other, calls_total, multi, most);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " P7=%" PRId64 " P8=%" PRId64
           " P9=%" PRId64 " total=%" PRId64 "\n", broke[1], broke[2],
           broke[3], broke[4], broke[5], broke[6], broke[7], broke[8],
           broke[9], total);
    return total == 0 ? 0 : 1;
}
