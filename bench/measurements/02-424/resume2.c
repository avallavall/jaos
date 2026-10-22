/* The reading behind 02-424: a stopped solve resumes to the straight
 * answer, infeasible and unbounded answers included.
 *
 * 02-247's resume.c reads optimal models only. This harness takes 02-235's
 * LPs, about half of them infeasible or unbounded, solves each with the
 * dual and with the primal simplex for the reference, then stops a fresh
 * copy at a third and at two thirds of the reference's work, lifts the
 * limit and solves on. Properties of each resumed solve:
 *   P1 its status is the reference's
 *   P2 its iterations and work units are the reference's
 *   P3 on an optimum, its objective is the reference's to the bit
 *
 * Usage: resume2 RUNS SEED
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

static jaos_model *load(const gen *g, int alg)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    if (jaos_load_lp(m, g->nc, g->nr,
                     g->maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE, g->offset,
                     g->cost, g->cl, g->cu, g->rl, g->ru, g->nz, g->ap,
                     g->ai, g->av) != JAOS_OK ||
        jaos_set_algorithm(m, (jaos_algorithm)alg) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

typedef struct {
    jaos_solve_status status;
    int64_t iters, work;
    double obj;
} outcome;

static void take(jaos_model *m, outcome *o)
{
    o->status = jaos_status_of(m);
    o->iters = jaos_iterations(m);
    o->work = jaos_work_units(m);
    o->obj = 0.0;
    if (o->status == JAOS_SOLVE_OPTIMAL && jaos_objective(m, &o->obj) != JAOS_OK)
        exit(1);
}

static int64_t broke[4];
static const char *const NAME[2] = {"dual", "primal"};
static const gen *current;

static void fail(int p, int64_t idx, int alg, const char *what)
{
    broke[p]++;
    if (broke[p] > 8)
        return;
    printf("P%d m%" PRId64 " %s %s\n", p, idx, NAME[alg], what);
    jaos_model *m = load(current, alg);
    char file[64];
    snprintf(file, sizeof file, "p%d-m%" PRId64 ".mps", p, idx);
    if (m == nullptr || jaos_write_mps(m, file) != JAOS_OK)
        printf("  could not write %s\n", file);
    jaos_model_free(m);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: resume2 RUNS SEED\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    int64_t seen[2][3] = {{0}}, stops = 0;
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        current = &g;
        for (int alg = 0; alg < 2; alg++) {
            jaos_model *m = load(&g, alg);
            if (m == nullptr || jaos_solve(m) != JAOS_OK)
                return 1;
            outcome ref;
            take(m, &ref);
            jaos_model_free(m);
            if (ref.status == JAOS_SOLVE_OPTIMAL) seen[alg][0]++;
            else if (ref.status == JAOS_SOLVE_INFEASIBLE) seen[alg][1]++;
            else if (ref.status == JAOS_SOLVE_UNBOUNDED) seen[alg][2]++;
            for (int part = 1; part <= 2; part++) {
                const int64_t limit = ref.work * part / 3;
                if (limit <= 0)
                    continue;
                jaos_model *p = load(&g, alg);
                if (p == nullptr || jaos_set_work_limit(p, limit) != JAOS_OK ||
                    jaos_solve(p) != JAOS_OK)
                    return 1;
                if (jaos_status_of(p) != JAOS_SOLVE_WORK_LIMIT) {
                    jaos_model_free(p);
                    continue;
                }
                stops++;
                if (jaos_set_work_limit(p, 0) != JAOS_OK || jaos_solve(p) != JAOS_OK)
                    return 1;
                outcome o;
                take(p, &o);
                char what[300];
                if (o.status != ref.status) {
                    snprintf(what, sizeof what, "stopped at %" PRId64 ": status %s, "
                             "straight %s", limit, jaos_solve_status_str(o.status),
                             jaos_solve_status_str(ref.status));
                    fail(1, r, alg, what);
                } else if (o.iters != ref.iters || o.work != ref.work) {
                    snprintf(what, sizeof what, "stopped at %" PRId64 ": %" PRId64
                             " iterations and %" PRId64 " work units, straight %"
                             PRId64 " and %" PRId64 " (%s)", limit, o.iters, o.work,
                             ref.iters, ref.work, jaos_solve_status_str(ref.status));
                    fail(2, r, alg, what);
                } else if (o.obj != ref.obj) {
                    snprintf(what, sizeof what, "stopped at %" PRId64 ": objective "
                             "%.17g, straight %.17g", limit, o.obj, ref.obj);
                    fail(3, r, alg, what);
                }
                jaos_model_free(p);
            }
        }
    }
    printf("models %" PRId64 " dual (optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 ") primal (optimal %" PRId64 " infeasible %"
           PRId64 " unbounded %" PRId64 ") stops %" PRId64 "\n", runs,
           seen[0][0], seen[0][1], seen[0][2], seen[1][0], seen[1][1],
           seen[1][2], stops);
    printf("P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 "\n", broke[1],
           broke[2], broke[3]);
    printf("broken total %" PRId64 "\n", broke[1] + broke[2] + broke[3]);
    return 0;
}
