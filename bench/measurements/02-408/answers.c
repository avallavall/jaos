/* The reading behind 02-408: a solved model's answer files read back.
 *
 * The generator is 02-235's, LPs only, as 02-396's algos.c: 8 to 24
 * columns, 6 to 16 rows, most models infeasible or unbounded. Each model
 * is solved by the dual simplex. Properties:
 *   P1 jaos_write_solution writes the answer, and
 *      jaos_solution_file_status reads back the solve's status
 *   P2 after an optimum, jaos_read_solution gives back the objective, the
 *      values, the reduced costs, the activities, the duals and the basis
 *      to the bit
 *   P3 the MPS basis file reads back as the solve's basis
 *   P4 a copy of the model started from that basis solves to the same
 *      optimum within 1e-9
 *   P5 the point and the duals files read back to the bit
 *   P6 after an infeasible or unbounded answer, jaos_read_certificate
 *      gives back the solve's certificate or ray to the bit, and the
 *      checker takes it
 *
 * Usage: answers RUNS SEED TMPDIR
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

static int64_t broke[7];

static void fail(int p, int64_t idx, const char *what)
{
    broke[p]++;
    if (broke[p] <= 8)
        printf("P%d m%" PRId64 " %s\n", p, idx, what);
}

static bool bits(const double *a, const double *b, int64_t n)
{
    return n == 0 || memcmp(a, b, (size_t)n * sizeof *a) == 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: answers RUNS SEED TMPDIR\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    char sol[512], bas[512], pt[512], du[512];
    snprintf(sol, sizeof sol, "%s/answers.sol", argv[3]);
    snprintf(bas, sizeof bas, "%s/answers.bas", argv[3]);
    snprintf(pt, sizeof pt, "%s/answers.pt", argv[3]);
    snprintf(du, sizeof du, "%s/answers.du", argv[3]);
    int64_t verdicts[3] = {0, 0, 0}, warm_iters = 0, warm_moved = 0;
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr || jaos_solve(m) != JAOS_OK)
            return 1;
        const jaos_solve_status st = jaos_status_of(m);
        const int64_t nc = g.nc, nr = g.nr;
        char what[200];
        if (jaos_write_solution(m, sol) != JAOS_OK) {
            snprintf(what, sizeof what, "write_solution: %s", jaos_model_error(m));
            fail(1, r, what);
            jaos_model_free(m);
            continue;
        }
        jaos_solve_status fst;
        if (jaos_solution_file_status(m, sol, &fst) != JAOS_OK || fst != st) {
            snprintf(what, sizeof what, "file status %d, the solve's %d: %s",
                     (int)fst, (int)st, jaos_model_error(m));
            fail(1, r, what);
        }
        if (st == JAOS_SOLVE_OPTIMAL) {
            verdicts[0]++;
            double obj, x[MAXC], d[MAXC], ra[MAXR], y[MAXR];
            jaos_basis_status cs[MAXC], rs[MAXR];
            if (jaos_objective(m, &obj) != JAOS_OK ||
                jaos_solution(m, x, ra, y, d) != JAOS_OK ||
                jaos_basis(m, cs, rs) != JAOS_OK)
                return 1;
            double obj2, x2[MAXC], d2[MAXC], ra2[MAXR], y2[MAXR];
            jaos_basis_status cs2[MAXC], rs2[MAXR];
            if (jaos_read_solution(m, sol, &obj2, x2, d2, cs2, ra2, y2, rs2) !=
                JAOS_OK) {
                snprintf(what, sizeof what, "read_solution: %s", jaos_model_error(m));
                fail(2, r, what);
            } else if (memcmp(&obj, &obj2, sizeof obj) || !bits(x, x2, nc) ||
                       !bits(d, d2, nc) || !bits(ra, ra2, nr) ||
                       !bits(y, y2, nr) ||
                       memcmp(cs, cs2, (size_t)nc * sizeof *cs) ||
                       memcmp(rs, rs2, (size_t)nr * sizeof *rs)) {
                fail(2, r, "the solution file reads back different");
            }
            jaos_basis_status cs3[MAXC], rs3[MAXR];
            if (jaos_write_mps_basis(m, bas) != JAOS_OK ||
                jaos_read_mps_basis(m, bas, cs3, rs3) != JAOS_OK) {
                snprintf(what, sizeof what, "MPS basis: %s", jaos_model_error(m));
                fail(3, r, what);
            } else if (memcmp(cs, cs3, (size_t)nc * sizeof *cs) ||
                       memcmp(rs, rs3, (size_t)nr * sizeof *rs)) {
                fail(3, r, "the MPS basis reads back different");
            } else {
                jaos_model *c = nullptr;
                double wobj;
                if (jaos_model_copy(m, &c) != JAOS_OK ||
                    jaos_set_basis(c, cs3, rs3) != JAOS_OK ||
                    jaos_solve(c) != JAOS_OK)
                    return 1;
                warm_iters += jaos_iterations(c);
                if (jaos_status_of(c) != JAOS_SOLVE_OPTIMAL ||
                    jaos_objective(c, &wobj) != JAOS_OK ||
                    fabs(wobj - obj) > 1e-9 * (1.0 + fabs(obj))) {
                    snprintf(what, sizeof what, "warm from the basis: %s %.12g, "
                             "the solve's %.12g",
                             jaos_solve_status_str(jaos_status_of(c)), wobj, obj);
                    fail(4, r, what);
                } else if (jaos_iterations(c) > 0) {
                    warm_moved++;
                }
                jaos_model_free(c);
            }
            double x4[MAXC], y5[MAXR];
            if (jaos_write_point(m, pt) != JAOS_OK ||
                jaos_read_point(m, pt, x4) != JAOS_OK || !bits(x, x4, nc) ||
                jaos_write_duals(m, du) != JAOS_OK ||
                jaos_read_duals(m, du, y5) != JAOS_OK || !bits(y, y5, nr)) {
                snprintf(what, sizeof what, "point or duals: %s", jaos_model_error(m));
                fail(5, r, what);
            }
        } else if (st == JAOS_SOLVE_INFEASIBLE || st == JAOS_SOLVE_UNBOUNDED) {
            verdicts[st == JAOS_SOLVE_INFEASIBLE ? 1 : 2]++;
            jaos_solve_status cst;
            double rr[MAXR], cr[MAXC];
            if (jaos_read_certificate(m, sol, &cst, rr, cr) != JAOS_OK || cst != st) {
                snprintf(what, sizeof what, "read_certificate: %s", jaos_model_error(m));
                fail(6, r, what);
            } else if (st == JAOS_SOLVE_INFEASIBLE) {
                double y[MAXR];
                jaos_certificate_report rep;
                if (jaos_certificate(m, y) != JAOS_OK || !bits(y, rr, nr) ||
                    jaos_check_certificate(m, rr, 1e-9, &rep) != JAOS_OK ||
                    !rep.certified)
                    fail(6, r, "the certificate read back is not the solve's, or "
                               "not certified");
            } else {
                double d[MAXC];
                jaos_ray_report rep;
                if (jaos_unbounded_ray(m, d) != JAOS_OK || !bits(d, cr, nc) ||
                    jaos_check_ray(m, cr, 1e-9, &rep) != JAOS_OK || !rep.certified)
                    fail(6, r, "the ray read back is not the solve's, or not "
                               "certified");
            }
        }
        jaos_model_free(m);
    }
    printf("models %" PRId64 " optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 "; warm starts that pivoted %" PRId64
           ", iterations %" PRId64 "\n", runs, verdicts[0], verdicts[1],
           verdicts[2], warm_moved, warm_iters);
    printf("P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64 " P5=%"
           PRId64 " P6=%" PRId64 " broken total %" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6],
           broke[1] + broke[2] + broke[3] + broke[4] + broke[5] + broke[6]);
    return 0;
}
