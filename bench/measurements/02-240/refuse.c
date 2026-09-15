/* The reading behind 02-240: a file with one bad line is refused, and the
 * message names that line.
 *
 * The generator is 02-237's with a third of the models carrying integer
 * marks, so the MPS files carry MARKER lines and the LP files a General
 * section. Every model is written as MPS and as LP; a copy of each gets
 * one defect line inserted (or one line replaced) at a position chosen
 * inside the section the defect belongs to, and the reader has to refuse
 * the copy with `line N:` in its message, N being the line the harness
 * put the defect on.
 *
 * MPS defects: a section the format has not got, in column 1 (D1); an
 * entry naming a row the file has not declared (D2); an RHS entry on an
 * unknown row (D3); a bound on an unknown column (D4); an unknown bound
 * type (D5); a value that is not a number (D6); an unknown row type (D7).
 * LP defects: content after End (L1); a bound on a variable no
 * constraint uses (L2); a right-hand side that is not a number (L3); a
 * constraint without any term (L4); `Subject To` misspelt (L5); a name in
 * the General section that is not a variable (L6).
 *
 * Properties:
 *   P1 the untouched files read back
 *   P2 every corrupted file is refused
 *   P3 the message names the line the defect is on
 *
 * Usage: refuse RUNS SEED DIR [EVERY]
 *   Every EVERY-th model leaves one corrupted MPS and one corrupted LP in
 *   DIR with a .expect holding the line, for the CLI and gzip runs in
 *   refuse.sh.
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

#define MAXLINES 2048
#define LINELEN 256

typedef struct {
    int64_t n;
    char line[MAXLINES][LINELEN];
} text;

static void slurp(const char *path, text *t)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr) abort();
    t->n = 0;
    while (t->n < MAXLINES && fgets(t->line[t->n], LINELEN, f) != nullptr) {
        char *nl = strchr(t->line[t->n], '\n');
        if (nl != nullptr) *nl = '\0';
        t->n++;
    }
    fclose(f);
    if (t->n >= MAXLINES) abort();
}

/* Writes t with `what` inserted so that it lands on line `at` (1-based);
 * when `replace` the line at `at` is overwritten instead. */
static void spill(const char *path, const text *t, int64_t at,
                  const char *what, bool replace)
{
    FILE *f = fopen(path, "w");
    if (f == nullptr) abort();
    for (int64_t k = 0; k < t->n; k++) {
        if (k + 1 == at) {
            fprintf(f, "%s\n", what);
            if (replace) continue;
        }
        fprintf(f, "%s\n", t->line[k]);
    }
    if (at == t->n + 1) fprintf(f, "%s\n", what);
    fclose(f);
}

/* 1-based line of the first line starting with `key` at column 1, or 0. */
static int64_t find(const text *t, const char *key, int64_t from)
{
    for (int64_t k = from - 1; k < t->n; k++)
        if (strncmp(t->line[k], key, strlen(key)) == 0) return k + 1;
    return 0;
}

static int64_t broke[4], refused_at, refused_elsewhere, accepted;
static int64_t by_kind[16];

static void fail(int p, int64_t idx, const char *what, int64_t a, int64_t b)
{
    broke[p]++;
    if (broke[p] <= 6)
        printf("P%d m%" PRId64 " %s %" PRId64 " %" PRId64 "\n", p, idx, what, a, b);
}

/* Reads the corrupted file and judges the message. */
static void judge(jaos_model *m, const char *path, bool is_lp, int64_t at,
                  const char *kind, int64_t idx, int kidx)
{
    const jaos_status st = is_lp ? jaos_read_lp(m, path) : jaos_read_mps(m, path);
    if (st == JAOS_OK) {
        accepted++;
        fail(2, idx, kind, at, 0);
        return;
    }
    const char *msg = jaos_model_error(m);
    const char *p = strstr(msg, "line ");
    int64_t n = -1;
    if (p != nullptr) n = strtoll(p + 5, nullptr, 10);
    if (n == at) {
        refused_at++;
        by_kind[kidx]++;
    } else {
        refused_elsewhere++;
        fail(3, idx, kind, at, n);
        if (broke[3] <= 6) printf("   %s\n", msg);
    }
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: refuse RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    static text t;
    int64_t files = 0, corrupted = 0, dumped = 0;
    char mps[4096], lp[4096], bad[4096], exp[4096];

    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        snprintf(mps, sizeof mps, "%s/m%" PRId64 ".mps", dir, idx);
        snprintf(lp, sizeof lp, "%s/m%" PRId64 ".lp", dir, idx);
        if (jaos_write_mps(m, mps) != JAOS_OK || jaos_write_lp(m, lp) != JAOS_OK) abort();
        jaos_model_free(m);
        files += 2;

        jaos_model *r = nullptr;
        if (jaos_model_new(&r) != JAOS_OK) abort();
        if (jaos_read_mps(r, mps) != JAOS_OK) fail(1, idx, "mps-not-read", 0, 0);
        jaos_model_free(r);
        if (jaos_model_new(&r) != JAOS_OK) abort();
        if (jaos_read_lp(r, lp) != JAOS_OK) fail(1, idx, "lp-not-read", 0, 0);
        jaos_model_free(r);
        if (jaos_model_new(&r) != JAOS_OK) abort();

        const bool dump = every > 0 && idx % every == 0;
        int64_t keep_mps = -1, keep_lp = -1;
        (void)keep_mps;
        (void)keep_lp;

        slurp(mps, &t);
        const int64_t rows = find(&t, "ROWS", 1), cols = find(&t, "COLUMNS", 1),
                      rhs = find(&t, "RHS", 1), bnds = find(&t, "BOUNDS", 1),
                      end = find(&t, "ENDATA", 1);
        int64_t next_after_cols = t.n, next_after_rhs = t.n, next_after_bnds = t.n;
        for (int64_t k = cols + 1; k <= t.n; k++)
            if (t.line[k - 1][0] != ' ') { next_after_cols = k; break; }
        if (rhs > 0)
            for (int64_t k = rhs + 1; k <= t.n; k++)
                if (t.line[k - 1][0] != ' ') { next_after_rhs = k; break; }
        if (bnds > 0)
            for (int64_t k = bnds + 1; k <= t.n; k++)
                if (t.line[k - 1][0] != ' ') { next_after_bnds = k; break; }
        if (rows <= 0 || cols <= 0 || end <= 0 || bnds <= 0) abort();
        snprintf(bad, sizeof bad, "%s/m%" PRId64 ".bad.mps", dir, idx);
        struct { const char *kind; const char *what; int64_t lo, hi; } md[] = {
            {"D1-section", "FOO", 2, end},
            {"D2-unknown-row", "    X1        NOSUCHROW  1", cols + 1, next_after_cols},
            {"D3-rhs-unknown-row", "    RHS       NOSUCHROW  1", rhs + 1, next_after_rhs},
            {"D4-bound-unknown-col", " UP BND       NOSUCHCOL  1", bnds + 1, next_after_bnds},
            {"D5-bound-type", " ZZ BND       X1         1", bnds + 1, next_after_bnds},
            {"D6-bad-number", "    X1        R1         12abc", cols + 1, next_after_cols},
            {"D7-row-type", " Q  FOO", rows + 1, cols},
        };
        int keep_k = (int)ri(0, 6);
        if (keep_k == 2 && rhs <= 0) keep_k = 0;
        for (int k = 0; k < 7; k++) {
            if (k == 2 && rhs <= 0) continue;
            const int64_t at = ri(md[k].lo, md[k].hi);
            spill(bad, &t, at, md[k].what, false);
            judge(r, bad, false, at, md[k].kind, idx, k);
            corrupted++;
            if (dump && k == keep_k) {
                keep_mps = at;
                snprintf(exp, sizeof exp, "%s/m%" PRId64 ".bad.mps.expect", dir, idx);
                FILE *f = fopen(exp, "w");
                if (f == nullptr) abort();
                fprintf(f, "%" PRId64 "\n", at);
                fclose(f);
                snprintf(exp, sizeof exp, "%s/m%" PRId64 ".keep.mps", dir, idx);
                spill(exp, &t, at, md[k].what, false);
            }
        }
        remove(bad);

        slurp(lp, &t);
        const int64_t subj = find(&t, "Subject To", 1), lbnd = find(&t, "Bounds", 1),
                      gen_ = find(&t, "General", 1), lend = find(&t, "End", 1);
        if (subj <= 0 || lbnd <= 0 || lend <= 0) abort();
        int64_t bnd_end = lend;
        for (int64_t k = lbnd + 1; k <= t.n; k++)
            if (t.line[k - 1][0] != ' ' && t.line[k - 1][0] != '\0') { bnd_end = k; break; }
        int64_t gen_end = lend;
        if (gen_ > 0)
            for (int64_t k = gen_ + 1; k <= t.n; k++)
                if (t.line[k - 1][0] != ' ' && t.line[k - 1][0] != '\0') { gen_end = k; break; }
        snprintf(bad, sizeof bad, "%s/m%" PRId64 ".bad.lp", dir, idx);
        struct { const char *kind; const char *what; int64_t lo, hi; bool replace; } ld[] = {
            {"L1-after-end", "junk", lend + 1, t.n + 1, false},
            {"L2-bound-unknown", " nosuchvar >= 1", lbnd + 1, bnd_end, false},
            {"L3-bad-rhs", " cbad: 3 X1 >= abc", lbnd, lbnd, false},
            {"L4-no-term", " cempty: >= 4", lbnd, lbnd, false},
            {"L5-subject", "Subjekt To", subj, subj, true},
            {"L6-general-unknown", " nosuchvar", gen_ + 1, gen_end, false},
        };
        keep_k = (int)ri(0, 5);
        if (keep_k == 5 && gen_ <= 0) keep_k = 0;
        for (int k = 0; k < 6; k++) {
            if (k == 5 && gen_ <= 0) continue;
            const int64_t at = ld[k].lo == ld[k].hi ? ld[k].lo : ri(ld[k].lo, ld[k].hi);
            spill(bad, &t, at, ld[k].what, ld[k].replace);
            judge(r, bad, true, at, ld[k].kind, idx, 8 + k);
            corrupted++;
            if (dump && k == keep_k) {
                keep_lp = at;
                snprintf(exp, sizeof exp, "%s/m%" PRId64 ".bad.lp.expect", dir, idx);
                FILE *f = fopen(exp, "w");
                if (f == nullptr) abort();
                fprintf(f, "%" PRId64 "\n", at);
                fclose(f);
                snprintf(exp, sizeof exp, "%s/m%" PRId64 ".keep.lp", dir, idx);
                spill(exp, &t, at, ld[k].what, ld[k].replace);
            }
        }
        remove(bad);
        jaos_model_free(r);
        if (dump) dumped++;
        remove(mps);
        remove(lp);
    }

    const int64_t total = broke[1] + broke[2] + broke[3];
    printf("models %" PRId64 " files %" PRId64 " corrupted %" PRId64
           " refused_at_line %" PRId64 " refused_elsewhere %" PRId64
           " accepted %" PRId64 " dumped %" PRId64 "\n",
           runs, files, corrupted, refused_at, refused_elsewhere, accepted, dumped);
    printf("by_kind D1=%" PRId64 " D2=%" PRId64 " D3=%" PRId64 " D4=%" PRId64
           " D5=%" PRId64 " D6=%" PRId64 " D7=%" PRId64 " L1=%" PRId64
           " L2=%" PRId64 " L3=%" PRId64 " L4=%" PRId64 " L5=%" PRId64
           " L6=%" PRId64 "\n", by_kind[0], by_kind[1], by_kind[2], by_kind[3],
           by_kind[4], by_kind[5], by_kind[6], by_kind[8], by_kind[9],
           by_kind[10], by_kind[11], by_kind[12], by_kind[13]);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " total=%" PRId64 "\n",
           broke[1], broke[2], broke[3], total);
    return total == 0 ? 0 : 1;
}
