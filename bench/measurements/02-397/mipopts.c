/* The reading behind 02-397: the MIP tree's answer under each of its
 * options.
 *
 * The generator is 02-235's with every model carrying integer marks: 8
 * to 24 columns, 6 to 16 rows of small integer coefficients, six models
 * in ten with the rows pulled off the planted point and some boxes
 * opened, so the set holds infeasible and unbounded models beside the
 * feasible ones.
 *
 * Each model is solved with the default options for the reference, then
 * once under each variant of VARIANTS, one or two options set through
 * jaos_set_option. Properties, for each variant:
 *   P1 the solve returns JAOS_OK and the status is the reference's
 *   P2 an optimum is within 1e-5 of (1 + |reference|) of the reference's
 *      objective, its integer columns are integral, and
 *      jaos_check_solution finds its point primal feasible
 *
 * Usage: mipopts RUNS SEED [DUMP_INDEX DUMP_PATH]
 * With DUMP_INDEX, writes that model to DUMP_PATH as MPS and solves
 * nothing.
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
    bool isint[MAXC];
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
        g->isint[j] = ri(0, 9) < 6;
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
    for (int64_t j = 0; j < g->nc; j++)
        if (g->isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) {
            jaos_model_free(m);
            return nullptr;
        }
    return m;
}

static const char *const VARIANTS[][4] = {
    {"mip_tree_batch", "4", nullptr, nullptr},
    {"mip_branching", "most-fractional", nullptr, nullptr},
    {"mip_reliability", "4", nullptr, nullptr},
    {"mip_reliability", "4", "mip_probe_cap", "2"},
    {"mip_cut_rounds", "0", nullptr, nullptr},
    {"mip_cut_rounds", "5", nullptr, nullptr},
    {"mip_cut_depth", "0", nullptr, nullptr},
    {"mip_cut_drop", "false", nullptr, nullptr},
    {"mip_node_cut_cap", "0", nullptr, nullptr},
    {"mip_cover_rounds", "0", nullptr, nullptr},
    {"mip_cover_lift", "true", nullptr, nullptr},
    {"mip_mir_rounds", "0", nullptr, nullptr},
    {"mip_node_mir", "true", nullptr, nullptr},
    {"mip_mir_aggregate", "1", nullptr, nullptr},
    {"mip_dive", "true", nullptr, nullptr},
    {"mip_dive", "true", "mip_dive_child", "up"},
    {"mip_dive", "true", "mip_dive_child", "pseudocost"},
    {"mip_dive", "true", "mip_dive_backtrack", "5"},
    {"mip_dive", "true", "mip_dive_degrade", "0.5"},
    {"mip_dive_heuristic", "0", nullptr, nullptr},
    {"mip_dive_heuristic_depth", "3", nullptr, nullptr},
    {"mip_rins", "0", nullptr, nullptr},
    {"mip_feaspump", "0", nullptr, nullptr},
    {"mip_pump_general", "true", nullptr, nullptr},
    {"mip_pump_always", "true", nullptr, nullptr},
    {"mip_rcfix", "false", nullptr, nullptr},
    {"mip_tighten", "false", nullptr, nullptr},
    {"mip_presolve", "false", nullptr, nullptr},
    {"mip_probing", "true", nullptr, nullptr},
    {"mip_clique_fix", "false", nullptr, nullptr},
    {"mip_conflicts", "false", nullptr, nullptr},
    {"mip_symmetry", "true", nullptr, nullptr},
    {"mip_orbital", "false", nullptr, nullptr},
    {"mip_propagate", "4", nullptr, nullptr},
    {"mip_heuristics", "false", nullptr, nullptr},
    {"mip_pool_size", "5", nullptr, nullptr},
    {"mip_clique_rounds", "0", nullptr, nullptr},
    {"mip_zero_half_rounds", "3", nullptr, nullptr},
    {"mip_flow_cover_rounds", "0", nullptr, nullptr},
    {"mip_cover_rounds", "10", nullptr, nullptr},
    {"mip_clique_rounds", "10", nullptr, nullptr},
    {"mip_cut_rounds", "8", nullptr, nullptr},
    {"mip_hull_rounds", "0", nullptr, nullptr},
    {"mip_local_branching", "5", nullptr, nullptr},
    {"mip_node_select", "0", nullptr, nullptr},
    {"mip_restart", "true", nullptr, nullptr},
    {"threads", "4", nullptr, nullptr},
    {"algorithm", "primal", nullptr, nullptr},
    {"mip_gap_rule", "relative", nullptr, nullptr},
};
#define NV ((int)(sizeof VARIANTS / sizeof VARIANTS[0]))

static int64_t broke[NV][3];

static void fail(int v, int p, int64_t idx, const char *what)
{
    broke[v][p]++;
    if (broke[v][p] <= 3)
        printf("%s=%s%s%s%s P%d m%" PRId64 " %s\n", VARIANTS[v][0],
               VARIANTS[v][1], VARIANTS[v][2] ? " " : "",
               VARIANTS[v][2] ? VARIANTS[v][2] : "",
               VARIANTS[v][2] ? "=..." : "", p, idx, what);
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 300;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t dump = argc > 4 ? atoll(argv[3]) : -1;
    rng_state = 0x9E3779B97F4A7C15ull ^ seed;
    int64_t verdicts[4] = {0, 0, 0, 0};
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        if (dump >= 0) {
            if (r != dump)
                continue;
            jaos_model *d = load(&g);
            if (d == nullptr || jaos_write_mps(d, argv[4]) != JAOS_OK)
                return 1;
            jaos_model_free(d);
            return 0;
        }
        jaos_model *m = load(&g);
        if (m == nullptr || jaos_solve(m) != JAOS_OK) {
            printf("m%" PRId64 " the default solve failed: %s\n", r,
                   m ? jaos_model_error(m) : "no model");
            verdicts[3]++;
            jaos_model_free(m);
            continue;
        }
        const jaos_solve_status ref = jaos_status_of(m);
        double ref_obj = 0.0;
        if (ref == JAOS_SOLVE_OPTIMAL && jaos_objective(m, &ref_obj) != JAOS_OK)
            return 1;
        verdicts[ref == JAOS_SOLVE_OPTIMAL ? 0
                 : ref == JAOS_SOLVE_INFEASIBLE ? 1
                 : ref == JAOS_SOLVE_UNBOUNDED ? 2 : 3]++;
        jaos_model_free(m);
        for (int v = 0; v < NV; v++) {
            jaos_model *c = load(&g);
            if (c == nullptr ||
                jaos_set_option(c, VARIANTS[v][0], VARIANTS[v][1]) != JAOS_OK ||
                (VARIANTS[v][2] != nullptr &&
                 jaos_set_option(c, VARIANTS[v][2], VARIANTS[v][3]) != JAOS_OK)) {
                printf("cannot set %s\n", VARIANTS[v][0]);
                return 1;
            }
            char what[200];
            if (jaos_solve(c) != JAOS_OK) {
                snprintf(what, sizeof what, "solve failed: %s",
                         jaos_model_error(c));
                fail(v, 1, r, what);
                jaos_model_free(c);
                continue;
            }
            const jaos_solve_status st = jaos_status_of(c);
            if (st != ref) {
                snprintf(what, sizeof what, "status %s, the default's %s",
                         jaos_solve_status_str(st), jaos_solve_status_str(ref));
                fail(v, 1, r, what);
            } else if (st == JAOS_SOLVE_OPTIMAL) {
                double obj = 0.0, x[MAXC];
                jaos_check_report ck;
                bool integral = true;
                if (jaos_objective(c, &obj) != JAOS_OK ||
                    jaos_solution(c, x, nullptr, nullptr, nullptr) != JAOS_OK ||
                    jaos_check_solution(c, x, nullptr, 1e-6, &ck) != JAOS_OK)
                    return 1;
                for (int64_t j = 0; j < g.nc; j++)
                    if (g.isint[j] && x[j] != round(x[j]))
                        integral = false;
                if (fabs(obj - ref_obj) > 1e-5 * (1.0 + fabs(ref_obj)) ||
                    !integral || !ck.primal_feasible) {
                    snprintf(what, sizeof what, "objective %.12g, the "
                             "default's %.12g, integral %d, primal %d", obj,
                             ref_obj, integral, ck.primal_feasible);
                    fail(v, 2, r, what);
                }
            }
            jaos_model_free(c);
        }
    }
    printf("models %" PRId64 " optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 " other %" PRId64 "\n", runs, verdicts[0],
           verdicts[1], verdicts[2], verdicts[3]);
    int64_t total = 0;
    for (int v = 0; v < NV; v++)
        if (broke[v][1] + broke[v][2] > 0) {
            printf("%s=%s%s%s: P1=%" PRId64 " P2=%" PRId64 "\n", VARIANTS[v][0],
                   VARIANTS[v][1], VARIANTS[v][2] ? " +" : "",
                   VARIANTS[v][2] ? VARIANTS[v][2] : "", broke[v][1],
                   broke[v][2]);
            total += broke[v][1] + broke[v][2];
        }
    printf("variants %d broken total %" PRId64 "\n", NV, total);
    return total > 0 ? 1 : 0;
}
