/* The reading behind 02-233: the presolve report over generated LPs.
 *
 * The generator is 02-229's: it plants what presolve removes, an empty
 * row, an empty column, a fixed column, a singleton row, a copy of another
 * row and a free column, and every row holds the zero point. On top of it
 * one column in three is a cost-0 singleton, so the cost-0 singleton
 * column fires too. Each model is solved twice.
 *
 * Six properties:
 *   P1 the report's reduced sizes are the sizes the presolve log line of
 *      the same solve prints, and when the log says nothing fired they
 *      are the model's own sizes with every count zero
 *   P2 the report's counts are the counts the log line prints
 *   P3 the accounting closes: the rows left are the rows less every row
 *      reduction, the columns left are the columns less every column
 *      reduction, where a free column singleton and an implied free
 *      column take one of each
 *   P4 the second solve, warm from the first, reports the same numbers
 *      field for field
 *   P5 nothing is negative and nothing left exceeds what was loaded
 *   P6 before any solve the report reads all zeros and JAOS_OK
 *
 * Usage: prestats RUNS SEED [DUMP_INDEX DUMP_PATH]
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs;
static uint64_t nx(void)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return rs;
}
static int64_t pick(int64_t n) { return (int64_t)(nx() % (uint64_t)n); }

#define MAXC 10
#define MAXR 8

typedef struct {
    int64_t nc, nr, nz;
    jaos_obj_sense sense;
    double offset;
    double cost[MAXC], cl[MAXC], cu[MAXC];
    double rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    double dense[MAXR][MAXC];
} model;

static void gen(model *g)
{
    memset(g, 0, sizeof *g);
    g->nc = 4 + pick(MAXC - 3);
    g->nr = 3 + pick(MAXR - 2);
    g->sense = pick(2) ? JAOS_MINIMIZE : JAOS_MAXIMIZE;
    g->offset = (double)(pick(9) - 4);

    const int64_t empty_row = pick(g->nr);
    const int64_t single_row = (empty_row + 1) % g->nr;
    const int64_t copy_row = (empty_row + 2) % g->nr;
    const int64_t empty_col = pick(g->nc);
    const int64_t fixed_col = (empty_col + 1) % g->nc;
    const int64_t free_col = (empty_col + 2) % g->nc;
    const int64_t single_col = (empty_col + 3) % g->nc;
    const bool single_col_costless = pick(3) == 0;

    for (int64_t j = 0; j < g->nc; j++) {
        g->cost[j] = (double)(pick(11) - 5);
        if (j == fixed_col) {
            const double v = (double)(pick(5) - 2);
            g->cl[j] = v; g->cu[j] = v;
        } else if (j == free_col) {
            g->cost[j] = 0.0;
            g->cl[j] = -INFINITY; g->cu[j] = INFINITY;
        } else {
            switch (pick(5)) {
            case 0: g->cl[j] = 0.0; g->cu[j] = (double)(1 + pick(4)); break;
            case 1: g->cl[j] = 0.0; g->cu[j] = INFINITY; break;
            case 2: g->cl[j] = -INFINITY; g->cu[j] = 0.0; break;
            default: g->cl[j] = -(double)(1 + pick(3));
                     g->cu[j] = (double)(1 + pick(3)); break;
            }
        }
        if (j == single_col && single_col_costless)
            g->cost[j] = 0.0;
    }

    for (int64_t i = 0; i < g->nr; i++)
        for (int64_t j = 0; j < g->nc; j++)
            g->dense[i][j] = 0.0;

    for (int64_t i = 0; i < g->nr; i++) {
        if (i == empty_row)
            continue;
        if (i == single_row) {
            g->dense[i][single_col] = (double)(1 + pick(3));
            continue;
        }
        if (i == copy_row)
            continue;
        for (int64_t j = 0; j < g->nc; j++) {
            if (j == empty_col)
                continue;
            if (j == single_col && single_col_costless)
                continue;
            if (pick(10) < 4)
                continue;
            const double v = (double)(pick(7) - 3);
            if (v == 0.0)
                continue;
            g->dense[i][j] = v;
        }
    }
    for (int64_t j = 0; j < g->nc; j++)
        g->dense[copy_row][j] = g->dense[(copy_row + 1) % g->nr][j];

    int64_t nz = 0;
    for (int64_t j = 0; j < g->nc; j++) {
        g->ap[j] = nz;
        for (int64_t i = 0; i < g->nr; i++) {
            if (g->dense[i][j] == 0.0)
                continue;
            g->ai[nz] = i; g->av[nz] = g->dense[i][j]; nz++;
        }
    }
    g->ap[g->nc] = nz;
    g->nz = nz;

    for (int64_t i = 0; i < g->nr; i++) {
        const double slack = fabs(g->dense[i][fixed_col] * g->cl[fixed_col]);
        const double b = (double)(1 + pick(12)) + slack;
        switch (pick(5)) {
        case 0: g->rl[i] = -INFINITY; g->ru[i] = b; break;
        case 1: g->rl[i] = -b; g->ru[i] = INFINITY; break;
        case 2: g->rl[i] = -b; g->ru[i] = b; break;
        case 3: g->rl[i] = -slack; g->ru[i] = slack; break;
        default: g->rl[i] = -b; g->ru[i] = slack;
        }
    }
    g->rl[copy_row] = g->rl[(copy_row + 1) % g->nr];
    g->ru[copy_row] = g->ru[(copy_row + 1) % g->nr];
}

static jaos_model *load(const model *g)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return nullptr;
    if (jaos_load_lp(m, g->nc, g->nr, g->sense, g->offset, g->cost, g->cl,
                     g->cu, g->rl, g->ru, g->nz, g->ap, g->ai,
                     g->av) != JAOS_OK) {
        jaos_model_free(m);
        return nullptr;
    }
    return m;
}

typedef struct {
    char line[512];
    bool have;
} logcap;

static void on_log(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    logcap *c = user;
    if (strncmp(line, "presolve:", 9) == 0) {
        snprintf(c->line, sizeof c->line, "%s", line);
        c->have = true;
    }
}

static int64_t field(const char *line, const char *key)
{
    const char *p = strstr(line, key);
    if (p == nullptr)
        return -1;
    return atoll(p + strlen(key));
}

static int64_t broke[8];

static void fail(int p, int64_t idx, const char *what, int64_t a, int64_t b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %" PRId64 " %" PRId64 "\n", p, idx, what,
               a, b);
}

static bool same_report(const jaos_presolve_report *a,
                        const jaos_presolve_report *b)
{
    return memcmp(a, b, sizeof *a) == 0;
}

int main(int argc, char **argv)
{
    const int64_t runs = argc > 1 ? atoll(argv[1]) : 1000;
    const uint64_t seed = argc > 2 ? (uint64_t)atoll(argv[2]) : 1;
    const int64_t dump = argc > 4 ? atoll(argv[3]) : -1;
    rs = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)nx();

    int64_t reduced = 0, nothing = 0, solved_by_presolve = 0, refused = 0;
    int64_t cost0 = 0;
    for (int64_t idx = 0; idx < runs; idx++) {
        model g;
        gen(&g);
        if (idx == dump) {
            jaos_model *d = load(&g);
            if (d == nullptr || jaos_write_mps(d, argv[4]) != JAOS_OK)
                abort();
            jaos_model_free(d);
        }
        jaos_model *m = load(&g);
        if (m == nullptr) abort();

        jaos_presolve_report before;
        if (jaos_presolve_result(m, &before) != JAOS_OK) {
            fail(6, idx, "before-solve-not-ok", 0, 0);
        } else {
            jaos_presolve_report zero;
            memset(&zero, 0, sizeof zero);
            if (!same_report(&before, &zero))
                fail(6, idx, "before-solve-not-zero", before.num_row,
                     before.rounds);
        }

        logcap cap = {0};
        if (jaos_set_log_callback(m, on_log, &cap) != JAOS_OK) abort();
        if (jaos_set_log_level(m, JAOS_LOG_SUMMARY) != JAOS_OK) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        const jaos_solve_status st = jaos_status_of(m);
        jaos_presolve_report r;
        if (jaos_presolve_result(m, &r) != JAOS_OK) abort();

        if (!cap.have) {
            fail(1, idx, "no-presolve-log-line", (int64_t)st, 0);
        } else if (strstr(cap.line, "nothing fired") != nullptr) {
            nothing++;
            if (r.num_row != g.nr || r.num_col != g.nc || r.num_nz != g.nz)
                fail(1, idx, "nothing-fired-sizes", r.num_row, g.nr);
            if (r.fixed_col || r.empty_row || r.empty_col ||
                r.singleton_row || r.singleton_col ||
                r.free_col_singleton || r.forcing_row || r.redundant_row ||
                r.implied_free_col)
                fail(1, idx, "nothing-fired-counts", r.fixed_col,
                     r.singleton_col);
        } else if (strstr(cap.line, "no simplex run") != nullptr) {
            refused++;
            if (r.empty_row != field(cap.line, "empty_row=") ||
                r.empty_col != field(cap.line, "empty_col=") ||
                r.singleton_row != field(cap.line, "singleton_row=") ||
                r.singleton_col != field(cap.line, "singleton_col=") ||
                r.free_col_singleton !=
                    field(cap.line, "free_col_singleton=") ||
                r.rounds != field(cap.line, "rounds="))
                fail(2, idx, "refused-counts", r.singleton_col,
                     field(cap.line, "singleton_col="));
        } else {
            int64_t lr, lc, lz, rr, rc, rz;
            if (sscanf(cap.line,
                       "presolve: %" SCNd64 " rows, %" SCNd64 " columns, %"
                       SCNd64 " nonzeros -> %" SCNd64 " rows, %" SCNd64
                       " columns, %" SCNd64 " nonzeros;",
                       &lr, &lc, &lz, &rr, &rc, &rz) != 6) {
                fail(1, idx, "unparsed-log-line", 0, 0);
            } else {
                if (lr != g.nr || lc != g.nc || lz != g.nz)
                    fail(1, idx, "log-loaded-sizes", lr, g.nr);
                if (r.num_row != rr || r.num_col != rc || r.num_nz != rz)
                    fail(1, idx, "reduced-sizes", r.num_row, rr);
                if (rc == 0) solved_by_presolve++; else reduced++;
            }
            if (r.fixed_col != field(cap.line, "fixed_col=") ||
                r.empty_row != field(cap.line, "empty_row=") ||
                r.empty_col != field(cap.line, "empty_col=") ||
                r.singleton_row != field(cap.line, "singleton_row=") ||
                r.singleton_col != field(cap.line, "singleton_col=") ||
                r.free_col_singleton !=
                    field(cap.line, "free_col_singleton=") ||
                r.rounds != field(cap.line, "rounds="))
                fail(2, idx, "counts", r.singleton_col,
                     field(cap.line, "singleton_col="));
        }
        if (r.singleton_col > 0) cost0++;

        if (strstr(cap.line, "no simplex run") == nullptr) {
            const int64_t rows_gone = r.empty_row + r.singleton_row +
                                      r.forcing_row + r.redundant_row +
                                      r.free_col_singleton +
                                      r.implied_free_col + r.duplicate_row;
            const int64_t cols_gone = r.fixed_col + r.empty_col +
                                      r.singleton_col +
                                      r.free_col_singleton +
                                      r.implied_free_col + r.duplicate_col +
                                      r.dominated_col;
            const bool off = g.nr - rows_gone != r.num_row ||
                             g.nc - cols_gone != r.num_col;
            if (off && broke[3] < 3)
                printf("  m%" PRId64 " %s\n    rows %" PRId64 " cols %" PRId64
                       " nz %" PRId64 " fixed_col=%" PRId64 " empty_row=%"
                       PRId64 " empty_col=%" PRId64 " singleton_row=%" PRId64
                       " singleton_col=%" PRId64 " free_col_singleton=%"
                       PRId64 " forcing_row=%" PRId64 " redundant_row=%"
                       PRId64 " implied_free_col=%" PRId64
                       " tightened_bound=%" PRId64 " rounds=%" PRId64 "\n",
                       idx, cap.line, g.nr, g.nc, g.nz, r.fixed_col,
                       r.empty_row, r.empty_col, r.singleton_row,
                       r.singleton_col, r.free_col_singleton, r.forcing_row,
                       r.redundant_row, r.implied_free_col,
                       r.tightened_bound, r.rounds);
            if (g.nr - rows_gone != r.num_row)
                fail(3, idx, "rows-accounting", g.nr - rows_gone, r.num_row);
            if (g.nc - cols_gone != r.num_col)
                fail(3, idx, "cols-accounting", g.nc - cols_gone, r.num_col);
        }

        if (r.num_row < 0 || r.num_col < 0 || r.num_nz < 0 ||
            r.num_row > g.nr || r.num_col > g.nc || r.num_nz > g.nz ||
            r.rounds < 0 || r.fixed_col < 0 || r.empty_row < 0 ||
            r.empty_col < 0 || r.singleton_row < 0 || r.singleton_col < 0 ||
            r.free_col_singleton < 0 || r.forcing_row < 0 ||
            r.redundant_row < 0 || r.implied_free_col < 0 ||
            r.tightened_bound < 0)
            fail(5, idx, "range", r.num_row, r.num_nz);

        if (jaos_solve(m) != JAOS_OK) abort();
        jaos_presolve_report r2;
        if (jaos_presolve_result(m, &r2) != JAOS_OK) abort();
        if (!same_report(&r, &r2))
            fail(4, idx, "second-solve-differs", r.num_row, r2.num_row);
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int p = 1; p <= 6; p++) total += broke[p];
    printf("models %" PRId64 " reduced %" PRId64 " solved-by-presolve %"
           PRId64 " nothing-fired %" PRId64 " refused %" PRId64
           " with-cost0-singleton %" PRId64 "\n", runs, reduced,
           solved_by_presolve, nothing, refused, cost0);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " total=%" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6], total);
    return total == 0 ? 0 : 1;
}
