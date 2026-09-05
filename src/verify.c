/* Exact verification of a final basis (D274).
 *
 * `jaos_check_solution` judges the published answer in floating point against
 * a tolerance. This judges the published BASIS with no tolerance anywhere: it
 * rebuilds the basic values and the duals over the integers, and either
 * proves the answer optimal or names the row or column that breaks it.
 *
 * ---------------------------------------------------------------- the system
 *
 * With a slack per row the model is [A | -I] [x; s] = 0, the columns of A
 * carrying the caller's bounds and the slack for row i carrying that row's.
 * A basis is num_row of those columns; every other variable rests at a bound
 * the model declared. So
 *
 *     B x_B = -N x_N        gives the basic values,
 *     B' y  =  c_B          gives the duals,
 *
 * and the answer is optimal exactly when x_B lies inside its bounds and every
 * nonbasic reduced cost points into the model. Complementary slackness needs
 * no separate check: a basic variable's reduced cost is zero by construction
 * and a nonbasic one is at a bound.
 *
 * ------------------------------------------------------- why over the integers
 *
 * Bareiss's fraction-free elimination is exact because its intermediate
 * entries are minors of an INTEGER matrix
 * (`docs/research/exact-verification.md` section 1). A basis entry is a
 * double, which is a dyadic rational: `1.06` has a 53-bit odd mantissa and an
 * exponent of -52. So each row is scaled by `2^-s`, with `s` the smallest
 * exponent in it. That is exact and adds no rounding. It does add bits, 53 to
 * 72 per row, and D273 measured the cost: the capacity question is the bound
 * on the SCALED matrix, and it refuses 24 of the 110 gate bases that the
 * unscaled bound accepted.
 *
 * ------------------------------------------------------ why block triangular
 *
 * The blocks do NOT buy width. The answer's denominator is `det B` whichever
 * way it is computed, so the whole-basis bound is what refuses. They buy
 * memory and time. A `jm_bigint` is a fixed-size struct, so a dense
 * elimination on n rows holds n*n of them whatever the entries are worth, and
 * `sierra` at 1227 rows would want 780 MiB when its largest block is a single
 * row. D272 measured that: `ken-11` and `ken-13` come out fully triangular
 * and `ken-18` is 105000 blocks over 105127 rows.
 *
 * Two deterministic stages, both cited and neither randomized: a maximum
 * transversal (Duff, ACM TOMS 7(3):315-330, 1981) then the strongly connected
 * components (Pothen and Fan, ACM TOMS 16(4):303-324, 1990) by Tarjan's
 * algorithm, iterative because 105127 nodes would take the stack down. Every
 * loop runs in index order and every candidate list is scanned in index
 * order, so the permutation is a function of the matrix alone and D8 holds.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* What one call may hold for a single block's dense elimination. A block of
 * k rows holds k*(k+1) numbers, and a `jm_bigint` is JM_EXACT_LIMBS 32-bit
 * limbs plus a sign, so this ceiling is arithmetic rather than a fitted
 * constant: at 128 limbs a number is 520 bytes and 512 MiB is a block of 990
 * rows. It is here so an oversized block is refused before the allocation
 * rather than by it. 536870912 is 512 * 1024 * 1024, written out because
 * `docs/tolerances.md` states the same number and `make record-check`
 * compares the two. */
constexpr size_t VERIFY_BLOCK_BYTES = 536870912;

/* **There is no safety margin below the capacity, deliberately.** The test is
 * `bound_bits > capacity_bits` and nothing else. An earlier draft kept 64 bits
 * of slack to cover the rounding in a floating-point bound; the bound is
 * integer arithmetic now and has no rounding to cover, and a margin with no
 * measurement on either side is the kind of constant this project loses weeks
 * to.
 *
 * The test is still not a guarantee, and the header says so. `whole + worst`
 * bounds the MATRIX minors; the right-hand side column an elimination carries
 * also holds model bound values and the accumulated denominator, and neither
 * is in the bound at all. A basis that passes here can still run out of limbs
 * during the work, which is a refusal too, with `terms` saying how far it
 * got. Widening a margin would not close that gap; only bounding the
 * right-hand side would, and nobody has. */

/* ------------------------------------------------------------- the basis
 *
 * Column-wise. Column c is the c-th basic variable: the structural basic
 * columns in index order, then the basic slacks in row order, which is a
 * fixed rule and not a choice the data can influence. `who[c]` says which
 * variable it is: j for structural column j, -(i + 1) for row i's slack. */
typedef struct {
    int64_t    n;
    int64_t   *start;    /* n + 1 */
    int64_t   *row;      /* nnz, the model row each entry sits in */
    double    *dval;     /* nnz, the entry as the model holds it */
    jm_bigint *zval;     /* nnz, the same entry scaled to an integer */
    int64_t   *who;      /* n */
    int64_t   *shift;    /* n, per MODEL ROW: the scale exponent, <= 0 */
    int64_t   *cshift;   /* n, per BASIS COLUMN: the power of two the whole
                            column shares once the rows are scaled, >= 0,
                            and divided out of `zval` */
    int64_t    nnz;
} vbasis;

static void vbasis_free(vbasis *b)
{
    free(b->start);
    free(b->row);
    free(b->dval);
    free(b->zval);
    free(b->who);
    free(b->shift);
    free(b->cshift);
    memset(b, 0, sizeof *b);
}

/* The dyadic exponent of a finite nonzero double, mantissa odd. frexp gives
 * v = f * 2^E with f in [0.5, 1), so f * 2^53 is an integer of at most 53
 * bits and E - 53 is the exponent before the odd part is taken out. Nothing
 * here rounds. */
static int64_t dyadic_exponent(double v)
{
    int e = 0;
    const double f = frexp(v, &e);
    int64_t mant = (int64_t)ldexp(fabs(f), 53);
    int64_t ex = (int64_t)e - 53;
    while (mant != 0 && (mant & 1) == 0) { mant >>= 1; ex++; }
    return ex;
}

static int64_t dyadic_mantissa(double v)
{
    int e = 0;
    const double f = frexp(v, &e);
    int64_t mant = (int64_t)ldexp(fabs(f), 53);
    while (mant != 0 && (mant & 1) == 0)
        mant >>= 1;
    return v < 0.0 ? -mant : mant;
}

static bool vbasis_build(const jaos_model *m, vbasis *b)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    memset(b, 0, sizeof *b);

    int64_t ncols = 0, nnz = 0;
    for (int64_t j = 0; j < nc; j++)
        if (m->sol_col_status[j] == JAOS_BASIS_BASIC) {
            ncols++;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                if (m->a_value[k] != 0.0)
                    nnz++;
        }
    for (int64_t i = 0; i < nr; i++)
        if (m->sol_row_status[i] == JAOS_BASIS_BASIC) { ncols++; nnz++; }

    /* D257 promises exactly num_row basics on every solve. Check rather than
     * assume: a verifier that assumed it would factor a matrix that is not
     * square and report something about it. */
    if (ncols != nr)
        return false;

    b->n = nr;
    b->nnz = nnz;
    b->start = calloc((size_t)nr + 1, sizeof *b->start);
    b->row   = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->row);
    b->dval  = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->dval);
    b->zval  = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->zval);
    b->who   = calloc((size_t)(nr > 0 ? nr : 1), sizeof *b->who);
    b->shift = calloc((size_t)(nr > 0 ? nr : 1), sizeof *b->shift);
    b->cshift = calloc((size_t)(nr > 0 ? nr : 1), sizeof *b->cshift);
    if (b->start == nullptr || b->row == nullptr || b->dval == nullptr ||
        b->zval == nullptr || b->who == nullptr || b->shift == nullptr ||
        b->cshift == nullptr) {
        vbasis_free(b);
        return false;
    }

    int64_t c = 0, p = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (m->sol_col_status[j] != JAOS_BASIS_BASIC)
            continue;
        b->start[c] = p;
        b->who[c] = j;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            if (m->a_value[k] == 0.0)
                continue;
            b->row[p] = m->a_index[k];
            b->dval[p] = m->a_value[k];
            p++;
        }
        c++;
    }
    for (int64_t i = 0; i < nr; i++) {
        if (m->sol_row_status[i] != JAOS_BASIS_BASIC)
            continue;
        b->start[c] = p;
        b->who[c] = -(i + 1);
        b->row[p] = i;
        b->dval[p] = -1.0;               /* the slack column of [A | -I] */
        p++;
        c++;
    }
    b->start[c] = p;
    return true;
}

/* The two scalings, both exact powers of two and both applied to the matrix
 * the elimination actually holds.
 *
 * ROWS first: `shift[i]` is the smallest dyadic exponent in row i, clamped at
 * zero, and multiplying the row by `2^-shift[i]` makes every entry an
 * integer. Only a negative exponent costs anything, so a row of integers is
 * left alone, which is what keeps every `ken` and `pds` basis free (D273).
 * Scaling a row does not move the solution: `D B x = D r` is the same system.
 *
 * COLUMNS second: `cshift[c]` is the power of two the whole column shares
 * once its rows are scaled, so dividing it out is exact. **This one DOES
 * move things**, and both are put back where they belong: `B G w = r` with
 * `G = diag(2^-cshift)` gives `x = G w`, and the transpose's right-hand side
 * takes the same factor because `(B G)' = G B'`.
 *
 * Leaving the column factor in the matrix while subtracting it from the bound
 * is what a bound has to be checked against the code for: it would understate
 * `log2 |det|` by the sum of the factors, and on a structured basis that is
 * about six bits a column. */
static bool vbasis_scale(vbasis *b)
{
    for (int64_t i = 0; i < b->n; i++)
        b->shift[i] = 1;                     /* 1 marks "nothing seen yet" */
    for (int64_t k = 0; k < b->nnz; k++) {
        const int64_t i = b->row[k];
        const int64_t e = dyadic_exponent(b->dval[k]);
        if (b->shift[i] == 1 || e < b->shift[i])
            b->shift[i] = e;
    }
    for (int64_t i = 0; i < b->n; i++)
        if (b->shift[i] > 0)
            b->shift[i] = 0;

    for (int64_t k = 0; k < b->nnz; k++) {
        const int64_t i = b->row[k];
        const int64_t e = dyadic_exponent(b->dval[k]) - b->shift[i];
        jm_bigint_set_i64(&b->zval[k], dyadic_mantissa(b->dval[k]));
        if (!jm_bigint_shl(&b->zval[k], &b->zval[k], e))
            return false;
    }

    /* Every entry of column c now has 2-adic valuation `e - shift[i]`, so the
     * minimum of those divides all of them and the shift below drops no set
     * bit. An empty column leaves cshift at zero and is refused later by the
     * transversal, not here. */
    for (int64_t c = 0; c < b->n; c++) {
        int64_t g = -1;
        for (int64_t k = b->start[c]; k < b->start[c + 1]; k++) {
            const int64_t i = b->row[k];
            const int64_t e = dyadic_exponent(b->dval[k]) - b->shift[i];
            if (g < 0 || e < g)
                g = e;
        }
        b->cshift[c] = (g > 0) ? g : 0;
        if (b->cshift[c] == 0)
            continue;
        for (int64_t k = b->start[c]; k < b->start[c + 1]; k++)
            jm_nat_shr(&b->zval[k].mag, &b->zval[k].mag, b->cshift[c]);
    }
    return true;
}

/* log2 of the 2-norm of column c of the scaled matrix, in the log domain
 * because 2^52 per row over a thousand rows leaves a double's range at once.
 * `comp` restricts the sum to one block, or is null for the whole matrix.
 *
 * Once the rows are scaled a column may still share a power of two across
 * all its entries. Dividing it out is exact, it only lowers the bound, and it
 * costs one extra pass, so it is done here rather than left on the table. */
/* The smallest e with 2^e >= x, for x >= 1. */
static int64_t ceil_log2_i64(int64_t x)
{
    int64_t e = 0;
    while (((int64_t)1 << e) < x)
        e++;
    return e;
}

/* --------------------------------------------------- the bound's arithmetic
 *
 * The bound is a product over the basis columns, and it has to be computed
 * without `log2`, because it decides a refusal and `log2` is not pinned
 * across C libraries: a verdict that came from it could differ between two
 * machines, which is the one thing D8 does not allow.
 *
 * Summing whole bit counts is the obvious way and it is far too coarse.
 * `jm_nat_bits` overstates `log2 v` by up to a full bit, so a basis of
 * `ken-11`'s 14694 columns of plus and minus one collects 14694 bits of slack
 * against a 4096-bit budget: its bound reads 23365 where the true figure is
 * 7855, and instances that fit comfortably are refused.
 *
 * So the product is kept as it is built: `m * 2^e` with `m` held to 256 bits
 * and **rounded up** whenever bits are shifted out. Rounding up is what keeps
 * it an upper bound. The slack is then one part in 2^255 per column, which
 * over a hundred thousand columns is still nothing, plus the one bit at the
 * end from reading `jm_nat_bits` of the final mantissa. */
constexpr int64_t VERIFY_PROD_BITS = 256;

typedef struct {
    jm_nat  m;
    int64_t e;
} vprod;

/* v as `m * 2^e` with m at most VERIFY_PROD_BITS bits, rounded UP. */
static void nat_top(const jm_nat *v, jm_nat *m, int64_t *e)
{
    const int64_t b = jm_nat_bits(v);
    if (b <= VERIFY_PROD_BITS) {
        *m = *v;
        *e = 0;
        return;
    }
    const int64_t d = b - VERIFY_PROD_BITS;
    jm_nat sh, back, one;
    jm_nat_shr(&sh, v, d);
    /* Anything shifted out has to push the mantissa up by one, or the
     * product stops bounding what it is meant to bound. The shift back
     * cannot fail: it restores a value no wider than v itself. */
    if (jm_nat_shl(&back, &sh, d) && jm_nat_cmp(&back, v) != 0) {
        jm_nat_set_u64(&one, 1);
        (void)jm_nat_add(&sh, &sh, &one);
    }
    *m = sh;
    *e = d;
}

static void vprod_one(vprod *p)
{
    jm_nat_set_u64(&p->m, 1);
    p->e = 0;
}

/* p *= v. False only if the 256-by-256-bit product does not fit, which it
 * always does at any sane JM_EXACT_LIMBS; the check is here because the
 * multiply can report it and swallowing that would be the same mistake as
 * swallowing an out-of-limbs comparison. */
static bool vprod_mul(vprod *p, const jm_nat *v)
{
    jm_nat vm, t, tm;
    int64_t ve = 0, te = 0;
    nat_top(v, &vm, &ve);
    if (!jm_nat_mul(&t, &p->m, &vm))
        return false;
    nat_top(&t, &tm, &te);
    p->m = tm;
    p->e += ve + te;
    return true;
}

/* An upper bound on log2 of the product. */
static int64_t vprod_log2(const vprod *p)
{
    return jm_nat_bits(&p->m) + p->e;
}

/* TWICE an upper bound, in bits, on log2 of the 2-norm of column c of the
 * scaled integer matrix. `comp` restricts it to one block, or is null for the
 * whole matrix.
 *
 * **Integer arithmetic, not `log2`, and that is not fussiness.** This number
 * decides a refusal, and `log2` and `exp2` are not pinned across C libraries,
 * so a verdict computed from them could differ between two machines. That is
 * the one thing D8 does not allow.
 *
 * The bound is `||col||_2 <= sqrt(nnz) * max |entry|`, so
 *
 *     2 * log2 ||col||  <=  2 * bits(max)  +  ceil(log2 nnz)
 *
 * and `jm_nat_bits(v) > log2 v` for every nonzero v. It returns the doubled
 * form so the caller sums over columns and halves ONCE: halving per column
 * would round up n times and leave about n/2 bits of slack against a 4096-bit
 * budget, which on a thousand columns moves the verdict for a reason that is
 * not the matrix.
 *
 * The sum of squares would be tighter and it is the wrong instrument: it
 * needs twice the bits of the norm, so it runs out of limbs at a column norm
 * of about 2048 and would refuse a basis whose determinant fits with room to
 * spare. Nothing here can overflow.
 *
 * `zval` already has `cshift[c]` divided out, so nothing is subtracted here.
 * Doing that subtraction against a matrix that still carried the factor is
 * how the bound stopped being one. */
static bool col_norm_sq(const vbasis *b, int64_t c, const int64_t *comp,
                        int64_t which, jm_nat *out)
{
    jm_nat ss, sq;
    jm_nat_set_zero(&ss);
    int64_t widest = 0, count = 0;
    bool exact = true;
    for (int64_t k = b->start[c]; k < b->start[c + 1]; k++) {
        const int64_t i = b->row[k];
        if (comp != nullptr && comp[i] != which)
            continue;
        const int64_t w = jm_nat_bits(&b->zval[k].mag);
        if (w > widest)
            widest = w;
        count++;
        if (exact &&
            (!jm_nat_mul(&sq, &b->zval[k].mag, &b->zval[k].mag) ||
             !jm_nat_add(&ss, &ss, &sq)))
            exact = false;
    }
    if (count == 0 || widest == 0) {
        jm_nat_set_u64(out, 1);         /* an empty column multiplies by one */
        return true;
    }
    if (exact) {
        *out = ss;
        return true;
    }
    /* The exact sum of squares needs twice the bits of the norm, so it can
     * run out at a column norm around 2048 while the norm itself still fits.
     * Then fall back on `||col|| <= sqrt(nnz) * max`, which needs no squaring
     * at all. It costs that one column up to half a bit of slack, and a
     * column that reaches this is already most of the whole budget. */
    jm_nat_set_u64(out, 1);
    if (!jm_nat_shl(out, out, 2 * widest + ceil_log2_i64(count)))
        return false;
    return true;
}

/* ------------------------------------------- 1. the maximum transversal
 *
 * `match_col[c]` is the row column c is matched to, `match_row[i]` the column
 * matched to row i, -1 for neither. Duff's MC21A: a greedy cheap start, then
 * an augmenting path per unmatched column with a lookahead in front of the
 * descent. `cheap[c]` remembers how far the lookahead got, which is sound
 * because a matched row is never unmatched again. */
static bool augment(const vbasis *b, int64_t c0, int64_t *match_col,
                    int64_t *match_row, int64_t *seen, int64_t stamp,
                    int64_t *stack_col, int64_t *stack_pos, int64_t *cheap)
{
    int64_t top = 0;
    stack_col[0] = c0;
    stack_pos[0] = b->start[c0];
    bool fresh = true;

    while (top >= 0) {
        const int64_t cj = stack_col[top];

        if (fresh) {
            fresh = false;
            int64_t got = -1, k = cheap[cj];
            for (; k < b->start[cj + 1]; k++)
                if (match_row[b->row[k]] < 0) { got = b->row[k]; break; }
            cheap[cj] = k;
            if (got >= 0) {
                int64_t row = got;
                for (int64_t t = top; t >= 0; t--) {
                    const int64_t col = stack_col[t];
                    const int64_t prev = match_col[col];
                    match_col[col] = row;
                    match_row[row] = col;
                    row = prev;
                }
                return true;
            }
        }

        if (stack_pos[top] >= b->start[cj + 1]) {
            top--;
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
        /* The lookahead proved every row of this column is held, so this
         * always descends into the column that holds row i. */
        top++;
        stack_col[top] = match_row[i];
        stack_pos[top] = b->start[match_row[i]];
        fresh = true;
    }
    return false;
}

static bool transversal(const vbasis *b, int64_t *match_col,
                        int64_t *match_row)
{
    const int64_t n = b->n;
    for (int64_t c = 0; c < n; c++) match_col[c] = -1;
    for (int64_t i = 0; i < n; i++) match_row[i] = -1;

    for (int64_t c = 0; c < n; c++)
        for (int64_t k = b->start[c]; k < b->start[c + 1]; k++) {
            const int64_t i = b->row[k];
            if (match_row[i] < 0) { match_col[c] = i; match_row[i] = c; break; }
        }

    int64_t *seen  = calloc((size_t)(n > 0 ? n : 1), sizeof *seen);
    int64_t *sc    = calloc((size_t)n + 1, sizeof *sc);
    int64_t *sp    = calloc((size_t)n + 1, sizeof *sp);
    int64_t *cheap = calloc((size_t)(n > 0 ? n : 1), sizeof *cheap);
    if (seen == nullptr || sc == nullptr || sp == nullptr || cheap == nullptr) {
        free(seen); free(sc); free(sp); free(cheap);
        return false;
    }
    for (int64_t i = 0; i < n; i++) seen[i] = -1;
    for (int64_t c = 0; c < n; c++) cheap[c] = b->start[c];

    bool ok = true;
    for (int64_t c = 0; c < n && ok; c++)
        if (match_col[c] < 0)
            ok = augment(b, c, match_col, match_row, seen, c, sc, sp, cheap);

    free(seen); free(sc); free(sp); free(cheap);
    return ok;
}

/* --------------------------------------------------------- 2. the blocks
 *
 * Node i is the pair (row i, the column matched to it). Edge i -> j when the
 * column matched to i has a nonzero in row j: "solving for i needs j".
 * Tarjan pops a component only once everything it reaches is finished, so a
 * component with a LOWER id is one a higher id needs. The permuted matrix is
 * block upper triangular in component order, so the primal solve walks the
 * components backwards and its transpose walks them forwards. */
typedef struct {
    int64_t *comp;
    int64_t  ncomp;
    int64_t  largest;
} vsccs;

static bool tarjan(const vbasis *b, const int64_t *match_row, vsccs *out)
{
    const int64_t n = b->n;
    int64_t *index = malloc((size_t)(n > 0 ? n : 1) * sizeof *index);
    int64_t *low   = malloc((size_t)(n > 0 ? n : 1) * sizeof *low);
    int64_t *stk   = malloc((size_t)(n > 0 ? n : 1) * sizeof *stk);
    bool    *on    = calloc((size_t)(n > 0 ? n : 1), sizeof *on);
    int64_t *comp  = malloc((size_t)(n > 0 ? n : 1) * sizeof *comp);
    int64_t *dn    = malloc(((size_t)n + 2) * sizeof *dn);
    int64_t *dp    = malloc(((size_t)n + 2) * sizeof *dp);
    if (!index || !low || !stk || !on || !comp || !dn || !dp) {
        free(index); free(low); free(stk); free(on); free(comp);
        free(dn); free(dp);
        return false;
    }
    for (int64_t i = 0; i < n; i++) { index[i] = -1; comp[i] = -1; }

    int64_t next = 0, sp = 0, nc = 0;
    out->largest = 0;

    for (int64_t root = 0; root < n; root++) {
        if (index[root] >= 0)
            continue;
        int64_t top = 0;
        dn[0] = root;
        dp[0] = -1;

        while (top >= 0) {
            const int64_t v = dn[top];
            if (dp[top] < 0) {
                index[v] = low[v] = next++;
                stk[sp++] = v;
                on[v] = true;
                dp[top] = b->start[match_row[v]];
            } else {
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

/* ------------------------------------------ 3. one block, over the integers
 *
 * Fraction-free Gauss-Jordan on `[M | r]`, k by k plus the right-hand side.
 * Every entry stays a minor of the original block, so every division is
 * exact; `jm_bigint_divexact` refuses a nonzero remainder rather than
 * rounding one away, which turns an elimination that is wrong into a refusal
 * instead of a wrong answer. At the end the LAST diagonal entry is the
 * determinant, the right-hand column holds the Cramer numerators, and the
 * answer is `r[i] / det`.
 *
 * Bareiss, Mathematics of Computation 22(103):565-578, 1968. The Jordan form
 * of it is what removes the back substitution, and with it the one place a
 * triangular solve would have to hold a product of two determinants. */
typedef struct {
    int64_t    k;
    jm_bigint *a;        /* k * (k + 1), row major: the block then the rhs */
    int64_t    terms;
} vblock;

static jm_bigint *bat(vblock *B, int64_t i, int64_t j)
{
    return &B->a[i * (B->k + 1) + j];
}

/* Why a block solve stopped. SINGULAR is a PROOF -- the basis is rank
 * deficient and that is decided exactly -- while NO_LIMBS is the absence of
 * one. Collapsing them into a single false reports a disproved basis as
 * "the arithmetic did not fit", which is the opposite of what happened. */
typedef enum {
    VBLOCK_SOLVED = 0,
    VBLOCK_SINGULAR,
    VBLOCK_NO_LIMBS,
} vblock_result;

static vblock_result block_solve(vblock *B, jm_bigint *num, jm_bigint *det)
{
    const int64_t k = B->k;
    jm_bigint prev, t1, t2, d;
    jm_bigint_set_i64(&prev, 1);
    int32_t sign = 1;

    for (int64_t p = 0; p < k; p++) {
        /* The first nonzero at or below the diagonal, in index order. The
         * transversal put a nonzero on every diagonal, so this normally
         * finds p itself; the search is here because elimination can still
         * cancel one. */
        int64_t piv = -1;
        for (int64_t i = p; i < k; i++)
            if (!jm_bigint_is_zero(bat(B, i, p))) { piv = i; break; }
        if (piv < 0)
            return VBLOCK_SINGULAR;
        if (piv != p) {
            for (int64_t j = 0; j <= k; j++) {
                const jm_bigint tmp = *bat(B, p, j);
                *bat(B, p, j) = *bat(B, piv, j);
                *bat(B, piv, j) = tmp;
            }
            sign = -sign;
        }

        const jm_bigint pivot = *bat(B, p, p);
        for (int64_t i = 0; i < k; i++) {
            if (i == p)
                continue;
            const jm_bigint aip = *bat(B, i, p);
            for (int64_t j = p + 1; j <= k; j++) {
                if (!jm_bigint_mul(&t1, bat(B, i, j), &pivot))
                    return VBLOCK_NO_LIMBS;
                B->terms++;
                if (!jm_bigint_is_zero(&aip)) {
                    /* A zero multiplier still leaves the Bareiss scaling to
                     * apply: skipping the row entirely would leave it a
                     * factor of pivot/prev out. */
                    if (!jm_bigint_mul(&t2, &aip, bat(B, p, j)) ||
                        !jm_bigint_sub(&t1, &t1, &t2))
                        return VBLOCK_NO_LIMBS;
                    B->terms++;
                }
                /* Bareiss's division is exact, so a nonzero remainder means
                 * the elimination is wrong rather than the input awkward.
                 * `jm_bigint_divexact` reports both that and a result too
                 * wide with the same false; neither can be told apart here,
                 * and both are honestly "the call could not finish". */
                if (!jm_bigint_divexact(&d, &t1, &prev))
                    return VBLOCK_NO_LIMBS;
                *bat(B, i, j) = d;
            }
            jm_bigint_set_zero(bat(B, i, p));
        }
        prev = pivot;
    }

    /* **The LAST diagonal entry is the determinant.** The earlier ones are
     * the leading minors each step left behind, and they are not equal to it:
     * over 3606 random blocks up to 5 by 5, 2944 had some `a[i][i] != det`.
     * Only `a[k-1][k-1]` and the right-hand column are read here, so the
     * answer is right, but a reader who took `x_i` off `a[i][i]` would be
     * wrong on four blocks in five. */
    *det = *bat(B, k - 1, k - 1);
    for (int64_t i = 0; i < k; i++)
        num[i] = *bat(B, i, k);
    if (sign < 0) {
        /* Negating both leaves every `num[i] / det` where it was, so this is
         * a no-op on the answer. It is done anyway so `det` is the actual
         * determinant of the block and not its negation. */
        jm_bigint_neg(det);
        for (int64_t i = 0; i < k; i++)
            jm_bigint_neg(&num[i]);
    }
    return jm_bigint_sign(det) != 0 ? VBLOCK_SOLVED : VBLOCK_SINGULAR;
}

/* ------------------------------------------------- the row-wise mirror
 *
 * The primal equation for model row i needs every basis column with an entry
 * in that row, which the column-wise store cannot answer. The transpose does
 * not need this: its equation for basis column c is that column itself. */
typedef struct {
    int64_t *rstart;     /* n + 1 */
    int64_t *rcol;       /* nnz, which basis column the entry belongs to */
    int64_t *rpos;       /* nnz, its index in the value arrays */
} vrowwise;

static void vrowwise_free(vrowwise *r)
{
    free(r->rstart); free(r->rcol); free(r->rpos);
    memset(r, 0, sizeof *r);
}

static bool vrowwise_build(const vbasis *b, vrowwise *r)
{
    memset(r, 0, sizeof *r);
    r->rstart = calloc((size_t)b->n + 1, sizeof *r->rstart);
    r->rcol   = calloc((size_t)(b->nnz > 0 ? b->nnz : 1), sizeof *r->rcol);
    r->rpos   = calloc((size_t)(b->nnz > 0 ? b->nnz : 1), sizeof *r->rpos);
    if (!r->rstart || !r->rcol || !r->rpos) { vrowwise_free(r); return false; }

    for (int64_t k = 0; k < b->nnz; k++)
        r->rstart[b->row[k] + 1]++;
    for (int64_t i = 0; i < b->n; i++)
        r->rstart[i + 1] += r->rstart[i];
    int64_t *fill = calloc((size_t)b->n + 1, sizeof *fill);
    if (fill == nullptr) { vrowwise_free(r); return false; }
    for (int64_t c = 0; c < b->n; c++)
        for (int64_t k = b->start[c]; k < b->start[c + 1]; k++) {
            const int64_t i = b->row[k];
            const int64_t at = r->rstart[i] + fill[i]++;
            r->rcol[at] = c;
            r->rpos[at] = k;
        }
    free(fill);
    return true;
}

/* ------------------------------------------------------ rational helpers */

static void rat_from_bigint(jm_rational *r, const jm_bigint *v)
{
    r->num = *v;
    jm_nat_set_u64(&r->den, 1);
}

/* --------------------------------------------------------- 4. the solve
 *
 * One rational per node. For the primal the unknown at node i is the value of
 * basis column `match_row[i]` and the equation at node i is model row i; for
 * the transpose the unknown at node i is the scaled dual of model row i and
 * the equation at node i is basis column `match_row[i]`. In both the
 * equation and the unknown of a diagonal block share a component, which is
 * what makes one routine serve both. */
typedef struct {
    const vbasis   *b;
    const vrowwise *rw;
    const int64_t  *match_row;
    const int64_t  *match_col;
    const vsccs    *s;
    bool            transpose;
    int64_t         terms;
    size_t          held;          /* the largest block allocation made */
    bool            singular;      /* it stopped because the basis has no
                                      rank, which is a proof and not a
                                      shortage of limbs */
} vsolver;

/* The terms of the equation at node `e`: writes the unknown's node into
 * `unode[t]` and the entry's index into `upos[t]`, and returns how many.
 * `cap` is the caller's array size; -1 says it did not fit. */
static int64_t equation_terms(const vsolver *V, int64_t e, int64_t *unode,
                              int64_t *upos, int64_t cap)
{
    const vbasis *b = V->b;
    int64_t n = 0;
    if (!V->transpose) {
        for (int64_t t = V->rw->rstart[e]; t < V->rw->rstart[e + 1]; t++) {
            if (n >= cap) return -1;
            unode[n] = V->match_col[V->rw->rcol[t]];
            upos[n] = V->rw->rpos[t];
            n++;
        }
    } else {
        const int64_t c = V->match_row[e];
        for (int64_t k = b->start[c]; k < b->start[c + 1]; k++) {
            if (n >= cap) return -1;
            unode[n] = b->row[k];
            upos[n] = k;
            n++;
        }
    }
    return n;
}

static bool solve_system(vsolver *V, const jm_rational *rhs, jm_rational *sol)
{
    const vbasis *b = V->b;
    const int64_t n = b->n, ncomp = V->s->ncomp;
    const int64_t *comp = V->s->comp;

    /* The nodes of each component, in index order, so the block a component
     * builds is a function of the matrix alone. */
    int64_t *head = calloc((size_t)ncomp + 1, sizeof *head);
    int64_t *list = calloc((size_t)(n > 0 ? n : 1), sizeof *list);
    int64_t *local = calloc((size_t)(n > 0 ? n : 1), sizeof *local);
    int64_t *unode = calloc((size_t)(n > 0 ? n : 1), sizeof *unode);
    int64_t *upos  = calloc((size_t)(n > 0 ? n : 1), sizeof *upos);
    if (!head || !list || !local || !unode || !upos) {
        free(head); free(list); free(local); free(unode); free(upos);
        return false;
    }
    for (int64_t i = 0; i < n; i++)
        head[comp[i] + 1]++;
    for (int64_t c = 0; c < ncomp; c++)
        head[c + 1] += head[c];
    {
        int64_t *fill = calloc((size_t)ncomp + 1, sizeof *fill);
        if (fill == nullptr) {
            free(head); free(list); free(local); free(unode); free(upos);
            return false;
        }
        for (int64_t i = 0; i < n; i++) {
            const int64_t c = comp[i];
            list[head[c] + fill[c]] = i;
            local[i] = fill[c];
            fill[c]++;
        }
        free(fill);
    }

    bool ok = true;
    jm_rational t;

    for (int64_t step = 0; step < ncomp && ok; step++) {
        /* Block upper triangular in component order for the primal, block
         * lower for the transpose. */
        const int64_t c = V->transpose ? step : ncomp - 1 - step;
        const int64_t k = head[c + 1] - head[c];
        const int64_t *nodes = &list[head[c]];

        /* Every equation's right-hand side, with the blocks already solved
         * substituted out. */
        jm_rational *r = calloc((size_t)k, sizeof *r);
        if (r == nullptr) { ok = false; break; }
        for (int64_t le = 0; le < k && ok; le++) {
            const int64_t e = nodes[le];
            r[le] = rhs[e];
            const int64_t nt = equation_terms(V, e, unode, upos, n);
            if (nt < 0) { ok = false; break; }
            for (int64_t t2 = 0; t2 < nt; t2++) {
                if (comp[unode[t2]] == c)
                    continue;
                /* r -= coef * x, the already-solved blocks substituted out. */
                jm_rational cf;
                rat_from_bigint(&cf, &b->zval[upos[t2]]);
                if (!jm_rational_mul(&t, &cf, &sol[unode[t2]]) ||
                    !jm_rational_sub(&r[le], &r[le], &t)) {
                    ok = false; break;
                }
                V->terms++;
            }
        }
        if (!ok) { free(r); break; }

        if (k == 1) {
            /* The common case by a distance: `ken-11` and `ken-13` are
             * nothing else. One exact division, no table, no allocation. */
            const int64_t e = nodes[0];
            const int64_t nt = equation_terms(V, e, unode, upos, n);
            int64_t diag = -1;
            for (int64_t t2 = 0; t2 < nt; t2++)
                if (comp[unode[t2]] == c) { diag = upos[t2]; break; }
            /* A one-row block with no entry of its own, or one that is zero,
             * is a rank deficiency and not a shortage of limbs. */
            if (diag < 0 || jm_bigint_is_zero(&b->zval[diag])) {
                V->singular = true;
                ok = false; free(r); break;
            }
            jm_rational dv;
            rat_from_bigint(&dv, &b->zval[diag]);
            if (!jm_rational_div(&sol[nodes[0]], &r[0], &dv))
                ok = false;
            V->terms++;
            free(r);
            continue;
        }

        /* A dense table, refused before the allocation rather than by it. */
        const size_t bytes = (size_t)k * (size_t)(k + 1) * sizeof(jm_bigint);
        if (bytes > VERIFY_BLOCK_BYTES) { ok = false; free(r); break; }
        if (bytes > V->held)
            V->held = bytes;

        vblock B = { .k = k, .a = calloc((size_t)k * (size_t)(k + 1),
                                         sizeof *B.a), .terms = 0 };
        jm_bigint *num = calloc((size_t)k, sizeof *num);
        jm_bigint det;
        if (B.a == nullptr || num == nullptr) {
            free(B.a); free(num); free(r); ok = false; break;
        }

        /* One denominator for the whole block, so the table is integral.
         * The least common multiple, not the product: the denominators here
         * are the accumulated determinants and they share nearly all of it. */
        jm_nat den;
        jm_nat_set_u64(&den, 1);
        for (int64_t le = 0; le < k && ok; le++) {
            jm_nat g, q, rem;
            if (!jm_nat_gcd(&g, &den, &r[le].den)) { ok = false; break; }
            if (!jm_nat_divmod(&q, &rem, &r[le].den, &g) ||
                !jm_nat_mul(&den, &den, &q)) { ok = false; break; }
        }
        for (int64_t le = 0; le < k && ok; le++) {
            const int64_t e = nodes[le];
            const int64_t nt = equation_terms(V, e, unode, upos, n);
            for (int64_t t2 = 0; t2 < nt; t2++)
                if (comp[unode[t2]] == c)
                    *bat(&B, le, local[unode[t2]]) = b->zval[upos[t2]];
            /* r[le] * den, exact because den is a multiple of r[le].den. */
            jm_nat q, rem;
            jm_bigint scaled;
            if (!jm_nat_divmod(&q, &rem, &den, &r[le].den)) { ok = false; break; }
            jm_bigint qi = { .mag = q, .sign = jm_nat_is_zero(&q) ? 0 : 1 };
            if (!jm_bigint_mul(&scaled, &r[le].num, &qi)) { ok = false; break; }
            *bat(&B, le, k) = scaled;
        }
        if (ok) {
            const vblock_result br = block_solve(&B, num, &det);
            if (br != VBLOCK_SOLVED) {
                ok = false;
                if (br == VBLOCK_SINGULAR)
                    V->singular = true;
            }
        }
        V->terms += B.terms;

        if (ok) {
            /* x = num / (det * den). */
            jm_bigint dd = { .mag = den, .sign = jm_nat_is_zero(&den) ? 0 : 1 };
            jm_bigint full;
            if (!jm_bigint_mul(&full, &det, &dd)) {
                ok = false;
            } else {
                jm_rational fr;
                rat_from_bigint(&fr, &full);
                for (int64_t lu = 0; lu < k && ok; lu++) {
                    jm_rational nr;
                    rat_from_bigint(&nr, &num[lu]);
                    if (!jm_rational_div(&sol[nodes[lu]], &nr, &fr))
                        ok = false;
                }
            }
        }
        free(B.a); free(num); free(r);
    }

    free(head); free(list); free(local); free(unode); free(upos);
    return ok;
}

/* ----------------------------------------------------------- 5. the checks */

/* Is `v` inside [lo, hi]? An infinite bound does not constrain. Returns 0
 * when it is, -1 when it is below lo, +1 when it is above hi, and writes how
 * far out it is into `by`.
 *
 * The distance is rounded once at the end, and it is the only rounding in
 * the whole verdict. It decides nothing: the comparison that decides is
 * exact, and this is what the caller is told afterwards. */
static int rat_in_bounds(const jm_rational *v, double lo, double hi,
                         double *by, bool *ok)
{
    jm_rational bd, diff;
    int c = 0;
    *ok = true;
    *by = 0.0;
    if (isfinite(lo)) {
        if (!jm_rational_from_double(&bd, lo)) { *ok = false; return 0; }
        /* The CHECKED comparison. The plain one reports 0 both for equal and
         * for a cross-multiply that did not fit, and this caller cannot rule
         * the second out: `v` is a solved basic value whose numerator reaches
         * the whole limb budget, and a bound like 0.1 carries a 56-bit
         * denominator. Reading that 0 as "inside" would certify a value the
         * call never compared. */
        if (!jm_rational_cmp_checked(v, &bd, &c)) { *ok = false; return 0; }
        if (c < 0) {
            if (jm_rational_sub(&diff, &bd, v))
                *by = jm_rational_to_double(&diff);
            return -1;
        }
    }
    if (isfinite(hi)) {
        if (!jm_rational_from_double(&bd, hi)) { *ok = false; return 0; }
        if (!jm_rational_cmp_checked(v, &bd, &c)) { *ok = false; return 0; }
        if (c > 0) {
            if (jm_rational_sub(&diff, v, &bd))
                *by = jm_rational_to_double(&diff);
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------- 6. the call */

/* Section 7 below: what a proof leaves on the model (D286). */
static bool exact_store(jaos_model *m, const vbasis *b, const int64_t *mr,
                        const jm_rational *xs, const jm_rational *us,
                        double sigma);

jaos_status jaos_verify(jaos_model *m, jaos_verify_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (m->solve_status != JAOS_SOLVE_OPTIMAL || m->sol_col_status == nullptr ||
        m->sol_row_status == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* A previous proof's values go first, so nothing stale survives a
     * verdict that is not OPTIMAL. */
    jm_model_drop_exact(m);

    jaos_verify_report rep = { .status = JAOS_PROOF_REFUSED,
                               .stage = JAOS_PROOF_STAGE_NONE,
                               .bound_bits = 0.0,
                               .capacity_bits = 32.0 * (double)JM_EXACT_LIMBS,
                               .blocks = 0,
                               .largest_block = 0,
                               .at_row = -1,
                               .at_col = -1,
                               .violation = 0.0,
                               .bytes_held = 0,
                               .terms = 0 };

    vbasis b;
    vrowwise rw = {0};
    vsccs s = {0};
    int64_t *mc = nullptr, *mr = nullptr;
    jm_rational *rhs = nullptr, *xs = nullptr, *us = nullptr;
    jaos_status rc = JAOS_ERR_NUMERICAL;

    if (!vbasis_build(m, &b)) {
        jm_set_err(m, "jaos_verify: the published basis is not %lld columns",
                   (long long)m->num_row);
        *out = rep;
        return JAOS_ERR_NUMERICAL;
    }
    if (!vbasis_scale(&b)) {
        jm_set_err(m, "jaos_verify: a basis row does not scale to an integer "
                      "inside the limb budget");
        goto done;
    }
    if (!vrowwise_build(&b, &rw))
        goto done;

    mc = malloc((size_t)(b.n > 0 ? b.n : 1) * sizeof *mc);
    mr = malloc((size_t)(b.n > 0 ? b.n : 1) * sizeof *mr);
    if (mc == nullptr || mr == nullptr)
        goto done;
    if (!transversal(&b, mc, mr)) {
        jm_set_err(m, "jaos_verify: the published basis is structurally "
                      "singular");
        rep.status = JAOS_PROOF_BROKEN;
        rep.stage = JAOS_PROOF_STAGE_RANK;
        rc = JAOS_OK;
        goto done;
    }
    if (!tarjan(&b, mr, &s))
        goto done;
    rep.blocks = s.ncomp;
    rep.largest_block = s.largest;

    /* The bound, and the refusal it decides. The whole basis says what the
     * answer's denominator costs and the largest block says what the
     * elimination's right-hand side column costs on top of it (D273). Both
     * are read before a limb is allocated. */
    {
        /* Every product is of SQUARED norms, so `vprod_log2` is twice the
         * bound and the halving happens once, at the end. */
        jm_nat sq;
        vprod whole;
        vprod_one(&whole);
        for (int64_t c = 0; c < b.n; c++)
            if (!col_norm_sq(&b, c, nullptr, 0, &sq) ||
                !vprod_mul(&whole, &sq))
                goto done;

        vprod *per = calloc((size_t)(s.ncomp > 0 ? s.ncomp : 1), sizeof *per);
        if (per == nullptr)
            goto done;
        for (int64_t c = 0; c < s.ncomp; c++)
            vprod_one(&per[c]);
        bool pok = true;
        for (int64_t i = 0; i < b.n && pok; i++)
            pok = col_norm_sq(&b, mr[i], s.comp, s.comp[i], &sq) &&
                  vprod_mul(&per[s.comp[i]], &sq);
        int64_t worst2 = 0;
        for (int64_t c = 0; c < s.ncomp; c++) {
            const int64_t v = vprod_log2(&per[c]);
            if (v > worst2)
                worst2 = v;
        }
        free(per);
        if (!pok)
            goto done;
        rep.bound_bits = (double)((vprod_log2(&whole) + worst2 + 1) / 2);
    }
    if (rep.bound_bits > rep.capacity_bits) {
        rep.status = JAOS_PROOF_REFUSED;
        rc = JAOS_OK;
        goto done;
    }

    rhs = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *rhs);
    xs  = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *xs);
    us  = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *us);
    if (rhs == nullptr || xs == nullptr || us == nullptr)
        goto done;

    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    /* --- the primal right-hand side, scaled with its row ------------------
     *
     * sum over basic of coef * value = -(sum over nonbasic of coef * value),
     * every nonbasic value being the bound the model declares. Row i's whole
     * equation is then multiplied by 2^-shift[i], the same scale the matrix
     * took. */
    for (int64_t i = 0; i < b.n; i++)
        jm_rational_set_zero(&rhs[i]);
    {
        jm_rational v, t2, cv;
        bool okrhs = true;
        for (int64_t j = 0; j < m->num_col && okrhs; j++) {
            if (m->sol_col_status[j] == JAOS_BASIS_BASIC)
                continue;
            double at = 0.0;
            switch (m->sol_col_status[j]) {
            case JAOS_BASIS_AT_LOWER: at = m->col_lower[j]; break;
            case JAOS_BASIS_AT_UPPER: at = m->col_upper[j]; break;
            default:                  at = 0.0;             break;
            }
            if (!isfinite(at)) {
                /* A REFUSAL, not an error. `jaos.h` does not promise that
                 * AT_LOWER and AT_UPPER name a finite bound: the simplex
                 * publishes a column resting on a bound it lent itself as
                 * nonbasic there, and four of `finnis`'s are like that. The
                 * basis then does not determine a point, so the call cannot
                 * decide -- which is exactly what REFUSED means. Returning
                 * an error instead would be a fourth outcome the header
                 * never describes, on a case the gate carries today. */
                jm_set_err(m, "jaos_verify: nonbasic column %lld rests on an "
                              "infinite bound, so the basis names no point",
                           (long long)j);
                rep.status = JAOS_PROOF_REFUSED;
                rc = JAOS_OK;
                goto done;
            }
            if (at == 0.0)
                continue;
            if (!jm_rational_from_double(&v, at)) { okrhs = false; break; }
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                if (m->a_value[k] == 0.0)
                    continue;
                const int64_t i = m->a_index[k];
                if (!jm_rational_from_double(&cv, -m->a_value[k]) ||
                    !jm_rational_mul(&t2, &cv, &v) ||
                    !jm_rational_add(&rhs[i], &rhs[i], &t2)) {
                    okrhs = false; break;
                }
            }
        }
        for (int64_t i = 0; i < m->num_row && okrhs; i++) {
            if (m->sol_row_status[i] == JAOS_BASIS_BASIC)
                continue;
            double at = 0.0;
            switch (m->sol_row_status[i]) {
            case JAOS_BASIS_AT_LOWER: at = m->row_lower[i]; break;
            case JAOS_BASIS_AT_UPPER: at = m->row_upper[i]; break;
            default:                  at = 0.0;             break;
            }
            if (!isfinite(at)) {
                jm_set_err(m, "jaos_verify: nonbasic row %lld rests on an "
                              "infinite bound, so the basis names no point",
                           (long long)i);
                rep.status = JAOS_PROOF_REFUSED;
                rc = JAOS_OK;
                goto done;
            }
            /* The slack column is -e_i, so moving it right adds +at. */
            if (at == 0.0)
                continue;
            if (!jm_rational_from_double(&v, at) ||
                !jm_rational_add(&rhs[i], &rhs[i], &v)) { okrhs = false; break; }
        }
        if (!okrhs)
            goto done;
        /* Scale each equation the way its row was scaled. */
        for (int64_t i = 0; i < b.n; i++) {
            if (b.shift[i] == 0)
                continue;
            jm_rational p2;
            jm_rational_set_i64(&p2, 1);
            if (!jm_nat_shl(&p2.num.mag, &p2.num.mag, -b.shift[i]))
                goto done;
            p2.num.sign = 1;
            if (!jm_rational_mul(&rhs[i], &rhs[i], &p2))
                goto done;
        }
    }

    {
        vsolver V = { .b = &b, .rw = &rw, .match_row = mr, .match_col = mc,
                      .s = &s, .transpose = false, .terms = 0, .held = 0,
                      .singular = false };
        if (!solve_system(&V, rhs, xs)) {
            rep.terms = V.terms;
            rep.bytes_held = (int64_t)V.held;
            /* A basis with a transversal can still be singular: two
             * proportional columns pass the matching and cancel in the
             * elimination. That is a PROOF that the basis is not one, and it
             * is a different answer from running out of limbs. */
            if (V.singular) {
                rep.status = JAOS_PROOF_BROKEN;
                rep.stage = JAOS_PROOF_STAGE_RANK;
                jm_set_err(m, "jaos_verify: the published basis is singular");
            } else {
                rep.status = JAOS_PROOF_REFUSED;
            }
            rc = JAOS_OK;
            goto done;
        }
        rep.terms += V.terms;
        rep.bytes_held = (int64_t)V.held;
    }

    /* The column scaling put back. `vbasis_scale` divided column c by
     * 2^cshift[c], so it solved `B G w = r` and the model's value is
     * `x = G w`, that is `w / 2^cshift`. Node i owns basis column mr[i]. */
    for (int64_t i = 0; i < b.n; i++) {
        const int64_t g = b.cshift[mr[i]];
        if (g == 0)
            continue;
        jm_rational p2;
        jm_rational_set_i64(&p2, 1);
        if (!jm_nat_shl(&p2.den, &p2.den, g))
            goto done;
        if (!jm_rational_mul(&xs[i], &xs[i], &p2))
            goto done;
    }

    /* --- the primal check ------------------------------------------------ */
    for (int64_t i = 0; i < b.n; i++) {
        const int64_t c = mr[i];
        const int64_t w = b.who[c];
        double lo, hi;
        if (w >= 0) { lo = m->col_lower[w]; hi = m->col_upper[w]; }
        else        { lo = m->row_lower[-w - 1]; hi = m->row_upper[-w - 1]; }
        bool okb = true;
        double by = 0.0;
        const int side = rat_in_bounds(&xs[i], lo, hi, &by, &okb);
        if (!okb) {
            /* The comparison itself ran out of limbs. That is a refusal and
             * not an error: the call could not decide, and saying so is the
             * whole contract. */
            rep.status = JAOS_PROOF_REFUSED;
            rc = JAOS_OK;
            goto done;
        }
        if (side != 0) {
            rep.status = JAOS_PROOF_BROKEN;
            rep.stage = JAOS_PROOF_STAGE_PRIMAL;
            rep.violation = by;
            if (w >= 0) rep.at_col = w; else rep.at_row = -w - 1;
            rc = JAOS_OK;
            goto done;
        }
    }

    /* --- the dual right-hand side, and its solve -------------------------
     *
     * Z' u = c_B, with u the dual scaled the way the rows were: the model's
     * dual is y_i = 2^-shift[i] * u_i, put back below. The cost of a slack
     * column is zero. */
    for (int64_t i = 0; i < b.n; i++)
        jm_rational_set_zero(&rhs[i]);
    for (int64_t i = 0; i < b.n; i++) {
        const int64_t c = mr[i];
        const int64_t w = b.who[c];
        if (w >= 0 && !jm_rational_from_double(&rhs[i], sigma * m->col_cost[w]))
            goto done;
        /* (B G)' = G B', so the transpose's right-hand side takes the column
         * factor and the dual itself does not. A slack column's cost is zero
         * and scaling zero is still zero, but the loop does not special-case
         * it: the rule is about the equation, not about the value. */
        const int64_t g = b.cshift[c];
        if (g == 0 || jm_rational_is_zero(&rhs[i]))
            continue;
        jm_rational p2;
        jm_rational_set_i64(&p2, 1);
        if (!jm_nat_shl(&p2.den, &p2.den, g))
            goto done;
        if (!jm_rational_mul(&rhs[i], &rhs[i], &p2))
            goto done;
    }
    {
        vsolver V = { .b = &b, .rw = &rw, .match_row = mr, .match_col = mc,
                      .s = &s, .transpose = true, .terms = 0, .held = 0,
                      .singular = false };
        if (!solve_system(&V, rhs, us)) {
            rep.terms += V.terms;
            if ((int64_t)V.held > rep.bytes_held)
                rep.bytes_held = (int64_t)V.held;
            if (V.singular) {
                rep.status = JAOS_PROOF_BROKEN;
                rep.stage = JAOS_PROOF_STAGE_RANK;
                jm_set_err(m, "jaos_verify: the published basis is singular");
            } else {
                rep.status = JAOS_PROOF_REFUSED;
            }
            rc = JAOS_OK;
            goto done;
        }
        rep.terms += V.terms;
        if ((int64_t)V.held > rep.bytes_held)
            rep.bytes_held = (int64_t)V.held;
    }

    /* Z = D B with D = diag(2^-shift), so Z' u = (D B)' u = B' (D u) = c_B
     * and the model's dual is y = D u, that is y_i = 2^-shift[i] * u_i. The
     * shift is at most zero, so this multiplies. Taking it the other way is
     * a silent factor of 2^104 on a row of decimal data: the primal stays
     * right, every dual collapses to nothing, and the reduced costs come out
     * as the costs themselves. */
    for (int64_t i = 0; i < b.n; i++) {
        if (b.shift[i] == 0)
            continue;
        jm_rational p2;
        jm_rational_set_i64(&p2, 1);
        if (!jm_nat_shl(&p2.num.mag, &p2.num.mag, -b.shift[i]))
            goto done;
        p2.num.sign = 1;
        if (!jm_rational_mul(&us[i], &us[i], &p2))
            goto done;
    }

    /* --- the dual check --------------------------------------------------
     *
     * d_v = c_v - a_v' y, minimize form. A nonbasic variable at its lower
     * bound needs d >= 0, at its upper d <= 0, and a free one d == 0. */
    {
        jm_rational d, t2, cv, zero;
        jm_rational_set_zero(&zero);
        for (int64_t j = 0; j < m->num_col; j++) {
            const jaos_basis_status st = m->sol_col_status[j];
            if (st == JAOS_BASIS_BASIC)
                continue;
            /* A fixed variable cannot move, so no sign of its reduced cost
             * contradicts optimality. Demanding one would reject every
             * equality-constrained model there is. */
            if (m->col_lower[j] == m->col_upper[j])
                continue;
            if (!jm_rational_from_double(&d, sigma * m->col_cost[j]))
                goto done;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                if (m->a_value[k] == 0.0)
                    continue;
                if (!jm_rational_from_double(&cv, m->a_value[k]) ||
                    !jm_rational_mul(&t2, &cv, &us[m->a_index[k]]) ||
                    !jm_rational_sub(&d, &d, &t2))
                    goto done;
                rep.terms++;
            }
            /* Against zero the checked form cannot fail -- either the signs
             * differ or both are zero, and neither multiplies -- but the
             * plain one would hide it if the threshold ever stopped being
             * zero, so the failure is handled rather than assumed away. */
            int sgn = 0;
            if (!jm_rational_cmp_checked(&d, &zero, &sgn)) {
                rep.status = JAOS_PROOF_REFUSED;
                rc = JAOS_OK;
                goto done;
            }
            if ((st == JAOS_BASIS_AT_LOWER && sgn < 0) ||
                (st == JAOS_BASIS_AT_UPPER && sgn > 0) ||
                (st == JAOS_BASIS_FREE && sgn != 0)) {
                rep.status = JAOS_PROOF_BROKEN;
                rep.stage = JAOS_PROOF_STAGE_DUAL;
                rep.violation = fabs(jm_rational_to_double(&d));
                rep.at_col = j;
                rc = JAOS_OK;
                goto done;
            }
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            const jaos_basis_status st = m->sol_row_status[i];
            if (st == JAOS_BASIS_BASIC)
                continue;
            /* An equality row's slack is fixed and its dual is free. This is
             * the common case, not an edge one: most netlib models are mostly
             * equalities. */
            if (m->row_lower[i] == m->row_upper[i])
                continue;
            /* The slack column is -e_i, so d = 0 - (-1) * y_i = y_i. */
            int sgn = 0;
            if (!jm_rational_cmp_checked(&us[i], &zero, &sgn)) {
                rep.status = JAOS_PROOF_REFUSED;
                rc = JAOS_OK;
                goto done;
            }
            if ((st == JAOS_BASIS_AT_LOWER && sgn < 0) ||
                (st == JAOS_BASIS_AT_UPPER && sgn > 0) ||
                (st == JAOS_BASIS_FREE && sgn != 0)) {
                rep.status = JAOS_PROOF_BROKEN;
                rep.stage = JAOS_PROOF_STAGE_DUAL;
                rep.violation = fabs(jm_rational_to_double(&us[i]));
                rep.at_row = i;
                rc = JAOS_OK;
                goto done;
            }
        }
    }

    rep.status = JAOS_PROOF_OPTIMAL;
    rc = JAOS_OK;
    /* The proof stands; what it proved is kept for the getters (D286). Out
     * of memory here leaves the values absent and the verdict as it is:
     * the bookkeeping cannot demote a proof. */
    (void)exact_store(m, &b, mr, xs, us, sigma);

done:
    free(rhs); free(xs); free(us);
    free(s.comp);
    free(mc); free(mr);
    vrowwise_free(&rw);
    vbasis_free(&b);
    *out = rep;
    return rc;
}

/* -------------------------------------------------- 7. the exact values */

/* What a proved basis says the answer IS, kept on the model as decimal
 * rationals (D286). Every column: a basic one's solved value, a nonbasic
 * one's the bound its status names, which a double spells exactly. Every
 * row's dual. And the objective, summed over the columns with no rounding,
 * which can outgrow the limb budget on a model the proof itself fitted;
 * then it alone is absent and the getter says so. */
static void exact_drop(jaos_model *m)
{
    if (m->exact_col != nullptr)
        for (int64_t j = 0; j < m->num_col; j++)
            free(m->exact_col[j]);
    if (m->exact_dual != nullptr)
        for (int64_t i = 0; i < m->num_row; i++)
            free(m->exact_dual[i]);
    free(m->exact_col);
    free(m->exact_dual);
    free(m->exact_obj);
    m->exact_col = nullptr;
    m->exact_dual = nullptr;
    m->exact_obj = nullptr;
}

void jm_model_drop_exact(jaos_model *m)
{
    exact_drop(m);
}

/* Fills the three from the solved systems: `xs[i]` is basis column
 * `mr[i]`'s value, `us[i]` row i's dual. Returns false out of memory; a
 * limb overflow in the objective is not a failure, it leaves exact_obj
 * null. */
static bool exact_store(jaos_model *m, const vbasis *b, const int64_t *mr,
                        const jm_rational *xs, const jm_rational *us,
                        double sigma)
{
    exact_drop(m);
    m->exact_col = jm_calloc_array(m->num_col, sizeof(char *));
    m->exact_dual = jm_calloc_array(m->num_row, sizeof(char *));
    if (m->exact_col == nullptr || m->exact_dual == nullptr)
        goto fail;

    /* Basic columns and rows from the solve. A basic row's activity is not
     * kept: the getters answer columns and duals, which is what an exact
     * answer is made of. */
    for (int64_t i = 0; i < b->n; i++) {
        const int64_t w = b->who[mr[i]];
        if (w < 0)
            continue;
        m->exact_col[w] = jm_rational_decimal(&xs[i]);
        if (m->exact_col[w] == nullptr)
            goto fail;
    }
    /* Nonbasic columns rest on a bound, and a bound is a double, which is
     * a rational exactly (a free nonbasic rests at zero). */
    for (int64_t j = 0; j < m->num_col; j++) {
        if (m->exact_col[j] != nullptr)
            continue;
        double at = 0.0;
        switch (m->sol_col_status[j]) {
        case JAOS_BASIS_AT_LOWER: at = m->col_lower[j]; break;
        case JAOS_BASIS_AT_UPPER: at = m->col_upper[j]; break;
        default:                  at = 0.0;             break;
        }
        jm_rational r;
        if (!isfinite(at) || !jm_rational_from_double(&r, at))
            goto fail;
        m->exact_col[j] = jm_rational_decimal(&r);
        if (m->exact_col[j] == nullptr)
            goto fail;
    }
    /* The duals, in the model's own sense: the proof worked in minimize
     * form and `sigma` is the sign that undoes it. */
    for (int64_t i = 0; i < m->num_row; i++) {
        jm_rational y = us[i];
        if (sigma < 0.0)
            jm_rational_neg(&y);
        m->exact_dual[i] = jm_rational_decimal(&y);
        if (m->exact_dual[i] == nullptr)
            goto fail;
    }
    /* The objective: c'x + c0 with no rounding. Basic values come from
     * `xs`, nonbasic ones from their bound, and any product or sum that
     * outgrows the limbs leaves the objective absent rather than wrong. */
    {
        jm_rational acc, t, cj, xj;
        bool fits = jm_rational_from_double(&acc, m->obj_offset);
        for (int64_t i = 0; fits && i < b->n; i++) {
            const int64_t w = b->who[mr[i]];
            if (w < 0 || m->col_cost[w] == 0.0)
                continue;
            fits = jm_rational_from_double(&cj, m->col_cost[w]) &&
                   jm_rational_mul(&t, &cj, &xs[i]) &&
                   jm_rational_add(&acc, &acc, &t);
        }
        for (int64_t j = 0; fits && j < m->num_col; j++) {
            if (m->sol_col_status[j] == JAOS_BASIS_BASIC ||
                m->col_cost[j] == 0.0)
                continue;
            double at = 0.0;
            if (m->sol_col_status[j] == JAOS_BASIS_AT_LOWER)
                at = m->col_lower[j];
            else if (m->sol_col_status[j] == JAOS_BASIS_AT_UPPER)
                at = m->col_upper[j];
            fits = jm_rational_from_double(&cj, m->col_cost[j]) &&
                   jm_rational_from_double(&xj, at) &&
                   jm_rational_mul(&t, &cj, &xj) &&
                   jm_rational_add(&acc, &acc, &t);
        }
        if (fits) {
            m->exact_obj = jm_rational_decimal(&acc);
            if (m->exact_obj == nullptr)
                goto fail;
        }
    }
    return true;
fail:
    exact_drop(m);
    return false;
}

/* The getters. A value is on the model only while a proof stands: the
 * arrays are dropped by the next solve, load or modification, so a stale
 * answer cannot be read. */
static jaos_status exact_get(const jaos_model *m, char **arr, int64_t k,
                             int64_t n, const char **out)
{
    if (m == nullptr || out == nullptr || k < 0 || k >= n)
        return JAOS_ERR_INVALID_INPUT;
    if (arr == nullptr) {
        jm_set_err((jaos_model *)m, "no exact values: jaos_verify has not "
                   "proved the last answer");
        return JAOS_ERR_INVALID_INPUT;
    }
    *out = arr[k];
    return JAOS_OK;
}

jaos_status jaos_exact_col_value(const jaos_model *m, int64_t col,
                                 const char **out)
{
    return exact_get(m, m ? m->exact_col : nullptr, col,
                     m ? m->num_col : 0, out);
}

jaos_status jaos_exact_row_dual(const jaos_model *m, int64_t row,
                                const char **out)
{
    return exact_get(m, m ? m->exact_dual : nullptr, row,
                     m ? m->num_row : 0, out);
}

jaos_status jaos_exact_objective(const jaos_model *m, const char **out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (m->exact_col == nullptr) {
        jm_set_err((jaos_model *)m, "no exact values: jaos_verify has not "
                   "proved the last answer");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (m->exact_obj == nullptr) {
        jm_set_err((jaos_model *)m, "the exact objective did not fit the "
                   "limb budget; the values did");
        return JAOS_ERR_INVALID_INPUT;
    }
    *out = m->exact_obj;
    return JAOS_OK;
}
