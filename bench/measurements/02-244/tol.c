/* The reading behind 02-244: the primal and dual tolerances.
 *
 * The generator is 02-237's, LPs only. Every model is solved once for the
 * reference, then with each tolerance set explicitly, loosened and
 * tightened. Two rows over one column are added, `x_j >= v` and
 * `x_j <= v - d`, with v the planted point's value there, so at d = 0
 * the model keeps a point and at d = 1e-9 it has none, missing one by d
 * in that column's own units; presolve folds the two rows into the
 * column's box and asks whether it is inverted beyond rounding, not
 * beyond the tolerance (`PRESOLVE_ROUND_ULPS` in docs/tolerances.md).
 *
 * Properties:
 *   P1 a negative, NaN or infinite tolerance is refused; zero is taken
 *   P2 the defaults set explicitly, 1e-7 primal and 1e-9 dual, give the
 *      reference to the bit; how often 1e-7 dual differs is counted
 *   P3 the pinned column at d = 0 is taken and published at v; at
 *      d = 1e-9 the model is refused as infeasible at the default and at
 *      1e-4, the folded box being inverted beyond rounding; and a primal
 *      tolerance of 1e-9, 1e-7, 1e-5 or 1e-3 on the model itself gives
 *      an optimum whose worst violation, read by the checker with a
 *      1e-12 window, is at most ten times the tolerance
 *   P4 the dual tolerance bounds what the checker reads: at 1e-11, 1e-9,
 *      1e-7, 1e-5 and 1e-3 the checker's max_dual_violation on the
 *      published answer is at most ten times the tolerance, and the
 *      objective is within that violation times the box widths of the
 *      reference
 *
 * Usage: tol RUNS SEED DIR [EVERY]
 *   Every EVERY-th model is written to DIR as m<idx>.mps for the CLI run
 *   in tol.sh.
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
#define MAXR 18

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool maximise;
    double offset;
    int64_t z[MAXC];
} gen;

static void generate(gen *g)
{
    g->nc = ri(8, 24);
    g->nr = ri(6, 16);
    g->maximise = ri(0, 1) == 1;
    g->offset = (double)ri(-3, 3);
    int64_t *z = g->z;
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
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

/* The model with two rows more, x_j >= v and x_j <= v - d. */
static void contradict(const gen *g, gen *h, int64_t j, double d, double *v)
{
    *h = *g;
    *v = (double)g->z[j];
    h->nz = 0;
    for (int64_t c = 0; c < g->nc; c++) {
        h->ap[c] = h->nz;
        for (int64_t p = g->ap[c]; p < g->ap[c + 1]; p++) {
            h->ai[h->nz] = g->ai[p];
            h->av[h->nz] = g->av[p];
            h->nz++;
        }
        if (c == j) {
            h->ai[h->nz] = g->nr;
            h->av[h->nz] = 1.0;
            h->nz++;
            h->ai[h->nz] = g->nr + 1;
            h->av[h->nz] = 1.0;
            h->nz++;
        }
    }
    h->ap[g->nc] = h->nz;
    h->nr = g->nr + 2;
    h->rl[g->nr] = *v;
    h->ru[g->nr] = INFINITY;
    h->rl[g->nr + 1] = -INFINITY;
    h->ru[g->nr + 1] = *v - d;
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

typedef struct {
    jaos_solve_status status;
    double obj, x[MAXC], y[MAXR];
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
                   jaos_solution(m, o->x, nullptr, o->y, nullptr) == JAOS_OK;
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

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: tol RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, dual7_differs = 0, taken = 0, refused = 0,
            odd = 0, dumped = 0;
    const double dtols[5] = {1e-11, 1e-9, 1e-7, 1e-5, 1e-3};
    double worst_ratio[5] = {0}, worst_gap[5] = {0};
    const double ptl[4] = {1e-9, 1e-7, 1e-5, 1e-3};
    double worst_pviol[4] = {0}, worst_pgap[4] = {0};
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        outcome ref;
        take(m, &ref);
        if (ref.status != JAOS_SOLVE_OPTIMAL) { jaos_model_free(m); continue; }
        optimal++;

        if (jaos_set_primal_tolerance(m, -1.0) == JAOS_OK) fail(1, idx, "negative-primal-accepted", 0.0, 0.0);
        if (jaos_set_primal_tolerance(m, NAN) == JAOS_OK) fail(1, idx, "nan-primal-accepted", 0.0, 0.0);
        if (jaos_set_dual_tolerance(m, INFINITY) == JAOS_OK) fail(1, idx, "inf-dual-accepted", 0.0, 0.0);
        if (jaos_set_dual_tolerance(m, -1e-9) == JAOS_OK) fail(1, idx, "negative-dual-accepted", 0.0, 0.0);
        if (jaos_set_primal_tolerance(m, 0.0) != JAOS_OK) fail(1, idx, "zero-primal-refused", 0.0, 0.0);
        if (jaos_set_dual_tolerance(m, 0.0) != JAOS_OK) fail(1, idx, "zero-dual-refused", 0.0, 0.0);
        jaos_model_free(m);

        {
            jaos_model *p = load(&g);
            if (p == nullptr) abort();
            if (jaos_set_primal_tolerance(p, 1e-7) != JAOS_OK || jaos_set_dual_tolerance(p, 1e-9) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            outcome o;
            take(p, &o);
            if (!same_outcome(&o, &ref, g.nc)) fail(2, idx, "explicit-defaults-differ", (double)o.status, o.obj);
            jaos_model_free(p);
            p = load(&g);
            if (p == nullptr || jaos_set_dual_tolerance(p, 1e-7) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            take(p, &o);
            if (!same_outcome(&o, &ref, g.nc)) dual7_differs++;
            jaos_model_free(p);
        }

        {
            const int64_t j = ri(0, g.nc - 1);
            const double ptols[2] = {1e-7, 1e-4};
            for (int t = 0; t < 2; t++) {
                const double ds[2] = {0.0, 1e-9};
                for (int k = 0; k < 2; k++) {
                    gen h;
                    double v;
                    contradict(&g, &h, j, ds[k], &v);
                    jaos_model *p = load(&h);
                    if (p == nullptr) abort();
                    if (t == 1 && jaos_set_primal_tolerance(p, ptols[t]) != JAOS_OK) abort();
                    if (jaos_solve(p) != JAOS_OK) abort();
                    outcome o;
                    take(p, &o);
                    if (k == 0) {
                        if (o.status != JAOS_SOLVE_OPTIMAL) {
                            fail(3, idx, "pinned-column-refused", ptols[t], (double)o.status);
                        } else {
                            taken++;
                            if (fabs(o.x[j] - v) > 1e-9)
                                fail(3, idx, "point-off-the-pin", o.x[j], v);
                        }
                    } else {
                        if (o.status == JAOS_SOLVE_INFEASIBLE) refused++;
                        else if (o.status == JAOS_SOLVE_OPTIMAL) fail(3, idx, "folded-contradiction-taken", ptols[t], ds[k]);
                        else { odd++; fail(3, idx, "contradiction-odd-status", ptols[t], (double)o.status); }
                    }
                    jaos_model_free(p);
                }
            }
            for (int t = 0; t < 4; t++) {
                jaos_model *p = load(&g);
                if (p == nullptr || jaos_set_primal_tolerance(p, ptl[t]) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                outcome o;
                take(p, &o);
                if (o.status != JAOS_SOLVE_OPTIMAL) {
                    fail(3, idx, "loosened-primal-not-optimal", ptl[t], (double)o.status);
                } else {
                    jaos_check_report rep;
                    if (jaos_check_solution(p, o.x, nullptr, 1e-12, &rep) != JAOS_OK) abort();
                    const double viol = rep.max_col_violation > rep.max_row_violation
                                      ? rep.max_col_violation : rep.max_row_violation;
                    if (viol > worst_pviol[t]) worst_pviol[t] = viol;
                    if (viol > 10.0 * ptl[t]) fail(3, idx, "primal-violation-past-ten-tolerances", ptl[t], viol);
                    const double gap = fabs(o.obj - ref.obj);
                    if (gap > worst_pgap[t]) worst_pgap[t] = gap;
                }
                jaos_model_free(p);
            }
        }

        {
            double widths = 0.0;
            for (int64_t j = 0; j < g.nc; j++) widths += g.cu[j] - g.cl[j];
            for (int t = 0; t < 5; t++) {
                jaos_model *p = load(&g);
                if (p == nullptr || jaos_set_dual_tolerance(p, dtols[t]) != JAOS_OK) abort();
                if (jaos_solve(p) != JAOS_OK) abort();
                outcome o;
                take(p, &o);
                if (o.status != JAOS_SOLVE_OPTIMAL) {
                    fail(4, idx, "loosened-dual-not-optimal", dtols[t], (double)o.status);
                } else {
                    jaos_check_report rep;
                    if (jaos_check_solution(p, o.x, o.y, 1e-12, &rep) != JAOS_OK) abort();
                    const double ratio = rep.max_dual_violation / dtols[t];
                    if (ratio > worst_ratio[t]) worst_ratio[t] = ratio;
                    if (ratio > 10.0) fail(4, idx, "dual-violation-past-ten-tolerances", dtols[t], rep.max_dual_violation);
                    const double gap = fabs(o.obj - ref.obj);
                    const double allowed = rep.max_dual_violation * widths + 1e-9 * (1.0 + fabs(ref.obj));
                    if (gap > worst_gap[t]) worst_gap[t] = gap;
                    if (gap > allowed) fail(4, idx, "objective-past-the-dual-bound", gap, allowed);
                }
                jaos_model_free(p);
            }
        }

        if (every > 0 && idx % every == 0) {
            char path[4096];
            jaos_model *p = load(&g);
            if (p == nullptr) abort();
            snprintf(path, sizeof path, "%s/m%" PRId64 ".mps", dir, idx);
            if (jaos_write_mps(p, path) != JAOS_OK) abort();
            jaos_model_free(p);
            dumped++;
        }
    }

    int64_t total = 0;
    for (int p = 1; p <= 4; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " dual_1e-7_differs %" PRId64
           " contradictions taken %" PRId64 " refused %" PRId64 " odd %" PRId64
           " dumped %" PRId64 "\n", runs, optimal, dual7_differs, taken,
           refused, odd, dumped);
    for (int t = 0; t < 4; t++)
        printf("primal_tol %g worst_violation %.3g worst_objective_gap %.3g\n",
               ptl[t], worst_pviol[t], worst_pgap[t]);
    for (int t = 0; t < 5; t++)
        printf("dual_tol %g worst_violation_ratio %.3g worst_objective_gap %.3g\n",
               dtols[t], worst_ratio[t], worst_gap[t]);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " total=%" PRId64 "\n", broke[1], broke[2], broke[3], broke[4], total);
    return total == 0 ? 0 : 1;
}
