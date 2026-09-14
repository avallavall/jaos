/* The reading behind 02-234: writes generated models to disk, so the
 * Linux tool and the Windows tool under wine can solve the same files.
 *
 * The generator is 02-232's: eight to twenty-four columns, six to sixteen
 * rows, a planted integer point inside the boxes with each row's bounds
 * set around it, a third of the models with no integer mark. One model
 * in ten has its rows pulled off the planted point so the model is
 * infeasible or unbounded, and the certificates are compared too.
 *
 * Usage: bitmodels RUNS SEED DIR
 * Writes DIR/m<idx>.mps for every model.
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
    const bool broken = ri(0, 9) == 0;
    int64_t z[MAXC];
    double act[MAXR];
    for (int64_t i = 0; i < g->nr; i++) act[i] = 0.0;
    g->nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->isint[j] = !g->lp_only && ri(0, 9) < 6;
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

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: bitmodels RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t lps = 0, mips = 0;
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        if (g.lp_only) lps++; else mips++;
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) abort();
        if (jaos_load_lp(m, g.nc, g.nr,
                         g.maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE,
                         g.offset, g.cost, g.cl, g.cu, g.rl, g.ru, g.nz,
                         g.ap, g.ai, g.av) != JAOS_OK) abort();
        for (int64_t j = 0; j < g.nc; j++)
            if (g.isint[j] && jaos_set_col_integer(m, j, true) != JAOS_OK)
                abort();
        char path[4096];
        snprintf(path, sizeof path, "%s/m%" PRId64 ".mps", dir, idx);
        if (jaos_write_mps(m, path) != JAOS_OK) abort();
        jaos_model_free(m);
    }
    printf("models %" PRId64 " lp %" PRId64 " mip %" PRId64 "\n", runs, lps,
           mips);
    return 0;
}
