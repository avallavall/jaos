/* D273. What the Hadamard bound costs once the basis is made integral.
 *
 * D271 and D272 both bounded log2 |det B| by the column norms of B AS READ,
 * that is, as if every entry were already an integer. Bareiss is a
 * FRACTION-FREE elimination: it is exact because its intermediate entries
 * are minors of an INTEGER matrix. The published basis is not one. An entry
 * like 1.06 is a dyadic rational whose odd mantissa needs 53 bits and whose
 * exponent is -52, and the only way to hand such a row to an integer
 * elimination is to scale it.
 *
 * Scaling by powers of two is exact and carries no rounding, but it is not
 * free of BITS. Scale row i by 2^-s_i, with s_i the smallest dyadic exponent
 * in that row, and every entry of the row becomes an integer. The
 * determinant of the scaled matrix Z is
 *
 *     det Z  =  det B * 2^(-sum_i s_i)
 *
 * and it is Z, not B, that the elimination holds. So the honest capacity
 * question is the Hadamard bound of Z, and every s_i below zero adds to it.
 * A column of Z may then share a power of two across all its entries;
 * dividing it out is exact and lowers the bound, so that pass is here too.
 *
 * The prediction this tests: ken and pds have entries of +-1, so their rows
 * need no scaling and their D271 and D272 numbers stand. Instances carrying
 * real decimal data pay about 52 bits per row, and for those the earlier
 * figures are not budgets the verifier can meet.
 *
 * **The figure that decides is the largest BLOCK, made integral.** A block
 * triangular solve holds one block at a time, and the scaling is per row, so
 * a block of k decimal rows costs about 52k bits on top of its own bound.
 * That is the number this file exists to produce.
 *
 * The basis build, the maximum transversal and the SCCs are D272's code
 * unchanged, taken verbatim so the two instruments read the same matrix and
 * find the same blocks. Everything from `dyadic_exponent` down is new.
 *
 * Not a gate tool. Built and run by run-integral.sh.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The basis, column-wise: n columns, each a list of row indices with values.
 * A basic slack for row i is the unit column e_i. */
typedef struct {
    int64_t  n;         /* rows, and columns: a basis is square */
    int64_t *start;     /* n + 1 */
    int64_t *row;       /* nnz */
    double  *val;       /* nnz */
    int64_t  nnz;
} basis;

static void basis_free(basis *b)
{
    free(b->start);
    free(b->row);
    free(b->val);
    memset(b, 0, sizeof *b);
}

/* Builds B from the published basis. Column order is the structural basic
 * columns in index order, then the basic slacks in row order, which is a
 * fixed rule and not a choice the data can influence. */
static bool basis_build(jaos_model *m, basis *b)
{
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    memset(b, 0, sizeof *b);

    jaos_basis_status *cs = calloc((size_t)(nc > 0 ? nc : 1), sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)(nr > 0 ? nr : 1), sizeof *rs);
    if (cs == nullptr || rs == nullptr) { free(cs); free(rs); return false; }
    if (jaos_basis(m, cs, rs) != JAOS_OK) { free(cs); free(rs); return false; }

    int64_t ncols = 0, nnz = 0;
    for (int64_t j = 0; j < nc; j++)
        if (cs[j] == JAOS_BASIS_BASIC) {
            ncols++;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                if (m->a_value[k] != 0.0)
                    nnz++;
        }
    for (int64_t i = 0; i < nr; i++)
        if (rs[i] == JAOS_BASIS_BASIC) { ncols++; nnz++; }

    if (ncols != nr) {           /* D257 promises this; check rather than assume */
        fprintf(stderr, "basis is %lld columns for %lld rows\n",
                (long long)ncols, (long long)nr);
        free(cs); free(rs);
        return false;
    }

    b->n = nr;
    b->nnz = nnz;
    b->start = calloc((size_t)nr + 1, sizeof *b->start);
    b->row   = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->row);
    b->val   = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->val);
    if (b->start == nullptr || b->row == nullptr || b->val == nullptr) {
        basis_free(b); free(cs); free(rs); return false;
    }

    int64_t c = 0, p = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] != JAOS_BASIS_BASIC)
            continue;
        b->start[c] = p;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            if (m->a_value[k] == 0.0)
                continue;
            b->row[p] = m->a_index[k];
            b->val[p] = m->a_value[k];
            p++;
        }
        c++;
    }
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i] != JAOS_BASIS_BASIC)
            continue;
        b->start[c] = p;
        b->row[p] = i;
        b->val[p] = 1.0;
        p++;
        c++;
    }
    b->start[c] = p;

    free(cs);
    free(rs);
    return true;
}

/* ------------------------------------------------- 1. maximum transversal
 *
 * match_col[j] is the row column j is matched to, or -1. match_row[i] is the
 * column matched to row i, or -1. Augmenting path from one column, depth
 * first, rows tried in the order the column stores them, which is row-index
 * order because the model's CSC is. `seen` is stamped per search so it is
 * not cleared n times. */
static bool augment(const basis *b, int64_t j, int64_t *match_col,
                    int64_t *match_row, int64_t *seen, int64_t stamp,
                    int64_t *stack_col, int64_t *stack_pos, int64_t *cheap)
{
    /* Iterative depth-first search, because a chain can be n long. */
    int64_t top = 0;
    stack_col[0] = j;
    stack_pos[0] = b->start[j];
    bool fresh = true;                   /* the top column was just pushed */

    while (top >= 0) {
        const int64_t cj = stack_col[top];

        /* Look ahead before descending anywhere: a row nothing holds yet,
         * anywhere in this column, ends the search here. This is the
         * lookahead of Duff's MC21A, and it is here because the cited
         * algorithm has it, not because it was measured to buy anything.
         * **It buys nothing measurable on this population.** The bound in
         * the paper is on the worst case; on these bases the matching is
         * already a small fraction of a run whose cost is the solve
         * (ken-13: 59.57 s solving, 0.00 s from the read through the basis
         * build). Keep it for the worst case a future basis might have.
         *
         * `cheap[cj]` remembers how far the scan got. That is sound because
         * a matched row is never unmatched again, so the lookahead costs
         * O(nnz) over the whole matching rather than O(nnz) per search.
         *
         * The answer does not depend on this. Pothen and Fan prove the fine
         * decomposition is the same for every maximum transversal, and the
         * 81 netlib instances the pre-lookahead code had finished come out
         * byte-identical under it. */
        if (fresh) {
            fresh = false;
            int64_t got = -1, k = cheap[cj];
            for (; k < b->start[cj + 1]; k++)
                if (match_row[b->row[k]] < 0) { got = b->row[k]; break; }
            cheap[cj] = k;
            if (got >= 0) {
                /* Unwind the stack, matching each column to the row it was
                 * trying when it descended. */
                int64_t row = got;
                for (int64_t t = top; t >= 0; t--) {
                    const int64_t col = stack_col[t];
                    const int64_t prev = match_col[col];
                    match_col[col] = row;
                    match_row[row] = col;
                    row = prev;          /* the row this column gave up */
                }
                return true;
            }
        }

        if (stack_pos[top] >= b->start[cj + 1]) {
            top--;                       /* exhausted; back up */
            if (top >= 0)
                stack_pos[top]++;
            continue;
        }
        const int64_t i = b->row[stack_pos[top]];
        if (seen[i] == stamp) {
            stack_pos[top]++;
            continue;
        }
        seen[i] = stamp;
        /* The lookahead has already proved every row of this column is
         * held, so this always descends into the column holding row i. */
        top++;
        stack_col[top] = match_row[i];
        stack_pos[top] = b->start[match_row[i]];
        fresh = true;
    }
    return false;
}

static bool transversal(const basis *b, int64_t *match_col, int64_t *match_row)
{
    const int64_t n = b->n;
    for (int64_t j = 0; j < n; j++) match_col[j] = -1;
    for (int64_t i = 0; i < n; i++) match_row[i] = -1;

    /* Greedy first pass, in index order: Duff's cheap start. */
    for (int64_t j = 0; j < n; j++)
        for (int64_t k = b->start[j]; k < b->start[j + 1]; k++) {
            const int64_t i = b->row[k];
            if (match_row[i] < 0) {
                match_col[j] = i;
                match_row[i] = j;
                break;
            }
        }

    int64_t *seen  = calloc((size_t)n, sizeof *seen);
    int64_t *sc    = calloc((size_t)n + 1, sizeof *sc);
    int64_t *sp    = calloc((size_t)n + 1, sizeof *sp);
    int64_t *cheap = calloc((size_t)n, sizeof *cheap);
    if (seen == nullptr || sc == nullptr || sp == nullptr || cheap == nullptr) {
        free(seen); free(sc); free(sp); free(cheap);
        return false;
    }
    for (int64_t i = 0; i < n; i++) seen[i] = -1;
    for (int64_t c = 0; c < n; c++) cheap[c] = b->start[c];

    bool ok = true;
    for (int64_t j = 0; j < n && ok; j++)
        if (match_col[j] < 0)
            ok = augment(b, j, match_col, match_row, seen, j, sc, sp, cheap);

    free(seen); free(sc); free(sp); free(cheap);
    return ok;
}

/* --------------------------------------------------------------- 2. SCCs
 *
 * After the matching, node i is the pair (row i, the column matched to it).
 * There is an edge i -> j when the column matched to i has a nonzero in row
 * j: that is "solving for i needs j". Tarjan, iterative. */
typedef struct {
    int64_t *comp;      /* component id per node, in reverse topological order */
    int64_t  ncomp;
    int64_t  largest;
} sccs;

static bool tarjan(const basis *b, const int64_t *match_row, sccs *out)
{
    const int64_t n = b->n;
    int64_t *index = malloc((size_t)n * sizeof *index);
    int64_t *low   = malloc((size_t)n * sizeof *low);
    int64_t *stk   = malloc((size_t)n * sizeof *stk);
    bool    *on    = calloc((size_t)n, sizeof *on);
    int64_t *comp  = malloc((size_t)n * sizeof *comp);
    /* the depth-first stack: node, and how far through its edges */
    int64_t *dn = malloc(((size_t)n + 1) * sizeof *dn);
    int64_t *dp = malloc(((size_t)n + 1) * sizeof *dp);
    if (!index || !low || !stk || !on || !comp || !dn || !dp) {
        free(index); free(low); free(stk); free(on); free(comp);
        free(dn); free(dp);
        return false;
    }
    for (int64_t i = 0; i < n; i++) { index[i] = -1; comp[i] = -1; }

    int64_t next = 0, sp = 0, nc = 0;

    for (int64_t root = 0; root < n; root++) {
        if (index[root] >= 0)
            continue;
        int64_t top = 0;
        dn[0] = root;
        dp[0] = -1;                      /* -1 means "not entered yet" */

        while (top >= 0) {
            const int64_t v = dn[top];
            if (dp[top] < 0) {
                index[v] = low[v] = next++;
                stk[sp++] = v;
                on[v] = true;
                dp[top] = b->start[match_row[v]];
            } else {
                /* returning from a child: take its lowlink */
                const int64_t w = dn[top + 1];
                if (low[w] < low[v])
                    low[v] = low[w];
                dp[top]++;
            }

            bool descended = false;
            while (dp[top] < b->start[match_row[v] + 1]) {
                const int64_t w = b->row[dp[top]];
                if (index[w] < 0) {
                    top++;
                    dn[top] = w;
                    dp[top] = -1;
                    descended = true;
                    break;
                }
                if (on[w] && index[w] < low[v])
                    low[v] = index[w];
                dp[top]++;
            }
            if (descended)
                continue;

            if (low[v] == index[v]) {
                int64_t size = 0;
                for (;;) {
                    const int64_t w = stk[--sp];
                    on[w] = false;
                    comp[w] = nc;
                    size++;
                    if (w == v)
                        break;
                }
                if (size > out->largest)
                    out->largest = size;
                nc++;
            }
            top--;
        }
    }

    free(index); free(low); free(stk); free(on); free(dn); free(dp);
    out->comp = comp;
    out->ncomp = nc;
    return true;
}

/* ------------------------------------------------------- dyadic exponents
 *
 * Every finite nonzero double is m * 2^e with m an odd integer of at most 53
 * bits. frexp gives the value as f * 2^E with f in [0.5, 1), so f * 2^53 is
 * that integer and E - 53 is the exponent before the odd part is taken out.
 * This is the decomposition jm_dyadic_from_double does, and it is exact: no
 * rounding happens anywhere in it. */
static int64_t dyadic_exponent(double v)
{
    int E = 0;
    const double f = frexp(v, &E);
    int64_t m = (int64_t)ldexp(fabs(f), 53);
    int64_t e = (int64_t)E - 53;
    while (m != 0 && (m & 1) == 0) { m >>= 1; e++; }
    return e;
}

/* Smallest dyadic exponent in each row, clamped at zero: only a negative
 * exponent costs anything, and a row of integers is left alone. That clamp
 * is what keeps ken and pds free. */
static void row_shifts(const basis *b, int64_t *shift)
{
    for (int64_t i = 0; i < b->n; i++)
        shift[i] = 1;                    /* 1 marks "no entry seen yet" */
    for (int64_t j = 0; j < b->n; j++)
        for (int64_t k = b->start[j]; k < b->start[j + 1]; k++) {
            const int64_t i = b->row[k];
            const int64_t e = dyadic_exponent(b->val[k]);
            if (shift[i] == 1 || e < shift[i])
                shift[i] = e;
        }
    for (int64_t i = 0; i < b->n; i++)
        if (shift[i] > 0)
            shift[i] = 0;
}

/* log2 of a column's 2-norm after each entry i is multiplied by
 * 2^(-shift[i]) and the whole column by 2^(-sub). Computed in the log
 * domain: 2^52 per row over a thousand rows leaves a double's range at
 * once. `only` restricts the sum to one block, or is null for the whole
 * matrix. Returns -INFINITY when the column has nothing in range. */
static double col_log_norm(const basis *b, int64_t j, const int64_t *shift,
                           int64_t sub, const int64_t *comp, int64_t which)
{
    double worst = -INFINITY;
    for (int64_t k = b->start[j]; k < b->start[j + 1]; k++) {
        const int64_t i = b->row[k];
        if (comp != nullptr && comp[i] != which)
            continue;
        const double t = log2(fabs(b->val[k]))
                       - (shift != nullptr ? (double)shift[i] : 0.0)
                       - (double)sub;
        if (t > worst)
            worst = t;
    }
    if (worst == -INFINITY)
        return -INFINITY;

    double ss = 0.0;
    for (int64_t k = b->start[j]; k < b->start[j + 1]; k++) {
        const int64_t i = b->row[k];
        if (comp != nullptr && comp[i] != which)
            continue;
        const double t = log2(fabs(b->val[k]))
                       - (shift != nullptr ? (double)shift[i] : 0.0)
                       - (double)sub;
        ss += exp2(2.0 * (t - worst));
    }
    return worst + 0.5 * log2(ss);
}

/* The power of two the whole of column j shares once the rows are scaled.
 * Restricted to one block when `comp` is given, because a block's own
 * elimination never sees the entries outside it. */
static int64_t col_common(const basis *b, int64_t j, const int64_t *shift,
                          const int64_t *comp, int64_t which)
{
    int64_t g = -1;
    bool any = false;
    for (int64_t k = b->start[j]; k < b->start[j + 1]; k++) {
        const int64_t i = b->row[k];
        if (comp != nullptr && comp[i] != which)
            continue;
        const int64_t e = dyadic_exponent(b->val[k]) - shift[i];
        if (!any || e < g) { g = e; any = true; }
    }
    return (any && g > 0) ? g : 0;
}

/* Per-block bound, with the same restriction D272 uses: node i owns column
 * match_row[i], and only the entries whose row is in the same component
 * enter that block's determinant. `shift` null gives D272's raw figure. */
static void block_bits(const basis *b, const int64_t *match_row,
                       const int64_t *comp, int64_t n, int64_t ncomp,
                       const int64_t *shift, double *worst)
{
    double *per = calloc((size_t)(ncomp > 0 ? ncomp : 1), sizeof *per);
    *worst = 0.0;
    if (per == nullptr)
        return;

    for (int64_t i = 0; i < n; i++) {
        const int64_t j = match_row[i];
        const int64_t g = shift != nullptr
                        ? col_common(b, j, shift, comp, comp[i]) : 0;
        const double lg = col_log_norm(b, j, shift, g, comp, comp[i]);
        if (lg > -INFINITY)
            per[comp[i]] += lg;
    }
    for (int64_t c = 0; c < ncomp; c++)
        if (per[c] > *worst)
            *worst = per[c];
    free(per);
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    const double cap = 32.0 * (double)JM_EXACT_LIMBS;

    printf("# the Hadamard bound once the basis is made integral (D273)\n");
    printf("# capacity: JM_EXACT_LIMBS=%d, %.0f bits\n",
           (int)JM_EXACT_LIMBS, cap);
    printf("# rawwhole:  D271's figure, the basis as read\n");
    printf("# rawblock:  D272's figure, its largest block as read\n");
    printf("# intwhole:  the whole basis made integral\n");
    printf("# intblock:  its largest block made integral -- THE budget\n");
    printf("# shift:     the largest row scale needed, in bits\n");
    printf("# nonint:    rows that needed a scale at all\n");
    printf("# was/now:   did D272 say it fits, and does it fit once integral\n\n");
    printf("%-14s %7s %10s %10s %11s %11s %6s %7s %4s %4s\n",
           "instance", "rows", "rawwhole", "rawblock", "intwhole",
           "intblock", "shift", "nonint", "was", "now");

    int64_t seen = 0, fit_raw = 0, fit_int = 0, lost = 0, clean = 0;

    for (int a = 1; a < argc; a++) {
        const char *path = argv[a];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;

        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) continue;
        if (jaos_read_mps(m, path) != JAOS_OK) { jaos_model_free(m); continue; }
        if (jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }

        basis b;
        if (!basis_build(m, &b)) { jaos_model_free(m); continue; }

        int64_t *mc = malloc((size_t)(b.n > 0 ? b.n : 1) * sizeof *mc);
        int64_t *mr = malloc((size_t)(b.n > 0 ? b.n : 1) * sizeof *mr);
        if (mc == nullptr || mr == nullptr || !transversal(&b, mc, mr)) {
            printf("%-14s no transversal\n", name);
            free(mc); free(mr); basis_free(&b); jaos_model_free(m);
            continue;
        }
        sccs s = {0};
        if (!tarjan(&b, mr, &s)) {
            printf("%-14s tarjan out of memory\n", name);
            free(mc); free(mr); basis_free(&b); jaos_model_free(m);
            continue;
        }

        int64_t *shift = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *shift);
        if (shift == nullptr) {
            free(s.comp); free(mc); free(mr);
            basis_free(&b); jaos_model_free(m);
            continue;
        }
        row_shifts(&b, shift);

        double rawwhole = 0.0, intwhole = 0.0;
        for (int64_t j = 0; j < b.n; j++) {
            const double r = col_log_norm(&b, j, nullptr, 0, nullptr, 0);
            if (r > -INFINITY) rawwhole += r;
            const int64_t g = col_common(&b, j, shift, nullptr, 0);
            const double t = col_log_norm(&b, j, shift, g, nullptr, 0);
            if (t > -INFINITY) intwhole += t;
        }

        double rawblock = 0.0, intblock = 0.0;
        block_bits(&b, mr, s.comp, b.n, s.ncomp, nullptr, &rawblock);
        block_bits(&b, mr, s.comp, b.n, s.ncomp, shift,   &intblock);

        int64_t worst_shift = 0, nonint = 0;
        for (int64_t i = 0; i < b.n; i++)
            if (shift[i] < 0) {
                nonint++;
                if (-shift[i] > worst_shift)
                    worst_shift = -shift[i];
            }

        seen++;
        const bool was = rawblock <= cap, now = intblock <= cap;
        if (was) fit_raw++;
        if (now) fit_int++;
        if (was && !now) lost++;
        if (nonint == 0) clean++;

        printf("%-14s %7lld %10.1f %10.1f %11.1f %11.1f %6lld %7lld %4s %4s\n",
               name, (long long)b.n, rawwhole, rawblock, intwhole, intblock,
               (long long)worst_shift, (long long)nonint,
               was ? "yes" : "NO", now ? "yes" : "NO");
        fflush(stdout);

        free(shift); free(s.comp); free(mc); free(mr);
        basis_free(&b);
        jaos_model_free(m);
    }

    printf("\n%lld bases\n", (long long)seen);
    printf("already integral: %lld\n", (long long)clean);
    printf("blocks fit as read: %lld;  blocks fit once integral: %lld;  "
           "lost to the scaling: %lld\n",
           (long long)fit_raw, (long long)fit_int, (long long)lost);
    return 0;
}
