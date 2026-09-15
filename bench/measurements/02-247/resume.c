/* The reading behind 02-247: a stopped solve resumes the walk it left.
 *
 * The generator is 02-237's with a third of the models carrying integer
 * marks. Every model is solved once with no limit for the reference:
 * status, objective, point, work W and iterations. Then it is stopped
 * and resumed in five ways, on the dual simplex and again on the
 * primal, and what the resumed solve reports is held against the
 * reference to the bit where the walk is deterministic (an LP), and to
 * 1e-9 in the objective where the tree starts again (a MIP).
 *
 * Properties, for each of the two algorithms:
 *   P1 stopped at L in {1, W/4, W/2, W-1} and solved on with no limit,
 *      an LP ends optimal at the reference to the bit, work and
 *      iterations included, since the counts go on across a stop; a
 *      MIP ends optimal at the reference objective
 *   P2 stopped at W/4, stopped again at W/2 (the second limit is over
 *      the whole walk, so the second stop lands there), and solved on:
 *      the same
 *   P3 stopped by a time limit of 1e-9 s and solved on: an LP is the
 *      reference to the bit, though where the clock cut is not
 *      reproducible
 *   P4 stopped by a progress callback answering STOP at its first call,
 *      the callback taken off, and solved on: the same
 *   P5 stopped at W/2, one cost set to the value it already has, which
 *      is an edit and drops the parked state, and solved on: optimal at
 *      the reference objective to 1e-9, by the warm re-entry that was
 *      the whole story before 2026-09-15
 *
 * Usage: resume RUNS SEED DIR
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
    bool maximise, integer[MAXC];
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->offset = (double)ri(-3, 3);
    const bool mip = ri(0, 2) == 0;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = (double)ri(1, 10);
        g->integer[j] = mip && ri(0, 1) == 1;
        z[j] = ri(0, (int64_t)g->cu[j]);
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

static jaos_model *load(const gen *g, bool primal)
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
    for (int64_t j = 0; j < g->nc; j++)
        if (g->integer[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) abort();
    if (primal && jaos_set_algorithm(m, JAOS_ALGORITHM_PRIMAL) != JAOS_OK) abort();
    return m;
}

typedef struct {
    jaos_solve_status status;
    double obj, x[MAXC];
    int64_t work, iters;
    bool has_point;
} outcome;

static void take(jaos_model *m, outcome *o)
{
    memset(o, 0, sizeof *o);
    o->status = jaos_status_of(m);
    o->work = jaos_work_units(m);
    o->iters = jaos_iterations(m);
    o->has_point = o->status == JAOS_SOLVE_OPTIMAL &&
                   jaos_objective(m, &o->obj) == JAOS_OK &&
                   jaos_solution(m, o->x, nullptr, nullptr, nullptr) == JAOS_OK;
}

static bool same_outcome(const outcome *a, const outcome *b, int64_t nc)
{
    if (a->status != b->status || a->work != b->work || a->iters != b->iters ||
        a->has_point != b->has_point) return false;
    if (!a->has_point) return true;
    return a->obj == b->obj && memcmp(a->x, b->x, (size_t)nc * sizeof a->x[0]) == 0;
}

static int64_t broke[8];

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

static bool near(double a, double b)
{
    return fabs(a - b) <= 1e-9 * (1.0 + fabs(b));
}

/* The resumed solve against the reference: to the bit for an LP, to
 * 1e-9 in the objective for a MIP. Returns whether it was exact. */
static bool judge(int p, int64_t idx, const char *what, bool mip,
                  const outcome *got, const outcome *ref, int64_t nc)
{
    if (got->status != JAOS_SOLVE_OPTIMAL) {
        fail(p, idx, what, (double)got->status, 0.0);
        return false;
    }
    if (mip) {
        if (!near(got->obj, ref->obj)) { fail(p, idx, what, got->obj, ref->obj); return false; }
        return got->obj == ref->obj;
    }
    if (!same_outcome(got, ref, nc)) {
        fail(p, idx, what, got->obj == ref->obj ? (double)got->work : got->obj,
             got->obj == ref->obj ? (double)ref->work : ref->obj);
        return false;
    }
    return true;
}

static jaos_callback_action stop_at_once(const jaos_progress *pr, void *user)
{
    (void)pr;
    (void)user;
    return JAOS_CALLBACK_STOP;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: resume RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    (void)argv[3];
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal[2] = {0}, mips = 0, stops[2] = {0}, exact[2] = {0},
            chains[2] = {0}, timed[2] = {0}, interrupted[2] = {0},
            edited[2] = {0};
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        bool mip = false;
        for (int64_t j = 0; j < g.nc; j++) mip |= g.integer[j];
        mips += mip;
        for (int alg = 0; alg < 2; alg++) {
            const bool primal = alg == 1;
            jaos_model *m = load(&g, primal);
            if (m == nullptr) abort();
            if (jaos_solve(m) != JAOS_OK) abort();
            outcome ref;
            take(m, &ref);
            jaos_model_free(m);
            if (ref.status != JAOS_SOLVE_OPTIMAL) continue;
            optimal[alg]++;
            const int64_t W = ref.work;

            /* P1 */
            const int64_t limits[4] = {1, W / 4, W / 2, W - 1};
            for (int a = 0; a < 4; a++) {
                const int64_t L = limits[a];
                if (L <= 0) continue;
                bool dup = false;
                for (int b = 0; b < a; b++) dup |= limits[b] == L;
                if (dup) continue;
                jaos_model *p = load(&g, primal);
                if (p == nullptr || jaos_set_work_limit(p, L) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                if (jaos_status_of(p) == JAOS_SOLVE_WORK_LIMIT) {
                    stops[alg]++;
                    if (jaos_set_work_limit(p, 0) != JAOS_OK) abort();
                    if (jaos_solve(p) != JAOS_OK) abort();
                    outcome o;
                    take(p, &o);
                    exact[alg] += judge(1, idx, primal ? "primal-resume" : "dual-resume", mip, &o, &ref, g.nc);
                }
                jaos_model_free(p);
            }

            /* P2 */
            if (W / 4 > 0 && W / 2 > W / 4) {
                jaos_model *p = load(&g, primal);
                if (p == nullptr || jaos_set_work_limit(p, W / 4) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                if (jaos_status_of(p) == JAOS_SOLVE_WORK_LIMIT) {
                    if (jaos_set_work_limit(p, W / 2) != JAOS_OK) abort();
                    if (jaos_solve(p) != JAOS_OK) abort();
                    if (jaos_status_of(p) == JAOS_SOLVE_WORK_LIMIT) {
                        if (!mip && jaos_work_units(p) < W / 2)
                            fail(2, idx, "second-stop-before-its-limit", (double)jaos_work_units(p), (double)(W / 2));
                        if (jaos_set_work_limit(p, 0) != JAOS_OK) abort();
                        if (jaos_solve(p) != JAOS_OK) abort();
                        outcome o;
                        take(p, &o);
                        chains[alg] += judge(2, idx, "chain-resume", mip, &o, &ref, g.nc);
                    } else if (jaos_status_of(p) != JAOS_SOLVE_OPTIMAL) {
                        fail(2, idx, "second-stop-odd-status", (double)jaos_status_of(p), 0.0);
                    }
                }
                jaos_model_free(p);
            }

            /* P3 */
            {
                jaos_model *p = load(&g, primal);
                if (p == nullptr || jaos_set_time_limit(p, 1e-9) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                if (jaos_status_of(p) == JAOS_SOLVE_TIME_LIMIT) {
                    if (jaos_set_time_limit(p, 0.0) != JAOS_OK) abort();
                    if (jaos_solve(p) != JAOS_OK) abort();
                    outcome o;
                    take(p, &o);
                    timed[alg] += judge(3, idx, "time-resume", mip, &o, &ref, g.nc);
                } else if (jaos_status_of(p) != JAOS_SOLVE_OPTIMAL) {
                    fail(3, idx, "time-stop-odd-status", (double)jaos_status_of(p), 0.0);
                }
                jaos_model_free(p);
            }

            /* P4 */
            {
                jaos_model *p = load(&g, primal);
                if (p == nullptr || jaos_set_progress_callback(p, stop_at_once, nullptr) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                if (jaos_status_of(p) == JAOS_SOLVE_INTERRUPTED) {
                    if (jaos_set_progress_callback(p, nullptr, nullptr) != JAOS_OK) abort();
                    if (jaos_solve(p) != JAOS_OK) abort();
                    outcome o;
                    take(p, &o);
                    interrupted[alg] += judge(4, idx, "interrupt-resume", mip, &o, &ref, g.nc);
                } else if (jaos_status_of(p) != JAOS_SOLVE_OPTIMAL) {
                    fail(4, idx, "no-interrupt", (double)jaos_status_of(p), 0.0);
                }
                jaos_model_free(p);
            }

            /* P5 */
            if (W / 2 > 0) {
                jaos_model *p = load(&g, primal);
                if (p == nullptr || jaos_set_work_limit(p, W / 2) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                if (jaos_status_of(p) == JAOS_SOLVE_WORK_LIMIT) {
                    if (jaos_set_col_cost(p, 0, g.cost[0]) != JAOS_OK) abort();
                    if (jaos_set_work_limit(p, 0) != JAOS_OK) abort();
                    if (jaos_solve(p) != JAOS_OK) abort();
                    outcome o;
                    take(p, &o);
                    if (o.status != JAOS_SOLVE_OPTIMAL) fail(5, idx, "edited-resume-not-optimal", (double)o.status, 0.0);
                    else if (!near(o.obj, ref.obj)) fail(5, idx, "edited-resume-objective-differs", o.obj, ref.obj);
                    else edited[alg]++;
                }
                jaos_model_free(p);
            }
        }
    }

    int64_t total = 0;
    for (int p = 1; p <= 5; p++) total += broke[p];
    printf("models %" PRId64 " mips %" PRId64 "\n", runs, mips);
    for (int alg = 0; alg < 2; alg++)
        printf("%s optimal %" PRId64 " stops %" PRId64 " exact %" PRId64
               " chains %" PRId64 " timed %" PRId64 " interrupted %" PRId64
               " edited %" PRId64 "\n", alg ? "primal" : "dual", optimal[alg],
               stops[alg], exact[alg], chains[alg], timed[alg],
               interrupted[alg], edited[alg]);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " total=%" PRId64 "\n", broke[1], broke[2],
           broke[3], broke[4], broke[5], total);
    return total == 0 ? 0 : 1;
}
