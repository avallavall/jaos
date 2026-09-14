/* The reading behind 02-230: jaos_feasrelax over generated models, one
 * line per model and scope, so two builds of the library can be compared
 * line for line.
 *
 * Every model plants an integer point z, with entries well outside the
 * column boxes, and sets each row's bounds around A z. So the rows admit
 * an integer point once the columns are freed, and both the free tree and
 * the growing box have something to find. About a quarter of the models
 * carry no integer mark: those take the single-solve path, which the box
 * must not touch, so their lines have to match to the last digit.
 *
 * Usage: relax RUNS SEED WORK_LIMIT [DUMP_INDEX DUMP_PATH]
 *
 * Per model and scope it prints
 *   m<idx> <scope> <status> total=<v> work=<n> rows=<k> cols=<k> moved=<ok|bad> sum=<ok|bad>
 * where `moved` says the model with every move applied solves feasible,
 * and `sum` that the total is the sum of the sizes of the moves.
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

#define MAXC 8
#define MAXR 6

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool isint[MAXC];
    int nint;
} gen;

static void generate(gen *g)
{
    g->nc = ri(3, 7);
    g->nr = ri(2, 5);
    const bool lp_only = ri(0, 3) == 0;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    g->nint = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = !lp_only && ri(0, 1) == 1;
        g->nint += g->isint[j];
        g->cost[j] = (double)ri(-2, 2);
        g->cl[j] = 0.0;
        g->cu[j] = (double)ri(0, 4);
        z[j] = ri(-6, 6);
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
        if (kind < 6) {
            g->rl[i] = g->ru[i] = act[i];
        } else if (kind < 8) {
            g->rl[i] = -INFINITY;
            g->ru[i] = act[i] + (double)ri(0, 2);
        } else if (kind < 9) {
            g->rl[i] = act[i] - (double)ri(0, 2);
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
    if (jaos_load_lp(m, g->nc, g->nr, JAOS_MINIMIZE, 0.0, g->cost, g->cl,
                     g->cu, g->rl, g->ru, g->nz, g->ap, g->ai,
                     g->av) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    for (int64_t j = 0; j < g->nc; j++) {
        if (g->isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) {
            jaos_model_free(m);
            return nullptr;
        }
    }
    return m;
}

static bool moved_solves(const gen *g, const double *rm, const double *cm,
                         int64_t work_limit)
{
    jaos_model *c = load(g);
    if (c == nullptr)
        return false;
    bool ok = jaos_set_work_limit(c, work_limit) == JAOS_OK;
    for (int64_t i = 0; ok && i < g->nr; i++) {
        double lo = g->rl[i], hi = g->ru[i];
        if (rm[i] < 0.0) lo += rm[i]; else hi += rm[i];
        ok = jaos_set_row_bounds(c, i, lo, hi) == JAOS_OK;
    }
    for (int64_t j = 0; ok && j < g->nc; j++) {
        double lo = g->cl[j], hi = g->cu[j];
        if (cm[j] < 0.0) lo += cm[j]; else hi += cm[j];
        ok = jaos_set_col_bounds(c, j, lo, hi) == JAOS_OK;
    }
    ok = ok && jaos_solve(c) == JAOS_OK &&
         jaos_status_of(c) == JAOS_SOLVE_OPTIMAL;
    jaos_model_free(c);
    return ok;
}

static const char *scope_name(jaos_relax_scope s)
{
    return s == JAOS_RELAX_ROWS ? "rows" : s == JAOS_RELAX_COLS ? "cols"
                                                                 : "both";
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t work_limit = argc > 3 ? atoll(argv[3]) : 20000000;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    const jaos_relax_scope scopes[3] = {JAOS_RELAX_COLS, JAOS_RELAX_BOTH,
                                        JAOS_RELAX_ROWS};
    int64_t lp_models = 0, mip_models = 0;
    const int64_t dump = argc > 5 ? atoll(argv[4]) : -1;
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        if (g.nint == 0) lp_models++; else mip_models++;
        if (idx == dump) {
            jaos_model *d = load(&g);
            if (d == nullptr || jaos_write_mps(d, argv[5]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }
        for (int s = 0; s < 3; s++) {
            jaos_model *m = load(&g);
            if (m == nullptr) {
                printf("m%" PRId64 " %s load-failed\n", idx,
                       scope_name(scopes[s]));
                continue;
            }
            double rm[MAXR], cm[MAXC];
            jaos_relax_report rep;
            if (jaos_set_work_limit(m, work_limit) != JAOS_OK) abort();
            const jaos_status rc =
                jaos_feasrelax(m, scopes[s], rm, cm, &rep);
            if (rc != JAOS_OK) {
                printf("m%" PRId64 " %s %s work=%" PRId64 "\n", idx,
                       scope_name(scopes[s]),
                       jaos_solve_status_str(rep.status), rep.work_units);
                jaos_model_free(m);
                continue;
            }
            double sum = 0.0;
            for (int64_t i = 0; i < g.nr; i++) sum += fabs(rm[i]);
            for (int64_t j = 0; j < g.nc; j++) sum += fabs(cm[j]);
            const bool sum_ok =
                fabs(sum - rep.total) <= 1e-9 * (1.0 + fabs(rep.total));
            const bool mv_ok = moved_solves(&g, rm, cm, work_limit);
            printf("m%" PRId64 " %s %s total=%.17g work=%" PRId64
                   " rows=%" PRId64 " cols=%" PRId64 " moved=%s sum=%s\n",
                   idx, scope_name(scopes[s]),
                   jaos_solve_status_str(rep.status), rep.total,
                   rep.work_units, rep.rows_moved, rep.cols_moved,
                   mv_ok ? "ok" : "bad", sum_ok ? "ok" : "bad");
            jaos_model_free(m);
        }
    }
    printf("models %" PRId64 " lp %" PRId64 " mip %" PRId64 "\n", runs,
           lp_models, mip_models);
    return 0;
}
