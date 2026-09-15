/* The reading behind 02-241: `diff` says whether two files are one model,
 * and `show` prints one row or column as the model holds it.
 *
 * The generator is 02-237's with column X1 binary in every model, so an
 * indicator can be put on a row with a single edit, and a third of the
 * models carrying more integer marks. Every model is written as A.mps
 * and, where the LP writer takes it, as A.lp. A copy of the model gets
 * exactly one edit through the API and is written as B.mps; `.expect`
 * says which difference line `diff A.mps B.mps` has to print first, and
 * `differences` has to be at least 1. `.showrow` and `.showcol` hold the
 * exact text `show --row` and `show --col` have to print for one row and
 * one column, formatted here from the model the harness built. The CLI
 * runs are in diffshow.sh; this program only writes the files.
 *
 * The edits, one per model, chosen at random: a cost (`cost X`), a column
 * bound (`col_bounds X`), a row bound (`row_bounds R`), a coefficient
 * that exists (`entry X R`), a coefficient that did not (`nonzeros`), an
 * integer mark (`integer X`), the sense (`sense`), the offset
 * (`offset`), a column name (`col_name j`), a row name (`row_name i`),
 * a semi-continuous mark (`semicontinuous X`), a quadratic diagonal
 * (`quadratic X`), an indicator (`indicator R`), an SOS set (`sos`).
 *
 * Usage: diffshow RUNS SEED DIR
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
        g->cu[j] = j == 0 ? 1.0 : (double)ri(1, 10);
        g->integer[j] = j == 0 || (mip && ri(0, 1) == 1);
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
    char nm[32];
    for (int64_t j = 0; j < g->nc; j++) {
        snprintf(nm, sizeof nm, "X%" PRId64, j + 1);
        if (jaos_set_col_name(m, j, nm) != JAOS_OK) abort();
        if (g->integer[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) abort();
    }
    for (int64_t i = 0; i < g->nr; i++) {
        snprintf(nm, sizeof nm, "R%" PRId64, i + 1);
        if (jaos_set_row_name(m, i, nm) != JAOS_OK) abort();
    }
    return m;
}

static const char *num(char *buf, double v)
{
    if (isinf(v)) return v > 0 ? "inf" : "-inf";
    snprintf(buf, 64, "%.17g", v);
    return buf;
}

static double coef(const gen *g, int64_t i, int64_t j)
{
    for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++)
        if (g->ai[p] == i) return g->av[p];
    return 0.0;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: diffshow RUNS SEED DIR\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t lps = 0, kinds[16] = {0};
    char path[4096], buf[64], nm[32], nm2[32];
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        jaos_model *a = load(&g);
        if (a == nullptr) abort();
        snprintf(path, sizeof path, "%s/m%" PRId64 ".a.mps", dir, idx);
        if (jaos_write_mps(a, path) != JAOS_OK) abort();
        snprintf(path, sizeof path, "%s/m%" PRId64 ".a.lp", dir, idx);
        if (jaos_write_lp(a, path) == JAOS_OK) lps++;
        else remove(path);

        jaos_model *b = nullptr;
        if (jaos_model_copy(a, &b) != JAOS_OK) abort();
        const int kind = (int)ri(0, 13);
        kinds[kind]++;
        const int64_t j = ri(1, g.nc - 1), i = ri(0, g.nr - 1);
        snprintf(nm, sizeof nm, "X%" PRId64, j + 1);
        snprintf(nm2, sizeof nm2, "R%" PRId64, i + 1);
        char expect[128];
        switch (kind) {
        case 0:
            if (jaos_set_col_cost(b, j, g.cost[j] + 1.0) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "cost %s", nm);
            break;
        case 1:
            if (jaos_set_col_bounds(b, j, g.cl[j], g.cu[j] + 1.0) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "col_bounds %s", nm);
            break;
        case 2: {
            const double lo = isfinite(g.rl[i]) ? g.rl[i] - 1.0 : g.rl[i];
            const double hi = isfinite(g.ru[i]) ? g.ru[i] + 1.0 : g.ru[i];
            if (jaos_set_row_bounds(b, i, lo, hi) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "row_bounds %s", nm2);
            break;
        }
        case 3: {
            int64_t jj = -1, ii = -1;
            for (int64_t t = 0; t < 64 && jj < 0; t++) {
                const int64_t c = ri(1, g.nc - 1);
                if (g.ap[c + 1] > g.ap[c]) { jj = c; ii = g.ai[ri(g.ap[c], g.ap[c + 1] - 1)]; }
            }
            if (jj < 0) { jaos_model_free(b); jaos_model_free(a); kinds[kind]--; idx--; continue; }
            const double was = coef(&g, ii, jj);
            if (jaos_set_coefficient(b, ii, jj, was == -1.0 ? 1.0 : was + 1.0) != JAOS_OK) abort();
            snprintf(nm, sizeof nm, "X%" PRId64, jj + 1);
            snprintf(expect, sizeof expect, "entry %s", nm);
            break;
        }
        case 4: {
            int64_t jj = -1, ii = -1;
            for (int64_t t = 0; t < 64 && jj < 0; t++) {
                const int64_t c = ri(1, g.nc - 1), r = ri(0, g.nr - 1);
                if (coef(&g, r, c) == 0.0) { jj = c; ii = r; }
            }
            if (jj < 0) { jaos_model_free(b); jaos_model_free(a); kinds[kind]--; idx--; continue; }
            if (jaos_set_coefficient(b, ii, jj, 1.0) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "nonzeros");
            break;
        }
        case 5:
            if (jaos_set_col_integer(b, j, !g.integer[j]) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "integer %s", nm);
            break;
        case 6:
            if (jaos_set_objective_sense(b, g.maximise ? JAOS_MINIMIZE : JAOS_MAXIMIZE) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "sense");
            break;
        case 7:
            if (jaos_set_objective_offset(b, g.offset + 1.0) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "offset");
            break;
        case 8:
            snprintf(buf, sizeof buf, "Y%" PRId64, j + 1);
            if (jaos_set_col_name(b, j, buf) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "col_name %" PRId64, j);
            break;
        case 9:
            snprintf(buf, sizeof buf, "S%" PRId64, i + 1);
            if (jaos_set_row_name(b, i, buf) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "row_name %" PRId64, i);
            break;
        case 10:
            if (jaos_set_col_semicontinuous(b, j, true) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "semicontinuous %s", nm);
            break;
        case 11: {
            const int64_t r[1] = {j}, c[1] = {j};
            const double v[1] = {2.0};
            if (jaos_set_quadratic(b, 1, r, c, v) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "quadratic %s", nm);
            break;
        }
        case 12:
            if (jaos_set_row_indicator(b, i, 0, 1) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "indicator %s", nm2);
            break;
        case 13: {
            const int64_t c[2] = {j, j == 1 ? 2 : 1};
            const double w[2] = {1.0, 2.0};
            if (jaos_add_sos(b, 1, 2, c, w) != JAOS_OK) abort();
            snprintf(expect, sizeof expect, "sos");
            break;
        }
        default:
            abort();
        }
        snprintf(path, sizeof path, "%s/m%" PRId64 ".b.mps", dir, idx);
        if (jaos_write_mps(b, path) != JAOS_OK) { printf("m%" PRId64 " kind %d: %s\n", idx, kind, jaos_model_error(b)); abort(); }
        snprintf(path, sizeof path, "%s/m%" PRId64 ".expect", dir, idx);
        FILE *f = fopen(path, "w");
        if (f == nullptr) abort();
        fprintf(f, "%s\n", expect);
        fclose(f);
        jaos_model_free(b);

        const int64_t si = ri(0, g.nr - 1), sj = ri(0, g.nc - 1);
        snprintf(path, sizeof path, "%s/m%" PRId64 ".showrow", dir, idx);
        f = fopen(path, "w");
        if (f == nullptr) abort();
        fprintf(f, "R%" PRId64 "\n", si + 1);
        fprintf(f, "row R%" PRId64 "\nindex %" PRId64 "\n", si + 1, si);
        fprintf(f, "lower %s\n", num(buf, g.rl[si]));
        fprintf(f, "upper %s\n", num(buf, g.ru[si]));
        int64_t n = 0;
        for (int64_t c = 0; c < g.nc; c++) if (coef(&g, si, c) != 0.0) n++;
        fprintf(f, "entries %" PRId64 "\n", n);
        for (int64_t c = 0; c < g.nc; c++) {
            const double v = coef(&g, si, c);
            if (v != 0.0) fprintf(f, "term X%" PRId64 " %s\n", c + 1, num(buf, v));
        }
        fclose(f);
        snprintf(path, sizeof path, "%s/m%" PRId64 ".showcol", dir, idx);
        f = fopen(path, "w");
        if (f == nullptr) abort();
        fprintf(f, "X%" PRId64 "\n", sj + 1);
        fprintf(f, "col X%" PRId64 "\nindex %" PRId64 "\n", sj + 1, sj);
        fprintf(f, "lower %s\n", num(buf, g.cl[sj]));
        fprintf(f, "upper %s\n", num(buf, g.cu[sj]));
        fprintf(f, "cost %s\n", num(buf, g.cost[sj]));
        fprintf(f, "integer %s\n", g.integer[sj] ? "yes" : "no");
        fprintf(f, "entries %" PRId64 "\n", g.ap[sj + 1] - g.ap[sj]);
        for (int64_t p = g.ap[sj]; p < g.ap[sj + 1]; p++)
            fprintf(f, "term R%" PRId64 " %s\n", g.ai[p] + 1, num(buf, g.av[p]));
        fclose(f);
        jaos_model_free(a);
    }
    printf("models %" PRId64 " lps %" PRId64 " kinds", runs, lps);
    for (int k = 0; k < 14; k++) printf(" %" PRId64, kinds[k]);
    printf("\n");
    return 0;
}
