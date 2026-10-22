/* The reading behind 02-420: exact solving of LPs whose data are decimals.
 *
 * The generator is 02-235's, LPs only, as 02-411's scaled.c. Each model is
 * then written in decimals: each cost and matrix entry is divided by 10,
 * each column bound by 10 and each row side by 100, so the planted point
 * divided by 10 still holds every row exactly in decimal. Each model is
 * solved in floating point, exactly on its doubles, and exactly on its
 * decimals. Properties of the decimal exact solve:
 *   P1 it ends optimal, infeasible or unbounded
 *   P2 its status is the floating-point solve's
 *   P3 on an optimum, its exact objective is within 1e-9 of (1 + |obj|)
 *      of the floating-point one
 *   P4 the proof file it writes is certified by jaos_check_proof on a
 *      fresh copy of the model with neither option set
 * The binary exact solves that end with another status are counted.
 *
 * Usage: decimals RUNS SEED DIR
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

static void decimalise(gen *g)
{
    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = g->cost[j] / 10.0 + (double)ri(-9, 9) / 100.0;
        g->cu[j] /= 10.0;
    }
    for (int64_t p = 0; p < g->nz; p++)
        g->av[p] /= 10.0;
    for (int64_t i = 0; i < g->nr; i++) {
        g->rl[i] /= 100.0;
        g->ru[i] /= 100.0;
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

static double rational_value(const char *s)
{
    const char *slash = strchr(s, '/');
    if (slash == nullptr)
        return strtod(s, nullptr);
    return strtod(s, nullptr) / strtod(slash + 1, nullptr);
}

static int64_t broke[5];
static const char *dump_dir;
static const gen *current;

static void fail(int p, int64_t idx, const char *what)
{
    broke[p]++;
    if (broke[p] > 6)
        return;
    printf("P%d m%" PRId64 " %s\n", p, idx, what);
    jaos_model *m = load(current);
    char file[4200];
    snprintf(file, sizeof file, "%s/p%d-m%" PRId64 ".mps", dump_dir, p, idx);
    if (m != nullptr && jaos_write_mps(m, file) != JAOS_OK)
        printf("  could not write %s\n", file);
    jaos_model_free(m);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: decimals RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)atoll(argv[2]);
    char path[4096];
    snprintf(path, sizeof path, "%s/decimals-%s.proof", argv[3], argv[2]);
    dump_dir = argv[3];
    int64_t seen[3] = {0, 0, 0}, binary_other = 0, binary_infeasible = 0;
    int64_t binary_numerical = 0;
    for (int64_t r = 0; r < runs; r++) {
        gen g;
        generate(&g);
        decimalise(&g);
        current = &g;
        char what[300];

        jaos_model *f = load(&g);
        if (f == nullptr || jaos_solve(f) != JAOS_OK)
            return 1;
        const jaos_solve_status fs = jaos_status_of(f);
        double fobj = 0.0;
        if (fs == JAOS_SOLVE_OPTIMAL && jaos_objective(f, &fobj) != JAOS_OK)
            return 1;
        jaos_model_free(f);
        if (fs == JAOS_SOLVE_OPTIMAL) seen[0]++;
        else if (fs == JAOS_SOLVE_INFEASIBLE) seen[1]++;
        else if (fs == JAOS_SOLVE_UNBOUNDED) seen[2]++;

        jaos_model *b = load(&g);
        if (b == nullptr || jaos_set_exact(b, true) != JAOS_OK ||
            jaos_solve(b) != JAOS_OK)
            return 1;
        if (jaos_status_of(b) != fs) {
            binary_other++;
            if (jaos_status_of(b) == JAOS_SOLVE_INFEASIBLE)
                binary_infeasible++;
            else if (jaos_status_of(b) == JAOS_SOLVE_NUMERICAL_ERROR) {
                binary_numerical++;
                snprintf(what, sizeof what, "binary exact: %s",
                         jaos_model_error(b));
                fail(0, r, what);
            }
        }
        jaos_model_free(b);

        jaos_model *d = load(&g);
        if (d == nullptr || jaos_set_exact(d, true) != JAOS_OK ||
            jaos_set_exact_decimal(d, true) != JAOS_OK || jaos_solve(d) != JAOS_OK)
            return 1;
        const jaos_solve_status ds = jaos_status_of(d);
        if (ds != JAOS_SOLVE_OPTIMAL && ds != JAOS_SOLVE_INFEASIBLE &&
            ds != JAOS_SOLVE_UNBOUNDED) {
            snprintf(what, sizeof what, "status %s: %s",
                     jaos_solve_status_str(ds), jaos_model_error(d));
            fail(1, r, what);
        } else if (ds != fs) {
            snprintf(what, sizeof what, "status %s, floating point %s",
                     jaos_solve_status_str(ds), jaos_solve_status_str(fs));
            fail(2, r, what);
        } else {
            if (ds == JAOS_SOLVE_OPTIMAL) {
                const char *ex = nullptr;
                if (jaos_exact_objective(d, &ex) != JAOS_OK)
                    return 1;
                const double v = rational_value(ex);
                if (fabs(v - fobj) > 1e-9 * (1.0 + fabs(fobj))) {
                    snprintf(what, sizeof what, "exact %s, floating point %.17g",
                             ex, fobj);
                    fail(3, r, what);
                }
            }
            jaos_model *plain = load(&g);
            jaos_proof_report pr;
            if (plain == nullptr)
                return 1;
            if (jaos_write_proof(d, path) != JAOS_OK) {
                snprintf(what, sizeof what, "no proof written: %s",
                         jaos_model_error(d));
                fail(4, r, what);
            } else if (jaos_check_proof(plain, path, &pr) != JAOS_OK ||
                       !pr.certified) {
                snprintf(what, sizeof what, "the proof is not certified: %s",
                         jaos_model_error(plain));
                fail(4, r, what);
            }
            jaos_model_free(plain);
        }
        jaos_model_free(d);
    }
    printf("models %" PRId64 " (optimal %" PRId64 " infeasible %" PRId64
           " unbounded %" PRId64 ") binary exact with another status %" PRId64
           " (infeasible %" PRId64 ", numerical_error %" PRId64 ")\n", runs,
           seen[0], seen[1], seen[2], binary_other, binary_infeasible,
           binary_numerical);
    printf("P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64 "\n",
           broke[1], broke[2], broke[3], broke[4]);
    printf("broken total %" PRId64 "\n", broke[1] + broke[2] + broke[3] + broke[4]);
    return 0;
}
