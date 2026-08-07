/* Reader robustness under damaged input — the gate's condition 4 (PLAN.md
 * §2.9), which asks that truncated and corrupted files produce errors and
 * never crash.
 *
 * tests/data/ already covers malformed *content*, one file per rejection
 * class, and that is a weaker claim than it looks: every one of those files
 * is well-formed enough to reach the check that rejects it. None is cut
 * mid-record, no byte is flipped inside a number, none is empty, and none is
 * random. The gap is fuzz-shaped, so this is a fuzzer.
 *
 * The property is deliberately narrow. It says nothing about whether a
 * mutated file is read *correctly*, because a damaged file has no correct
 * reading to compare against. What it asserts is:
 *
 *   - the reader returns, with one of the statuses the API declares for it;
 *   - the error message is never NULL, and is empty exactly when the read
 *     succeeded;
 *   - a failed read leaves the model's previous problem untouched, which is
 *     the contract in jaos.h and the one a fuzzer can actually check;
 *   - a successful read leaves a model whose dimensions are consistent;
 *   - reading the same bytes twice gives the same answer — D8 reaches
 *     parsing, which is why src/mps.c switches locale at all;
 *   - and under ASan+UBSan, that nothing was read or written out of bounds.
 *
 * That last one carries most of the weight. Without `make sanitize` this
 * file proves only that the readers do not segfault, which is the weakest of
 * the six and the one a fuzzer is least needed for.
 *
 * Every case is offered to both readers. An LP file handed to the MPS reader
 * is corrupted input by any definition, and costs nothing to try.
 *
 * Determinism (D8): the corpus is the sorted contents of tests/data, the
 * mutations come from a splitmix64 written out below rather than rand(),
 * and the case counts are fixed constants. The same commit fuzzes the same
 * bytes on every machine, so a failure reproduces from the case label alone.
 * JAOS_FUZZ_SCALE multiplies the seeded classes for a longer campaign; it
 * changes how many cases run, never which ones run first.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _POSIX_C_SOURCE 200809L

#include "jaos.h"
#include "unity.h"

#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

/* --------------------------------------------------------------------- */
/* Deterministic PRNG                                                    */
/* --------------------------------------------------------------------- */

/* splitmix64. Written out rather than taken from rand() because the C
 * library's generator is not specified to produce the same sequence on two
 * implementations, and a fuzz corpus that differs per machine cannot be
 * reproduced from a failure report. The modulo below is biased; a fuzzer
 * needs its draws reproducible, not uniform. */
static uint64_t rng_state;

static void rng_seed(uint64_t s) { rng_state = s; }

static uint64_t rng_next(void)
{
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15u);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}

static uint64_t rng_below(uint64_t n) { return n == 0 ? 0 : rng_next() % n; }

/* --------------------------------------------------------------------- */
/* Case budget                                                           */
/* --------------------------------------------------------------------- */

/* Sized so the whole file runs in a few seconds under ASan: the suite is
 * run on every change, and a fuzzer nobody waits for is a fuzzer nobody
 * runs. Truncation is exhaustive because the corpus is 3 KB in total and
 * the gate names that class specifically. */
constexpr int EDITS_PER_FILE = 200;
constexpr int CHAOS_CASES    = 1500;
constexpr int SALAD_CASES    = 3000;

/* Every Nth case is read twice and the two answers compared. */
constexpr int64_t REPEAT_STRIDE = 7;

static int64_t fuzz_scale = 1;

/* --------------------------------------------------------------------- */
/* Corpus                                                                */
/* --------------------------------------------------------------------- */

constexpr int    CORPUS_MAX = 64;
constexpr size_t GEN_MAX    = 1u << 18; /* generated cases stay bounded */

/* NAME_MAX is 255, and a name that does not fit is a name this test would
 * report wrongly, so the buffer holds whatever readdir can hand over. */
typedef struct {
    char           name[256];
    unsigned char *b;
    size_t         n;
} blob;

static blob corpus[CORPUS_MAX];
static int  ncorpus;

static int by_name(const void *a, const void *b)
{
    return strcmp(((const blob *)a)->name, ((const blob *)b)->name);
}

static bool slurp(const char *path, unsigned char **out, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (f == nullptr)
        return false;
    size_t         cap = 4096, len = 0;
    unsigned char *b   = malloc(cap);
    if (b == nullptr) {
        fclose(f);
        return false;
    }
    for (;;) {
        if (len == cap) {
            unsigned char *nb = realloc(b, cap * 2);
            if (nb == nullptr) {
                free(b);
                fclose(f);
                return false;
            }
            b = nb;
            cap *= 2;
        }
        size_t got = fread(b + len, 1, cap - len, f);
        len += got;
        if (got == 0)
            break;
    }
    fclose(f);
    *out = b;
    *n   = len;
    return true;
}

/* readdir's order is not specified, and an unordered corpus would make the
 * seeded classes depend on the filesystem. Names are collected, sorted, then
 * read. */
static void corpus_load(void)
{
    DIR *d = opendir("tests/data");
    TEST_ASSERT_NOT_NULL_MESSAGE(d, "tests/data not found; run make from the "
                                    "repository root");
    for (struct dirent *e; (e = readdir(d)) != nullptr;) {
        if (e->d_name[0] == '.')
            continue;
        if (ncorpus == CORPUS_MAX)
            break;
        snprintf(corpus[ncorpus].name, sizeof corpus[ncorpus].name, "%s",
                 e->d_name);
        ncorpus++;
    }
    closedir(d);
    TEST_ASSERT_TRUE_MESSAGE(ncorpus > 0, "tests/data is empty");
    qsort(corpus, (size_t)ncorpus, sizeof *corpus, by_name);

    for (int i = 0; i < ncorpus; i++) {
        char path[512];
        snprintf(path, sizeof path, "tests/data/%s", corpus[i].name);
        TEST_ASSERT_TRUE_MESSAGE(slurp(path, &corpus[i].b, &corpus[i].n),
                                 "cannot read a corpus file");
    }
}

static void corpus_free(void)
{
    for (int i = 0; i < ncorpus; i++)
        free(corpus[i].b);
    ncorpus = 0;
}

/* --------------------------------------------------------------------- */
/* One case                                                              */
/* --------------------------------------------------------------------- */

static char    tmp_path[512];
static char    case_id[512];
static int64_t cases_run;

/* The scratch file goes to TMPDIR, not into the repository: the repository
 * may sit on a filesystem shared with a host OS, where tens of thousands of
 * create/write/read cycles cost minutes rather than seconds. */
static void tmp_init(void)
{
    const char *dir = getenv("TMPDIR");
    if (dir == nullptr || dir[0] == '\0')
        dir = "/tmp";
    snprintf(tmp_path, sizeof tmp_path, "%s/jaos_fuzz_XXXXXX", dir);
    int fd = mkstemp(tmp_path);
    TEST_ASSERT_TRUE_MESSAGE(fd >= 0, "cannot create a temporary file");
    close(fd);
}

static void tmp_done(void)
{
    if (tmp_path[0])
        remove(tmp_path);
}

static void tmp_write(const unsigned char *b, size_t n)
{
    FILE *f = fopen(tmp_path, "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot write the temporary file");
    if (n > 0)
        TEST_ASSERT_TRUE_MESSAGE(fwrite(b, 1, n, f) == n, "short write");
    TEST_ASSERT_TRUE_MESSAGE(fclose(f) == 0, "cannot close the temporary file");
}

/* Statuses a reader may return. JAOS_ERR_NUMERICAL is not among them:
 * reading a file performs no arithmetic that can be abandoned, so seeing it
 * here would mean a solver status leaked into the reader's contract. */
static bool status_allowed(jaos_status s)
{
    return s == JAOS_OK || s == JAOS_ERR_INVALID_INPUT ||
           s == JAOS_ERR_OUT_OF_MEMORY || s == JAOS_ERR_IO;
}

/* A small model with fully known dimensions, loaded from memory so that
 * proving "a failed read preserves the previous problem" costs no file I/O.
 * Its shape (2 columns, 1 row, 2 nonzeros) is what every failed read must
 * leave behind. */
static void load_reference(jaos_model *m)
{
    static const double  cost[2] = {1.0, -2.0};
    static const double  cl[2]   = {0.0, 0.0};
    static const double  cu[2]   = {5.0, 5.0};
    static const double  rl[1]   = {1.0};
    static const double  ru[1]   = {4.0};
    static const int64_t as[3]   = {0, 1, 2};
    static const int64_t ai[2]   = {0, 0};
    static const double  av[2]   = {1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.5, cost, cl,
                                       cu, rl, ru, 2, as, ai, av));
}

typedef jaos_status (*reader_fn)(jaos_model *, const char *);

typedef struct {
    jaos_status status;
    int64_t     ncol, nrow, nnz;
    char        err[256];
} outcome;

static void fail_case(const char *reader, const char *what)
{
    char msg[1024];
    snprintf(msg, sizeof msg, "%s reader, case %s: %s", reader, case_id, what);
    TEST_FAIL_MESSAGE(msg);
}

static void read_once(reader_fn rd, outcome *o)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    load_reference(m);

    o->status = rd(m, tmp_path);
    o->ncol   = jaos_num_col(m);
    o->nrow   = jaos_num_row(m);
    o->nnz    = jaos_num_nz(m);
    snprintf(o->err, sizeof o->err, "%s", jaos_model_error(m));
    jaos_model_free(m);
}

static void feed(const unsigned char *b, size_t n)
{
    tmp_write(b, n);
    cases_run++;

    for (int which = 0; which < 2; which++) {
        const char *name = which == 0 ? "MPS" : "LP";
        reader_fn   rd   = which == 0 ? jaos_read_mps : jaos_read_lp;

        outcome o;
        read_once(rd, &o);

        if (!status_allowed(o.status))
            fail_case(name, "status outside the reader's contract");

        if (o.status == JAOS_OK) {
            if (o.err[0] != '\0')
                fail_case(name, "success left an error message behind");
            /* Nothing is claimed about *which* model a damaged file yields,
             * only that the dimensions agree with each other. At most one
             * entry can exist per (row, column) pair, since the loader
             * rejects duplicates. */
            if (o.ncol < 0 || o.nrow < 0 || o.nnz < 0)
                fail_case(name, "negative dimension after a successful read");
            if (o.nnz > 0 && o.ncol == 0)
                fail_case(name, "nonzeros in a model with no columns");
            if (o.ncol < (1 << 20) && o.nrow < (1 << 20) &&
                o.nnz > o.ncol * o.nrow)
                fail_case(name, "more nonzeros than the shape can hold");
        } else {
            if (o.err[0] == '\0')
                fail_case(name, "failure left no error message");
            /* jaos.h: on failure the model's problem data is left as it
             * was. A half-applied read is the failure mode this catches —
             * the reader assembling into the model as it goes and then
             * abandoning it mid-file. */
            if (o.ncol != 2 || o.nrow != 1 || o.nnz != 2)
                fail_case(name, "failed read damaged the previous model");
        }

        /* Parsing is covered by D8, so the same bytes must give the same
         * answer. Checked on a stride rather than every case: the property
         * is about the reader, not about the input, and doubling every read
         * would buy little for twice the runtime. */
        if (cases_run % REPEAT_STRIDE == 0) {
            outcome again;
            read_once(rd, &again);
            if (again.status != o.status || again.ncol != o.ncol ||
                again.nrow != o.nrow || again.nnz != o.nnz ||
                strcmp(again.err, o.err) != 0)
                fail_case(name, "two reads of the same bytes disagreed");
        }
    }
}

/* --------------------------------------------------------------------- */
/* Mutation classes                                                      */
/* --------------------------------------------------------------------- */

/* Bytes that mean something to one of the two grammars. Drawing half the
 * replacements from here rather than uniformly is what gets a mutated file
 * past the first rejection and into the parser proper. */
static const unsigned char interesting[] =
    "0123456789.eEdD+-* \t\n\r:<>=\\'NLGE";

static unsigned char pick_byte(void)
{
    if (rng_next() & 1u)
        return interesting[rng_below(sizeof interesting - 1)];
    return (unsigned char)rng_below(256);
}

/* The class the gate names: every prefix of every corpus file, including
 * the empty one. Exhaustive because the corpus is small enough for that to
 * cost nothing, and because "cut at byte k" is exactly the shape of a
 * transfer that died halfway. */
static void test_truncation_at_every_offset(void)
{
    for (int i = 0; i < ncorpus; i++) {
        for (size_t k = 0; k <= corpus[i].n; k++) {
            snprintf(case_id, sizeof case_id, "truncate/%s/%zu",
                     corpus[i].name, k);
            feed(corpus[i].b, k);
        }
    }
}

/* One to four random edits per case: replace, insert, delete. Small edit
 * counts on purpose — a file with three bytes changed still reaches deep
 * into the parser, while one with three hundred is indistinguishable from
 * the chaos class below. */
static void test_small_edits(void)
{
    unsigned char *buf = malloc(GEN_MAX);
    TEST_ASSERT_NOT_NULL(buf);

    rng_seed(0x9a1c05d2u);
    for (int i = 0; i < ncorpus; i++) {
        int64_t cases = EDITS_PER_FILE * fuzz_scale;
        for (int64_t c = 0; c < cases; c++) {
            size_t n = corpus[i].n;
            if (n + 8 > GEN_MAX)
                continue;
            memcpy(buf, corpus[i].b, n);

            int edits = 1 + (int)rng_below(4);
            for (int e = 0; e < edits; e++) {
                int op = (int)rng_below(3);
                if (op == 0 && n > 0) { /* replace */
                    buf[rng_below(n)] = pick_byte();
                } else if (op == 1 && n + 1 < GEN_MAX) { /* insert */
                    size_t at = rng_below(n + 1);
                    memmove(buf + at + 1, buf + at, n - at);
                    buf[at] = pick_byte();
                    n++;
                } else if (n > 0) { /* delete */
                    size_t at = rng_below(n);
                    memmove(buf + at, buf + at + 1, n - at - 1);
                    n--;
                }
            }
            snprintf(case_id, sizeof case_id, "edit/%s/%" PRId64,
                     corpus[i].name, c);
            feed(buf, n);
        }
    }
    free(buf);
}

/* Uniform noise. Finds little on its own — almost every such file is
 * rejected on its first line — but it is the class that covers the paths
 * taken before any structure exists, and it is where an empty or
 * single-byte file lands naturally. */
static void test_random_bytes(void)
{
    unsigned char *buf = malloc(4096);
    TEST_ASSERT_NOT_NULL(buf);

    rng_seed(0x51ed2701u);
    int64_t cases = CHAOS_CASES * fuzz_scale;
    for (int64_t c = 0; c < cases; c++) {
        size_t n = (size_t)rng_below(2049);
        for (size_t k = 0; k < n; k++)
            buf[k] = (unsigned char)rng_below(256);
        snprintf(case_id, sizeof case_id, "chaos/%" PRId64, c);
        feed(buf, n);
    }
    free(buf);
}

/* Random sequences of real keywords. This is the class that reaches the
 * deep parser states — section transitions, bound types, continuation
 * columns — because every token it emits is one the reader recognizes and
 * only their order is wrong. Leading whitespace is generated deliberately:
 * in MPS it is what separates a section header from a data line. */
static const char *const vocab[] = {
    "NAME",  "ROWS",     "COLUMNS", "RHS",       "RANGES",   "BOUNDS",
    "ENDATA", "OBJSENSE", "MAX",    "MIN",       "MAXIMIZE", "MINIMIZE",
    "N",     "L",        "G",       "E",         "UP",       "LO",
    "FX",    "FR",       "MI",      "PL",        "BV",       "LI",
    "UI",    "SC",       "SI",      "'MARKER'",  "'INTORG'", "'INTEND'",
    "MARKER", "Subject", "To",      "Such",      "That",     "st",
    "s.t.",  "Bounds",   "End",     "free",      "inf",      "infinity",
    "General", "Binary", "Integer", "Semi",      "sos",
    "<=",    ">=",       "=",       "=<",        "=>",       "+",
    "-",     ":",        "<",       ">",         "*",        "\\",
    "r0",    "r1",       "r2",      "c0",        "c1",       "c2",
    "obj",   "COST",     "1",       "0",         "-1",       "2.5",
    "1e30",  "-1e30",    "1.0D+00", "1e999",     "nan",      "inf",
    "0.0",   "1e-30",    ".5",      "5.",        "1.2.3",    "--3",
};

static void test_token_salad(void)
{
    unsigned char *buf = malloc(GEN_MAX);
    TEST_ASSERT_NOT_NULL(buf);

    rng_seed(0xc0ffee11u);
    int64_t cases = SALAD_CASES * fuzz_scale;
    for (int64_t c = 0; c < cases; c++) {
        size_t n      = 0;
        int    tokens = 1 + (int)rng_below(120);
        bool   fresh  = true;
        for (int t = 0; t < tokens; t++) {
            const char *w   = vocab[rng_below(sizeof vocab / sizeof *vocab)];
            size_t      len = strlen(w);
            if (n + len + 4 >= GEN_MAX)
                break;
            if (fresh) {
                /* An MPS line is a header or a data line by this one byte. */
                if (rng_next() & 1u)
                    buf[n++] = ' ';
                fresh = false;
            } else {
                buf[n++] = (rng_next() & 3u) == 0 ? '\t' : ' ';
            }
            memcpy(buf + n, w, len);
            n += len;
            if ((int)rng_below(5) == 0) {
                buf[n++] = '\n';
                fresh    = true;
            }
        }
        if (n + 1 < GEN_MAX)
            buf[n++] = '\n';
        snprintf(case_id, sizeof case_id, "salad/%" PRId64, c);
        feed(buf, n);
    }
    free(buf);
}

/* Shapes worth naming, so that a failure points at the shape rather than at
 * a seed. Several of these are the sizes at which a buffer decision inside
 * the readers changes: getline's growth, the 64 KiB slurp chunk, the token
 * limit, the name and number length caps. */
static void test_named_edge_cases(void)
{
    /* Empty, and the smallest non-empty files. */
    static const unsigned char nul_byte[1] = {0};
    snprintf(case_id, sizeof case_id, "edge/empty");
    feed((const unsigned char *)"", 0);
    snprintf(case_id, sizeof case_id, "edge/one-newline");
    feed((const unsigned char *)"\n", 1);
    snprintf(case_id, sizeof case_id, "edge/one-nul");
    feed(nul_byte, 1);
    snprintf(case_id, sizeof case_id, "edge/blank-lines");
    feed((const unsigned char *)"\n\n\n\n\n", 5);
    snprintf(case_id, sizeof case_id, "edge/spaces-only");
    feed((const unsigned char *)"     \t\t   ", 10);
    snprintf(case_id, sizeof case_id, "edge/star-only");
    feed((const unsigned char *)"*", 1);

    /* No newline anywhere, at three sizes: under getline's first buffer,
     * over it, and over the LP reader's 64 KiB read chunk. */
    static const size_t no_nl[] = {60, 5000, 200000};
    for (size_t i = 0; i < sizeof no_nl / sizeof *no_nl; i++) {
        size_t         n = no_nl[i];
        unsigned char *b = malloc(n);
        TEST_ASSERT_NOT_NULL(b);
        memset(b, 'A', n);
        snprintf(case_id, sizeof case_id, "edge/one-long-line/%zu", n);
        feed(b, n);
        free(b);
    }

    /* A single line carrying far more fields than the tokenizer keeps. */
    {
        size_t         n = 0, cap = 80000;
        unsigned char *b = malloc(cap);
        TEST_ASSERT_NOT_NULL(b);
        b[n++] = ' ';
        while (n + 3 < cap) {
            b[n++] = 'X';
            b[n++] = ' ';
        }
        b[n++] = '\n';
        snprintf(case_id, sizeof case_id, "edge/too-many-fields");
        feed(b, n);
        free(b);
    }

    /* One token longer than every name and number buffer in either reader. */
    {
        size_t         n = 0, cap = 40000;
        unsigned char *b = malloc(cap);
        TEST_ASSERT_NOT_NULL(b);
        b[n++] = ' ';
        memset(b + n, 'Z', 20000);
        n += 20000;
        b[n++] = ' ';
        memset(b + n, '9', 19000);
        n += 19000;
        b[n++] = '\n';
        snprintf(case_id, sizeof case_id, "edge/huge-tokens");
        feed(b, n);
        free(b);
    }

    /* Valid instances rewritten: CRLF, bare CR (one line as far as any
     * line-oriented reader is concerned), and a NUL punched into the middle
     * of the data. */
    for (int i = 0; i < ncorpus; i++) {
        size_t         n = corpus[i].n;
        unsigned char *b = malloc(n * 2 + 1);
        TEST_ASSERT_NOT_NULL(b);

        size_t m = 0;
        for (size_t k = 0; k < n; k++) {
            if (corpus[i].b[k] == '\n')
                b[m++] = '\r';
            b[m++] = corpus[i].b[k];
        }
        snprintf(case_id, sizeof case_id, "edge/crlf/%s", corpus[i].name);
        feed(b, m);

        memcpy(b, corpus[i].b, n);
        for (size_t k = 0; k < n; k++)
            if (b[k] == '\n')
                b[k] = '\r';
        snprintf(case_id, sizeof case_id, "edge/bare-cr/%s", corpus[i].name);
        feed(b, n);

        if (n > 0) {
            memcpy(b, corpus[i].b, n);
            b[n / 2] = '\0';
            snprintf(case_id, sizeof case_id, "edge/embedded-nul/%s",
                     corpus[i].name);
            feed(b, n);
        }

        /* Every byte set to the high half of the range: isalpha and friends
         * are the functions that go wrong here when a char reaches them
         * unconverted. */
        memcpy(b, corpus[i].b, n);
        for (size_t k = 0; k < n; k++)
            if (b[k] != '\n')
                b[k] = (unsigned char)(0x80u | (b[k] & 0x7fu));
        snprintf(case_id, sizeof case_id, "edge/high-bytes/%s",
                 corpus[i].name);
        feed(b, n);

        free(b);
    }

    /* A model large enough that the readers grow every array they own
     * several times, then a claim that contradicts what was just built. */
    {
        size_t         cap = GEN_MAX, n = 0;
        unsigned char *b   = malloc(cap);
        TEST_ASSERT_NOT_NULL(b);
        n += (size_t)snprintf((char *)b + n, cap - n, "NAME BIG\nROWS\n N COST\n");
        for (int i = 0; i < 2000 && n + 64 < cap; i++)
            n += (size_t)snprintf((char *)b + n, cap - n, " L R%d\n", i);
        n += (size_t)snprintf((char *)b + n, cap - n, "COLUMNS\n");
        for (int j = 0; j < 2000 && n + 64 < cap; j++)
            n += (size_t)snprintf((char *)b + n, cap - n,
                                  " C%d R%d 1.0 COST 1.0\n", j, j);
        snprintf(case_id, sizeof case_id, "edge/large-then-cut");
        feed(b, n); /* no ENDATA: rejected after all that growth */

        n += (size_t)snprintf((char *)b + n, cap - n, "ENDATA\n");
        snprintf(case_id, sizeof case_id, "edge/large-complete");
        feed(b, n);
        free(b);
    }
}

/* --------------------------------------------------------------------- */

int main(void)
{
    const char *scale = getenv("JAOS_FUZZ_SCALE");
    if (scale != nullptr && scale[0] != '\0') {
        long v = strtol(scale, nullptr, 10);
        if (v > 0 && v <= 10000)
            fuzz_scale = v;
    }

    tmp_init();
    corpus_load();

    UNITY_BEGIN();
    RUN_TEST(test_truncation_at_every_offset);
    RUN_TEST(test_small_edits);
    RUN_TEST(test_random_bytes);
    RUN_TEST(test_token_salad);
    RUN_TEST(test_named_edge_cases);
    int rc = UNITY_END();

    printf("fuzz: %" PRId64 " cases, %d corpus files, scale %" PRId64 "\n",
           cases_run, ncorpus, fuzz_scale);

    corpus_free();
    tmp_done();
    return rc;
}
