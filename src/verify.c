/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t VERIFY_BLOCK_BYTES = 536870912;
constexpr int64_t EXACT_PIVOT_CAP = 1000;

typedef struct {
    int64_t    n;
    int64_t   *start;
    int64_t   *row;
    double    *dval;
    jm_bigint *zval;
    int64_t   *who;
    int64_t   *shift;
    int64_t   *cshift;
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
        b->dval[p] = -1.0;
        p++;
        c++;
    }
    b->start[c] = p;
    return true;
}

static bool vbasis_scale(vbasis *b)
{
    for (int64_t i = 0; i < b->n; i++)
        b->shift[i] = 1;
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

static int64_t ceil_log2_i64(int64_t x)
{
    int64_t e = 0;
    while (((int64_t)1 << e) < x)
        e++;
    return e;
}

constexpr int64_t VERIFY_PROD_BITS = 256;

typedef struct {
    jm_nat  m;
    int64_t e;
} vprod;

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

static int64_t vprod_log2(const vprod *p)
{
    return jm_nat_bits(&p->m) + p->e;
}

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
        jm_nat_set_u64(out, 1);
        return true;
    }
    if (exact) {
        *out = ss;
        return true;
    }

    jm_nat_set_u64(out, 1);
    if (!jm_nat_shl(out, out, 2 * widest + ceil_log2_i64(count)))
        return false;
    return true;
}

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

typedef struct {
    int64_t *comp;
    int64_t  ncomp;
    int64_t  largest;
} vsccs;

static bool tarjan(const vbasis *b, const int64_t *match_row, vsccs *out)
{
    const int64_t n = b->n;
    int64_t *index = jm_alloc_array(n > 0 ? n : 1, sizeof *index);
    int64_t *low   = jm_alloc_array(n > 0 ? n : 1, sizeof *low);
    int64_t *stk   = jm_alloc_array(n > 0 ? n : 1, sizeof *stk);
    bool    *on    = calloc((size_t)(n > 0 ? n : 1), sizeof *on);
    int64_t *comp  = jm_alloc_array(n > 0 ? n : 1, sizeof *comp);
    int64_t *dn    = jm_alloc_array(n + 2, sizeof *dn);
    int64_t *dp    = jm_alloc_array(n + 2, sizeof *dp);
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

typedef struct {
    int64_t    k;
    jm_bigint *a;
    int64_t    terms;
} vblock;

static jm_bigint *bat(vblock *B, int64_t i, int64_t j)
{
    return &B->a[i * (B->k + 1) + j];
}

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

                    if (!jm_bigint_mul(&t2, &aip, bat(B, p, j)) ||
                        !jm_bigint_sub(&t1, &t1, &t2))
                        return VBLOCK_NO_LIMBS;
                    B->terms++;
                }

                if (!jm_bigint_divexact(&d, &t1, &prev))
                    return VBLOCK_NO_LIMBS;
                *bat(B, i, j) = d;
            }
            jm_bigint_set_zero(bat(B, i, p));
        }
        prev = pivot;
    }

    *det = *bat(B, k - 1, k - 1);
    for (int64_t i = 0; i < k; i++)
        num[i] = *bat(B, i, k);
    if (sign < 0) {

        jm_bigint_neg(det);
        for (int64_t i = 0; i < k; i++)
            jm_bigint_neg(&num[i]);
    }
    return jm_bigint_sign(det) != 0 ? VBLOCK_SOLVED : VBLOCK_SINGULAR;
}

typedef struct {
    int64_t *rstart;
    int64_t *rcol;
    int64_t *rpos;
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

static void rat_from_bigint(jm_rational *r, const jm_bigint *v)
{
    r->num = *v;
    jm_nat_set_u64(&r->den, 1);
}

typedef struct {
    const vbasis   *b;
    const vrowwise *rw;
    const int64_t  *match_row;
    const int64_t  *match_col;
    const vsccs    *s;
    bool            transpose;
    int64_t         terms;
    size_t          held;
    bool            singular;
} vsolver;

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

        const int64_t c = V->transpose ? step : ncomp - 1 - step;
        const int64_t k = head[c + 1] - head[c];
        const int64_t *nodes = &list[head[c]];

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

            const int64_t e = nodes[0];
            const int64_t nt = equation_terms(V, e, unode, upos, n);
            int64_t diag = -1;
            for (int64_t t2 = 0; t2 < nt; t2++)
                if (comp[unode[t2]] == c) { diag = upos[t2]; break; }

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

static int rat_in_bounds(const jm_rational *v, double lo, double hi,
                         double *by, bool *ok)
{
    jm_rational bd, diff;
    int c = 0;
    *ok = true;
    *by = 0.0;
    if (isfinite(lo)) {
        if (!jm_rational_from_double(&bd, lo)) { *ok = false; return 0; }

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

static bool exact_store(jaos_model *m, const vbasis *b, const int64_t *mr,
                        const jm_rational *xs, const jm_rational *us,
                        double sigma);

static jaos_status verify_core(jaos_model *m, jaos_verify_report *out)
{

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

    if (jm_model_has_quadratic(m)) {
        jm_set_err(m, "the exact proof is about a linear objective, and this "
                      "one has a quadratic term");
        *out = rep;
        return JAOS_ERR_INVALID_INPUT;
    }
    if (jm_model_has_integer(m)) {
        jm_set_err(m, "the exact proof is about the basis behind an answer, "
                      "and this model is a MIP: the basis is the last node's, "
                      "with that node's branching bounds, so the proof would "
                      "judge a linear program the model as loaded is not");
        *out = rep;
        return JAOS_ERR_INVALID_INPUT;
    }

    vbasis b;
    vrowwise rw = {0};
    vsccs s = {0};
    int64_t *mc = nullptr, *mr = nullptr;
    jm_rational *rhs = nullptr, *xs = nullptr, *us = nullptr;
    jaos_status rc = JAOS_ERR_NUMERICAL;

    if (!vbasis_build(m, &b)) {
        jm_set_err(m, "the basis handed to the proof is not %lld columns",
                   (long long)m->num_row);
        *out = rep;
        return JAOS_ERR_NUMERICAL;
    }
    if (!vbasis_scale(&b)) {
        jm_set_err(m, "a basis row does not scale to an integer "
                      "inside the limb budget");
        goto done;
    }
    if (!vrowwise_build(&b, &rw))
        goto done;

    mc = jm_alloc_array(b.n > 0 ? b.n : 1, sizeof *mc);
    mr = jm_alloc_array(b.n > 0 ? b.n : 1, sizeof *mr);
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

    {

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
    rc = JAOS_OK;

    rhs = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *rhs);
    xs  = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *xs);
    us  = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *us);
    if (rhs == nullptr || xs == nullptr || us == nullptr)
        goto done;

    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

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

            if (at == 0.0)
                continue;
            if (!jm_rational_from_double(&v, at) ||
                !jm_rational_add(&rhs[i], &rhs[i], &v)) { okrhs = false; break; }
        }
        if (!okrhs)
            goto done;

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

    for (int64_t i = 0; i < b.n; i++)
        jm_rational_set_zero(&rhs[i]);
    for (int64_t i = 0; i < b.n; i++) {
        const int64_t c = mr[i];
        const int64_t w = b.who[c];
        if (w >= 0 && !jm_rational_from_double(&rhs[i], sigma * m->col_cost[w]))
            goto done;

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

    {
        jm_rational d, t2, cv, zero;
        jm_rational_set_zero(&zero);
        for (int64_t j = 0; j < m->num_col; j++) {
            const jaos_basis_status st = m->sol_col_status[j];
            if (st == JAOS_BASIS_BASIC)
                continue;

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

            if (m->row_lower[i] == m->row_upper[i])
                continue;

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

typedef struct {
    vbasis   b;
    vrowwise rw;
    vsccs    s;
    int64_t *mc;
    int64_t *mr;
} xbasis;

static void xbasis_free(xbasis *x)
{
    free(x->s.comp);
    free(x->mc);
    free(x->mr);
    vrowwise_free(&x->rw);
    vbasis_free(&x->b);
}

static bool xbasis_build(const jaos_model *m, xbasis *x)
{
    memset(x, 0, sizeof *x);
    if (!vbasis_build(m, &x->b))
        return false;
    x->mc = jm_alloc_array(x->b.n > 0 ? x->b.n : 1, sizeof *x->mc);
    x->mr = jm_alloc_array(x->b.n > 0 ? x->b.n : 1, sizeof *x->mr);
    return x->mc != nullptr && x->mr != nullptr && vbasis_scale(&x->b) &&
           vrowwise_build(&x->b, &x->rw) &&
           transversal(&x->b, x->mc, x->mr) && tarjan(&x->b, x->mr, &x->s);
}

static bool rat_scale2(jm_rational *r, int64_t e)
{
    if (e == 0 || jm_rational_is_zero(r))
        return true;
    jm_rational p2;
    jm_rational_set_i64(&p2, 1);
    if (e > 0 ? !jm_nat_shl(&p2.num.mag, &p2.num.mag, e)
              : !jm_nat_shl(&p2.den, &p2.den, -e))
        return false;
    return jm_rational_mul(r, r, &p2);
}

static bool xbasis_cols(xbasis *x, jm_rational *rhs, jm_rational *out)
{
    for (int64_t i = 0; i < x->b.n; i++)
        if (!rat_scale2(&rhs[i], -x->b.shift[i]))
            return false;
    vsolver V = { .b = &x->b, .rw = &x->rw, .match_row = x->mr,
                  .match_col = x->mc, .s = &x->s, .transpose = false };
    if (!solve_system(&V, rhs, out))
        return false;
    for (int64_t i = 0; i < x->b.n; i++)
        if (!rat_scale2(&out[i], -x->b.cshift[x->mr[i]]))
            return false;
    return true;
}

static bool xbasis_rows(xbasis *x, jm_rational *rhs, jm_rational *out)
{
    for (int64_t i = 0; i < x->b.n; i++)
        if (!rat_scale2(&rhs[i], -x->b.cshift[x->mr[i]]))
            return false;
    vsolver V = { .b = &x->b, .rw = &x->rw, .match_row = x->mr,
                  .match_col = x->mc, .s = &x->s, .transpose = true };
    if (!solve_system(&V, rhs, out))
        return false;
    for (int64_t i = 0; i < x->b.n; i++)
        if (!rat_scale2(&out[i], -x->b.shift[i]))
            return false;
    return true;
}

static double xvar_lo(const jaos_model *m, int64_t v)
{
    return v < m->num_col ? m->col_lower[v] : m->row_lower[v - m->num_col];
}

static double xvar_hi(const jaos_model *m, int64_t v)
{
    return v < m->num_col ? m->col_upper[v] : m->row_upper[v - m->num_col];
}

static jaos_basis_status *xvar_st(jaos_model *m, int64_t v)
{
    return v < m->num_col ? &m->sol_col_status[v]
                          : &m->sol_row_status[v - m->num_col];
}

static bool xvar_dot(const jaos_model *m, int64_t v, const jm_rational *z,
                     jm_rational *out)
{
    if (v >= m->num_col) {
        *out = z[v - m->num_col];
        jm_rational_neg(out);
        return true;
    }
    jm_rational a, t;
    jm_rational_set_zero(out);
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
        if (m->a_value[k] == 0.0)
            continue;
        if (!jm_rational_from_double(&a, m->a_value[k]) ||
            !jm_rational_mul(&t, &a, &z[m->a_index[k]]) ||
            !jm_rational_add(out, out, &t))
            return false;
    }
    return true;
}

static double xvar_at(const jaos_model *m, int64_t v, jaos_basis_status st)
{
    if (st == JAOS_BASIS_AT_LOWER)
        return xvar_lo(m, v);
    if (st == JAOS_BASIS_AT_UPPER)
        return xvar_hi(m, v);
    return 0.0;
}

typedef enum {
    XSTEP_PIVOT = 0,
    XSTEP_FLIP,
    XSTEP_OPTIMAL,
    XSTEP_INFEASIBLE,
    XSTEP_UNBOUNDED,
    XSTEP_SHIFT,
    XSTEP_FAIL,
} xstep;

static bool rat_abs_ratio(jm_rational *out, const jm_rational *a,
                          const jm_rational *b)
{
    jm_rational na = *a, nb = *b;
    if (jm_rational_sign(&na) < 0)
        jm_rational_neg(&na);
    if (jm_rational_sign(&nb) < 0)
        jm_rational_neg(&nb);
    return jm_rational_div(out, &na, &nb);
}

static xstep exact_step(jaos_model *m, jm_rational *cost, bool publish,
                        double *ray)
{
    const int64_t nc = m->num_col, nr = m->num_row, nv = nc + nr;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    xbasis x;
    memset(&x, 0, sizeof x);
    xstep res = XSTEP_FAIL;
    jm_rational *rhs = calloc((size_t)(nr > 0 ? nr : 1), sizeof *rhs);
    jm_rational *xs = calloc((size_t)(nr > 0 ? nr : 1), sizeof *xs);
    jm_rational *us = calloc((size_t)(nr > 0 ? nr : 1), sizeof *us);
    jm_rational *dv = calloc((size_t)(nv > 0 ? nv : 1), sizeof *dv);
    int64_t *pos = jm_alloc_array(nv > 0 ? nv : 1, sizeof *pos);
    int64_t *var = jm_alloc_array(nr > 0 ? nr : 1, sizeof *var);
    const bool built = rhs != nullptr && xs != nullptr && us != nullptr &&
                       dv != nullptr && pos != nullptr && var != nullptr &&
                       xbasis_build(m, &x);
    if (!built)
        goto out;

    for (int64_t v = 0; v < nv; v++)
        pos[v] = -1;
    for (int64_t i = 0; i < nr; i++) {
        const int64_t w = x.b.who[x.mr[i]];
        var[i] = w >= 0 ? w : nc + (-w - 1);
        pos[var[i]] = i;
    }

    for (int64_t i = 0; i < nr; i++)
        jm_rational_set_zero(&rhs[i]);
    for (int64_t v = 0; v < nv; v++) {
        if (pos[v] >= 0)
            continue;
        const double at = xvar_at(m, v, *xvar_st(m, v));
        if (!isfinite(at))
            goto out;
        if (at == 0.0)
            continue;
        jm_rational r, a, t;
        if (!jm_rational_from_double(&r, at))
            goto out;
        if (v >= nc) {
            if (!jm_rational_add(&rhs[v - nc], &rhs[v - nc], &r))
                goto out;
            continue;
        }
        for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
            if (m->a_value[k] == 0.0)
                continue;
            if (!jm_rational_from_double(&a, -m->a_value[k]) ||
                !jm_rational_mul(&t, &a, &r) ||
                !jm_rational_add(&rhs[m->a_index[k]], &rhs[m->a_index[k]], &t))
                goto out;
        }
    }
    if (!xbasis_cols(&x, rhs, xs))
        goto out;

    int64_t leave = -1, side = 0;
    for (int64_t v = 0; v < nv && leave < 0; v++) {
        if (pos[v] < 0)
            continue;
        bool okb = true;
        double by = 0.0;
        const int s = rat_in_bounds(&xs[pos[v]], xvar_lo(m, v), xvar_hi(m, v),
                                    &by, &okb);
        if (!okb)
            goto out;
        if (s != 0) {
            leave = v;
            side = s;
        }
    }

    for (int64_t i = 0; i < nr; i++)
        rhs[i] = cost[var[i]];
    if (!xbasis_rows(&x, rhs, us))
        goto out;

    int64_t enter = -1;
    for (int64_t v = 0; v < nv; v++) {
        jm_rational_set_zero(&dv[v]);
        if (pos[v] >= 0)
            continue;
        jm_rational au;
        if (!xvar_dot(m, v, us, &au) || !jm_rational_sub(&dv[v], &cost[v], &au))
            goto out;
        if (xvar_lo(m, v) == xvar_hi(m, v))
            continue;
        const jaos_basis_status st = *xvar_st(m, v);
        const int32_t sg = jm_rational_sign(&dv[v]);
        if (enter < 0 && ((st == JAOS_BASIS_AT_LOWER && sg < 0) ||
                          (st == JAOS_BASIS_AT_UPPER && sg > 0) ||
                          (st == JAOS_BASIS_FREE && sg != 0)))
            enter = v;
    }

    if (leave < 0 && enter < 0 && !publish) {
        res = XSTEP_OPTIMAL;
        goto out;
    }
    if (leave < 0 && enter < 0) {
        for (int64_t j = 0; j < nc; j++) {
            m->sol_col[j] = pos[j] >= 0
                ? jm_rational_to_double(&xs[pos[j]])
                : xvar_at(m, j, m->sol_col_status[j]);
            jm_rational r = dv[j];
            if (sigma < 0.0)
                jm_rational_neg(&r);
            m->sol_redcost[j] = pos[j] >= 0 ? 0.0 : jm_rational_to_double(&r);
        }
        for (int64_t i = 0; i < nr; i++) {
            m->sol_row[i] = pos[nc + i] >= 0
                ? jm_rational_to_double(&xs[pos[nc + i]])
                : xvar_at(m, nc + i, m->sol_row_status[i]);
            jm_rational y = us[i];
            if (sigma < 0.0)
                jm_rational_neg(&y);
            m->sol_dual[i] = jm_rational_to_double(&y);
        }
        jm_rational acc, t, cj, xj;
        bool fits = jm_rational_from_double(&acc, m->obj_offset);
        for (int64_t j = 0; fits && j < nc; j++) {
            if (m->col_cost[j] == 0.0)
                continue;
            if (pos[j] >= 0)
                xj = xs[pos[j]];
            else
                fits = jm_rational_from_double(
                    &xj, xvar_at(m, j, m->sol_col_status[j]));
            fits = fits && jm_rational_from_double(&cj, m->col_cost[j]) &&
                   jm_rational_mul(&t, &cj, &xj) &&
                   jm_rational_add(&acc, &acc, &t);
        }
        if (fits)
            m->objective = jm_rational_to_double(&acc);
        res = XSTEP_OPTIMAL;
        goto out;
    }
    if (leave >= 0 && enter >= 0) {
        for (int64_t v = 0; v < nv; v++) {
            if (pos[v] >= 0 || xvar_lo(m, v) == xvar_hi(m, v))
                continue;
            const jaos_basis_status st = *xvar_st(m, v);
            const int32_t sg = jm_rational_sign(&dv[v]);
            if (((st == JAOS_BASIS_AT_LOWER && sg < 0) ||
                 (st == JAOS_BASIS_AT_UPPER && sg > 0) ||
                 (st == JAOS_BASIS_FREE && sg != 0)) &&
                !jm_rational_sub(&cost[v], &cost[v], &dv[v]))
                goto out;
        }
        res = XSTEP_SHIFT;
        goto out;
    }

    if (leave >= 0) {
        for (int64_t i = 0; i < nr; i++)
            jm_rational_set_zero(&rhs[i]);
        jm_rational_set_i64(&rhs[pos[leave]], 1);
        jm_rational *z = us;
        if (!xbasis_rows(&x, rhs, z))
            goto out;
        int64_t best = -1;
        jm_rational bratio;
        jm_rational_set_zero(&bratio);
        for (int64_t v = 0; v < nv; v++) {
            if (pos[v] >= 0 || xvar_lo(m, v) == xvar_hi(m, v))
                continue;
            jm_rational al;
            if (!xvar_dot(m, v, z, &al))
                goto out;
            const int32_t sa = jm_rational_sign(&al);
            if (sa == 0)
                continue;
            const jaos_basis_status st = *xvar_st(m, v);
            const bool ok = st == JAOS_BASIS_FREE ||
                (st == JAOS_BASIS_AT_LOWER && sa == (side < 0 ? -1 : 1)) ||
                (st == JAOS_BASIS_AT_UPPER && sa == (side < 0 ? 1 : -1));
            if (!ok)
                continue;
            jm_rational ratio;
            if (!rat_abs_ratio(&ratio, &dv[v], &al))
                goto out;
            int cmp = 0;
            if (best >= 0 && !jm_rational_cmp_checked(&ratio, &bratio, &cmp))
                goto out;
            if (best < 0 || cmp < 0) {
                best = v;
                bratio = ratio;
            }
        }
        if (best < 0) {
            if (ray != nullptr)
                for (int64_t i = 0; i < nr; i++)
                    ray[i] = jm_rational_to_double(&z[i]);
            res = XSTEP_INFEASIBLE;
            goto out;
        }
        *xvar_st(m, best) = JAOS_BASIS_BASIC;
        *xvar_st(m, leave) = side < 0 ? JAOS_BASIS_AT_LOWER
                                      : JAOS_BASIS_AT_UPPER;
        res = XSTEP_PIVOT;
        goto out;
    }

    {
        const int32_t dir = jm_rational_sign(&dv[enter]) < 0 ? 1 : -1;
        for (int64_t i = 0; i < nr; i++)
            jm_rational_set_zero(&rhs[i]);
        if (enter >= nc) {
            jm_rational_set_i64(&rhs[enter - nc], -1);
        } else {
            for (int64_t k = m->a_start[enter]; k < m->a_start[enter + 1];
                 k++)
                if (m->a_value[k] != 0.0 &&
                    !jm_rational_from_double(&rhs[m->a_index[k]],
                                             m->a_value[k]))
                    goto out;
        }
        jm_rational *w = us;
        if (!xbasis_cols(&x, rhs, w))
            goto out;
        int64_t best = -1;
        int32_t best_to = 0;
        jm_rational bt;
        jm_rational_set_zero(&bt);
        const double elo = xvar_lo(m, enter), ehi = xvar_hi(m, enter);
        const jaos_basis_status est = *xvar_st(m, enter);
        if (est != JAOS_BASIS_FREE && isfinite(elo) && isfinite(ehi)) {
            jm_rational l, h;
            if (!jm_rational_from_double(&l, elo) ||
                !jm_rational_from_double(&h, ehi) ||
                !jm_rational_sub(&bt, &h, &l))
                goto out;
            best = enter;
        }
        for (int64_t v = 0; v < nv; v++) {
            if (pos[v] < 0)
                continue;
            const int64_t i = pos[v];
            const int32_t sd = -dir * jm_rational_sign(&w[i]);
            if (sd == 0)
                continue;
            const double bd = sd < 0 ? xvar_lo(m, v) : xvar_hi(m, v);
            if (!isfinite(bd))
                continue;
            jm_rational b, gap, t;
            if (!jm_rational_from_double(&b, bd) ||
                !jm_rational_sub(&gap, &b, &xs[i]) ||
                !rat_abs_ratio(&t, &gap, &w[i]))
                goto out;
            int cmp = 0;
            if (best >= 0 && !jm_rational_cmp_checked(&t, &bt, &cmp))
                goto out;
            if (best < 0 || cmp < 0 || (cmp == 0 && v < best)) {
                best = v;
                bt = t;
                best_to = sd;
            }
        }
        if (best < 0) {
            res = XSTEP_UNBOUNDED;
            goto out;
        }
        if (best == enter) {
            *xvar_st(m, enter) = est == JAOS_BASIS_AT_LOWER
                                     ? JAOS_BASIS_AT_UPPER
                                     : JAOS_BASIS_AT_LOWER;
            res = XSTEP_FLIP;
            goto out;
        }
        *xvar_st(m, enter) = JAOS_BASIS_BASIC;
        *xvar_st(m, best) = best_to < 0 ? JAOS_BASIS_AT_LOWER
                                        : JAOS_BASIS_AT_UPPER;
        res = XSTEP_PIVOT;
    }

out:
    xbasis_free(&x);
    free(rhs); free(xs); free(us); free(dv); free(pos); free(var);
    return res;
}

static jaos_status exact_fail(jaos_model *m, const char *why)
{
    jm_model_drop_exact(m);
    m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
    jm_set_err(m, "exact solving: %s", why);
    return JAOS_OK;
}

static bool exact_costs(const jaos_model *m, jm_rational *cost)
{
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    for (int64_t j = 0; j < m->num_col; j++)
        if (!jm_rational_from_double(&cost[j], sigma * m->col_cost[j]))
            return false;
    for (int64_t i = 0; i < m->num_row; i++)
        jm_rational_set_zero(&cost[m->num_col + i]);
    return true;
}

static jaos_status exact_infeasible(jaos_model *m, int64_t pivots)
{
    m->solve_status = JAOS_SOLVE_INFEASIBLE;
    m->farkas_ok = true;
    m->ray_ok = false;
    for (int pass = 0; pass < 2; pass++) {
        jaos_exact_ray_report r;
        if (jaos_exact_certificate(m, &r) == JAOS_OK && r.derived) {
            jm_log(m, JAOS_LOG_SUMMARY, "exact: the floating-point optimum "
                   "is infeasible over the rationals, shown after %lld exact "
                   "pivots, with an exact certificate", (long long)pivots);
            return JAOS_OK;
        }
        for (int64_t i = 0; i < m->num_row; i++)
            m->sol_farkas[i] = -m->sol_farkas[i];
    }
    return exact_fail(m, "the model is infeasible over the rationals, and no "
                         "exact certificate came from the row that shows it");
}

jaos_status jm_exact_finish(jaos_model *m)
{
    if (m->solve_status == JAOS_SOLVE_INFEASIBLE) {
        jaos_exact_ray_report r;
        if (jaos_exact_certificate(m, &r) != JAOS_OK || !r.derived)
            return exact_fail(m, "no exact certificate of infeasibility "
                                 "came from the ray the solve published");
        jm_log(m, JAOS_LOG_SUMMARY, "exact: infeasibility proved over the "
               "rationals");
        return JAOS_OK;
    }
    if (m->solve_status == JAOS_SOLVE_UNBOUNDED) {
        jaos_exact_ray_report r;
        if (jaos_exact_unbounded_ray(m, &r) != JAOS_OK || !r.derived)
            return exact_fail(m, "no exact unbounded ray came from the one "
                                 "the solve published");
        jm_log(m, JAOS_LOG_SUMMARY, "exact: unboundedness proved over the "
               "rationals");
        return JAOS_OK;
    }
    if (m->solve_status != JAOS_SOLVE_OPTIMAL)
        return JAOS_OK;
    if (!m->sol_basis_ok || m->sol_col_status == nullptr ||
        m->sol_row_status == nullptr)
        return exact_fail(m, "the optimum carries no basis to prove");

    const int64_t nv = m->num_col + m->num_row;
    jm_rational *cost = calloc((size_t)(nv > 0 ? nv : 1), sizeof *cost);
    if (cost == nullptr || !exact_costs(m, cost)) {
        free(cost);
        return exact_fail(m, "a cost does not fit the limb budget");
    }
    int64_t pivots = 0, flips = 0, shifts = 0;
    bool shifted = false;
    const char *why = nullptr;
    for (;;) {
        if (!shifted) {
            jaos_verify_report rep;
            const jaos_status st = verify_core(m, &rep);
            if (st != JAOS_OK) {
                why = "the proof could not be set up";
                break;
            }
            if (rep.status == JAOS_PROOF_OPTIMAL)
                break;
            if (rep.status == JAOS_PROOF_REFUSED) {
                why = "a number in the proof outgrew the limb budget";
                break;
            }
            if (rep.stage == JAOS_PROOF_STAGE_RANK) {
                why = "the basis is singular over the rationals";
                break;
            }
        }
        if (pivots + flips + shifts >= EXACT_PIVOT_CAP) {
            why = "the exact pivots reached their cap";
            break;
        }
        const xstep s = exact_step(m, cost, false, m->sol_farkas);
        if (s == XSTEP_PIVOT) {
            pivots++;
        } else if (s == XSTEP_FLIP) {
            flips++;
        } else if (s == XSTEP_SHIFT) {
            shifts++;
            shifted = true;
        } else if (s == XSTEP_OPTIMAL && shifted) {
            if (!exact_costs(m, cost)) {
                why = "a cost does not fit the limb budget";
                break;
            }
            shifted = false;
        } else if (s == XSTEP_INFEASIBLE) {
            free(cost);
            return exact_infeasible(m, pivots);
        } else if (s == XSTEP_UNBOUNDED) {
            why = "the model is unbounded over the rationals";
            break;
        } else {
            why = "a number in the proof outgrew the limb budget";
            break;
        }
    }
    if (why == nullptr && exact_step(m, cost, true, nullptr) != XSTEP_OPTIMAL)
        why = "a number in the proof outgrew the limb budget";
    free(cost);
    if (why != nullptr)
        return exact_fail(m, why);
    jm_log(m, JAOS_LOG_SUMMARY, "exact: optimum proved over the rationals "
           "after %lld exact pivots, %lld bound flips and %lld cost shifts",
           (long long)pivots, (long long)flips, (long long)shifts);
    return JAOS_OK;
}

jaos_status jaos_verify(jaos_model *m, jaos_verify_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (jm_model_has_conic(m)) {
        jm_set_err(m, "exact verification is about a basis, and a model with "
                      "cones or quadratic rows has none");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (m->solve_status != JAOS_SOLVE_OPTIMAL || m->sol_col_status == nullptr ||
        m->sol_row_status == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    return verify_core(m, out);
}

static bool vstatus_in_range(jaos_basis_status s)
{
    return s == JAOS_BASIS_BASIC || s == JAOS_BASIS_AT_LOWER ||
           s == JAOS_BASIS_AT_UPPER || s == JAOS_BASIS_FREE;
}

jaos_status jaos_verify_basis(jaos_model *m,
                              const jaos_basis_status *col_status,
                              const jaos_basis_status *row_status,
                              jaos_verify_report *out)
{
    if (m == nullptr || out == nullptr || col_status == nullptr ||
        row_status == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (jm_model_has_conic(m)) {
        jm_set_err(m, "exact verification is about a basis, and a model with "
                      "cones or quadratic rows has none");
        return JAOS_ERR_INVALID_INPUT;
    }

    int64_t basic = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!vstatus_in_range(col_status[j])) {
            jm_set_err(m, "column %lld has no such basis status: %d",
                       (long long)j, (int)col_status[j]);
            return JAOS_ERR_INVALID_INPUT;
        }
        basic += col_status[j] == JAOS_BASIS_BASIC;
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        if (!vstatus_in_range(row_status[i])) {
            jm_set_err(m, "row %lld has no such basis status: %d",
                       (long long)i, (int)row_status[i]);
            return JAOS_ERR_INVALID_INPUT;
        }
        basic += row_status[i] == JAOS_BASIS_BASIC;
    }
    if (basic != m->num_row) {
        jm_set_err(m, "the basis has %lld basic variables and this model has "
                   "%lld rows", (long long)basic, (long long)m->num_row);
        return JAOS_ERR_INVALID_INPUT;
    }

    jaos_basis_status *cs = jm_alloc_array(m->num_col, sizeof *cs);
    jaos_basis_status *rs = jm_alloc_array(m->num_row, sizeof *rs);
    if (cs == nullptr || rs == nullptr) {
        free(cs);
        free(rs);
        jm_set_err(m, "out of memory");
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    memcpy(cs, col_status, (size_t)m->num_col * sizeof *cs);
    memcpy(rs, row_status, (size_t)m->num_row * sizeof *rs);

    jaos_basis_status *saved_c = m->sol_col_status;
    jaos_basis_status *saved_r = m->sol_row_status;
    m->sol_col_status = cs;
    m->sol_row_status = rs;
    const jaos_status st = verify_core(m, out);
    m->sol_col_status = saved_c;
    m->sol_row_status = saved_r;
    free(cs);
    free(rs);
    return st;
}

static void exact_drop(jaos_model *m)
{
    if (m->exact_col != nullptr)
        for (int64_t j = 0; j < m->num_col; j++)
            free(m->exact_col[j]);
    if (m->exact_dual != nullptr)
        for (int64_t i = 0; i < m->num_row; i++)
            free(m->exact_dual[i]);
    if (m->exact_farkas != nullptr)
        for (int64_t i = 0; i < m->num_row; i++)
            free(m->exact_farkas[i]);
    if (m->exact_uray != nullptr)
        for (int64_t j = 0; j < m->num_col; j++)
            free(m->exact_uray[j]);
    free(m->exact_col);
    free(m->exact_dual);
    free(m->exact_obj);
    free(m->exact_farkas);
    free(m->exact_uray);
    m->exact_col = nullptr;
    m->exact_dual = nullptr;
    m->exact_obj = nullptr;
    m->exact_farkas = nullptr;
    m->exact_uray = nullptr;
}

void jm_model_drop_exact(jaos_model *m)
{
    exact_drop(m);
}

static bool exact_store(jaos_model *m, const vbasis *b, const int64_t *mr,
                        const jm_rational *xs, const jm_rational *us,
                        double sigma)
{
    exact_drop(m);
    m->exact_col = jm_calloc_array(m->num_col, sizeof(char *));
    m->exact_dual = jm_calloc_array(m->num_row, sizeof(char *));
    if (m->exact_col == nullptr || m->exact_dual == nullptr)
        goto fail;

    for (int64_t i = 0; i < b->n; i++) {
        const int64_t w = b->who[mr[i]];
        if (w < 0)
            continue;
        m->exact_col[w] = jm_rational_decimal(&xs[i]);
        if (m->exact_col[w] == nullptr)
            goto fail;
    }

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

    for (int64_t i = 0; i < m->num_row; i++) {
        jm_rational y = us[i];
        if (sigma < 0.0)
            jm_rational_neg(&y);
        m->exact_dual[i] = jm_rational_decimal(&y);
        if (m->exact_dual[i] == nullptr)
            goto fail;
    }

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

jaos_status jaos_exact_certificate(jaos_model *m, jaos_exact_ray_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *out = (jaos_exact_ray_report){ .derived = false,
                                    .bound_bits = 0.0,
                                    .capacity_bits =
                                        32.0 * (double)JM_EXACT_LIMBS,
                                    .blocks = 0, .largest_block = 0,
                                    .terms = 0, .bytes_held = 0,
                                    .at_row = -1 };
    if (m->solve_status != JAOS_SOLVE_INFEASIBLE || !m->farkas_ok ||
        m->sol_farkas == nullptr || !m->sol_basis_ok) {
        jm_set_err(m, "an exact certificate needs an INFEASIBLE answer with "
                      "both a published ray and the basis it stopped on");
        return JAOS_ERR_INVALID_INPUT;
    }

    jm_model_drop_exact(m);

    jaos_exact_ray_report rep = *out;
    vbasis b;
    vrowwise rw = {0};
    vsccs s = {0};
    int64_t *mc = nullptr, *mr = nullptr;
    jm_rational *rhs = nullptr, *us = nullptr;
    jaos_status rc = JAOS_ERR_NUMERICAL;
    int64_t pos = -1;
    double best = 0.0, best_signed = 0.0;

    if (!vbasis_build(m, &b)) {
        jm_set_err(m, "jaos_exact_certificate: the published basis is not "
                      "%lld columns", (long long)m->num_row);
        *out = rep;
        return JAOS_ERR_NUMERICAL;
    }
    if (!vbasis_scale(&b)) {
        jm_set_err(m, "jaos_exact_certificate: a basis row does not scale to "
                      "an integer inside the limb budget");
        goto done;
    }
    if (!vrowwise_build(&b, &rw))
        goto done;

    mc = jm_alloc_array(b.n > 0 ? b.n : 1, sizeof *mc);
    mr = jm_alloc_array(b.n > 0 ? b.n : 1, sizeof *mr);
    if (mc == nullptr || mr == nullptr)
        goto done;
    if (!transversal(&b, mc, mr)) {
        jm_set_err(m, "jaos_exact_certificate: the published basis is "
                      "structurally singular");
        goto done;
    }
    if (!tarjan(&b, mr, &s))
        goto done;
    rep.blocks = s.ncomp;
    rep.largest_block = s.largest;

    {
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

    for (int64_t c = 0; c < b.n; c++) {
        const int64_t w = b.who[c];
        double t = 0.0;
        if (w >= 0) {
            for (int64_t k = m->a_start[w]; k < m->a_start[w + 1]; k++)
                t += m->a_value[k] * m->sol_farkas[m->a_index[k]];
        } else {

            t = -m->sol_farkas[-w - 1];
        }
        if (fabs(t) > best) {
            best = fabs(t);
            best_signed = t;
            pos = c;
        }
    }
    if (pos < 0) {
        jm_set_err(m, "jaos_exact_certificate: the published ray is zero on "
                      "every basis column, so it names no position");
        goto done;
    }

    rep.at_row = b.who[pos] < 0 ? -b.who[pos] - 1 : -1;
    rc = JAOS_OK;

    rhs = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *rhs);
    us  = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *us);
    if (rhs == nullptr || us == nullptr)
        goto done;

    for (int64_t i = 0; i < b.n; i++)
        jm_rational_set_zero(&rhs[i]);
    for (int64_t i = 0; i < b.n; i++) {
        if (mr[i] != pos)
            continue;
        if (!jm_rational_from_double(&rhs[i], best_signed > 0.0 ? 1.0 : -1.0))
            goto done;
        const int64_t g = b.cshift[pos];
        if (g != 0) {
            jm_rational p2;
            jm_rational_set_i64(&p2, 1);
            if (!jm_nat_shl(&p2.den, &p2.den, g) ||
                !jm_rational_mul(&rhs[i], &rhs[i], &p2))
                goto done;
        }
        break;
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
                jm_set_err(m, "jaos_exact_certificate: the published basis "
                              "is singular");
                rc = JAOS_ERR_NUMERICAL;
            }
            goto done;
        }
        rep.terms += V.terms;
        if ((int64_t)V.held > rep.bytes_held)
            rep.bytes_held = (int64_t)V.held;
    }

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

    m->exact_farkas = jm_calloc_array(m->num_row, sizeof(char *));
    if (m->exact_farkas == nullptr)
        goto done;
    for (int64_t i = 0; i < m->num_row; i++) {
        m->exact_farkas[i] = jm_rational_decimal(&us[i]);
        if (m->exact_farkas[i] == nullptr) {
            jm_model_drop_exact(m);
            goto done;
        }
    }
    rep.derived = true;
    rc = JAOS_OK;

done:
    free(rhs); free(us);
    free(s.comp);
    free(mc); free(mr);
    vrowwise_free(&rw);
    vbasis_free(&b);
    *out = rep;
    return rc;
}

jaos_status jaos_exact_row_multiplier(const jaos_model *m, int64_t row,
                                      const char **out)
{
    return exact_get(m, m ? m->exact_farkas : nullptr, row,
                     m ? m->num_row : 0, out);
}

jaos_status jaos_exact_unbounded_ray(jaos_model *m, jaos_exact_ray_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *out = (jaos_exact_ray_report){ .derived = false, .bound_bits = 0.0,
                                    .capacity_bits =
                                        32.0 * (double)JM_EXACT_LIMBS,
                                    .blocks = 0, .largest_block = 0,
                                    .terms = 0, .bytes_held = 0,
                                    .at_row = -1 };
    if (m->solve_status != JAOS_SOLVE_UNBOUNDED || !m->ray_ok ||
        m->sol_ray == nullptr || !m->sol_basis_ok) {
        jm_set_err(m, "an exact ray needs an UNBOUNDED answer with both a "
                      "published direction and the basis it stopped on");
        return JAOS_ERR_INVALID_INPUT;
    }
    jm_model_drop_exact(m);

    jaos_exact_ray_report rep = *out;
    vbasis b;
    vrowwise rw = {0};
    vsccs s = {0};
    int64_t *mc = nullptr, *mr = nullptr;
    jm_rational *rhs = nullptr, *xs = nullptr;
    jaos_status rc = JAOS_ERR_NUMERICAL;
    int64_t moving = 0;

    for (int64_t j = 0; j < m->num_col; j++)
        if (m->sol_col_status[j] != JAOS_BASIS_BASIC && m->sol_ray[j] != 0.0)
            moving++;
    if (moving == 0) {
        jm_set_err(m, "jaos_exact_unbounded_ray: the published direction is "
                      "zero on every nonbasic column, so there is nothing "
                      "to solve for");
        *out = rep;
        return JAOS_ERR_NUMERICAL;
    }

    if (!vbasis_build(m, &b)) {
        jm_set_err(m, "jaos_exact_unbounded_ray: the published basis is not "
                      "%lld columns", (long long)m->num_row);
        *out = rep;
        return JAOS_ERR_NUMERICAL;
    }
    if (!vbasis_scale(&b)) {
        jm_set_err(m, "jaos_exact_unbounded_ray: a basis row does not scale "
                      "to an integer inside the limb budget");
        goto done;
    }
    if (!vrowwise_build(&b, &rw))
        goto done;
    mc = jm_alloc_array(b.n > 0 ? b.n : 1, sizeof *mc);
    mr = jm_alloc_array(b.n > 0 ? b.n : 1, sizeof *mr);
    if (mc == nullptr || mr == nullptr)
        goto done;
    if (!transversal(&b, mc, mr)) {
        jm_set_err(m, "jaos_exact_unbounded_ray: the published basis is "
                      "structurally singular");
        goto done;
    }
    if (!tarjan(&b, mr, &s))
        goto done;
    rep.blocks = s.ncomp;
    rep.largest_block = s.largest;

    {
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
    rc = JAOS_OK;

    rhs = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *rhs);
    xs  = calloc((size_t)(b.n > 0 ? b.n : 1), sizeof *xs);
    if (rhs == nullptr || xs == nullptr)
        goto done;

    {
        jm_rational v, d, t;
        for (int64_t i = 0; i < b.n; i++)
            jm_rational_set_zero(&rhs[i]);
        for (int64_t j = 0; j < m->num_col; j++) {
            if (m->sol_col_status[j] == JAOS_BASIS_BASIC ||
                m->sol_ray[j] == 0.0)
                continue;
            if (!jm_rational_from_double(&d, -m->sol_ray[j]))
                goto done;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                if (m->a_value[k] == 0.0)
                    continue;
                const int64_t i = m->a_index[k];
                if (!jm_rational_from_double(&v, m->a_value[k]) ||
                    !jm_rational_mul(&t, &v, &d) ||
                    !jm_rational_add(&rhs[i], &rhs[i], &t))
                    goto done;
            }
        }
        for (int64_t i = 0; i < b.n; i++) {
            if (b.shift[i] == 0 || jm_rational_is_zero(&rhs[i]))
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
            rep.terms += V.terms;
            if ((int64_t)V.held > rep.bytes_held)
                rep.bytes_held = (int64_t)V.held;
            if (V.singular) {
                jm_set_err(m, "jaos_exact_unbounded_ray: the published basis "
                              "is singular");
                rc = JAOS_ERR_NUMERICAL;
            }
            goto done;
        }
        rep.terms += V.terms;
        if ((int64_t)V.held > rep.bytes_held)
            rep.bytes_held = (int64_t)V.held;
    }

    m->exact_uray = jm_calloc_array(m->num_col, sizeof(char *));
    if (m->exact_uray == nullptr)
        goto done;
    {
        jm_rational zero, neg;
        jm_rational_set_zero(&zero);
        for (int64_t j = 0; j < m->num_col; j++) {
            m->exact_uray[j] = jm_rational_decimal(&zero);
            if (m->exact_uray[j] == nullptr) {
                jm_model_drop_exact(m);
                goto done;
            }
        }

        for (int64_t i = 0; i < b.n; i++) {
            const int64_t w = b.who[mr[i]];
            if (w < 0)
                continue;
            free(m->exact_uray[w]);
            m->exact_uray[w] = jm_rational_decimal(&xs[i]);
            if (m->exact_uray[w] == nullptr) {
                jm_model_drop_exact(m);
                goto done;
            }
        }

        for (int64_t j = 0; j < m->num_col; j++) {
            if (m->sol_col_status[j] == JAOS_BASIS_BASIC ||
                m->sol_ray[j] == 0.0)
                continue;
            if (!jm_rational_from_double(&neg, m->sol_ray[j])) {
                jm_model_drop_exact(m);
                goto done;
            }
            free(m->exact_uray[j]);
            m->exact_uray[j] = jm_rational_decimal(&neg);
            if (m->exact_uray[j] == nullptr) {
                jm_model_drop_exact(m);
                goto done;
            }
        }
        (void)zero;
    }
    rep.derived = true;
    rc = JAOS_OK;

done:
    free(rhs); free(xs);
    free(s.comp);
    free(mc); free(mr);
    vrowwise_free(&rw);
    vbasis_free(&b);
    *out = rep;
    return rc;
}

jaos_status jaos_exact_col_direction(const jaos_model *m, int64_t col,
                                     const char **out)
{
    return exact_get(m, m ? m->exact_uray : nullptr, col,
                     m ? m->num_col : 0, out);
}
