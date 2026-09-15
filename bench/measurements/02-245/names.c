/* The reading behind 02-245: row, column and objective names.
 *
 * The generator is 02-237's, LPs only so that the `.nl` writer keeps the
 * column order. Every column, row, the objective and the model get a
 * name drawn from eleven kinds: plain letters; letters with digits, `_`
 * and `.`; 255 bytes long; starting with a digit; holding `:`, `[`, `*`
 * or `^`; an LP keyword; CPLEX's symbols; XML's `<`, `&`, `"` and `'`;
 * UTF-8 letters; the positional spelling of another index (`C3` on
 * column 7); a single letter.
 *
 * Properties:
 *   P1 every name the setter takes reads back byte for byte through the
 *      getter and resolves to its index by lookup; a name with a space,
 *      a control character or 256 bytes is refused; after a rename the
 *      old name no longer resolves
 *   P2 MPS carries every name: the file read back has every column,
 *      row, objective and model name equal
 *   P3 LP carries the names its scanner can read back as one token and
 *      spells the others `c<j+1>`, `r<i+1>` or `obj`, each with a
 *      comment line `\ column c<j+1> was <name>` at the top; the file
 *      read back has exactly those names
 *   P4 `.nl` with its `.col` and `.row` carries every name
 *   P5 OSiL carries every name, XML's own characters escaped and read
 *      back
 *   P6 QPLIB carries every name
 *   P7 a model with no names is spelled `C<j+1>` and `R<i+1>` and those
 *      spellings resolve by lookup
 *   P8 two columns of one name are refused by every writer
 *
 * Usage: names RUNS SEED DIR [EVERY]
 *   Every EVERY-th model is written to DIR as m<idx>.mps with m<idx>.long
 *   holding a 255-byte column name, for the CLI run in names.sh.
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
#define NB (MAXC + MAXR + 2)
#define NAMEBUF 300

typedef struct {
    int64_t nr, nc, nz;
    double cost[MAXC], cl[MAXC], cu[MAXC], rl[MAXR], ru[MAXR];
    int64_t ap[MAXC + 1], ai[MAXC * MAXR];
    double av[MAXC * MAXR];
    bool maximise;
    double offset;
    char name[NB][NAMEBUF];
    int kind[NB];
} gen;

static const char *const KEYWORDS[] = {"free", "end", "st", "min", "max", "inf",
    "subject", "bounds", "general", "binary", "integer", "semi", "infinity"};

static void draw_name(char *out, int kind, int64_t k)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const int64_t nl = (int64_t)strlen(letters);
    char body[8];
    for (int t = 0; t < 4; t++) body[t] = letters[ri(0, nl - 1)];
    body[4] = '\0';
    switch (kind) {
    case 0: snprintf(out, NAMEBUF, "%s%" PRId64, body, k); break;
    case 1: snprintf(out, NAMEBUF, "%s_%" PRId64 ".%c", body, k, letters[ri(0, nl - 1)]); break;
    case 2:
        for (int t = 0; t < 255; t++) out[t] = letters[(k * 7 + t) % nl];
        out[255] = '\0';
        break;
    case 3: snprintf(out, NAMEBUF, "%" PRId64 "%s", k, body); break;
    case 4: snprintf(out, NAMEBUF, "%s%c%" PRId64, body, ":[*^"[ri(0, 3)], k); break;
    case 5: snprintf(out, NAMEBUF, "%s", KEYWORDS[ri(0, 12)]); break;
    case 6: snprintf(out, NAMEBUF, "%c%s%c%" PRId64, "!#$%&(){}|~"[ri(0, 10)], body, "?@/,;"[ri(0, 4)], k); break;
    case 7: snprintf(out, NAMEBUF, "%s%c%" PRId64, body, "<&\"'"[ri(0, 3)], k); break;
    case 8: snprintf(out, NAMEBUF, "%s\xc3\xa9\xce\xbb%" PRId64, body, k); break;
    case 9: snprintf(out, NAMEBUF, "%c%" PRId64, ri(0, 1) ? 'C' : 'R', ri(1, 24)); break;
    default: snprintf(out, NAMEBUF, "%c", letters[ri(0, nl - 1)]); break;
    }
}

static bool unique_among(const gen *g, int64_t upto, const char *name)
{
    for (int64_t t = 0; t < upto; t++)
        if (strcmp(g->name[t], name) == 0) return false;
    return true;
}

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
    /* names: columns 0..nc-1, rows nc..nc+nr-1, objective, model; every
     * name distinct across the whole model, and no name that is the
     * positional spelling of a column or row of this model, which the
     * writers rightly refuse as a pair */
    const int64_t total = g->nc + g->nr + 2;
    for (int64_t k = 0; k < total; k++) {
        for (int tries = 0; tries < 64; tries++) {
            const int kind = (int)ri(0, 10);
            char cand[NAMEBUF];
            draw_name(cand, kind, k);
            if (kind == 9) {
                const int64_t n = atoll(cand + 1);
                if ((cand[0] == 'C' && n <= g->nc) || (cand[0] == 'R' && n <= g->nr)) continue;
            }
            if (!unique_among(g, k, cand)) continue;
            memcpy(g->name[k], cand, NAMEBUF);
            g->kind[k] = kind;
            break;
        }
    }
}

static jaos_model *load(const gen *g, bool named)
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
    if (!named) return m;
    for (int64_t j = 0; j < g->nc; j++)
        if (jaos_set_col_name(m, j, g->name[j]) != JAOS_OK) abort();
    for (int64_t i = 0; i < g->nr; i++)
        if (jaos_set_row_name(m, i, g->name[g->nc + i]) != JAOS_OK) abort();
    if (jaos_set_objective_name(m, g->name[g->nc + g->nr]) != JAOS_OK) abort();
    if (jaos_set_model_name(m, g->name[g->nc + g->nr + 1]) != JAOS_OK) abort();
    return m;
}

static int64_t broke[10];

static void fail(int p, int64_t idx, const char *what, const char *a, const char *b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s '%s' '%s'\n", p, idx, what, a, b);
}

/* Reads every name of m into out[NB]. */
static void names_of(const jaos_model *m, const gen *g, char out[][NAMEBUF])
{
    for (int64_t j = 0; j < g->nc; j++)
        if (jaos_col_name(m, j, out[j], NAMEBUF) != JAOS_OK) abort();
    for (int64_t i = 0; i < g->nr; i++)
        if (jaos_row_name(m, i, out[g->nc + i], NAMEBUF) != JAOS_OK) abort();
    if (jaos_objective_name(m, out[g->nc + g->nr], NAMEBUF) != JAOS_OK) abort();
    if (jaos_model_name(m, out[g->nc + g->nr + 1], NAMEBUF) != JAOS_OK) out[g->nc + g->nr + 1][0] = '\0';
}

typedef jaos_status (*writer)(jaos_model *, const char *);
typedef jaos_status (*reader)(jaos_model *, const char *);

/* P2, P4, P5, P6: write, read back, compare every name (model name
 * excepted for the formats that have no place for it). */
static void round_trip(const gen *g, jaos_model *m, const char *path,
                       writer w, reader r, int p, const char *tag, int64_t idx,
                       bool model_name, int64_t *ok, int64_t *refused)
{
    if (w(m, path) != JAOS_OK) {
        (*refused)++;
        char what[64];
        snprintf(what, sizeof what, "%s-writer-refused", tag);
        fail(p, idx, what, jaos_model_error(m), "");
        return;
    }
    jaos_model *b = nullptr;
    if (jaos_model_new(&b) != JAOS_OK) abort();
    if (r(b, path) != JAOS_OK) {
        char what[64];
        snprintf(what, sizeof what, "%s-reader-refused", tag);
        fail(p, idx, what, jaos_model_error(b), "");
        jaos_model_free(b);
        return;
    }
    if (jaos_num_col(b) != g->nc || jaos_num_row(b) != g->nr) {
        fail(p, idx, "shape-differs", "", "");
        jaos_model_free(b);
        return;
    }
    static char got[NB][NAMEBUF];
    names_of(b, g, got);
    const int64_t upto = g->nc + g->nr + (model_name ? 2 : strcmp(tag, "qplib") == 0 ? 0 : 1);
    bool same = true;
    for (int64_t k = 0; k < upto && same; k++)
        if (strcmp(got[k], g->name[k]) != 0) {
            char what[64];
            snprintf(what, sizeof what, "%s-name-differs-at-%" PRId64, tag, k);
            fail(p, idx, what, g->name[k], got[k]);
            same = false;
        }
    if (same) (*ok)++;
    jaos_model_free(b);
}

static bool lp_spellable(const char *s)
{
    static const char start[] = "!\"#$%&()/,;?@`'{}|~_";
    const unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z') || strchr(start, (char)c0) != nullptr))
        return false;
    for (const char *p = s; *p; p++) {
        const unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || strchr(start, (char)c) != nullptr))
            return false;
    }
    for (int k = 0; k < 13; k++)
        if (strcmp(s, KEYWORDS[k]) == 0) return false;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: names RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t names = 0, kinds[11] = {0}, mps_ok = 0, lp_ok = 0, lp_mapped = 0,
            nl_ok = 0, osil_ok = 0, qplib_ok = 0, qplib_refused = 0,
            refused[5] = {0}, dumped = 0, dup_refused = 0;
    char path[4096];
    static char got[NB][NAMEBUF];
    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        const int64_t total = g.nc + g.nr + 2;
        names += total;
        for (int64_t k = 0; k < total; k++) kinds[g.kind[k]]++;
        jaos_model *m = load(&g, true);
        if (m == nullptr) abort();

        /* P1 */
        names_of(m, &g, got);
        for (int64_t k = 0; k < total; k++)
            if (strcmp(got[k], g.name[k]) != 0) { fail(1, idx, "getter-differs", g.name[k], got[k]); break; }
        for (int64_t j = 0; j < g.nc; j++) {
            int64_t at = -1;
            if (jaos_col_index(m, g.name[j], &at) != JAOS_OK || at != j) { fail(1, idx, "col-lookup-fails", g.name[j], ""); break; }
        }
        for (int64_t i = 0; i < g.nr; i++) {
            int64_t at = -1;
            if (jaos_row_index(m, g.name[g.nc + i], &at) != JAOS_OK || at != i) { fail(1, idx, "row-lookup-fails", g.name[g.nc + i], ""); break; }
        }
        {
            char bad[NAMEBUF];
            snprintf(bad, sizeof bad, "a b");
            if (jaos_set_col_name(m, 0, bad) == JAOS_OK) fail(1, idx, "space-accepted", bad, "");
            snprintf(bad, sizeof bad, "a\tb");
            if (jaos_set_col_name(m, 0, bad) == JAOS_OK) fail(1, idx, "tab-accepted", "", "");
            for (int t = 0; t < 256; t++) bad[t] = 'x';
            bad[256] = '\0';
            if (jaos_set_col_name(m, 0, bad) == JAOS_OK) fail(1, idx, "256-bytes-accepted", "", "");
            if (jaos_col_name(m, 0, got[0], NAMEBUF) != JAOS_OK || strcmp(got[0], g.name[0]) != 0)
                fail(1, idx, "refused-name-changed-the-column", g.name[0], got[0]);
            jaos_model *c = nullptr;
            if (jaos_model_copy(m, &c) != JAOS_OK) abort();
            if (jaos_set_col_name(c, 0, "renamed_0") != JAOS_OK) abort();
            int64_t at = -1;
            if (jaos_col_index(c, g.name[0], &at) == JAOS_OK) fail(1, idx, "old-name-still-resolves", g.name[0], "");
            if (jaos_col_index(c, "renamed_0", &at) != JAOS_OK || at != 0) fail(1, idx, "new-name-does-not-resolve", "", "");
            jaos_model_free(c);
        }

        /* P2 */
        snprintf(path, sizeof path, "%s/m%" PRId64 ".mps", dir, idx);
        round_trip(&g, m, path, jaos_write_mps, jaos_read_mps, 2, "mps", idx, true, &mps_ok, &refused[0]);
        if (!(every > 0 && idx % every == 0)) remove(path);

        /* P3 */
        snprintf(path, sizeof path, "%s/m%" PRId64 ".lp", dir, idx);
        if (jaos_write_lp(m, path) != JAOS_OK) {
            refused[1]++;
            fail(3, idx, "lp-writer-refused", jaos_model_error(m), "");
        } else {
            jaos_model *b = nullptr;
            if (jaos_model_new(&b) != JAOS_OK) abort();
            if (jaos_read_lp(b, path) != JAOS_OK) {
                fail(3, idx, "lp-reader-refused", jaos_model_error(b), "");
            } else {
                names_of(b, &g, got);
                bool same = true;
                static char text[1 << 16];
                FILE *f = fopen(path, "r");
                if (f == nullptr) abort();
                const size_t n = fread(text, 1, sizeof text - 1, f);
                text[n] = '\0';
                fclose(f);
                for (int64_t k = 0; k < g.nc + g.nr + 1 && same; k++) {
                    char want[NAMEBUF], line[NAMEBUF + 64];
                    const bool col = k < g.nc, row = k >= g.nc && k < g.nc + g.nr;
                    if (lp_spellable(g.name[k])) {
                        snprintf(want, sizeof want, "%s", g.name[k]);
                    } else {
                        if (col) snprintf(want, sizeof want, "c%" PRId64, k + 1);
                        else if (row) snprintf(want, sizeof want, "r%" PRId64, k - g.nc + 1);
                        else snprintf(want, sizeof want, "obj");
                        snprintf(line, sizeof line, "\\ %s %s was %s\n",
                                 col ? "column" : row ? "row" : "objective", want, g.name[k]);
                        if (strstr(text, line) == nullptr) {
                            char alt[NAMEBUF];
                            snprintf(alt, sizeof alt, "%.290s_", want);
                            snprintf(line, sizeof line, " %s was %s\n", alt, g.name[k]);
                            if (strstr(text, line) != nullptr) snprintf(want, sizeof want, "%s", alt);
                            else { fail(3, idx, "no-map-line", g.name[k], want); same = false; break; }
                        }
                        lp_mapped++;
                    }
                    if (strcmp(got[k], want) != 0) { fail(3, idx, "lp-name-differs", want, got[k]); same = false; }
                }
                if (same) lp_ok++;
            }
            jaos_model_free(b);
        }
        remove(path);

        /* P4 */
        snprintf(path, sizeof path, "%s/m%" PRId64 ".nl", dir, idx);
        round_trip(&g, m, path, jaos_write_nl, jaos_read_nl, 4, "nl", idx, false, &nl_ok, &refused[2]);
        remove(path);
        snprintf(path, sizeof path, "%s/m%" PRId64 ".col", dir, idx);
        remove(path);
        snprintf(path, sizeof path, "%s/m%" PRId64 ".row", dir, idx);
        remove(path);

        /* P5 */
        snprintf(path, sizeof path, "%s/m%" PRId64 ".osil", dir, idx);
        round_trip(&g, m, path, jaos_write_osil, jaos_read_osil, 5, "osil", idx, false, &osil_ok, &refused[3]);
        remove(path);

        /* P6: QPLIB has no objective name, and its reader takes `#` and
         * `!` as comment marks and a leading `%` as a comment line, so a
         * name holding one has to be refused by the writer by name */
        {
            bool hostile = false;
            for (int64_t k = 0; k < g.nc + g.nr + 2; k++)
                if (k != g.nc + g.nr &&
                    (strchr(g.name[k], '#') != nullptr || strchr(g.name[k], '!') != nullptr ||
                     g.name[k][0] == '%')) hostile = true;
            snprintf(path, sizeof path, "%s/m%" PRId64 ".qplib", dir, idx);
            if (hostile) {
                if (jaos_write_qplib(m, path) == JAOS_OK) fail(6, idx, "qplib-wrote-a-comment-mark", "", "");
                else qplib_refused++;
            } else {
                round_trip(&g, m, path, jaos_write_qplib, jaos_read_qplib, 6, "qplib", idx, false, &qplib_ok, &refused[4]);
            }
            if (getenv("KEEP") == nullptr) remove(path);
        }

        /* P7 */
        {
            jaos_model *u = load(&g, false);
            if (u == nullptr) abort();
            char want[32];
            for (int64_t j = 0; j < g.nc; j++) {
                snprintf(want, sizeof want, "C%" PRId64, j + 1);
                int64_t at = -1;
                if (jaos_col_name(u, j, got[0], NAMEBUF) != JAOS_OK || strcmp(got[0], want) != 0 ||
                    jaos_col_index(u, want, &at) != JAOS_OK || at != j) { fail(7, idx, "positional-column", want, got[0]); break; }
            }
            for (int64_t i = 0; i < g.nr; i++) {
                snprintf(want, sizeof want, "R%" PRId64, i + 1);
                int64_t at = -1;
                if (jaos_row_name(u, i, got[0], NAMEBUF) != JAOS_OK || strcmp(got[0], want) != 0 ||
                    jaos_row_index(u, want, &at) != JAOS_OK || at != i) { fail(7, idx, "positional-row", want, got[0]); break; }
            }
            jaos_model_free(u);
        }

        /* P8 */
        {
            jaos_model *d = nullptr;
            if (jaos_model_copy(m, &d) != JAOS_OK) abort();
            if (jaos_set_col_name(d, 1, g.name[0]) != JAOS_OK) abort();
            writer ws[5] = {jaos_write_mps, jaos_write_lp, jaos_write_nl, jaos_write_osil, jaos_write_qplib};
            const char *ext[5] = {"mps", "lp", "nl", "osil", "qplib"};
            for (int w = 0; w < 5; w++) {
                snprintf(path, sizeof path, "%s/dup%" PRId64 ".%s", dir, idx, ext[w]);
                if (ws[w](d, path) == JAOS_OK) fail(8, idx, "duplicate-written", ext[w], g.name[0]);
                else dup_refused++;
                remove(path);
            }
            snprintf(path, sizeof path, "%s/dup%" PRId64 ".col", dir, idx); remove(path);
            snprintf(path, sizeof path, "%s/dup%" PRId64 ".row", dir, idx); remove(path);
            jaos_model_free(d);
        }

        if (every > 0 && idx % every == 0) {
            int64_t j = -1;
            for (int64_t c = 0; c < g.nc && j < 0; c++) if (g.kind[c] == 2) j = c;
            snprintf(path, sizeof path, "%s/m%" PRId64 ".long", dir, idx);
            FILE *f = fopen(path, "w");
            if (f == nullptr) abort();
            fprintf(f, "%s\n", j >= 0 ? g.name[j] : g.name[0]);
            fclose(f);
            dumped++;
        }
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int p = 1; p <= 8; p++) total += broke[p];
    printf("models %" PRId64 " names %" PRId64 " kinds", runs, names);
    for (int k = 0; k < 11; k++) printf(" %" PRId64, kinds[k]);
    printf("\nmps %" PRId64 " lp %" PRId64 " mapped %" PRId64 " nl %" PRId64
           " osil %" PRId64 " qplib %" PRId64 " qplib_refused_by_name %" PRId64
           " writer_refused %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64 " dup_refused %" PRId64 " dumped %" PRId64 "\n",
           mps_ok, lp_ok, lp_mapped, nl_ok, osil_ok, qplib_ok, qplib_refused,
           refused[0], refused[1], refused[2], refused[3], refused[4],
           dup_refused, dumped);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " P7=%" PRId64 " P8=%" PRId64
           " total=%" PRId64 "\n", broke[1], broke[2], broke[3], broke[4],
           broke[5], broke[6], broke[7], broke[8], total);
    return total == 0 ? 0 : 1;
}
