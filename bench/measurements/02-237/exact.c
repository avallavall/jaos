/* The reading behind 02-237: the exact rational values of a proved basis,
 * over generated LPs.
 *
 * The generator is 02-232's with every model an LP: eight to twenty-four
 * columns, six to sixteen rows, a planted point inside the boxes with each
 * row's bounds set around it, integer data throughout, so every optimum
 * is a rational the exact arithmetic can hold.
 *
 * Six properties, on every model the verifier proves optimal:
 *   P1 every exact column value, row dual and the exact objective is an
 *      integer or a ratio of two, in decimal, and its value agrees with
 *      the published double to 1e-9 of itself
 *   P2 the proof file written from them holds under jaos_check_proof:
 *      primal, dual and objective, and the file claims an optimum
 *   P3 the exact point sits inside every row and every box, read in
 *      extended precision
 *   P4 a second solve and verification gives the same strings, byte for
 *      byte
 *   P5 the proof file with one column value replaced does not hold: the
 *      exact checker is not vacuous
 *   P6 the verifier never says BROKEN: it proves, or it refuses for want
 *      of capacity, and a refusal is counted
 *
 * Usage: exact RUNS SEED DIR [DUMP_INDEX DUMP_PATH]
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <ctype.h>
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
    int64_t z[MAXC];
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

/* "p" or "p/q" in decimal, nothing else; the value in extended precision,
 * or false when the text is not that shape. */
static bool rational(const char *s, long double *v)
{
    const char *p = s;
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return false;
    while (isdigit((unsigned char)*p)) p++;
    long double num = strtold(s, nullptr), den = 1.0L;
    if (*p == '/') {
        p++;
        if (!isdigit((unsigned char)*p)) return false;
        den = strtold(p, nullptr);
        while (isdigit((unsigned char)*p)) p++;
        if (den <= 0.0L) return false;
    }
    if (*p != '\0') return false;
    *v = num / den;
    return true;
}

static int64_t broke[8];

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

static bool agrees(long double exact, double published)
{
    const long double d = fabsl(exact - (long double)published);
    return d <= 1e-9L * (1.0L + fabsl((long double)published));
}

#define MAXSTR 512

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: exact RUNS SEED DIR [DUMP_INDEX DUMP_PATH]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t dump = argc > 5 ? atoll(argv[4]) : -1;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, proved = 0, refused = 0, brokenv = 0, terms = 0;
    static char first[MAXC + MAXR + 1][MAXSTR];
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        if (idx == dump) {
            jaos_model *d = load(&g);
            if (d == nullptr || jaos_write_mps(d, argv[5]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }
        optimal++;
        jaos_verify_report vr;
        if (jaos_verify(m, &vr) != JAOS_OK) abort();
        if (vr.status == JAOS_PROOF_REFUSED) {
            refused++;
            jaos_model_free(m);
            continue;
        }
        if (vr.status == JAOS_PROOF_BROKEN) {
            brokenv++;
            fail(6, idx, "verifier-says-broken", (double)vr.stage,
                 vr.violation);
            jaos_model_free(m);
            continue;
        }
        proved++;
        terms += vr.terms;

        double x[MAXC], y[MAXR], obj;
        if (jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK) abort();
        if (jaos_objective(m, &obj) != JAOS_OK) abort();
        long double ex[MAXC], ey[MAXR], eobj = 0.0L;
        const char *s = nullptr;
        for (int64_t j = 0; j < g.nc; j++) {
            if (jaos_exact_col_value(m, j, &s) != JAOS_OK) { fail(1, idx, "no-col-value", (double)j, 0.0); ex[j] = 0.0L; continue; }
            snprintf(first[j], MAXSTR, "%s", s);
            if (!rational(s, &ex[j])) { fail(1, idx, "col-not-rational", (double)j, 0.0); ex[j] = 0.0L; continue; }
            if (!agrees(ex[j], x[j])) fail(1, idx, "col-value-disagrees", (double)ex[j], x[j]);
        }
        for (int64_t i = 0; i < g.nr; i++) {
            if (jaos_exact_row_dual(m, i, &s) != JAOS_OK) { fail(1, idx, "no-row-dual", (double)i, 0.0); ey[i] = 0.0L; continue; }
            snprintf(first[g.nc + i], MAXSTR, "%s", s);
            if (!rational(s, &ey[i])) { fail(1, idx, "dual-not-rational", (double)i, 0.0); ey[i] = 0.0L; continue; }
            if (!agrees(ey[i], y[i])) fail(1, idx, "dual-disagrees", (double)ey[i], y[i]);
        }
        if (jaos_exact_objective(m, &s) != JAOS_OK || !rational(s, &eobj))
            fail(1, idx, "objective-not-rational", 0.0, 0.0);
        else if (!agrees(eobj, obj))
            fail(1, idx, "objective-disagrees", (double)eobj, obj);
        snprintf(first[g.nc + g.nr], MAXSTR, "%s", s ? s : "");

        for (int64_t i = 0; i < g.nr; i++) {
            long double act = 0.0L;
            for (int64_t j = 0; j < g.nc; j++)
                for (int64_t p = g.ap[j]; p < g.ap[j + 1]; p++)
                    if (g.ai[p] == i) act += (long double)g.av[p] * ex[j];
            if ((isfinite(g.rl[i]) && act < (long double)g.rl[i] - 1e-9L) ||
                (isfinite(g.ru[i]) && act > (long double)g.ru[i] + 1e-9L))
                fail(3, idx, "exact-point-outside-row", (double)i, (double)act);
        }
        for (int64_t j = 0; j < g.nc; j++)
            if (ex[j] < (long double)g.cl[j] - 1e-9L || ex[j] > (long double)g.cu[j] + 1e-9L)
                fail(3, idx, "exact-point-outside-box", (double)j, (double)ex[j]);

        char path[4096], bad[4096];
        snprintf(path, sizeof path, "%s/m%" PRId64 ".proof", dir, idx);
        snprintf(bad, sizeof bad, "%s/m%" PRId64 ".bad", dir, idx);
        if (jaos_write_proof(m, path) != JAOS_OK) {
            fail(2, idx, "proof-not-written", 0.0, 0.0);
        } else {
            jaos_proof_report pr;
            if (jaos_check_proof(m, path, &pr) != JAOS_OK)
                fail(2, idx, "proof-not-checked", 0.0, 0.0);
            else if (!pr.primal || !pr.dual || !pr.objective ||
                     pr.kind != JAOS_PROOF_FILE_OPTIMAL)
                fail(2, idx, "proof-does-not-hold", pr.primal ? 1.0 : 0.0,
                     pr.dual ? 1.0 : 0.0);

            FILE *in = fopen(path, "r"), *out = fopen(bad, "w");
            if (in == nullptr || out == nullptr) abort();
            char line[MAXSTR * 2];
            bool changed = false;
            while (fgets(line, sizeof line, in) != nullptr) {
                if (!changed && strncmp(line, "col ", 4) == 0) {
                    char name[MAXSTR];
                    if (sscanf(line, "col %511s", name) == 1) {
                        fprintf(out, "col %s 12345/7\n", name);
                        changed = true;
                        continue;
                    }
                }
                fputs(line, out);
            }
            fclose(in);
            fclose(out);
            jaos_proof_report br;
            if (!changed)
                fail(5, idx, "no-col-line-to-corrupt", 0.0, 0.0);
            else if (jaos_check_proof(m, bad, &br) == JAOS_OK &&
                     br.primal && br.dual && br.objective)
                fail(5, idx, "corrupted-proof-holds", 0.0, 0.0);
            remove(path);
            remove(bad);
        }
        jaos_model_free(m);

        jaos_model *again = load(&g);
        if (again == nullptr) abort();
        if (jaos_solve(again) != JAOS_OK) abort();
        jaos_verify_report v2;
        if (jaos_status_of(again) != JAOS_SOLVE_OPTIMAL ||
            jaos_verify(again, &v2) != JAOS_OK || v2.status != JAOS_PROOF_OPTIMAL) {
            fail(4, idx, "second-verify-differs", 0.0, 0.0);
        } else {
            bool same = true;
            for (int64_t j = 0; j < g.nc && same; j++)
                same = jaos_exact_col_value(again, j, &s) == JAOS_OK && strcmp(s, first[j]) == 0;
            for (int64_t i = 0; i < g.nr && same; i++)
                same = jaos_exact_row_dual(again, i, &s) == JAOS_OK && strcmp(s, first[g.nc + i]) == 0;
            if (same)
                same = jaos_exact_objective(again, &s) == JAOS_OK && strcmp(s, first[g.nc + g.nr]) == 0;
            if (!same)
                fail(4, idx, "second-strings-differ", 0.0, 0.0);
        }
        jaos_model_free(again);
    }

    int64_t total = 0;
    for (int p = 1; p <= 6; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " proved %" PRId64
           " refused %" PRId64 " broken %" PRId64 " terms %" PRId64 "\n",
           runs, optimal, proved, refused, brokenv, terms);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " total=%" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6], total);
    return total == 0 ? 0 : 1;
}
