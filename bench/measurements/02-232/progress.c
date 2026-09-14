/* The reading behind 02-232: the progress callback over generated models.
 *
 * Every model plants an integer point inside its boxes and sets each row's
 * bounds around it, so every model is feasible. A third of the models
 * carry no integer mark and solve as LPs; the rest run the tree, where
 * the node relaxations are copies that inherit the callback. Each model is
 * solved four times: once with no callback, for the reference answer; once
 * recording every call; once with the callback answering STOP at its
 * first call and then solved again from where it stopped; and once more
 * recording every call.
 *
 * Seven properties:
 *   P1 every call of an LP solve comes on the beat, at an iteration count
 *      that is a multiple of 64; a MIP reports the tree's running total
 *   P2 the reported work never goes back between two calls of one solve
 *   P3 the last call's iterations and work are at or below what the solve
 *      reports at the end
 *   P4 the answer with the callback is the answer without it, to the bit:
 *      status, objective, point, iterations and work
 *   P5 a STOP at the first call ends the solve INTERRUPTED with nothing to
 *      read, and solving again reaches the reference status and objective
 *   P6 the reported primal infeasibility is not below zero; the calls
 *      that report it infinite, which is the dual's phase 1 before any
 *      primal infeasibility exists, are counted
 *   P7 the second recording solve makes the same calls, field for field
 *
 * P2 is read for LPs and for MIPs separately, because a MIP's calls come
 * from its node relaxations.
 *
 * Usage: progress RUNS SEED [DUMP_INDEX DUMP_PATH]
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
#define MAXCALLS 65536

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool isint[MAXC];
    bool maximise, lp_only;
    double offset;
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->lp_only = ri(0, 2) == 0;
    g->offset = (double)ri(-3, 3);
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = !g->lp_only && ri(0, 9) < 6;
        g->cost[j] = (double)ri(-9, 9);
        g->cl[j] = 0.0;
        g->cu[j] = (double)ri(1, 10);
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
    for (int64_t j = 0; j < g->nc; j++) {
        if (g->isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) {
            jaos_model_free(m);
            return nullptr;
        }
    }
    return m;
}

typedef struct {
    int64_t iters[MAXCALLS], work[MAXCALLS];
    double infeas[MAXCALLS];
    int64_t calls, stop_at;
} record;

static jaos_callback_action on_progress(const jaos_progress *p, void *user)
{
    record *r = user;
    if (r->calls < MAXCALLS) {
        r->iters[r->calls] = p->iterations;
        r->work[r->calls] = p->work_units;
        r->infeas[r->calls] = p->primal_infeasibility;
    }
    r->calls++;
    return r->stop_at > 0 && r->calls >= r->stop_at ? JAOS_CALLBACK_STOP
                                                     : JAOS_CALLBACK_CONTINUE;
}

static int64_t broke[8], broke_lp2, broke_mip2;

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

typedef struct {
    jaos_solve_status st;
    double obj, x[MAXC];
    int64_t iters, work;
} answer;

static void read_answer(jaos_model *m, int64_t nc, answer *a)
{
    a->st = jaos_status_of(m);
    a->obj = 0.0;
    memset(a->x, 0, sizeof a->x);
    if (a->st == JAOS_SOLVE_OPTIMAL) {
        if (jaos_objective(m, &a->obj) != JAOS_OK) abort();
        if (jaos_solution(m, a->x, nullptr, nullptr, nullptr) != JAOS_OK)
            abort();
    }
    (void)nc;
    a->iters = jaos_iterations(m);
    a->work = jaos_work_units(m);
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t dump = argc > 4 ? atoll(argv[3]) : -1;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t lps = 0, mips = 0, optimal = 0, calls_total = 0, with_calls = 0;
    int64_t stopped = 0, resumed_same = 0, infinite = 0;
    static record r1, r2, r3;
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        if (g.lp_only) lps++; else mips++;
        if (idx == dump) {
            jaos_model *d = load(&g);
            if (d == nullptr || jaos_write_mps(d, argv[4]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }

        answer ref, seen;
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        read_answer(m, g.nc, &ref);
        jaos_model_free(m);
        if (ref.st == JAOS_SOLVE_OPTIMAL) optimal++;

        m = load(&g);
        if (m == nullptr) abort();
        memset(&r1, 0, sizeof r1);
        if (jaos_set_progress_callback(m, on_progress, &r1) != JAOS_OK)
            abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        read_answer(m, g.nc, &seen);
        jaos_model_free(m);
        calls_total += r1.calls;
        if (r1.calls > 0) with_calls++;
        const int64_t n = r1.calls < MAXCALLS ? r1.calls : MAXCALLS;

        for (int64_t k = 0; k < n; k++) {
            if (g.lp_only && r1.iters[k] % 64 != 0)
                fail(1, idx, "off-the-beat", (double)r1.iters[k], 64.0);
            if (k > 0 && r1.work[k] < r1.work[k - 1]) {
                if (g.lp_only) broke_lp2++; else broke_mip2++;
                fail(2, idx, g.lp_only ? "lp-work-went-back"
                                       : "mip-work-went-back",
                     (double)r1.work[k - 1], (double)r1.work[k]);
            }
            if (!(r1.infeas[k] >= 0.0))
                fail(6, idx, "infeasibility", r1.infeas[k], 0.0);
            else if (!isfinite(r1.infeas[k]))
                infinite++;
        }
        if (n > 0 && (r1.iters[n - 1] > seen.iters ||
                      r1.work[n - 1] > seen.work))
            fail(3, idx, "last-call-past-the-end", (double)r1.work[n - 1],
                 (double)seen.work);
        if (seen.st != ref.st || seen.obj != ref.obj ||
            seen.iters != ref.iters || seen.work != ref.work ||
            memcmp(seen.x, ref.x, sizeof ref.x) != 0)
            fail(4, idx, "answer-differs-with-callback", ref.obj, seen.obj);

        if (n > 0) {
            m = load(&g);
            if (m == nullptr) abort();
            memset(&r2, 0, sizeof r2);
            r2.stop_at = 1;
            if (jaos_set_progress_callback(m, on_progress, &r2) != JAOS_OK)
                abort();
            if (jaos_solve(m) != JAOS_OK) abort();
            stopped++;
            if (jaos_status_of(m) != JAOS_SOLVE_INTERRUPTED)
                fail(5, idx, "stop-status", (double)jaos_status_of(m), 0.0);
            double obj;
            if (jaos_objective(m, &obj) == JAOS_OK)
                fail(5, idx, "stop-left-an-objective", obj, 0.0);
            if (jaos_set_progress_callback(m, nullptr, nullptr) != JAOS_OK)
                abort();
            if (jaos_solve(m) != JAOS_OK) abort();
            answer res;
            read_answer(m, g.nc, &res);
            if (res.st != ref.st)
                fail(5, idx, "resume-status", (double)res.st, (double)ref.st);
            else if (res.st == JAOS_SOLVE_OPTIMAL &&
                     fabs(res.obj - ref.obj) > 1e-9 * (1.0 + fabs(ref.obj)))
                fail(5, idx, "resume-objective", res.obj, ref.obj);
            else
                resumed_same++;
            jaos_model_free(m);
        }

        m = load(&g);
        if (m == nullptr) abort();
        memset(&r3, 0, sizeof r3);
        if (jaos_set_progress_callback(m, on_progress, &r3) != JAOS_OK)
            abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        jaos_model_free(m);
        if (r3.calls != r1.calls) {
            fail(7, idx, "second-solve-calls", (double)r1.calls,
                 (double)r3.calls);
        } else {
            for (int64_t k = 0; k < n; k++)
                if (r3.iters[k] != r1.iters[k] || r3.work[k] != r1.work[k] ||
                    r3.infeas[k] != r1.infeas[k]) {
                    fail(7, idx, "second-solve-differs", (double)k,
                         (double)(r3.work[k] - r1.work[k]));
                    break;
                }
        }
    }

    int64_t total = 0;
    for (int p = 1; p <= 7; p++) total += broke[p];
    printf("models %" PRId64 " lp %" PRId64 " mip %" PRId64 " optimal %"
           PRId64 " calls %" PRId64 " models-with-calls %" PRId64
           " stopped %" PRId64 " resumed-same %" PRId64
           " infinite-infeasibility %" PRId64 "\n",
           runs, lps, mips, optimal, calls_total, with_calls, stopped,
           resumed_same, infinite);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " (lp %" PRId64 ", mip %"
           PRId64 ") P3=%" PRId64 " P4=%" PRId64 " P5=%" PRId64 " P6=%"
           PRId64 " P7=%" PRId64 " total=%" PRId64 "\n",
           broke[1], broke[2], broke_lp2, broke_mip2, broke[3], broke[4],
           broke[5], broke[6], broke[7], total);
    return total == 0 ? 0 : 1;
}
