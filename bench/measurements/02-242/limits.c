/* The reading behind 02-242: the work limit and the time limit.
 *
 * The generator is 02-237's with a third of the models carrying integer
 * marks, so the tree runs on those. Every model is solved once with no
 * limit for the reference: status, objective, point, work W and
 * iterations. Then, for each limit L in {1, W/4, W/2, W-1, W, 2W} that
 * is positive and distinct, a fresh copy is solved at L, a second fresh
 * copy at L again, and the first is solved on with the limit lifted.
 *
 * Properties:
 *   P1 L >= W: the solve ends optimal and is the reference to the bit
 *      (status, objective, point, work, iterations)
 *   P2 L < W: the solve ends `work_limit`, with the work at or past L
 *      (it did not stop early); the overshoot past L is recorded, and a
 *      solve that ended optimal past L is counted as `past`
 *   P3 two solves at the same L agree to the bit: status, work,
 *      iterations, and the answer where there is one
 *   P4 the stopped solve, solved on with no limit, ends optimal at the
 *      reference objective to 1e-9; a bit-identical objective is counted
 *   P5 a time limit of 1e-9 s ends `time_limit` or optimal, never
 *      anything else, and solving on reaches the reference objective; a
 *      time limit of 1000 s ends optimal and is the reference to the bit
 *   P6 a NaN time limit is refused; a work limit of 0 or below means no
 *      limit and gives the reference to the bit
 *
 * Usage: limits RUNS SEED DIR [EVERY]
 *   Every EVERY-th model is written to DIR as m<idx>.mps with .limit
 *   holding max(1, W/2), for the CLI run in limits.sh.
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
    for (int64_t j = 0; j < g->nc; j++)
        if (g->integer[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) abort();
    return m;
}

typedef struct {
    jaos_solve_status status;
    double obj, x[MAXC];
    int64_t work, iters;
    bool has_point;
} outcome;

static void take(jaos_model *m, int64_t nc, outcome *o)
{
    memset(o, 0, sizeof *o);
    o->status = jaos_status_of(m);
    o->work = jaos_work_units(m);
    o->iters = jaos_iterations(m);
    o->has_point = o->status == JAOS_SOLVE_OPTIMAL &&
                   jaos_objective(m, &o->obj) == JAOS_OK &&
                   jaos_solution(m, o->x, nullptr, nullptr, nullptr) == JAOS_OK;
    (void)nc;
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

static void debug_line(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    fprintf(stderr, "log: %s\n", line);
}

static void debug_hook(jaos_model *p, int64_t idx)
{
    const char *want = getenv("DEBUG_IDX");
    if (want == nullptr || atoll(want) != idx) return;
    if (jaos_set_log_callback(p, debug_line, nullptr) != JAOS_OK) abort();
    if (jaos_set_log_level(p, JAOS_LOG_DETAIL) != JAOS_OK) abort();
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: limits RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, mips = 0, resumed = 0, resumed_exact = 0,
            timed_stopped = 0, timed_optimal = 0, work_ref = 0,
            work_stop_plus_resume = 0, dumped = 0;
    int64_t stopped[2] = {0}, at_or_past[2] = {0}, past[2] = {0},
            max_over[2] = {0}, sum_over[2] = {0}, past_over_max[2] = {0};
    double max_frac[2] = {0.0, 0.0};
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        bool mip = false;
        for (int64_t j = 0; j < g.nc; j++) mip |= g.integer[j];
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        outcome ref;
        take(m, g.nc, &ref);
        jaos_model_free(m);
        if (ref.status != JAOS_SOLVE_OPTIMAL) {
            fail(1, idx, "reference-not-optimal", (double)ref.status, 0.0);
            continue;
        }
        optimal++;
        mips += mip;
        const int64_t W = ref.work;
        int64_t limits[6] = {1, W / 4, W / 2, W - 1, W, 2 * W};
        for (int a = 0; a < 6; a++) {
            const int64_t L = limits[a];
            if (L <= 0) continue;
            bool dup = false;
            for (int b = 0; b < a; b++) dup |= limits[b] == L;
            if (dup) continue;

            jaos_model *p = load(&g);
            if (p == nullptr || jaos_set_work_limit(p, L) != JAOS_OK) abort();
            if (L == 1) debug_hook(p, idx);
            if (jaos_solve(p) != JAOS_OK) abort();
            outcome o1;
            take(p, g.nc, &o1);

            if (L >= W) {
                if (!same_outcome(&o1, &ref, g.nc))
                    fail(1, idx, "limit-at-or-above-work-changes-answer", (double)L, (double)o1.work);
            } else if (o1.status == JAOS_SOLVE_WORK_LIMIT) {
                stopped[mip]++;
                if (o1.work < L) fail(2, idx, "stopped-before-limit", (double)L, (double)o1.work);
                else at_or_past[mip]++;
                const int64_t over = o1.work - L;
                if (over > max_over[mip]) max_over[mip] = over;
                sum_over[mip] += over;
                if ((double)over / (double)L > max_frac[mip]) max_frac[mip] = (double)over / (double)L;
            } else if (o1.status == JAOS_SOLVE_OPTIMAL) {
                past[mip]++;
                if (o1.work < L) fail(2, idx, "optimal-under-limit-but-reference-past-it", (double)L, (double)o1.work);
                if (o1.work - L > past_over_max[mip]) past_over_max[mip] = o1.work - L;
            } else {
                fail(2, idx, "unexpected-status", (double)L, (double)o1.status);
            }

            jaos_model *q = load(&g);
            if (q == nullptr || jaos_set_work_limit(q, L) != JAOS_OK) abort();
            if (jaos_solve(q) != JAOS_OK) abort();
            outcome o2;
            take(q, g.nc, &o2);
            if (!same_outcome(&o1, &o2, g.nc))
                fail(3, idx, "two-runs-differ", (double)L, (double)o1.work - (double)o2.work);
            jaos_model_free(q);

            if (o1.status == JAOS_SOLVE_WORK_LIMIT) {
                if (jaos_set_work_limit(p, 0) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                outcome o3;
                take(p, g.nc, &o3);
                if (o3.status != JAOS_SOLVE_OPTIMAL)
                    fail(4, idx, "resume-not-optimal", (double)L, (double)o3.status);
                else if (!near(o3.obj, ref.obj))
                    fail(4, idx, "resume-objective-differs", o3.obj, ref.obj);
                else {
                    resumed++;
                    resumed_exact += o3.obj == ref.obj;
                    work_ref += W;
                    work_stop_plus_resume += o1.work + o3.work;
                }
            }
            jaos_model_free(p);
        }

        {
            jaos_model *p = load(&g);
            if (p == nullptr || jaos_set_time_limit(p, 1e-9) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            outcome o;
            take(p, g.nc, &o);
            if (o.status == JAOS_SOLVE_TIME_LIMIT) {
                timed_stopped++;
                if (jaos_set_time_limit(p, 0.0) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                take(p, g.nc, &o);
                if (o.status != JAOS_SOLVE_OPTIMAL || !near(o.obj, ref.obj))
                    fail(5, idx, "time-resume-differs", (double)o.status, o.obj);
            } else if (o.status == JAOS_SOLVE_OPTIMAL) {
                timed_optimal++;
                if (!near(o.obj, ref.obj)) fail(5, idx, "time-optimal-differs", o.obj, ref.obj);
            } else {
                fail(5, idx, "time-limit-odd-status", (double)o.status, 0.0);
            }
            jaos_model_free(p);

            p = load(&g);
            if (p == nullptr || jaos_set_time_limit(p, 1000.0) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            take(p, g.nc, &o);
            if (!same_outcome(&o, &ref, g.nc)) fail(5, idx, "generous-time-limit-changes-answer", (double)o.status, (double)o.work);
            jaos_model_free(p);
        }

        {
            jaos_model *p = load(&g);
            if (p == nullptr) abort();
            if (jaos_set_time_limit(p, NAN) == JAOS_OK) fail(6, idx, "nan-time-limit-accepted", 0.0, 0.0);
            if (jaos_set_work_limit(p, -5) != JAOS_OK) fail(6, idx, "negative-work-limit-refused", 0.0, 0.0);
            if (jaos_solve(p) != JAOS_OK) abort();
            outcome o;
            take(p, g.nc, &o);
            if (!same_outcome(&o, &ref, g.nc)) fail(6, idx, "no-limit-changes-answer", (double)o.status, (double)o.work);
            jaos_model_free(p);
        }

        if (every > 0 && idx % every == 0) {
            char path[4096];
            jaos_model *p = load(&g);
            if (p == nullptr) abort();
            snprintf(path, sizeof path, "%s/m%" PRId64 ".mps", dir, idx);
            if (jaos_write_mps(p, path) != JAOS_OK) abort();
            snprintf(path, sizeof path, "%s/m%" PRId64 ".limit", dir, idx);
            FILE *f = fopen(path, "w");
            if (f == nullptr) abort();
            fprintf(f, "%" PRId64 "\n%" PRId64 "\n", W / 2 > 0 ? W / 2 : 1, W);
            fclose(f);
            jaos_model_free(p);
            dumped++;
        }
    }

    int64_t total = 0;
    for (int p = 1; p <= 6; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " mips %" PRId64
           " resumed %" PRId64 " exact %" PRId64 " work_ratio %.3f"
           " timed_stopped %" PRId64 " timed_optimal %" PRId64
           " dumped %" PRId64 "\n",
           runs, optimal, mips, resumed, resumed_exact,
           work_ref > 0 ? (double)work_stop_plus_resume / (double)work_ref : 0.0,
           timed_stopped, timed_optimal, dumped);
    for (int k = 0; k < 2; k++)
        printf("%s stopped %" PRId64 " at_or_past %" PRId64 " max_over %" PRId64
               " mean_over %.2f max_over_frac %.3f past %" PRId64
               " past_over_max %" PRId64 "\n",
               k ? "mip" : "lp", stopped[k], at_or_past[k], max_over[k],
               stopped[k] > 0 ? (double)sum_over[k] / (double)stopped[k] : 0.0,
               max_frac[k], past[k], past_over_max[k]);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " total=%" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6], total);
    return total == 0 ? 0 : 1;
}
