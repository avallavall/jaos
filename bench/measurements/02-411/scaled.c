/* The reading behind 02-411: the same LP scaled by powers of ten.
 *
 * The generator is 02-235's, LPs only, as 02-396's algos.c. Each model is
 * solved by the dual simplex, then scaled: each row by 10^a, each column
 * by 10^b (x = 10^b x'), and the objective by 10^c, with a, b and c drawn
 * from -K to K. The scaled model is solved by the dual, the primal and the
 * barrier. Properties, for each:
 *   P1 the status is the unscaled model's
 *   P2 an optimum, divided by 10^c, is within 1e-6 of (1 + |unscaled|) of
 *      the unscaled objective
 *   P3 the checker passes its point and duals at 1e-7; each refusal also
 *      prints the certified suboptimality, and one without a certified
 *      gap or with a relative bound past 1e-9 is counted apart
 *
 * Usage: scaled RUNS SEED K
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

static int64_t broke[3][4];
static const char *const NAME[3] = {"dual", "primal", "barrier"};

static void fail(int a, int p, int64_t idx, int k, const char *what)
{
    broke[a][p]++;
    if (broke[a][p] <= 6)
        printf("%s P%d m%" PRId64 " k=%d %s\n", NAME[a], p, idx, k, what);
}

static double p10(int64_t e)
{
    return pow(10.0, (double)e);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: scaled RUNS SEED K\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    const int k = atoi(argv[3]);
    int64_t verdicts[3] = {0, 0, 0};
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr || jaos_solve(m) != JAOS_OK)
            return 1;
        const jaos_solve_status ref = jaos_status_of(m);
        double ref_obj = 0.0;
        if (ref == JAOS_SOLVE_OPTIMAL && jaos_objective(m, &ref_obj) != JAOS_OK)
            return 1;
        jaos_model_free(m);
        if (ref == JAOS_SOLVE_OPTIMAL) verdicts[0]++;
        else if (ref == JAOS_SOLVE_INFEASIBLE) verdicts[1]++;
        else if (ref == JAOS_SOLVE_UNBOUNDED) verdicts[2]++;
        gen h = g;
        double rs[MAXR], cs[MAXC];
        const double os = p10(ri(-k, k));
        for (int64_t i = 0; i < h.nr; i++) {
            rs[i] = p10(ri(-k, k));
            h.rl[i] *= rs[i];
            h.ru[i] *= rs[i];
        }
        for (int64_t j = 0; j < h.nc; j++) {
            cs[j] = p10(ri(-k, k));
            h.cost[j] *= cs[j] * os;
            h.cl[j] /= cs[j];
            h.cu[j] /= cs[j];
            for (int64_t p = h.ap[j]; p < h.ap[j + 1]; p++)
                h.av[p] *= rs[h.ai[p]] * cs[j];
        }
        h.offset *= os;
        for (int a = 0; a < 3; a++) {
            jaos_model *s = load(&h);
            char what[200];
            if (s == nullptr || jaos_set_algorithm(s, (jaos_algorithm)a) != JAOS_OK)
                return 1;
            if (jaos_solve(s) != JAOS_OK) {
                snprintf(what, sizeof what, "solve failed: %s", jaos_model_error(s));
                fail(a, 1, r, k, what);
            } else if (jaos_status_of(s) != ref) {
                snprintf(what, sizeof what, "status %s, unscaled %s: %s",
                         jaos_solve_status_str(jaos_status_of(s)),
                         jaos_solve_status_str(ref), jaos_model_error(s));
                fail(a, 1, r, k, what);
            } else if (ref == JAOS_SOLVE_OPTIMAL) {
                double obj = 0.0, x[MAXC], y[MAXR];
                jaos_check_report ck;
                if (jaos_objective(s, &obj) != JAOS_OK ||
                    jaos_solution(s, x, nullptr, y, nullptr) != JAOS_OK ||
                    jaos_check_solution(s, x, y, 1e-7, &ck) != JAOS_OK)
                    return 1;
                if (fabs(obj / os - ref_obj) > 1e-6 * (1.0 + fabs(ref_obj))) {
                    snprintf(what, sizeof what, "objective %.12g, unscaled %.12g",
                             obj / os, ref_obj);
                    fail(a, 2, r, k, what);
                } else if (!ck.primal_feasible || !ck.dual_feasible) {
                    snprintf(what, sizeof what, "the checker refuses: rows %.3g "
                             "duals %.3g; gap certified %d, relative "
                             "suboptimality %.3g", ck.max_row_violation_relative,
                             ck.max_dual_violation, (int)ck.gap_certified,
                             ck.relative_suboptimality);
                    fail(a, 3, r, k, what);
                    if (!ck.gap_certified || ck.relative_suboptimality > 1e-9) {
                        broke[a][0]++;
                        printf("UNCERTIFIED %s m%lld: %s; objective gap %.3g\n",
                               NAME[a], (long long)r, what, ck.objective_gap);
                    }
                }
            }
            jaos_model_free(s);
        }
    }
    printf("models %" PRId64 " (optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 ") scaled by up to 1e%d\n", runs, verdicts[0],
           verdicts[1], verdicts[2], k);
    int64_t total = 0;
    for (int a = 0; a < 3; a++) {
        printf("%-8s P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " of them uncertified %" PRId64 "\n", NAME[a],
               broke[a][1], broke[a][2], broke[a][3], broke[a][0]);
        total += broke[a][1] + broke[a][2] + broke[a][3];
    }
    printf("broken total %" PRId64 "\n", total);
    return 0;
}
