/* The reading behind 02-238: a basis another solver produced, proved.
 *
 * The generator is 02-237's: LPs of eight to twenty-four columns and six
 * to sixteen rows, integer data, a planted point inside the boxes with
 * each row's bounds set around it, so every model is feasible and every
 * optimum is a rational the exact arithmetic can hold.
 *
 * Three sources stand in for another solver's basis: JAOS's dual simplex
 * (S1), its primal simplex (S2), and the dual simplex on a copy with the
 * rows and columns shuffled, mapped back (S3). From each, neighbours are
 * made: an equality row's card moved to the other bound, one pivot, one
 * nonbasic variable moved to its other bound, and a basis built singular.
 * The harness judges every basis itself in extended precision, sharing
 * no code with the verifier: it solves B x_B = -N x_N and B'y = c_B by
 * Gaussian elimination and reads the primal and dual violations.
 *
 * Properties:
 *   P1 every basis a solver produced is proved optimal or refused for
 *      capacity; never broken
 *   P2 the MPS basis file JAOS writes reads back as the same statuses,
 *      and so does the same basis in another solver's spelling: cards
 *      shuffled, LL cards written out, pairs made in the other order,
 *      tabs, comments, another NAME
 *   P3 the verifier's verdict agrees with the harness's own reading on
 *      every basis handed in: optimal when the harness finds no violation
 *      past 1e-6, broken at the rank stage when the harness finds B
 *      singular, broken at the primal or dual stage when the harness
 *      finds that violation, and the row or column named is one the
 *      harness finds violated
 *   P4 jaos_verify_basis on the solve's own basis gives the report and
 *      the exact strings jaos_verify gives, and leaves the published
 *      solution alone
 *   P5 the proof file written after a foreign basis is proved holds under
 *      jaos_check_proof
 *   P6 a second jaos_verify_basis on the same basis gives the same report
 *      and the same strings
 *
 * Usage: basis RUNS SEED DIR [EVERY]
 *   Every EVERY-th model is dumped to DIR as m<idx>.mps, m<idx>.bas (S3
 *   in the foreign spelling), m<idx>.bad.bas (a neighbour the harness
 *   finds broken) and m<idx>.expect, for the CLI chain in basis.sh.
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
#define NB (MAXC + MAXR)

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

static void name_of(char *out, size_t cap, bool is_col, int64_t k)
{
    snprintf(out, cap, "%s%" PRId64, is_col ? "X" : "R", k + 1);
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
        name_of(nm, sizeof nm, true, j);
        if (jaos_set_col_name(m, j, nm) != JAOS_OK) abort();
    }
    for (int64_t i = 0; i < g->nr; i++) {
        name_of(nm, sizeof nm, false, i);
        if (jaos_set_row_name(m, i, nm) != JAOS_OK) abort();
    }
    return m;
}

/* The same model with its columns and rows in another order. cp[k] is the
 * old column at new position k; rp[r] the old row at new position r. */
static void permute(const gen *g, gen *h, int64_t *cp, int64_t *rp)
{
    for (int64_t k = 0; k < g->nc; k++) cp[k] = k;
    for (int64_t k = g->nc - 1; k > 0; k--) {
        const int64_t t = ri(0, k);
        const int64_t s = cp[k]; cp[k] = cp[t]; cp[t] = s;
    }
    for (int64_t r = 0; r < g->nr; r++) rp[r] = r;
    for (int64_t r = g->nr - 1; r > 0; r--) {
        const int64_t t = ri(0, r);
        const int64_t s = rp[r]; rp[r] = rp[t]; rp[t] = s;
    }
    int64_t inv[MAXR];
    for (int64_t r = 0; r < g->nr; r++) inv[rp[r]] = r;
    *h = *g;
    h->nz = 0;
    for (int64_t k = 0; k < g->nc; k++) {
        const int64_t j = cp[k];
        h->cost[k] = g->cost[j];
        h->cl[k] = g->cl[j];
        h->cu[k] = g->cu[j];
        h->ap[k] = h->nz;
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++) {
            h->ai[h->nz] = inv[g->ai[p]];
            h->av[h->nz] = g->av[p];
            h->nz++;
        }
    }
    h->ap[g->nc] = h->nz;
    for (int64_t r = 0; r < g->nr; r++) {
        h->rl[r] = g->rl[rp[r]];
        h->ru[r] = g->ru[rp[r]];
    }
}

/* Gaussian elimination with partial pivoting on a copy. False when a
 * pivot is below 1e-9 in size. */
static bool ld_solve(int64_t n, const long double A[MAXR][MAXR],
                     long double *b, long double *x)
{
    long double M[MAXR][MAXR + 1];
    for (int64_t i = 0; i < n; i++) {
        for (int64_t k = 0; k < n; k++) M[i][k] = A[i][k];
        M[i][n] = b[i];
    }
    for (int64_t c = 0; c < n; c++) {
        int64_t best = c;
        for (int64_t r = c + 1; r < n; r++)
            if (fabsl(M[r][c]) > fabsl(M[best][c])) best = r;
        if (fabsl(M[best][c]) < 1e-9L) return false;
        if (best != c)
            for (int64_t k = 0; k <= n; k++) {
                const long double t = M[c][k]; M[c][k] = M[best][k]; M[best][k] = t;
            }
        for (int64_t r = c + 1; r < n; r++) {
            const long double f = M[r][c] / M[c][c];
            if (f == 0.0L) continue;
            for (int64_t k = c; k <= n; k++) M[r][k] -= f * M[c][k];
        }
    }
    for (int64_t c = n - 1; c >= 0; c--) {
        long double s = M[c][n];
        for (int64_t k = c + 1; k < n; k++) s -= M[c][k] * x[k];
        x[c] = s / M[c][c];
    }
    return true;
}

typedef struct {
    bool count_bad, singular;
    long double pviol, dviol;
    long double own[NB];
} judge_out;

static long double outside(long double v, double lo, double hi)
{
    long double d = 0.0L;
    if (isfinite(lo) && v < (long double)lo) d = (long double)lo - v;
    if (isfinite(hi) && v > (long double)hi && v - (long double)hi > d) d = v - (long double)hi;
    return d;
}

static long double sign_viol(jaos_basis_status st, long double d)
{
    if (st == JAOS_BASIS_AT_LOWER) return d < 0.0L ? -d : 0.0L;
    if (st == JAOS_BASIS_AT_UPPER) return d > 0.0L ? d : 0.0L;
    return fabsl(d);
}

static void judge(const gen *g, const jaos_basis_status *cs,
                  const jaos_basis_status *rs, judge_out *o)
{
    memset(o, 0, sizeof *o);
    const int64_t n = g->nr;
    const long double sigma = g->maximise ? -1.0L : 1.0L;
    int64_t who[MAXR], nb = 0;
    for (int64_t j = 0; j < g->nc; j++)
        if (cs[j] == JAOS_BASIS_BASIC) { if (nb == n) { o->count_bad = true; return; } who[nb++] = j; }
    for (int64_t i = 0; i < n; i++)
        if (rs[i] == JAOS_BASIS_BASIC) { if (nb == n) { o->count_bad = true; return; } who[nb++] = -i - 1; }
    if (nb != n) { o->count_bad = true; return; }

    long double B[MAXR][MAXR], BT[MAXR][MAXR];
    memset(B, 0, sizeof B);
    for (int64_t k = 0; k < n; k++) {
        if (who[k] >= 0)
            for (int64_t p = g->ap[who[k]]; p < g->ap[who[k] + 1]; p++)
                B[g->ai[p]][k] = (long double)g->av[p];
        else
            B[-who[k] - 1][k] = -1.0L;
    }
    for (int64_t i = 0; i < n; i++)
        for (int64_t k = 0; k < n; k++) BT[k][i] = B[i][k];

    long double xn[MAXC], sn[MAXR], rhs[MAXR], xb[MAXR], cb[MAXR], y[MAXR];
    for (int64_t j = 0; j < g->nc; j++)
        xn[j] = cs[j] == JAOS_BASIS_AT_LOWER ? (long double)g->cl[j]
              : cs[j] == JAOS_BASIS_AT_UPPER ? (long double)g->cu[j] : 0.0L;
    for (int64_t i = 0; i < n; i++)
        sn[i] = rs[i] == JAOS_BASIS_AT_LOWER ? (long double)g->rl[i]
              : rs[i] == JAOS_BASIS_AT_UPPER ? (long double)g->ru[i] : 0.0L;
    for (int64_t i = 0; i < n; i++)
        rhs[i] = rs[i] != JAOS_BASIS_BASIC ? sn[i] : 0.0L;
    for (int64_t j = 0; j < g->nc; j++)
        if (cs[j] != JAOS_BASIS_BASIC && xn[j] != 0.0L)
            for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++)
                rhs[g->ai[p]] -= (long double)g->av[p] * xn[j];
    if (!ld_solve(n, B, rhs, xb)) { o->singular = true; return; }

    for (int64_t k = 0; k < n; k++) {
        const int64_t w = who[k];
        const long double v = w >= 0 ? outside(xb[k], g->cl[w], g->cu[w])
                                     : outside(xb[k], g->rl[-w - 1], g->ru[-w - 1]);
        o->own[w >= 0 ? w : MAXC + (-w - 1)] = v;
        if (v > o->pviol) o->pviol = v;
        cb[k] = w >= 0 ? sigma * (long double)g->cost[w] : 0.0L;
    }
    if (!ld_solve(n, BT, cb, y)) { o->singular = true; return; }
    for (int64_t j = 0; j < g->nc; j++) {
        if (cs[j] == JAOS_BASIS_BASIC || g->cl[j] == g->cu[j]) continue;
        long double d = sigma * (long double)g->cost[j];
        for (int64_t p = g->ap[j]; p < g->ap[j + 1]; p++)
            d -= (long double)g->av[p] * y[g->ai[p]];
        const long double v = sign_viol(cs[j], d);
        o->own[j] = v;
        if (v > o->dviol) o->dviol = v;
    }
    for (int64_t i = 0; i < n; i++) {
        if (rs[i] == JAOS_BASIS_BASIC || g->rl[i] == g->ru[i]) continue;
        const long double v = sign_viol(rs[i], y[i]);
        o->own[MAXC + i] = v;
        if (v > o->dviol) o->dviol = v;
    }
}

/* The same basis in another solver's spelling. */
static void write_foreign(const char *path, const gen *g,
                          const jaos_basis_status *cs,
                          const jaos_basis_status *rs)
{
    char cards[NB][96];
    int64_t ncards = 0;
    int64_t bc[MAXC], nbc = 0, nr_[MAXR], nnr = 0;
    for (int64_t j = 0; j < g->nc; j++) if (cs[j] == JAOS_BASIS_BASIC) bc[nbc++] = j;
    for (int64_t i = 0; i < g->nr; i++) if (rs[i] != JAOS_BASIS_BASIC) nr_[nnr++] = i;
    if (nbc != nnr) abort();
    char cn[32], rn[32];
    for (int64_t k = 0; k < nbc; k++) {
        const int64_t j = bc[k], i = nr_[nnr - 1 - k];
        name_of(cn, sizeof cn, true, j);
        name_of(rn, sizeof rn, false, i);
        snprintf(cards[ncards++], 96, "%s%s%s%s%s",
                 rs[i] == JAOS_BASIS_AT_UPPER ? " XU" : " XL",
                 ri(0, 1) ? "\t" : "  ", cn, ri(0, 1) ? "\t" : " ", rn);
    }
    for (int64_t j = 0; j < g->nc; j++) {
        if (cs[j] == JAOS_BASIS_BASIC || cs[j] == JAOS_BASIS_FREE) continue;
        name_of(cn, sizeof cn, true, j);
        snprintf(cards[ncards++], 96, "%s%s%s",
                 cs[j] == JAOS_BASIS_AT_UPPER ? " UL" : " LL",
                 ri(0, 1) ? "\t" : " ", cn);
    }
    for (int64_t k = ncards - 1; k > 0; k--) {
        const int64_t t = ri(0, k);
        char tmp[96];
        memcpy(tmp, cards[k], 96); memcpy(cards[k], cards[t], 96); memcpy(cards[t], tmp, 96);
    }
    FILE *f = fopen(path, "w");
    if (f == nullptr) abort();
    fprintf(f, "* a basis from somewhere else\nNAME\tsomething-else\n");
    for (int64_t k = 0; k < ncards; k++) {
        if (ri(0, 4) == 0) fprintf(f, "* comment %" PRId64 "\n", k);
        fprintf(f, "%s\n", cards[k]);
    }
    fprintf(f, "ENDATA\n");
    fclose(f);
}

static int64_t broke[8];

static void fail(int p, int64_t idx, const char *what, double a, double b)
{
    broke[p]++;
    if (broke[p] <= 5)
        printf("P%d m%" PRId64 " %s %.17g %.17g\n", p, idx, what, a, b);
}

static int64_t verdict_optimal, verdict_rank, verdict_primal, verdict_dual,
               verdict_refused, ambiguous, judged;

/* P3: hand a basis to the verifier and compare with the harness. Returns
 * the verifier's status. Also P2 on the foreign spelling of that basis. */
static jaos_proof submit(const gen *g, jaos_model *m,
                         const jaos_basis_status *cs,
                         const jaos_basis_status *rs, int64_t idx,
                         const char *tag, const char *dir,
                         jaos_verify_report *rep_out, judge_out *jo)
{
    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    if (jaos_verify_basis(m, cs, rs, &rep) != JAOS_OK) {
        fail(3, idx, "verify-basis-refused-call", 0.0, 0.0);
        printf("   %s: %s\n", tag, jaos_model_error(m));
        rep.status = JAOS_PROOF_REFUSED;
        if (rep_out) *rep_out = rep;
        return rep.status;
    }
    if (rep_out) *rep_out = rep;
    judge(g, cs, rs, jo);
    judged++;
    if (jo->count_bad) abort();

    if (rep.status == JAOS_PROOF_REFUSED) {
        verdict_refused++;
    } else if (rep.status == JAOS_PROOF_OPTIMAL) {
        verdict_optimal++;
        if (jo->singular) fail(3, idx, "optimal-but-singular", 0.0, 0.0);
        else if (jo->pviol > 1e-6L || jo->dviol > 1e-6L)
            fail(3, idx, "optimal-but-violated", (double)jo->pviol, (double)jo->dviol);
        else if (jo->pviol > 1e-9L || jo->dviol > 1e-9L) ambiguous++;
    } else if (rep.stage == JAOS_PROOF_STAGE_RANK) {
        verdict_rank++;
        if (!jo->singular) fail(3, idx, "rank-broken-but-nonsingular", 0.0, 0.0);
    } else if (rep.stage == JAOS_PROOF_STAGE_PRIMAL) {
        verdict_primal++;
        const int64_t k = rep.at_col >= 0 ? rep.at_col : rep.at_row >= 0 ? MAXC + rep.at_row : -1;
        if (jo->singular) fail(3, idx, "primal-broken-but-singular", 0.0, 0.0);
        else if (jo->pviol <= 1e-9L) fail(3, idx, "primal-broken-but-feasible", (double)jo->pviol, rep.violation);
        else if (k < 0 || jo->own[k] <= 1e-9L) fail(3, idx, "primal-names-a-fine-variable", (double)k, rep.violation);
        else if (jo->pviol <= 1e-6L) ambiguous++;
    } else if (rep.stage == JAOS_PROOF_STAGE_DUAL) {
        verdict_dual++;
        const int64_t k = rep.at_col >= 0 ? rep.at_col : rep.at_row >= 0 ? MAXC + rep.at_row : -1;
        if (jo->singular) fail(3, idx, "dual-broken-but-singular", 0.0, 0.0);
        else if (jo->pviol > 1e-6L) fail(3, idx, "dual-broken-but-primal-violated", (double)jo->pviol, 0.0);
        else if (jo->dviol <= 1e-9L) fail(3, idx, "dual-broken-but-dual-feasible", (double)jo->dviol, rep.violation);
        else if (k < 0 || jo->own[k] <= 1e-9L) fail(3, idx, "dual-names-a-fine-variable", (double)k, rep.violation);
        else if (jo->dviol <= 1e-6L) ambiguous++;
    } else {
        fail(3, idx, "broken-with-no-stage", 0.0, 0.0);
    }

    char path[4096];
    snprintf(path, sizeof path, "%s/%s-%" PRId64 ".bas", dir, tag, idx);
    write_foreign(path, g, cs, rs);
    jaos_basis_status rc[MAXC], rr[MAXR];
    if (jaos_read_mps_basis(m, path, rc, rr) != JAOS_OK) {
        fail(2, idx, "foreign-spelling-refused", 0.0, 0.0);
        printf("   %s: %s\n", tag, jaos_model_error(m));
    } else if (memcmp(rc, cs, (size_t)g->nc * sizeof *cs) != 0 ||
               memcmp(rr, rs, (size_t)g->nr * sizeof *rs) != 0) {
        fail(2, idx, "foreign-spelling-differs", 0.0, 0.0);
    }
    remove(path);
    return rep.status;
}

static bool same_report(const jaos_verify_report *a, const jaos_verify_report *b)
{
    return a->status == b->status && a->stage == b->stage &&
           a->bound_bits == b->bound_bits && a->blocks == b->blocks &&
           a->largest_block == b->largest_block && a->terms == b->terms &&
           a->at_row == b->at_row && a->at_col == b->at_col;
}

#define MAXSTR 512

static bool grab_strings(jaos_model *m, const gen *g, char out[][MAXSTR])
{
    const char *s = nullptr;
    for (int64_t j = 0; j < g->nc; j++) {
        if (jaos_exact_col_value(m, j, &s) != JAOS_OK) return false;
        snprintf(out[j], MAXSTR, "%s", s);
    }
    for (int64_t i = 0; i < g->nr; i++) {
        if (jaos_exact_row_dual(m, i, &s) != JAOS_OK) return false;
        snprintf(out[MAXC + i], MAXSTR, "%s", s);
    }
    if (jaos_exact_objective(m, &s) != JAOS_OK) return false;
    snprintf(out[NB], MAXSTR, "%s", s);
    return true;
}

static bool same_strings(const gen *g, char a[][MAXSTR], char b[][MAXSTR])
{
    for (int64_t j = 0; j < g->nc; j++) if (strcmp(a[j], b[j]) != 0) return false;
    for (int64_t i = 0; i < g->nr; i++) if (strcmp(a[MAXC + i], b[MAXC + i]) != 0) return false;
    return strcmp(a[NB], b[NB]) == 0;
}

static jaos_basis_status to_bound(double lo, double hi)
{
    if (isfinite(lo)) return JAOS_BASIS_AT_LOWER;
    if (isfinite(hi)) return JAOS_BASIS_AT_UPPER;
    return JAOS_BASIS_FREE;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: basis RUNS SEED DIR [EVERY]\n");
        return 2;
    }
    const int64_t runs = atoll(argv[1]);
    const uint64_t seed = (uint64_t)atoll(argv[2]);
    const char *dir = argv[3];
    const int64_t every = argc > 4 ? atoll(argv[4]) : 0;
    rng_state = 0x9E3779B97F4A7C15ull ^ (seed * 0xD1B54A32D192ED03ull);
    for (int k = 0; k < 8; k++) (void)rnd();

    int64_t optimal = 0, s2_differs = 0, s3_differs = 0, sources_proved = 0,
            sources_refused = 0, eq_flips = 0, proofs = 0, dumped = 0;
    static char str1[NB + 1][MAXSTR], str2[NB + 1][MAXSTR];

    for (int64_t idx = 0; idx < runs; idx++) {
        gen g;
        generate(&g);
        jaos_model *m = load(&g);
        if (m == nullptr) abort();
        if (jaos_solve(m) != JAOS_OK) abort();
        if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }
        optimal++;
        jaos_basis_status cs[3][MAXC], rs[3][MAXR];
        if (jaos_basis(m, cs[0], rs[0]) != JAOS_OK) abort();
        double x0[MAXC], y0[MAXR], obj0;
        if (jaos_solution(m, x0, nullptr, y0, nullptr) != JAOS_OK) abort();
        if (jaos_objective(m, &obj0) != JAOS_OK) abort();

        {
            jaos_model *p = load(&g);
            if (p == nullptr || jaos_set_algorithm(p, JAOS_ALGORITHM_PRIMAL) != JAOS_OK) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            if (jaos_status_of(p) != JAOS_SOLVE_OPTIMAL) {
                fail(1, idx, "primal-not-optimal", (double)jaos_status_of(p), 0.0);
                memcpy(cs[1], cs[0], sizeof cs[0]);
                memcpy(rs[1], rs[0], sizeof rs[0]);
            } else if (jaos_basis(p, cs[1], rs[1]) != JAOS_OK) {
                abort();
            }
            jaos_model_free(p);
        }
        {
            gen h;
            int64_t cp[MAXC], rp[MAXR];
            permute(&g, &h, cp, rp);
            jaos_model *p = load(&h);
            if (p == nullptr) abort();
            if (jaos_solve(p) != JAOS_OK) abort();
            jaos_basis_status pc[MAXC], pr[MAXR];
            if (jaos_status_of(p) != JAOS_SOLVE_OPTIMAL) {
                fail(1, idx, "permuted-not-optimal", (double)jaos_status_of(p), 0.0);
                memcpy(cs[2], cs[0], sizeof cs[0]);
                memcpy(rs[2], rs[0], sizeof rs[0]);
            } else {
                if (jaos_basis(p, pc, pr) != JAOS_OK) abort();
                for (int64_t k = 0; k < g.nc; k++) cs[2][cp[k]] = pc[k];
                for (int64_t r = 0; r < g.nr; r++) rs[2][rp[r]] = pr[r];
            }
            jaos_model_free(p);
        }
        if (memcmp(cs[1], cs[0], (size_t)g.nc * sizeof cs[0][0]) != 0 ||
            memcmp(rs[1], rs[0], (size_t)g.nr * sizeof rs[0][0]) != 0) s2_differs++;
        if (memcmp(cs[2], cs[0], (size_t)g.nc * sizeof cs[0][0]) != 0 ||
            memcmp(rs[2], rs[0], (size_t)g.nr * sizeof rs[0][0]) != 0) s3_differs++;

        /* P2: JAOS's own file round trip. */
        {
            char path[4096];
            snprintf(path, sizeof path, "%s/own-%" PRId64 ".bas", dir, idx);
            jaos_basis_status rc[MAXC], rr[MAXR];
            if (jaos_write_mps_basis(m, path) != JAOS_OK)
                fail(2, idx, "own-file-not-written", 0.0, 0.0);
            else if (jaos_read_mps_basis(m, path, rc, rr) != JAOS_OK)
                fail(2, idx, "own-file-not-read", 0.0, 0.0);
            else if (memcmp(rc, cs[0], (size_t)g.nc * sizeof rc[0]) != 0 ||
                     memcmp(rr, rs[0], (size_t)g.nr * sizeof rr[0]) != 0)
                fail(2, idx, "own-file-differs", 0.0, 0.0);
            remove(path);
        }

        /* P4: verify against verify_basis on the same basis. */
        jaos_verify_report vr;
        if (jaos_verify(m, &vr) != JAOS_OK) abort();
        const bool have1 = vr.status == JAOS_PROOF_OPTIMAL && grab_strings(m, &g, str1);
        jaos_verify_report br;
        judge_out jo;
        const jaos_proof st0 = submit(&g, m, cs[0], rs[0], idx, "s1", dir, &br, &jo);
        if (!same_report(&vr, &br))
            fail(4, idx, "verify-basis-report-differs", (double)vr.status, (double)br.status);
        else if (have1 && (!grab_strings(m, &g, str2) || !same_strings(&g, str1, str2)))
            fail(4, idx, "verify-basis-strings-differ", 0.0, 0.0);
        {
            double x1[MAXC], y1[MAXR], obj1;
            if (jaos_solution(m, x1, nullptr, y1, nullptr) != JAOS_OK) abort();
            if (jaos_objective(m, &obj1) != JAOS_OK) abort();
            if (memcmp(x1, x0, (size_t)g.nc * sizeof x0[0]) != 0 ||
                memcmp(y1, y0, (size_t)g.nr * sizeof y0[0]) != 0 || obj1 != obj0)
                fail(4, idx, "verify-basis-moved-the-solution", obj0, obj1);
        }
        if (st0 == JAOS_PROOF_BROKEN) fail(1, idx, "s1-broken", (double)br.stage, br.violation);
        if (st0 == JAOS_PROOF_OPTIMAL) sources_proved++; else if (st0 == JAOS_PROOF_REFUSED) sources_refused++;

        /* P6: the same basis twice. */
        {
            jaos_verify_report b2;
            judge_out j2;
            (void)submit(&g, m, cs[0], rs[0], idx, "s1b", dir, &b2, &j2);
            if (!same_report(&br, &b2))
                fail(6, idx, "second-verify-basis-differs", (double)br.status, (double)b2.status);
            else if (have1 && (!grab_strings(m, &g, str2) || !same_strings(&g, str1, str2)))
                fail(6, idx, "second-strings-differ", 0.0, 0.0);
        }

        /* P1 and P5 on the two foreign sources. */
        for (int s = 1; s < 3; s++) {
            jaos_verify_report bs;
            judge_out js;
            char tag[8];
            snprintf(tag, sizeof tag, "s%d", s + 1);
            const jaos_proof st = submit(&g, m, cs[s], rs[s], idx, tag, dir, &bs, &js);
            if (st == JAOS_PROOF_BROKEN) fail(1, idx, s == 1 ? "s2-broken" : "s3-broken", (double)bs.stage, bs.violation);
            if (st == JAOS_PROOF_OPTIMAL) sources_proved++; else if (st == JAOS_PROOF_REFUSED) sources_refused++;
            if (st == JAOS_PROOF_OPTIMAL) {
                char path[4096];
                snprintf(path, sizeof path, "%s/%s-%" PRId64 ".proof", dir, tag, idx);
                jaos_proof_report pr;
                if (jaos_write_proof(m, path) != JAOS_OK)
                    fail(5, idx, "proof-not-written", 0.0, 0.0);
                else if (jaos_check_proof(m, path, &pr) != JAOS_OK)
                    fail(5, idx, "proof-not-checked", 0.0, 0.0);
                else if (!pr.primal || !pr.dual || !pr.objective || pr.kind != JAOS_PROOF_FILE_OPTIMAL)
                    fail(5, idx, "proof-does-not-hold", pr.primal ? 1.0 : 0.0, pr.dual ? 1.0 : 0.0);
                else
                    proofs++;
                remove(path);
            }
        }

        /* Neighbours of S1: the equality flip, pivots, bound swaps, a
         * singular basis. Every one goes through P3. */
        jaos_basis_status nc[MAXC], nr[MAXR];
        bool bad_found = false;
        jaos_basis_status badc[MAXC], badr[MAXR];
        const char *bad_stage = "";
        {
            memcpy(nc, cs[0], sizeof nc);
            memcpy(nr, rs[0], sizeof nr);
            bool any = false;
            for (int64_t i = 0; i < g.nr; i++)
                if (nr[i] != JAOS_BASIS_BASIC && g.rl[i] == g.ru[i]) {
                    nr[i] = nr[i] == JAOS_BASIS_AT_LOWER ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;
                    any = true;
                }
            if (any) {
                eq_flips++;
                jaos_verify_report bf;
                judge_out jf;
                (void)submit(&g, m, nc, nr, idx, "eq", dir, &bf, &jf);
                if (bf.status != br.status || bf.stage != br.stage)
                    fail(3, idx, "equality-flip-changes-verdict", (double)br.status, (double)bf.status);
            }
        }
        for (int t = 0; t < 3; t++) {
            memcpy(nc, cs[0], sizeof nc);
            memcpy(nr, rs[0], sizeof nr);
            int64_t enter = -1, leave = -1;
            for (int tries = 0; tries < 64 && enter < 0; tries++) {
                const int64_t k = ri(0, g.nc + g.nr - 1);
                if (k < g.nc ? nc[k] != JAOS_BASIS_BASIC : nr[k - g.nc] != JAOS_BASIS_BASIC) enter = k;
            }
            for (int tries = 0; tries < 64 && leave < 0; tries++) {
                const int64_t k = ri(0, g.nc + g.nr - 1);
                if (k < g.nc ? nc[k] == JAOS_BASIS_BASIC : nr[k - g.nc] == JAOS_BASIS_BASIC) leave = k;
            }
            if (enter < 0 || leave < 0) continue;
            if (enter < g.nc) nc[enter] = JAOS_BASIS_BASIC; else nr[enter - g.nc] = JAOS_BASIS_BASIC;
            if (leave < g.nc) nc[leave] = to_bound(g.cl[leave], g.cu[leave]);
            else nr[leave - g.nc] = to_bound(g.rl[leave - g.nc], g.ru[leave - g.nc]);
            jaos_verify_report bp;
            judge_out jp;
            const jaos_proof st = submit(&g, m, nc, nr, idx, "pv", dir, &bp, &jp);
            if (!bad_found && st == JAOS_PROOF_BROKEN && !jp.singular &&
                (bp.stage == JAOS_PROOF_STAGE_PRIMAL ? jp.pviol : jp.dviol) > 1e-6L) {
                bad_found = true;
                memcpy(badc, nc, sizeof nc);
                memcpy(badr, nr, sizeof nr);
                bad_stage = bp.stage == JAOS_PROOF_STAGE_PRIMAL ? "primal" : "dual";
            }
        }
        for (int t = 0; t < 2; t++) {
            memcpy(nc, cs[0], sizeof nc);
            memcpy(nr, rs[0], sizeof nr);
            int64_t k = -1;
            for (int tries = 0; tries < 64 && k < 0; tries++) {
                const int64_t c = ri(0, g.nc + g.nr - 1);
                if (c < g.nc) {
                    if (nc[c] != JAOS_BASIS_BASIC && nc[c] != JAOS_BASIS_FREE && g.cl[c] != g.cu[c]) k = c;
                } else {
                    const int64_t i = c - g.nc;
                    if (nr[i] != JAOS_BASIS_BASIC && nr[i] != JAOS_BASIS_FREE &&
                        isfinite(g.rl[i]) && isfinite(g.ru[i]) && g.rl[i] != g.ru[i]) k = c;
                }
            }
            if (k < 0) continue;
            jaos_basis_status *s = k < g.nc ? &nc[k] : &nr[k - g.nc];
            *s = *s == JAOS_BASIS_AT_LOWER ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;
            jaos_verify_report bp;
            judge_out jp;
            (void)submit(&g, m, nc, nr, idx, "bd", dir, &bp, &jp);
        }
        {
            int64_t j = -1, r = -1;
            for (int tries = 0; tries < 64 && j < 0; tries++) {
                const int64_t c = ri(0, g.nc - 1);
                if (g.ap[c + 1] > g.ap[c] && g.ap[c + 1] - g.ap[c] < g.nr) j = c;
            }
            if (j >= 0) {
                bool in[MAXR] = {0};
                for (int64_t p = g.ap[j]; p < g.ap[j + 1]; p++) in[g.ai[p]] = true;
                for (int tries = 0; tries < 64 && r < 0; tries++) {
                    const int64_t i = ri(0, g.nr - 1);
                    if (!in[i]) r = i;
                }
            }
            if (j >= 0 && r >= 0) {
                for (int64_t c = 0; c < g.nc; c++) nc[c] = JAOS_BASIS_AT_LOWER;
                for (int64_t i = 0; i < g.nr; i++) nr[i] = JAOS_BASIS_BASIC;
                nc[j] = JAOS_BASIS_BASIC;
                nr[r] = to_bound(g.rl[r], g.ru[r]);
                jaos_verify_report bp;
                judge_out jp;
                const jaos_proof st = submit(&g, m, nc, nr, idx, "sg", dir, &bp, &jp);
                if (!jp.singular) fail(3, idx, "singular-construction-failed", 0.0, 0.0);
                if (st == JAOS_PROOF_OPTIMAL) fail(3, idx, "singular-proved-optimal", 0.0, 0.0);
            }
        }

        if (every > 0 && idx % every == 0) {
            char path[4096];
            snprintf(path, sizeof path, "%s/m%" PRId64 ".mps", dir, idx);
            if (jaos_write_mps(m, path) != JAOS_OK) abort();
            snprintf(path, sizeof path, "%s/m%" PRId64 ".bas", dir, idx);
            write_foreign(path, &g, cs[2], rs[2]);
            snprintf(path, sizeof path, "%s/m%" PRId64 ".expect", dir, idx);
            FILE *f = fopen(path, "w");
            if (f == nullptr) abort();
            jaos_verify_report b3;
            if (jaos_verify_basis(m, cs[2], rs[2], &b3) != JAOS_OK) abort();
            fprintf(f, "%s\n", b3.status == JAOS_PROOF_OPTIMAL ? "optimal"
                             : b3.status == JAOS_PROOF_REFUSED ? "refused" : "broken");
            if (bad_found) {
                fprintf(f, "broken %s\n", bad_stage);
                snprintf(path, sizeof path, "%s/m%" PRId64 ".bad.bas", dir, idx);
                write_foreign(path, &g, badc, badr);
            }
            fclose(f);
            dumped++;
        }
        jaos_model_free(m);
    }

    int64_t total = 0;
    for (int p = 1; p <= 6; p++) total += broke[p];
    printf("models %" PRId64 " optimal %" PRId64 " s2_differs %" PRId64
           " s3_differs %" PRId64 " sources_proved %" PRId64
           " sources_refused %" PRId64 " eq_flips %" PRId64 " proofs %" PRId64
           " dumped %" PRId64 "\n",
           runs, optimal, s2_differs, s3_differs, sources_proved,
           sources_refused, eq_flips, proofs, dumped);
    printf("judged %" PRId64 " optimal %" PRId64 " rank %" PRId64
           " primal %" PRId64 " dual %" PRId64 " refused %" PRId64
           " ambiguous %" PRId64 "\n",
           judged, verdict_optimal, verdict_rank, verdict_primal,
           verdict_dual, verdict_refused, ambiguous);
    printf("broken P1=%" PRId64 " P2=%" PRId64 " P3=%" PRId64 " P4=%" PRId64
           " P5=%" PRId64 " P6=%" PRId64 " total=%" PRId64 "\n", broke[1],
           broke[2], broke[3], broke[4], broke[5], broke[6], total);
    return total == 0 ? 0 : 1;
}
