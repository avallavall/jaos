/* The reading behind 02-396: every LP algorithm on the same generated
 * models.
 *
 * The generator is 02-235's, LPs only: 8 to 24 columns, 6 to 16 rows of
 * small integer coefficients, six models in ten with the rows pulled off
 * the planted point and some boxes opened, so the set holds infeasible
 * and unbounded models beside the feasible ones.
 *
 * Each model is solved by the dual simplex for the reference, then by the
 * primal simplex, the barrier, PDLP and the concurrent solve. Properties,
 * for each of the four:
 *   P1 the status is the reference's
 *   P2 an optimum is within 1e-6 of (1 + |reference|) of the reference's
 *      objective, and jaos_check_solution passes its point and duals
 *   P3 an infeasible answer publishes a row certificate that
 *      jaos_check_certificate takes
 *   P4 an unbounded answer publishes a ray that jaos_check_ray takes
 *
 * Usage: algos RUNS SEED
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

static const char *const NAME[5] = {"dual", "primal", "barrier", "pdlp",
                                    "concurrent"};
static int64_t broke[5][5];

static void fail(int a, int p, int64_t idx, const char *what)
{
    broke[a][p]++;
    if (broke[a][p] <= 5)
        printf("%s P%d m%" PRId64 " %s\n", NAME[a], p, idx, what);
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    rng_state = 0x9E3779B97F4A7C15ull ^ seed;
    int64_t verdicts[3] = {0, 0, 0};
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        jaos_solve_status ref = JAOS_SOLVE_NUMERICAL_ERROR;
        double ref_obj = 0.0;
        for (int a = 0; a < 5; a++) {
            jaos_model *m = load(&g);
            if (m == nullptr ||
                jaos_set_algorithm(m, (jaos_algorithm)a) != JAOS_OK)
                return 1;
            char what[160];
            if (jaos_solve(m) != JAOS_OK) {
                snprintf(what, sizeof what, "solve failed: %s",
                         jaos_model_error(m));
                fail(a, 1, r, what);
                jaos_model_free(m);
                continue;
            }
            const jaos_solve_status st = jaos_status_of(m);
            double obj = 0.0;
            if (st == JAOS_SOLVE_OPTIMAL && jaos_objective(m, &obj) != JAOS_OK)
                return 1;
            if (a == 0) {
                ref = st;
                ref_obj = obj;
                if (st == JAOS_SOLVE_OPTIMAL) verdicts[0]++;
                else if (st == JAOS_SOLVE_INFEASIBLE) verdicts[1]++;
                else if (st == JAOS_SOLVE_UNBOUNDED) verdicts[2]++;
            }
            if (st != ref) {
                snprintf(what, sizeof what, "status %s, the dual's %s: %s",
                         jaos_solve_status_str(st),
                         jaos_solve_status_str(ref), jaos_model_error(m));
                fail(a, 1, r, what);
            } else if (st == JAOS_SOLVE_OPTIMAL) {
                double x[MAXC], y[MAXR];
                jaos_check_report ck;
                if (fabs(obj - ref_obj) > 1e-6 * (1.0 + fabs(ref_obj))) {
                    snprintf(what, sizeof what, "objective %.12g, the "
                             "dual's %.12g", obj, ref_obj);
                    fail(a, 2, r, what);
                } else if (jaos_solution(m, x, nullptr, y, nullptr) !=
                               JAOS_OK ||
                           jaos_check_solution(m, x, y, 1e-7, &ck) !=
                               JAOS_OK ||
                           !ck.primal_feasible || !ck.dual_feasible) {
                    fail(a, 2, r, "the checker refuses the optimum");
                }
            } else if (st == JAOS_SOLVE_INFEASIBLE) {
                double y[MAXR];
                jaos_certificate_report cr;
                if (jaos_certificate(m, y) != JAOS_OK ||
                    jaos_check_certificate(m, y, 1e-9, &cr) != JAOS_OK ||
                    !cr.certified)
                    fail(a, 3, r, "no certified row certificate");
            } else if (st == JAOS_SOLVE_UNBOUNDED) {
                double d[MAXC];
                jaos_ray_report rr;
                if (jaos_unbounded_ray(m, d) != JAOS_OK ||
                    jaos_check_ray(m, d, 1e-9, &rr) != JAOS_OK ||
                    !rr.certified)
                    fail(a, 4, r, "no certified ray");
            }
            jaos_model_free(m);
        }
    }
    printf("models %" PRId64 " optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 "\n", runs, verdicts[0], verdicts[1],
           verdicts[2]);
    int64_t total = 0;
    for (int a = 1; a < 5; a++) {
        printf("%-10s P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%"
               PRId64 "\n", NAME[a], broke[a][1], broke[a][2], broke[a][3],
               broke[a][4]);
        for (int p = 1; p < 5; p++)
            total += broke[a][p];
    }
    printf("dual failed solves %" PRId64 " broken total %" PRId64 "\n",
           broke[0][1], total);
    return total > 0 ? 1 : 0;
}
