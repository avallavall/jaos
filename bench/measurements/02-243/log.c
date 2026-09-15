/* The reading behind 02-243: logging with levels.
 *
 * The generator is 02-237's with a third of the models carrying integer
 * marks. Every model is solved once with no callback for the reference,
 * then once per level with a callback that records every line and the
 * level it came with, and once more at `detail`.
 *
 * Properties:
 *   P1 at `off` the callback is never called; with no callback set, any
 *      level solves in silence
 *   P2 every line arrives with a level of `summary`, `progress` or
 *      `detail`, at or below the level set; the lines at `summary` are a
 *      subsequence of the lines at `progress`, and those of the lines at
 *      `detail`
 *   P3 the answer with logging on, at any level, is the answer without it
 *      to the bit: status, objective, point, work, iterations
 *   P4 a second solve at `detail` gives the same lines, byte for byte
 *   P5 a level outside the four is refused
 *   P6 no line holds a newline, and none reaches the 1023 bytes the
 *      buffer allows
 *
 * Usage: log RUNS SEED DIR [EVERY]
 *   Every EVERY-th model is written to DIR as m<idx>.mps for the CLI run
 *   in log.sh.
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
        g->cu[j] = (double)ri(1, 10);
        g->integer[j] = mip && ri(0, 1) == 1;
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
    for (int64_t j = 0; j < g->nc; j++)
        if (g->integer[j] && jaos_set_col_integer(m, j, true) != JAOS_OK) abort();
    return m;
}

typedef struct {
    jaos_solve_status status;
    double obj, x[MAXC];
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
                   jaos_solution(m, o->x, nullptr, nullptr, nullptr) == JAOS_OK;
}

static bool same_outcome(const outcome *a, const outcome *b, int64_t nc)
{
    if (a->status != b->status || a->work != b->work || a->iters != b->iters ||
        a->has_point != b->has_point) return false;
    if (!a->has_point) return true;
    return a->obj == b->obj && memcmp(a->x, b->x, (size_t)nc * sizeof a->x[0]) == 0;
}

typedef struct {
    char **line;
    int *level;
    int64_t n, cap;
    bool bad_level, newline, long_line;
    int64_t max_len;
} record;

static void on_line(void *user, jaos_log_level level, const char *line)
{
    record *r = user;
    if (r->n == r->cap) {
        r->cap = r->cap ? 2 * r->cap : 256;
        r->line = realloc(r->line, (size_t)r->cap * sizeof *r->line);
        r->level = realloc(r->level, (size_t)r->cap * sizeof *r->level);
        if (r->line == nullptr || r->level == nullptr) abort();
    }
    r->line[r->n] = strdup(line);
    r->level[r->n] = (int)level;
    if (r->line[r->n] == nullptr) abort();
    r->n++;
    if (level < JAOS_LOG_SUMMARY || level > JAOS_LOG_DETAIL) r->bad_level = true;
    if (strchr(line, '\n') != nullptr) r->newline = true;
    const int64_t len = (int64_t)strlen(line);
    if (len > r->max_len) r->max_len = len;
    if (len >= 1023) r->long_line = true;
}

static void record_free(record *r)
{
    for (int64_t k = 0; k < r->n; k++) free(r->line[k]);
    free(r->line);
    free(r->level);
    memset(r, 0, sizeof *r);
}

static bool subsequence(const record *small, const record *big)
{
    int64_t j = 0;
    for (int64_t i = 0; i < small->n; i++) {
        while (j < big->n && strcmp(small->line[i], big->line[j]) != 0) j++;
        if (j == big->n) return false;
        j++;
    }
    return true;
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
        fprintf(stderr, "usage: log RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, mips = 0, lines[4] = {0}, max_len = 0, dumped = 0;
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        bool mip = false;
        for (int64_t j = 0; j < g.nc; j++) mip |= g.integer[j];
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        outcome ref;
        take(m, &ref);
        if (ref.status == JAOS_SOLVE_OPTIMAL) optimal++;
        mips += mip;

        if (jaos_set_log_level(m, (jaos_log_level)7) == JAOS_OK) fail(5, idx, "level-7-accepted", 0.0, 0.0);
        if (jaos_set_log_level(m, (jaos_log_level)-1) == JAOS_OK) fail(5, idx, "level-minus-one-accepted", 0.0, 0.0);
        jaos_model_free(m);
        m = load(&g);
        if (m == nullptr) abort();
        if (jaos_set_log_level(m, JAOS_LOG_DETAIL) != JAOS_OK) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        outcome silent;
        take(m, &silent);
        if (!same_outcome(&silent, &ref, g.nc)) fail(1, idx, "detail-without-callback-changes-answer", 0.0, 0.0);
        jaos_model_free(m);

        record rec[4];
        memset(rec, 0, sizeof rec);
        for (int lv = 0; lv < 4; lv++) {
            jaos_model *p = load(&g);
            if (p == nullptr) abort();
            if (jaos_set_log_callback(p, on_line, &rec[lv]) != JAOS_OK) abort();
            if (jaos_set_log_level(p, (jaos_log_level)lv) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            outcome o;
            take(p, &o);
            if (!same_outcome(&o, &ref, g.nc)) fail(3, idx, "logging-changes-answer", (double)lv, 0.0);
            jaos_model_free(p);
            lines[lv] += rec[lv].n;
            if (rec[lv].max_len > max_len) max_len = rec[lv].max_len;
            if (lv == 0 && rec[lv].n > 0) fail(1, idx, "lines-at-off", (double)rec[lv].n, 0.0);
            if (rec[lv].bad_level) fail(2, idx, "line-with-bad-level", (double)lv, 0.0);
            for (int64_t k = 0; k < rec[lv].n; k++)
                if (rec[lv].level[k] > lv) { fail(2, idx, "line-above-set-level", (double)lv, (double)rec[lv].level[k]); break; }
            if (rec[lv].newline) fail(6, idx, "line-holds-newline", (double)lv, 0.0);
            if (rec[lv].long_line) fail(6, idx, "line-at-the-buffer", (double)lv, (double)rec[lv].max_len);
        }
        if (!subsequence(&rec[1], &rec[2])) fail(2, idx, "summary-not-in-progress", (double)rec[1].n, (double)rec[2].n);
        if (!subsequence(&rec[2], &rec[3])) fail(2, idx, "progress-not-in-detail", (double)rec[2].n, (double)rec[3].n);
        {
            record again = {0};
            jaos_model *p = load(&g);
            if (p == nullptr) abort();
            if (jaos_set_log_callback(p, on_line, &again) != JAOS_OK) abort();
            if (jaos_set_log_level(p, JAOS_LOG_DETAIL) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            jaos_model_free(p);
            bool same = again.n == rec[3].n;
            for (int64_t k = 0; same && k < again.n; k++)
                same = strcmp(again.line[k], rec[3].line[k]) == 0 && again.level[k] == rec[3].level[k];
            if (!same) fail(4, idx, "second-detail-log-differs", (double)again.n, (double)rec[3].n);
            record_free(&again);
        }
        for (int lv = 0; lv < 4; lv++) record_free(&rec[lv]);

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
    for (int p = 1; p <= 6; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " mips %" PRId64
           " lines off %" PRId64 " summary %" PRId64 " progress %" PRId64
           " detail %" PRId64 " longest %" PRId64 " dumped %" PRId64 "\n",
           runs, optimal, mips, lines[0], lines[1], lines[2], lines[3],
           max_len, dumped);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " total=%" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6], total);
    return total == 0 ? 0 : 1;
}
