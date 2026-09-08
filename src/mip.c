/* SPDX-License-Identifier: Apache-2.0 */

#include "jaos_internal.h"
#include "jaos_sys.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double MIP_INT_TOL = 1e-6;

constexpr double MIP_GAP = 1e-6;

constexpr int64_t MIP_CUT_ROUNDS = 1;

constexpr int64_t MIP_CUT_DEPTH = 3;

constexpr int64_t MIP_NODE_CUT_CAP = 4;

constexpr int64_t MIP_COVER_ROUNDS = 4;

constexpr int64_t MIP_CLIQUE_ROUNDS = 4;
constexpr int64_t MIP_CLIQUE_ROW_CAP = 64;

constexpr double MIP_CUT_STALL = 0.0;

constexpr double MIP_NODE_CUT_STALL = 0.0;

constexpr bool MIP_ROOT_CUT_DROP = true;

constexpr bool MIP_COVER_LIFT = false;

constexpr int64_t MIP_MIR_ROUNDS = 6;

constexpr int64_t MIP_MIR_DELTAS = 8;

constexpr double MIP_MIR_ROUND = 1e-9;

constexpr int64_t MIP_MIR_AGGREGATE = 0;

constexpr int64_t MIP_DIVE_HEURISTIC = 50;

constexpr int64_t MIP_DIVE_HEURISTIC_DEPTH = 0;

constexpr int64_t MIP_RINS = 0;

constexpr double MIP_MIR_LAMBDA = 1e6;

constexpr int64_t MIP_DIVE_BACKTRACK = 0;

constexpr double MIP_DIVE_GAP = 0.0;

constexpr double MIP_DIVE_DEGRADE = 0.0;

constexpr int64_t MIP_FEASPUMP = 20;

constexpr int64_t MIP_PUMP_FLIPS = 10;

constexpr bool MIP_PUMP_GENERAL = false;

constexpr double MIP_PUMP_OBJ = 0.5;

constexpr bool MIP_PUMP_ALWAYS = false;

constexpr bool MIP_RCFIX = false;
constexpr bool MIP_TIGHTEN = true;
constexpr bool MIP_PROBING = false;
constexpr int64_t MIP_PROBING_ROUNDS = 2;
constexpr double MIP_PROBING_CAP = 1.0;
constexpr bool MIP_CLIQUE_FIX = false;

constexpr double MIP_RCFIX_SLACK = 1e-6;

constexpr int64_t MIP_PROPAGATE = 0;

constexpr int64_t MIP_PROPAGATE_DEPTH = -1;

constexpr double MIP_PROP_SLACK = 1e-9;
constexpr double MIP_TIGHTEN_MIN = 1e-9;

constexpr double MIP_PROP_INFEAS = 1e-7;

constexpr double MIP_PROP_MOVE = 0.5;

constexpr bool MIP_NODE_MIR = false;

constexpr double MIP_CUT_AWAY = 0.01;

constexpr double MIP_CUT_DROP = 1e-9;

constexpr double MIP_CUT_DYNAMISM = 1e6;

constexpr double MIP_PC_EPS = 1e-6;

constexpr int64_t MIP_RELIABILITY = 0;
constexpr int64_t MIP_STRONG_CANDIDATES = 8;

constexpr double MIP_PROBE_CAP = 0.0;

constexpr int64_t MIP_PROBE_DEPTH = -1;

typedef struct {
    int64_t *start, *idx;
    double *val, *lo;
    double *eff;
    int64_t n, nnz;
    int64_t cap_start, cap_lo, cap_idx, cap_val, cap_eff;
} cutbuf;

static inline bool semi_live(const jaos_model *m, int64_t j)
{
    return m->col_semi != nullptr && m->col_semi[j] && m->col_lower[j] > 0.0;
}

static inline bool semi_broken(const jaos_model *m, int64_t j, double x)
{
    return semi_live(m, j) && x > MIP_INT_TOL &&
           x < m->col_lower[j] - MIP_INT_TOL;
}

static inline bool ind_inactive(const jaos_model *m, const double *x,
                                int64_t i)
{
    if (m->row_ind_col == nullptr || m->row_ind_col[i] < 0)
        return false;
    return fabs(x[m->row_ind_col[i]] - (double)m->row_ind_val[i]) > 0.5;
}

static int64_t indicator_violated(const jaos_model *m, const double *x)
{
    if (m->row_ind_col == nullptr)
        return -1;
    const double tol = jm_primal_tolerance(m);
    for (int64_t i = 0; i < m->num_row; i++) {
        if (m->row_ind_col[i] < 0 || ind_inactive(m, x, i))
            continue;
        double act = 0.0;
        for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++)
            act += m->ar_value[k] * x[m->ar_index[k]];
        if (act < m->row_lower[i] - tol || act > m->row_upper[i] + tol)
            return m->row_ind_col[i];
    }
    return -1;
}

static void cutbuf_free(cutbuf *cb)
{
    free(cb->start); free(cb->idx); free(cb->val); free(cb->lo); free(cb->eff);
}

static int64_t cutbuf_keep_best(cutbuf *cb, int64_t cap)
{
    if (cap <= 0 || cb->n <= cap)
        return cb->n;
    bool *keep = calloc((size_t)cb->n, sizeof *keep);
    if (keep == nullptr)
        return -1;
    for (int64_t k = 0; k < cap; k++) {
        int64_t best = -1;
        for (int64_t r = 0; r < cb->n; r++)
            if (!keep[r] && (best < 0 || cb->eff[r] > cb->eff[best]))
                best = r;
        keep[best] = true;
    }
    int64_t n = 0, nnz = 0;
    for (int64_t r = 0; r < cb->n; r++) {
        if (!keep[r])
            continue;
        const int64_t s = cb->start[r], e = cb->start[r + 1];
        if (nnz != s) {
            memmove(cb->idx + nnz, cb->idx + s, (size_t)(e - s) * sizeof *cb->idx);
            memmove(cb->val + nnz, cb->val + s, (size_t)(e - s) * sizeof *cb->val);
        }
        cb->start[n] = nnz;
        cb->lo[n] = cb->lo[r];
        cb->eff[n] = cb->eff[r];
        nnz += e - s;
        n++;
    }
    cb->start[n] = nnz;
    cb->n = n;
    cb->nnz = nnz;
    free(keep);
    return n;
}

static bool cutbuf_append(cutbuf *dst, const cutbuf *src, int64_t r)
{
    const int64_t s = src->start[r], nz = src->start[r + 1] - s;
    if (!JM_GROW(dst->start, dst->cap_start, dst->n + 2) ||
        !JM_GROW(dst->lo, dst->cap_lo, dst->n + 2) ||
        !JM_GROW(dst->idx, dst->cap_idx, dst->nnz + nz) ||
        !JM_GROW(dst->val, dst->cap_val, dst->nnz + nz))
        return false;
    dst->start[dst->n] = dst->nnz;
    if (nz > 0) {
        memcpy(dst->idx + dst->nnz, src->idx + s, (size_t)nz * sizeof *dst->idx);
        memcpy(dst->val + dst->nnz, src->val + s, (size_t)nz * sizeof *dst->val);
    }
    dst->nnz += nz;
    dst->lo[dst->n] = src->lo[r];
    dst->n++;
    dst->start[dst->n] = dst->nnz;
    return true;
}

static jaos_status pool_add(jaos_model *lp, const cutbuf *pool,
                            const int64_t *which, int64_t n)
{
    int64_t nnz = 0;
    for (int64_t k = 0; k < n; k++)
        nnz += pool->start[which[k] + 1] - pool->start[which[k]];
    int64_t *start = malloc((size_t)(n + 1) * sizeof *start);
    int64_t *idx = malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof *idx);
    double *val = malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof *val);
    double *lo = malloc((size_t)n * sizeof *lo);
    double *up = malloc((size_t)n * sizeof *up);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (start == nullptr || idx == nullptr || val == nullptr ||
        lo == nullptr || up == nullptr)
        goto out;
    int64_t p = 0;
    for (int64_t k = 0; k < n; k++) {
        const int64_t c = which[k], s = pool->start[c], e = pool->start[c + 1];
        start[k] = p;
        for (int64_t q = s; q < e; q++) {
            idx[p] = pool->idx[q];
            val[p] = pool->val[q];
            p++;
        }
        lo[k] = pool->lo[c];
        up[k] = INFINITY;
    }
    start[n] = p;
    st = jaos_add_rows(lp, n, lo, up, p, start, idx, val);
out:
    free(start); free(idx); free(val); free(lo); free(up);
    return st;
}

typedef struct {
    double key;
    int64_t id;
    int64_t depth;
    int64_t nfix;
    int64_t *col;
    double *lo, *hi;
    jaos_basis_status *cs, *rs;
    double frac;
    bool up;
    int64_t *cuts;
    int64_t ncuts;
    bool no_cuts;
} bnode;

static void node_free(bnode *n)
{
    if (n == nullptr)
        return;
    free(n->col); free(n->lo); free(n->hi); free(n->cs); free(n->rs);
    free(n->cuts);
    free(n);
}

typedef struct {
    bnode **v;
    int64_t n, cap;
} bheap;

static bool node_before(const bnode *a, const bnode *b)
{
    return a->key < b->key || (a->key == b->key && a->id < b->id);
}

static bool heap_push(bheap *h, bnode *x)
{
    if (!JM_GROW(h->v, h->cap, h->n + 1))
        return false;
    int64_t i = h->n++;
    h->v[i] = x;
    while (i > 0) {
        const int64_t p = (i - 1) / 2;
        if (!node_before(h->v[i], h->v[p]))
            break;
        bnode *t = h->v[i]; h->v[i] = h->v[p]; h->v[p] = t;
        i = p;
    }
    return true;
}

static bnode *heap_pop(bheap *h)
{
    if (h->n == 0)
        return nullptr;
    bnode *top = h->v[0];
    h->v[0] = h->v[--h->n];
    int64_t i = 0;
    for (;;) {
        const int64_t l = 2 * i + 1, r = l + 1;
        int64_t best = i;
        if (l < h->n && node_before(h->v[l], h->v[best])) best = l;
        if (r < h->n && node_before(h->v[r], h->v[best])) best = r;
        if (best == i)
            break;
        bnode *t = h->v[i]; h->v[i] = h->v[best]; h->v[best] = t;
        i = best;
    }
    return top;
}

static double open_key(const bheap *h, bnode *const *stack, int64_t n)
{
    double k = h->n > 0 ? h->v[0]->key : INFINITY;
    for (int64_t i = 0; i < n; i++)
        if (stack[i]->key < k)
            k = stack[i]->key;
    return k;
}

static bool resume_within(const bnode *n, const bheap *h, bnode *const *stack,
                          int64_t stack_n, double frac)
{
    if (frac <= 0.0)
        return true;
    const double best = open_key(h, stack, stack_n);
    if (!isfinite(best))
        return true;
    return n->key - best <= frac * (1.0 + fabs(best));
}

static bnode *node_child(const bnode *parent, int64_t nc, int64_t nr,
                         const jaos_basis_status *cs,
                         const jaos_basis_status *rs, int64_t nfix,
                         const int64_t *fcol, const double *flo,
                         const double *fhi, double key, int64_t id,
                         double frac, bool up, const int64_t *act,
                         int64_t act_n, bool no_cuts)
{
    bnode *n = calloc(1, sizeof *n);
    if (n == nullptr)
        return nullptr;
    const int64_t d = (parent ? parent->depth : 0) + 1;
    const int64_t pf = parent ? parent->nfix : 0;
    const int64_t nf = pf + nfix;
    n->col = malloc((size_t)(nf > 0 ? nf : 1) * sizeof *n->col);
    n->lo = malloc((size_t)(nf > 0 ? nf : 1) * sizeof *n->lo);
    n->hi = malloc((size_t)(nf > 0 ? nf : 1) * sizeof *n->hi);
    n->cs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *n->cs);
    n->rs = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *n->rs);
    n->cuts = malloc((size_t)(act_n > 0 ? act_n : 1) * sizeof *n->cuts);
    if (!n->col || !n->lo || !n->hi || !n->cs || !n->rs || !n->cuts) {
        node_free(n);
        return nullptr;
    }
    if (act_n > 0)
        memcpy(n->cuts, act, (size_t)act_n * sizeof *n->cuts);
    n->ncuts = act_n;
    if (parent != nullptr && pf > 0) {
        memcpy(n->col, parent->col, (size_t)pf * sizeof *n->col);
        memcpy(n->lo, parent->lo, (size_t)pf * sizeof *n->lo);
        memcpy(n->hi, parent->hi, (size_t)pf * sizeof *n->hi);
    }
    for (int64_t k = 0; k < nfix; k++) {
        n->col[pf + k] = fcol[k];
        n->lo[pf + k] = flo[k];
        n->hi[pf + k] = fhi[k];
    }
    n->nfix = nf;
    n->depth = d;
    n->key = key;
    n->id = id;
    n->frac = frac;
    n->up = up;
    n->no_cuts = no_cuts;
    if (nc > 0)
        memcpy(n->cs, cs, (size_t)nc * sizeof *n->cs);
    if (nr > 0)
        memcpy(n->rs, rs, (size_t)nr * sizeof *n->rs);
    return n;
}

typedef struct {
    int64_t *v;
    int64_t n, cap;
} cutlist;

static bool cutlist_set(cutlist *l, const int64_t *v, int64_t n)
{
    if (!JM_GROW(l->v, l->cap, n > 0 ? n : 1))
        return false;
    if (n > 0)
        memcpy(l->v, v, (size_t)n * sizeof *v);
    l->n = n;
    return true;
}

static bool cutlist_same(const cutlist *l, const int64_t *v, int64_t n)
{
    return l->n == n && (n == 0 || memcmp(l->v, v, (size_t)n * sizeof *v) == 0);
}

static jaos_status node_apply(jaos_model *lp, const jaos_model *m,
                              const double *ilo, const double *ihi,
                              const bnode *n, const cutbuf *pool,
                              int64_t nfixed, cutlist *in_copy)
{
    jaos_status st = JAOS_OK;
    const int64_t want_n = n != nullptr ? n->ncuts : 0;
    const int64_t *want = n != nullptr ? n->cuts : nullptr;

    if (!cutlist_same(in_copy, want, want_n)) {
        if (in_copy->n > 0) {
            int64_t *del = malloc((size_t)in_copy->n * sizeof *del);
            if (del == nullptr)
                return JAOS_ERR_OUT_OF_MEMORY;
            for (int64_t k = 0; k < in_copy->n; k++)
                del[k] = nfixed + k;
            st = jaos_delete_rows(lp, in_copy->n, del);
            free(del);
            in_copy->n = 0;
        }
        if (st == JAOS_OK && want_n > 0)
            st = pool_add(lp, pool, want, want_n);
        if (st == JAOS_OK && !cutlist_set(in_copy, want, want_n))
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t k = 0; st == JAOS_OK && k < m->num_sos; k++)
        for (int64_t t = m->sos_start[k];
             st == JAOS_OK && t < m->sos_start[k + 1]; t++) {
            const int64_t j = m->sos_col[t];
            if (!m->col_integer[j] && !semi_live(m, j))
                st = jaos_set_col_bounds(lp, j, m->col_lower[j],
                                         m->col_upper[j]);
        }
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++)
        if (m->col_integer[j] || semi_live(m, j))
            st = jaos_set_col_bounds(lp, j, ilo[j], ihi[j]);
    for (int64_t k = 0; st == JAOS_OK && n != nullptr && k < n->nfix; k++)
        st = jaos_set_col_bounds(lp, n->col[k], n->lo[k], n->hi[k]);
    if (m->row_ind_col != nullptr)
        for (int64_t i = 0; st == JAOS_OK && i < m->num_row; i++) {
            const int64_t j = m->row_ind_col[i];
            if (j < 0)
                continue;
            const bool on = lp->col_lower[j] == lp->col_upper[j] &&
                            lp->col_lower[j] == (double)m->row_ind_val[i];
            st = jaos_set_row_bounds(lp, i, on ? m->row_lower[i] : -INFINITY,
                                     on ? m->row_upper[i] : INFINITY);
        }
    if (st == JAOS_OK && n != nullptr)
        st = jaos_set_basis(lp, n->cs, n->rs);
    return st;
}

static double now_seconds(void)
{
    return jm_monotonic_seconds();
}

typedef struct {
    bool have;
    double obj, key;
    double *x, *ra, *rd, *cd;
    jaos_basis_status *cs, *rs;
} incumbent;

static bool incumbent_take(incumbent *inc, const jaos_model *lp, int64_t nr,
                           double key)
{
    const int64_t nc = lp->num_col;
    if (inc->x == nullptr) {
        inc->x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *inc->x);
        inc->cd = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *inc->cd);
        inc->cs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *inc->cs);
        inc->ra = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *inc->ra);
        inc->rd = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *inc->rd);
        inc->rs = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *inc->rs);
        if (!inc->x || !inc->cd || !inc->cs || !inc->ra || !inc->rd || !inc->rs)
            return false;
    }
    if (nc > 0) {
        memcpy(inc->x, lp->sol_col, (size_t)nc * sizeof *inc->x);
        memcpy(inc->cd, lp->sol_redcost, (size_t)nc * sizeof *inc->cd);
        memcpy(inc->cs, lp->sol_col_status, (size_t)nc * sizeof *inc->cs);
    }
    if (nr > 0) {
        memcpy(inc->ra, lp->sol_row, (size_t)nr * sizeof *inc->ra);
        memcpy(inc->rd, lp->sol_dual, (size_t)nr * sizeof *inc->rd);
        memcpy(inc->rs, lp->sol_row_status, (size_t)nr * sizeof *inc->rs);
    }
    inc->obj = lp->objective;
    inc->key = key;
    inc->have = true;
    return true;
}

static void incumbent_free(incumbent *inc)
{
    free(inc->x); free(inc->ra); free(inc->rd); free(inc->cd);
    free(inc->cs); free(inc->rs);
}

static bool republish_at_the_incumbent(jaos_model *m, const double *point,
                                       int64_t nc, int64_t nr,
                                       int64_t *extra_work)
{
    jaos_model *fin = nullptr;
    bool ok = false;
    if (jaos_model_copy(m, &fin) != JAOS_OK)
        return false;
    fin->cfg.node_solve = true;

    free(fin->col_integer);
    fin->col_integer = nullptr;
    free(fin->col_semi);
    fin->col_semi = nullptr;
    for (int64_t k = 0; k < m->num_sos; k++)
        for (int64_t t = m->sos_start[k]; t < m->sos_start[k + 1]; t++) {
            const int64_t j = m->sos_col[t];
            if (fabs(point[j]) <= MIP_INT_TOL &&
                jaos_set_col_bounds(fin, j, 0.0, 0.0) != JAOS_OK)
                goto out;
        }
    fin->num_sos = 0;
    if (m->row_ind_col != nullptr) {
        for (int64_t i = 0; i < nr; i++)
            if (m->row_ind_col[i] >= 0 && ind_inactive(m, point, i) &&
                jaos_set_row_bounds(fin, i, -INFINITY, INFINITY) != JAOS_OK)
                goto out;
        free(fin->row_ind_col);
        fin->row_ind_col = nullptr;
        free(fin->row_ind_val);
        fin->row_ind_val = nullptr;
    }

    fin->cfg.log_cb = nullptr;

    for (int64_t j = 0; j < nc; j++) {
        if (semi_live(m, j) && point[j] < 0.5 * m->col_lower[j]) {
            if (jaos_set_col_bounds(fin, j, 0.0, 0.0) != JAOS_OK)
                goto out;
            continue;
        }
        if (m->col_integer == nullptr || !m->col_integer[j])
            continue;

        const double v = floor(point[j] + 0.5);
        if (jaos_set_col_bounds(fin, j, v, v) != JAOS_OK)
            goto out;
    }
    if (jaos_solve(fin) != JAOS_OK)
        goto out;
    *extra_work += jaos_work_units(fin);
    if (jaos_status_of(fin) != JAOS_SOLVE_OPTIMAL || !fin->sol_basis_ok)
        goto out;

    if (nc > 0) {
        memcpy(m->sol_col, fin->sol_col, (size_t)nc * sizeof *m->sol_col);
        memcpy(m->sol_redcost, fin->sol_redcost,
               (size_t)nc * sizeof *m->sol_redcost);
        memcpy(m->sol_col_status, fin->sol_col_status,
               (size_t)nc * sizeof *m->sol_col_status);
    }
    if (nr > 0) {
        memcpy(m->sol_row, fin->sol_row, (size_t)nr * sizeof *m->sol_row);
        memcpy(m->sol_dual, fin->sol_dual, (size_t)nr * sizeof *m->sol_dual);
        memcpy(m->sol_row_status, fin->sol_row_status,
               (size_t)nr * sizeof *m->sol_row_status);
    }

    for (int64_t j = 0; j < nc; j++)
        if (m->col_integer != nullptr && m->col_integer[j])
            m->sol_col[j] = floor(point[j] + 0.5);
    ok = true;
out:
    jaos_model_free(fin);
    return ok;
}

static int64_t sos_violated(const jaos_model *m, const double *x,
                            int64_t from, int64_t *first, int64_t *last)
{
    for (int64_t k = from; k < m->num_sos; k++) {
        const int64_t b = m->sos_start[k], e = m->sos_start[k + 1];
        int64_t cnt = 0, lo = -1, hi = -1;
        for (int64_t t = b; t < e; t++) {
            if (fabs(x[m->sos_col[t]]) <= MIP_INT_TOL)
                continue;
            if (cnt == 0)
                lo = t - b;
            hi = t - b;
            cnt++;
        }
        const bool bad = m->sos_type[k] == 1 ? cnt > 1
                                             : cnt > 2 || (cnt == 2 && hi - lo > 1);
        if (!bad)
            continue;
        if (first != nullptr)
            *first = lo;
        if (last != nullptr)
            *last = hi;
        return k;
    }
    return -1;
}

double jm_mip_default(enum jm_mip_key key)
{
    switch (key) {
    case JM_DEF_GAP: return MIP_GAP;
    case JM_DEF_CUT_ROUNDS: return (double)MIP_CUT_ROUNDS;
    case JM_DEF_CUT_DEPTH: return (double)MIP_CUT_DEPTH;
    case JM_DEF_NODE_CUT_CAP: return (double)MIP_NODE_CUT_CAP;
    case JM_DEF_COVER_ROUNDS: return (double)MIP_COVER_ROUNDS;
    case JM_DEF_CLIQUE_ROUNDS: return (double)MIP_CLIQUE_ROUNDS;
    case JM_DEF_CUT_STALL: return MIP_CUT_STALL;
    case JM_DEF_NODE_CUT_STALL: return MIP_NODE_CUT_STALL;
    case JM_DEF_ROOT_CUT_DROP: return MIP_ROOT_CUT_DROP ? 1.0 : 0.0;
    case JM_DEF_COVER_LIFT: return MIP_COVER_LIFT ? 1.0 : 0.0;
    case JM_DEF_MIR_ROUNDS: return (double)MIP_MIR_ROUNDS;
    case JM_DEF_MIR_AGGREGATE: return (double)MIP_MIR_AGGREGATE;
    case JM_DEF_DIVE_HEURISTIC: return (double)MIP_DIVE_HEURISTIC;
    case JM_DEF_DIVE_HEURISTIC_DEPTH: return (double)MIP_DIVE_HEURISTIC_DEPTH;
    case JM_DEF_RINS: return (double)MIP_RINS;
    case JM_DEF_DIVE_BACKTRACK: return (double)MIP_DIVE_BACKTRACK;
    case JM_DEF_DIVE_GAP: return MIP_DIVE_GAP;
    case JM_DEF_DIVE_DEGRADE: return MIP_DIVE_DEGRADE;
    case JM_DEF_FEASPUMP: return (double)MIP_FEASPUMP;
    case JM_DEF_PUMP_GENERAL: return MIP_PUMP_GENERAL ? 1.0 : 0.0;
    case JM_DEF_PUMP_OBJ: return MIP_PUMP_OBJ;
    case JM_DEF_PUMP_ALWAYS: return MIP_PUMP_ALWAYS ? 1.0 : 0.0;
    case JM_DEF_RCFIX: return MIP_RCFIX ? 1.0 : 0.0;
    case JM_DEF_TIGHTEN: return MIP_TIGHTEN ? 1.0 : 0.0;
    case JM_DEF_PROBING: return MIP_PROBING ? 1.0 : 0.0;
    case JM_DEF_PROBING_CAP: return MIP_PROBING_CAP;
    case JM_DEF_CLIQUE_FIX: return MIP_CLIQUE_FIX ? 1.0 : 0.0;
    case JM_DEF_PROPAGATE: return (double)MIP_PROPAGATE;
    case JM_DEF_PROPAGATE_DEPTH: return (double)MIP_PROPAGATE_DEPTH;
    case JM_DEF_NODE_MIR: return MIP_NODE_MIR ? 1.0 : 0.0;
    case JM_DEF_RELIABILITY: return (double)MIP_RELIABILITY;
    }
    return 0.0;
}

bool jm_model_has_integer(const jaos_model *m)
{
    if (m->col_integer == nullptr)
        return false;
    if (m->num_sos > 0)
        return true;
    for (int64_t j = 0; j < m->num_col; j++)
        if (m->col_integer[j] || semi_live(m, j))
            return true;
    return false;
}

static int64_t most_fractional(const jaos_model *m, const double *x)
{
    int64_t branch = -1;
    double worst = MIP_INT_TOL;
    for (int64_t j = 0; j < m->num_col; j++) {
        double f;
        if (m->col_integer[j] && fabs(x[j] - round(x[j])) > MIP_INT_TOL)
            f = fabs(x[j] - round(x[j]));
        else if (semi_broken(m, j, x[j]))
            f = fmin(x[j], m->col_lower[j] - x[j]) / m->col_lower[j];
        else
            continue;
        if (f > worst) {
            worst = f;
            branch = j;
        }
    }
    return branch;
}

static double pseudocost(int64_t j, int d, int64_t nc, const double *pc_sum,
                         const int64_t *pc_n)
{
    if (pc_n[d * nc + j] > 0)
        return pc_sum[d * nc + j] / (double)pc_n[d * nc + j];
    double sum = 0.0;
    int64_t cnt = 0;
    for (int64_t k = 0; k < nc; k++)
        if (pc_n[d * nc + k] > 0) {
            sum += pc_sum[d * nc + k] / (double)pc_n[d * nc + k];
            cnt++;
        }
    return cnt > 0 ? sum / (double)cnt : 1.0;
}

static int64_t select_branch(const jaos_model *m, const double *x,
                             jaos_branching rule, const double *pc_sum,
                             const int64_t *pc_n)
{
    if (rule == JAOS_BRANCH_MOST_FRACTIONAL)
        return most_fractional(m, x);
    const int64_t nc = m->num_col;
    int64_t branch = -1;
    double best = -1.0, best_away = -1.0;
    for (int64_t j = 0; j < nc; j++) {
        double f = m->col_integer[j] ? x[j] - floor(x[j]) : 0.0;
        if (f <= MIP_INT_TOL || f >= 1.0 - MIP_INT_TOL) {
            if (!semi_broken(m, j, x[j]))
                continue;
            f = x[j] / m->col_lower[j];
        }
        const double qd = f * pseudocost(j, 0, nc, pc_sum, pc_n);
        const double qu = (1.0 - f) * pseudocost(j, 1, nc, pc_sum, pc_n);
        const double score = (qd > MIP_PC_EPS ? qd : MIP_PC_EPS) *
                             (qu > MIP_PC_EPS ? qu : MIP_PC_EPS);

        const double away = f < 1.0 - f ? f : 1.0 - f;
        if (score > best || (score == best && away > best_away)) {
            best = score;
            best_away = away;
            branch = j;
        }
    }
    return branch;
}

static void pseudocost_learn(const bnode *n, double key, int64_t nc,
                             double *pc_sum, int64_t *pc_n)
{
    if (n == nullptr || n->nfix == 0 || n->frac <= 0.0)
        return;
    const int64_t j = n->col[n->nfix - 1];
    const int d = n->up ? 1 : 0;
    double gain = key - n->key;
    if (gain < 0.0)
        gain = 0.0;
    pc_sum[d * nc + j] += gain / n->frac;
    pc_n[d * nc + j] += 1;
}

static double pc_score(int64_t j, double f, int64_t nc, const double *pc_sum,
                       const int64_t *pc_n)
{
    const double qd = f * pseudocost(j, 0, nc, pc_sum, pc_n);
    const double qu = (1.0 - f) * pseudocost(j, 1, nc, pc_sum, pc_n);
    return (qd > MIP_PC_EPS ? qd : MIP_PC_EPS) * (qu > MIP_PC_EPS ? qu : MIP_PC_EPS);
}

static jaos_status strong_probe(jaos_model *lp, const jaos_model *m,
                                const double *x, double key, double sigma,
                                int64_t reliability, int64_t cap,
                                double *pc_sum, int64_t *pc_n, int64_t *work,
                                int64_t *solves, int64_t *probes,
                                int64_t *capped, jaos_basis_status *cs,
                                jaos_basis_status *rs, int64_t *cand)
{
    const int64_t nc = m->num_col, nr = lp->num_row;
    int64_t ncand = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        const double f = x[j] - floor(x[j]);
        if (f <= MIP_INT_TOL || f >= 1.0 - MIP_INT_TOL)
            continue;
        if (pc_n[j] >= reliability && pc_n[nc + j] >= reliability)
            continue;

        const double sc = pc_score(j, f, nc, pc_sum, pc_n);
        int64_t p = ncand < MIP_STRONG_CANDIDATES ? ncand : MIP_STRONG_CANDIDATES - 1;
        if (p == MIP_STRONG_CANDIDATES - 1 && ncand == MIP_STRONG_CANDIDATES) {
            const int64_t last = cand[p];
            const double fl = x[last] - floor(x[last]);
            if (sc <= pc_score(last, fl, nc, pc_sum, pc_n))
                continue;
        }
        while (p > 0) {
            const int64_t k = cand[p - 1];
            const double fk = x[k] - floor(x[k]);
            if (pc_score(k, fk, nc, pc_sum, pc_n) >= sc)
                break;
            cand[p] = cand[p - 1];
            p--;
        }
        cand[p] = j;
        if (ncand < MIP_STRONG_CANDIDATES)
            ncand++;
    }
    if (ncand == 0)
        return JAOS_OK;
    if (nc > 0)
        memcpy(cs, lp->sol_col_status, (size_t)nc * sizeof *cs);
    if (nr > 0)
        memcpy(rs, lp->sol_row_status, (size_t)nr * sizeof *rs);
    jaos_status st = JAOS_OK;
    for (int64_t c = 0; c < ncand && st == JAOS_OK; c++) {
        const int64_t j = cand[c];
        const double f = x[j] - floor(x[j]);
        const double lo0 = lp->col_lower[j], hi0 = lp->col_upper[j];
        for (int d = 0; d < 2 && st == JAOS_OK; d++) {
            if (pc_n[d * nc + j] >= reliability)
                continue;
            st = d == 0 ? jaos_set_col_bounds(lp, j, lo0, floor(x[j]))
                        : jaos_set_col_bounds(lp, j, ceil(x[j]), hi0);
            if (st == JAOS_OK)
                st = jaos_set_basis(lp, cs, rs);
            const int64_t caller_limit = lp->cfg.work_limit;
            const bool capping = cap > 0 &&
                                 (caller_limit <= 0 || cap < caller_limit);
            if (capping)
                lp->cfg.work_limit = cap;
            if (st == JAOS_OK)
                st = jaos_solve(lp);
            lp->cfg.work_limit = caller_limit;
            (*solves)++;
            (*probes)++;
            *work += jaos_work_units(lp);
            if (st == JAOS_OK && capping &&
                jaos_status_of(lp) == JAOS_SOLVE_WORK_LIMIT)
                (*capped)++;
            if (st == JAOS_OK && jaos_status_of(lp) == JAOS_SOLVE_OPTIMAL) {
                double obj = 0.0;
                if (jaos_objective(lp, &obj) == JAOS_OK) {
                    double gain = sigma * obj - key;
                    if (gain < 0.0)
                        gain = 0.0;
                    pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                    pc_n[d * nc + j] += 1;
                }
            }
            const jaos_status back = jaos_set_col_bounds(lp, j, lo0, hi0);
            if (st == JAOS_OK)
                st = back;
        }
    }
    if (st != JAOS_OK)
        return st;

    st = jaos_set_basis(lp, cs, rs);
    if (st == JAOS_OK)
        st = jaos_solve(lp);
    (*solves)++;
    *work += jaos_work_units(lp);
    if (st == JAOS_OK && jaos_status_of(lp) != JAOS_SOLVE_OPTIMAL)
        st = JAOS_ERR_NUMERICAL;
    return st;
}

static bool cutbuf_push(cutbuf *cb, const double *cut, int64_t nc, double lo,
                        double eff)
{
    if (!JM_GROW(cb->start, cb->cap_start, cb->n + 2) ||
        !JM_GROW(cb->lo, cb->cap_lo, cb->n + 2) ||
        !JM_GROW(cb->eff, cb->cap_eff, cb->n + 2))
        return false;
    cb->eff[cb->n] = eff;
    int64_t nz = 0;
    for (int64_t k = 0; k < nc; k++)
        nz += cut[k] != 0.0;
    if (!JM_GROW(cb->idx, cb->cap_idx, cb->nnz + nz) ||
        !JM_GROW(cb->val, cb->cap_val, cb->nnz + nz))
        return false;
    cb->start[cb->n] = cb->nnz;
    for (int64_t k = 0; k < nc; k++) {
        if (cut[k] == 0.0)
            continue;
        cb->idx[cb->nnz] = k;
        cb->val[cb->nnz] = cut[k];
        cb->nnz++;
    }
    cb->lo[cb->n] = lo;
    cb->n++;
    cb->start[cb->n] = cb->nnz;
    return true;
}

static int cut_finish(const jaos_model *lp, cutbuf *cb, double *cut,
                      double rhs, const double *x)
{
    const int64_t nc = lp->num_col;
    double amax = 0.0;
    for (int64_t k = 0; k < nc; k++)
        if (fabs(cut[k]) > amax)
            amax = fabs(cut[k]);
    if (amax == 0.0)
        return 0;
    double amin = INFINITY;
    int64_t nnz = 0;
    for (int64_t k = 0; k < nc; k++) {
        const double c = cut[k];
        if (c == 0.0)
            continue;
        if (fabs(c) < MIP_CUT_DROP * amax) {

            const double b = c > 0.0 ? lp->col_upper[k] : lp->col_lower[k];
            if (isfinite(b)) {
                rhs -= c * b;
                cut[k] = 0.0;
                continue;
            }
        }
        if (fabs(c) < amin)
            amin = fabs(c);
        nnz++;
    }
    if (nnz == 0 || amax / amin > MIP_CUT_DYNAMISM)
        return 0;
    double act = 0.0, nrm = 0.0;
    for (int64_t k = 0; k < nc; k++) {
        act += cut[k] * x[k];
        nrm += cut[k] * cut[k];
    }
    if (!(rhs - act > 0.0))
        return 0;
    if (!cutbuf_push(cb, cut, nc, rhs, (rhs - act) / sqrt(nrm)))
        return -1;
    return 1;
}

static int64_t gomory_round(jaos_model *lp, const jaos_model *m,
                            const double *x, cutbuf *cb, double *row,
                            double *cut, int64_t *work)
{
    const int64_t nc = lp->num_col, nr = lp->num_row, nv = nc + nr;
    jm_tableau *tb = nullptr;
    if (jm_tableau_build(lp, &tb) != JAOS_OK)
        return -1;
    int64_t added = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        const int64_t p = jm_tableau_position(tb, j);
        if (p < 0)
            continue;
        const double xb = jm_tableau_value(tb, p);
        const double f0 = xb - floor(xb);
        if (f0 < MIP_CUT_AWAY || f0 > 1.0 - MIP_CUT_AWAY)
            continue;
        if (jm_tableau_row(tb, p, row) != JAOS_OK) {
            added = -1;
            break;
        }
        memset(cut, 0, (size_t)nc * sizeof *cut);
        double rhs = 1.0;
        bool ok = true;
        for (int64_t v = 0; v < nv && ok; v++) {
            const double a = row[v];
            if (a == 0.0)
                continue;
            const bool structural = v < nc;
            const int64_t i = v - nc;
            const jaos_basis_status s = structural ? lp->sol_col_status[v]
                                                   : lp->sol_row_status[i];
            double ap, shift;
            bool at_upper;
            if (s == JAOS_BASIS_AT_LOWER) {
                at_upper = false;
                ap = a;
                shift = structural ? lp->col_lower[v] : lp->row_lower[i];
            } else if (s == JAOS_BASIS_AT_UPPER) {
                at_upper = true;
                ap = -a;
                shift = structural ? lp->col_upper[v] : lp->row_upper[i];
            } else {
                ok = false;
                break;
            }
            if (!isfinite(shift)) {
                ok = false;
                break;
            }
            const bool integral = structural && m->col_integer[v] &&
                                  floor(shift) == shift;
            double g;
            if (integral) {
                const double f = ap - floor(ap);
                g = f <= f0 ? f / f0 : (1.0 - f) / (1.0 - f0);
            } else {
                g = ap >= 0.0 ? ap / f0 : -ap / (1.0 - f0);
            }
            if (g == 0.0)
                continue;

            const double sgn = at_upper ? -g : g;
            rhs += sgn * shift;
            if (structural) {
                cut[v] += sgn;
            } else {
                for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++)
                    cut[lp->ar_index[k]] += sgn * lp->ar_value[k];
            }
        }
        if (!ok)
            continue;
        const int pushed = cut_finish(lp, cb, cut, rhs, x);
        if (pushed < 0) {
            added = -1;
            break;
        }
        added += pushed;
    }
    *work += jm_tableau_work(tb);
    jm_tableau_free(tb);
    return added;
}

static jaos_status cuts_add(jaos_model *lp, const cutbuf *cb)
{
    double *up = malloc((size_t)cb->n * sizeof *up);
    if (up == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t r = 0; r < cb->n; r++)
        up[r] = INFINITY;
    const jaos_status st = jaos_add_rows(lp, cb->n, cb->lo, up, cb->nnz,
                                         cb->start, cb->idx, cb->val);
    free(up);
    return st;
}

typedef struct {
    int64_t col;
    double a, xv;
    bool compl;
} kitem;

static int kitem_cmp(const void *pa, const void *pb)
{
    const kitem *p = pa, *q = pb;
    const double kp = (1.0 - p->xv) / p->a, kq = (1.0 - q->xv) / q->a;
    if (kp < kq) return -1;
    if (kp > kq) return 1;
    return p->col < q->col ? -1 : p->col > q->col;
}

static int kitem_weight_cmp(const void *pa, const void *pb)
{
    const kitem *p = pa, *q = pb;
    if (p->a > q->a) return -1;
    if (p->a < q->a) return 1;
    return p->col < q->col ? -1 : p->col > q->col;
}

static int64_t cover_round(const jaos_model *m, jaos_model *lp,
                           const double *x, const double *ilo,
                           const double *ihi, cutbuf *cb, kitem *items,
                           double *cut, double *mu, bool lift,
                           int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    const double tol = jm_primal_tolerance(m);
    int64_t added = 0;

    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;
    *work += m->num_nz + nc + nr;
    for (int64_t i = 0; i < nr; i++) {
        bool binary = true;
        for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1] && binary; k++) {
            const int64_t j = lp->ar_index[k];
            binary = m->col_integer[j] && ilo[j] == 0.0 && ihi[j] == 1.0;
        }
        if (!binary)
            continue;
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            double b = sg * bound, total = 0.0;
            int64_t n = 0;
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++) {
                const int64_t j = lp->ar_index[k];
                const double a = sg * lp->ar_value[k];
                if (a == 0.0)
                    continue;
                if (a > 0.0) {
                    items[n] = (kitem){ .col = j, .a = a, .xv = x[j], .compl = false };
                } else {
                    items[n] = (kitem){ .col = j, .a = -a, .xv = 1.0 - x[j],
                                        .compl = true };
                    b += -a;
                }
                total += items[n].a;
                n++;
            }

            const double over = b + tol * (1.0 + fabs(b));
            if (n == 0 || !(total > over) || !(b >= 0.0))
                continue;
            qsort(items, (size_t)n, sizeof *items, kitem_cmp);
            double weight = 0.0;
            int64_t c = 0;
            while (c < n && !(weight > over))
                weight += items[c++].a;
            if (!(weight > over))
                continue;
            double amax = 0.0;
            for (int64_t k = 0; k < c; k++)
                if (items[k].a > amax)
                    amax = items[k].a;
            if (lift) {

                qsort(items, (size_t)c, sizeof *items, kitem_weight_cmp);
                mu[0] = 0.0;
                for (int64_t k = 0; k < c; k++)
                    mu[k + 1] = mu[k] + items[k].a;
            }

            double act = 0.0, nrm = 0.0, shift = 0.0;
            memset(cut, 0, (size_t)nc * sizeof *cut);
            for (int64_t k = 0; k < n; k++) {
                double alpha = 1.0;
                if (k >= c) {
                    if (lift) {
                        int64_t h = 0;
                        while (h < c && mu[h + 1] <= items[k].a)
                            h++;
                        alpha = (double)h;
                    } else {
                        alpha = items[k].a >= amax ? 1.0 : 0.0;
                    }
                    if (alpha == 0.0)
                        continue;
                }
                act += alpha * items[k].xv;
                cut[items[k].col] = items[k].compl ? alpha : -alpha;
                if (items[k].compl)
                    shift += alpha;
                nrm += alpha * alpha;
            }
            if (!(act > (double)(c - 1)))
                continue;

            if (!cutbuf_push(cb, cut, nc, shift - (double)(c - 1),
                             (act - (double)(c - 1)) / sqrt(nrm)))
                return -1;
            added++;
        }
    }
    return added;
}

typedef struct {
    int64_t a, b;
} lpair;

static int lpair_cmp(const void *pa, const void *pb)
{
    const lpair *p = pa, *q = pb;
    if (p->a != q->a)
        return p->a < q->a ? -1 : 1;
    return p->b < q->b ? -1 : p->b > q->b;
}

static bool lpair_adjacent(const lpair *e, int64_t ne, int64_t u, int64_t v)
{
    if (u == v)
        return false;
    const lpair key = {u < v ? u : v, u < v ? v : u};
    int64_t lo = 0, hi = ne;
    while (lo < hi) {
        const int64_t mid = lo + (hi - lo) / 2;
        const int c = lpair_cmp(&e[mid], &key);
        if (c == 0)
            return true;
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return false;
}

typedef struct {
    int64_t lit;
    double v;
} litval;

static int litval_cmp(const void *pa, const void *pb)
{
    const litval *p = pa, *q = pb;
    if (p->v > q->v) return -1;
    if (p->v < q->v) return 1;
    return p->lit < q->lit ? -1 : p->lit > q->lit;
}

static void set_coefficient(jaos_model *lp, int64_t i, int64_t j,
                            int64_t p, double value, int64_t *cost)
{
    lp->ar_value[p] = value;
    for (int64_t k = lp->a_start[j]; k < lp->a_start[j + 1]; k++) {
        if (lp->a_index[k] == i) {
            lp->a_value[k] = value;
            break;
        }
    }
    *cost += lp->a_start[j + 1] - lp->a_start[j];
}

static int64_t propagate_bounds(jaos_model *m, double *plo, double *phi,
                                int64_t rounds, int64_t *work);

typedef struct {
    int64_t fractional, probed, fixed, implied, conflicts;
    bool infeasible, capped;
} probe_report;

static bool probe_conflicts(const jaos_model *m, const jaos_model *lp,
                            int64_t j, int v, const double *lo,
                            const double *hi, lpair **extra, int64_t *ne,
                            int64_t *ecap, int64_t *found)
{
    const int64_t lu = 2 * j + (v == 0 ? 1 : 0);
    for (int64_t k = 0; k < m->num_col; k++) {
        if (k == j || !m->col_integer[k] || lo[k] != hi[k])
            continue;
        if (lp->col_lower[k] != 0.0 || lp->col_upper[k] != 1.0)
            continue;
        const int64_t lv = 2 * k + (lo[k] == 0.0 ? 0 : 1);
        if (!JM_GROW(*extra, *ecap, *ne + 1))
            return false;
        (*extra)[*ne].a = lu < lv ? lu : lv;
        (*extra)[*ne].b = lu < lv ? lv : lu;
        (*ne)++;
        (*found)++;
    }
    return true;
}

static int64_t probe_bound_moves(jaos_model *m, jaos_model *lp,
                                 const double *plo, const double *phi,
                                 const double *qlo, const double *qhi,
                                 double *ilo, double *ihi)
{
    int64_t moved = 0;
    for (int64_t k = 0; k < m->num_col; k++) {
        if (!m->col_integer[k])
            continue;
        const double lo = qlo == nullptr || plo[k] < qlo[k] ? plo[k] : qlo[k];
        const double hi = qhi == nullptr || phi[k] > qhi[k] ? phi[k] : qhi[k];
        const bool up = lo > lp->col_lower[k] + MIP_PROP_MOVE;
        const bool down = hi < lp->col_upper[k] - MIP_PROP_MOVE;
        if (!up && !down)
            continue;
        const double nlo = up ? lo : lp->col_lower[k];
        const double nhi = down ? hi : lp->col_upper[k];
        if (jaos_set_col_bounds(lp, k, nlo, nhi) != JAOS_OK)
            return -2;
        ilo[k] = nlo;
        ihi[k] = nhi;
        moved++;
    }
    return moved;
}

static jaos_status probe_root(jaos_model *m, jaos_model *lp, const double *x,
                              double *ilo, double *ihi, double *buf,
                              litval *order, int64_t cap, lpair **extra,
                              int64_t *nextra, int64_t *ecap,
                              probe_report *rep, int64_t *work)
{
    const int64_t nc = m->num_col;
    double *plo = buf, *phi = buf + nc, *qlo = buf + 2 * nc, *qhi = buf + 3 * nc;
    memset(rep, 0, sizeof *rep);
    int64_t n = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j] || ilo[j] != 0.0 || ihi[j] != 1.0)
            continue;
        if (x[j] <= MIP_INT_TOL || x[j] >= 1.0 - MIP_INT_TOL)
            continue;
        order[n].lit = j;
        order[n].v = 0.5 - fabs(x[j] - 0.5);
        n++;
    }
    rep->fractional = n;
    if (n > 1)
        qsort(order, (size_t)n, sizeof *order, litval_cmp);
    const int64_t start = *work;
    for (int64_t t = 0; t < n; t++) {
        if (cap > 0 && *work - start >= cap) {
            rep->capped = true;
            break;
        }
        const int64_t j = order[t].lit;
        if (lp->col_lower[j] == lp->col_upper[j])
            continue;
        rep->probed++;
        int64_t r[2] = {0, 0};
        for (int v = 0; v < 2; v++) {
            double *lo = v == 0 ? qlo : plo, *hi = v == 0 ? qhi : phi;
            memcpy(lo, lp->col_lower, (size_t)nc * sizeof *lo);
            memcpy(hi, lp->col_upper, (size_t)nc * sizeof *hi);
            lo[j] = (double)v;
            hi[j] = (double)v;
            *work += 2 * nc;
            r[v] = propagate_bounds(m, lo, hi, MIP_PROBING_ROUNDS, work);
            if (r[v] == -2)
                return JAOS_ERR_OUT_OF_MEMORY;
        }
        if (r[0] == -1 && r[1] == -1) {
            rep->infeasible = true;
            return JAOS_OK;
        }
        int64_t moved = 0;
        if (r[0] == -1)
            moved = probe_bound_moves(m, lp, plo, phi, nullptr, nullptr,
                                      ilo, ihi);
        else if (r[1] == -1)
            moved = probe_bound_moves(m, lp, qlo, qhi, nullptr, nullptr,
                                      ilo, ihi);
        else {
            if (!probe_conflicts(m, lp, j, 0, qlo, qhi, extra, nextra, ecap,
                                 &rep->conflicts) ||
                !probe_conflicts(m, lp, j, 1, plo, phi, extra, nextra, ecap,
                                 &rep->conflicts))
                return JAOS_ERR_OUT_OF_MEMORY;
            *work += 2 * nc;
            moved = probe_bound_moves(m, lp, plo, phi, qlo, qhi, ilo, ihi);
        }
        *work += nc;
        if (moved == -2)
            return JAOS_ERR_OUT_OF_MEMORY;
        if (r[0] == -1 || r[1] == -1) {
            rep->fixed++;
            moved--;
        }
        rep->implied += moved;
    }
    return JAOS_OK;
}

static int64_t tighten_coefficients(const jaos_model *m, jaos_model *lp,
                                    const double *ilo, const double *ihi,
                                    int64_t *rows_touched, int64_t *work)
{
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return 0;
    int64_t tightened = 0, cost = 0;
    *rows_touched = 0;
    for (int64_t i = 0; i < lp->num_row; i++) {
        double rl = lp->row_lower[i], ru = lp->row_upper[i];
        const bool le = isfinite(ru) && !isfinite(rl);
        const bool ge = isfinite(rl) && !isfinite(ru);
        if (!le && !ge)
            continue;
        const int64_t p0 = lp->ar_start[i], p1 = lp->ar_start[i + 1];
        double umax = 0.0, umin = 0.0;
        int64_t inf_max = 0, inf_min = 0;
        for (int64_t p = p0; p < p1; p++) {
            const int64_t j = lp->ar_index[p];
            const double a = lp->ar_value[p];
            const double hi = a > 0.0 ? ihi[j] : ilo[j];
            const double lo = a > 0.0 ? ilo[j] : ihi[j];
            if (isfinite(hi))
                umax += a * hi;
            else
                inf_max++;
            if (isfinite(lo))
                umin += a * lo;
            else
                inf_min++;
        }
        cost += p1 - p0;
        bool touched = false;
        for (int64_t p = p0; p < p1; p++) {
            const int64_t j = lp->ar_index[p];
            const double a = lp->ar_value[p];
            if (!m->col_integer[j] || ilo[j] != 0.0 || ihi[j] != 1.0 ||
                a == 0.0)
                continue;
            if (le) {
                if (inf_max > 0)
                    continue;
                const double without = umax - (a > 0.0 ? a : 0.0);
                const double d = ru - without;
                if (!(d > MIP_TIGHTEN_MIN * (1.0 + fabs(ru))))
                    continue;
                if (a > 0.0 && a > d) {
                    set_coefficient(lp, i, j, p, a - d, &cost);
                    ru -= d;
                    umax -= d;
                    touched = true;
                    tightened++;
                } else if (a < 0.0 && -a > d) {
                    set_coefficient(lp, i, j, p, a + d, &cost);
                    umin += d;
                    touched = true;
                    tightened++;
                }
            } else {
                if (inf_min > 0)
                    continue;
                const double without = umin - (a < 0.0 ? a : 0.0);
                const double d = without - rl;
                if (!(d > MIP_TIGHTEN_MIN * (1.0 + fabs(rl))))
                    continue;
                if (a < 0.0 && -a > d) {
                    set_coefficient(lp, i, j, p, a + d, &cost);
                    rl += d;
                    umin += d;
                    touched = true;
                    tightened++;
                } else if (a > 0.0 && a > d) {
                    set_coefficient(lp, i, j, p, a - d, &cost);
                    umax -= d;
                    touched = true;
                    tightened++;
                }
            }
        }
        if (touched) {
            lp->row_lower[i] = rl;
            lp->row_upper[i] = ru;
            (*rows_touched)++;
        }
    }
    *work += cost;
    return tightened;
}

typedef struct {
    lpair *edge;
    int64_t ne;
    int64_t *adj_start, *adj;
} clique_table;

static void clique_table_free(clique_table *t)
{
    free(t->edge);
    free(t->adj_start);
    free(t->adj);
    memset(t, 0, sizeof *t);
}

static int64_t clique_table_build(const jaos_model *m, jaos_model *lp,
                                  const double *ilo, const double *ihi,
                                  kitem *items, const lpair *extra,
                                  int64_t nextra, clique_table *t,
                                  int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    const double tol = jm_primal_tolerance(m);
    clique_table_free(t);
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;
    lpair *edges = nullptr;
    int64_t ne = 0, ecap = 0;
    *work += m->num_nz + nc + nr;
    for (int64_t i = 0; i < nr; i++) {
        bool binary = true;
        for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1] && binary; k++) {
            const int64_t j = lp->ar_index[k];
            binary = m->col_integer[j] && ilo[j] == 0.0 && ihi[j] == 1.0;
        }
        if (!binary)
            continue;
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            double b = sg * bound;
            int64_t n = 0;
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++) {
                const int64_t j = lp->ar_index[k];
                const double a = sg * lp->ar_value[k];
                if (a == 0.0)
                    continue;
                if (a > 0.0) {
                    items[n] = (kitem){ .col = j, .a = a, .xv = 0.0, .compl = false };
                } else {
                    items[n] = (kitem){ .col = j, .a = -a, .xv = 0.0,
                                        .compl = true };
                    b += -a;
                }
                n++;
            }
            if (n < 2 || !(b >= 0.0))
                continue;
            qsort(items, (size_t)n, sizeof *items, kitem_weight_cmp);
            if (n > MIP_CLIQUE_ROW_CAP)
                n = MIP_CLIQUE_ROW_CAP;
            const double over = b + tol * (1.0 + fabs(b));
            for (int64_t p = 0; p < n; p++)
                for (int64_t q = p + 1; q < n; q++) {
                    if (!(items[p].a + items[q].a > over))
                        break;
                    if (items[p].col == items[q].col)
                        continue;
                    const int64_t lu = 2 * items[p].col + (items[p].compl ? 1 : 0);
                    const int64_t lv = 2 * items[q].col + (items[q].compl ? 1 : 0);
                    if (!JM_GROW(edges, ecap, ne + 1)) {
                        free(edges);
                        return -1;
                    }
                    edges[ne].a = lu < lv ? lu : lv;
                    edges[ne].b = lu < lv ? lv : lu;
                    ne++;
                }
        }
    }
    for (int64_t k = 0; k < nextra; k++) {
        if (!JM_GROW(edges, ecap, ne + 1)) {
            free(edges);
            return -1;
        }
        edges[ne++] = extra[k];
    }
    if (ne == 0) {
        free(edges);
        return 0;
    }
    qsort(edges, (size_t)ne, sizeof *edges, lpair_cmp);
    {
        int64_t w = 0;
        for (int64_t k = 0; k < ne; k++)
            if (w == 0 || lpair_cmp(&edges[w - 1], &edges[k]) != 0)
                edges[w++] = edges[k];
        ne = w;
    }
    int64_t *adj_start = calloc((size_t)(2 * nc + 1), sizeof *adj_start);
    int64_t *adj = malloc((size_t)(2 * ne) * sizeof *adj);
    if (adj_start == nullptr || adj == nullptr) {
        free(edges); free(adj_start); free(adj);
        return -1;
    }
    for (int64_t k = 0; k < ne; k++) {
        adj_start[edges[k].a + 1]++;
        adj_start[edges[k].b + 1]++;
    }
    for (int64_t u = 0; u < 2 * nc; u++)
        adj_start[u + 1] += adj_start[u];
    for (int64_t k = 0; k < ne; k++) {
        adj[adj_start[edges[k].a]++] = edges[k].b;
        adj[adj_start[edges[k].b]++] = edges[k].a;
    }
    for (int64_t u = 2 * nc; u > 0; u--)
        adj_start[u] = adj_start[u - 1];
    adj_start[0] = 0;
    *work += 6 * ne;
    t->edge = edges;
    t->ne = ne;
    t->adj_start = adj_start;
    t->adj = adj;
    return ne;
}

static int64_t clique_fix(const jaos_model *m, jaos_model *lp,
                          const clique_table *t, int64_t *stack,
                          int64_t *work)
{
    const int64_t nc = m->num_col;
    if (t->ne == 0)
        return 0;
    int64_t n = 0, fixed = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (!m->col_integer[j] || lp->col_lower[j] != lp->col_upper[j])
            continue;
        if (t->adj_start[2 * j + 2] == t->adj_start[2 * j])
            continue;
        if (lp->col_lower[j] == 1.0)
            stack[n++] = 2 * j;
        else if (lp->col_lower[j] == 0.0)
            stack[n++] = 2 * j + 1;
    }
    *work += nc;
    while (n > 0) {
        const int64_t u = stack[--n];
        *work += t->adj_start[u + 1] - t->adj_start[u];
        for (int64_t p = t->adj_start[u]; p < t->adj_start[u + 1]; p++) {
            const int64_t v = t->adj[p], k = v / 2;
            if (v % 2 == 0) {
                if (lp->col_lower[k] > 0.0)
                    return -1;
                if (lp->col_upper[k] > 0.0) {
                    if (jaos_set_col_bounds(lp, k, lp->col_lower[k], 0.0)
                        != JAOS_OK)
                        return -2;
                    fixed++;
                    stack[n++] = 2 * k + 1;
                }
            } else {
                if (lp->col_upper[k] < 1.0)
                    return -1;
                if (lp->col_lower[k] < 1.0) {
                    if (jaos_set_col_bounds(lp, k, 1.0, lp->col_upper[k])
                        != JAOS_OK)
                        return -2;
                    fixed++;
                    stack[n++] = 2 * k;
                }
            }
        }
    }
    return fixed;
}

static int64_t clique_round(const jaos_model *m, const clique_table *t,
                            const double *x, cutbuf *cb, double *cut,
                            int64_t *work)
{
    const int64_t nc = m->num_col;
    const double tol = jm_primal_tolerance(m);
    const lpair *edges = t->edge;
    const int64_t ne = t->ne;
    int64_t added = 0;
    if (ne == 0)
        return 0;
    *work += 2 * ne;

    litval *lits = malloc((size_t)(2 * ne) * sizeof *lits);
    bool *used = calloc((size_t)(2 * nc), sizeof *used);
    int64_t *members = malloc((size_t)(2 * nc) * sizeof *members);
    if (lits == nullptr || used == nullptr || members == nullptr) {
        free(lits); free(used); free(members);
        return -1;
    }
    int64_t nl = 0;
    for (int64_t k = 0; k < ne; k++) {
        lits[nl++].lit = edges[k].a;
        lits[nl++].lit = edges[k].b;
    }
    for (int64_t k = 0; k < nl; k++) {
        const int64_t j = lits[k].lit / 2;
        lits[k].v = lits[k].lit % 2 ? 1.0 - x[j] : x[j];
    }
    qsort(lits, (size_t)nl, sizeof *lits, litval_cmp);
    {
        int64_t w = 0;
        for (int64_t k = 0; k < nl; k++)
            if (w == 0 || lits[w - 1].lit != lits[k].lit)
                lits[w++] = lits[k];
        nl = w;
    }
    for (int64_t s = 0; s < nl; s++) {
        if (used[lits[s].lit] || !(lits[s].v > tol))
            continue;
        int64_t nm = 0;
        members[nm++] = lits[s].lit;
        double sum = lits[s].v;
        for (int64_t t = 0; t < nl; t++) {
            if (t == s || used[lits[t].lit])
                continue;
            const int64_t v = lits[t].lit;
            bool ok = true;
            for (int64_t q = 0; q < nm && ok; q++)
                ok = members[q] / 2 != v / 2 &&
                     lpair_adjacent(edges, ne, members[q], v);
            if (!ok)
                continue;
            members[nm++] = v;
            sum += lits[t].v;
        }
        *work += nm * nm;
        if (nm < 2 || !(sum > 1.0 + tol))
            continue;
        memset(cut, 0, (size_t)nc * sizeof *cut);
        int64_t ncompl = 0;
        for (int64_t q = 0; q < nm; q++) {
            const int64_t j = members[q] / 2;
            const bool compl = members[q] % 2 == 1;
            cut[j] = compl ? 1.0 : -1.0;
            ncompl += compl;
            used[members[q]] = true;
        }
        if (!cutbuf_push(cb, cut, nc, (double)ncompl - 1.0,
                         (sum - 1.0) / sqrt((double)nm))) {
            added = -1;
            break;
        }
        added++;
    }
    free(lits); free(used); free(members);
    return added;
}

static bool shift_to_upper(double lo, double hi, double xj)
{
    if (!isfinite(lo))
        return true;
    if (!isfinite(hi))
        return false;
    return hi - xj < xj - lo;
}

static int mir_side(const jaos_model *m, jaos_model *lp, const double *x,
                    const double *ilo, const double *ihi, const double *a_in,
                    double b_in, double mag_in, int64_t terms_in,
                    const double *cmag, cutbuf *cb, double *cut, double *best,
                    double *delta)
{
    const int64_t nc = m->num_col;
    if (cmag != nullptr)
        for (int64_t j = 0; j < nc; j++)
            if (DBL_EPSILON * cmag[j] * (double)terms_in > MIP_MIR_ROUND)
                return 0;
    {
        {
            double b = b_in, mag = fabs(b_in) + mag_in;
            int64_t terms = 1 + terms_in;
            bool ok = true;
            int64_t nd = 1;
            delta[0] = 1.0;
            for (int64_t j = 0; j < nc; j++) {
                const double a = a_in[j];
                if (a == 0.0)
                    continue;
                if (!isfinite(ilo[j]) && !isfinite(ihi[j])) {
                    ok = false;
                    break;
                }
                const bool at_up = shift_to_upper(ilo[j], ihi[j], x[j]);
                const double shift = at_up ? a * ihi[j] : a * ilo[j];
                b -= shift;
                mag += fabs(shift);
                terms++;
                if (!m->col_integer[j] || nd > MIP_MIR_DELTAS)
                    continue;
                const double xs = at_up ? ihi[j] - x[j] : x[j] - ilo[j];
                const double fr = xs - floor(xs);
                if (fr <= MIP_INT_TOL || fr >= 1.0 - MIP_INT_TOL)
                    continue;
                const double d = fabs(a);
                bool seen = false;
                for (int64_t q = 0; q < nd && !seen; q++)
                    seen = delta[q] == d;
                if (!seen)
                    delta[nd++] = d;
            }

            if (!ok || DBL_EPSILON * mag * (double)terms > MIP_MIR_ROUND)
                return 0;
            double best_eff = 0.0, best_rhs = 0.0;
            bool have = false;
            for (int64_t q = 0; q < nd; q++) {
                const double d = delta[q], b0 = b / d;
                const double f0 = b0 - floor(b0);
                if (f0 < MIP_CUT_AWAY || f0 > 1.0 - MIP_CUT_AWAY)
                    continue;

                memset(cut, 0, (size_t)nc * sizeof *cut);
                double rhs = floor(b0);
                for (int64_t j = 0; j < nc; j++) {
                    const double a0 = a_in[j];
                    if (a0 == 0.0)
                        continue;
                    const bool at_up = shift_to_upper(ilo[j], ihi[j], x[j]);
                    const double a = (at_up ? -a0 : a0) / d;
                    double c;
                    if (m->col_integer[j]) {
                        const double fa = floor(a), fj = a - fa;
                        c = fa + (fj > f0 ? (fj - f0) / (1.0 - f0) : 0.0);
                    } else {
                        c = a < 0.0 ? a / (1.0 - f0) : 0.0;
                    }
                    if (c == 0.0)
                        continue;
                    if (at_up) {
                        cut[j] -= c;
                        rhs -= c * ihi[j];
                    } else {
                        cut[j] += c;
                        rhs += c * ilo[j];
                    }
                }
                double act = 0.0, nrm = 0.0;
                for (int64_t k = 0; k < nc; k++) {
                    act += cut[k] * x[k];
                    nrm += cut[k] * cut[k];
                }
                if (nrm == 0.0)
                    continue;
                const double eff = (act - rhs) / sqrt(nrm);
                if (eff > best_eff) {
                    best_eff = eff;
                    best_rhs = rhs;
                    have = true;
                    memcpy(best, cut, (size_t)nc * sizeof *best);
                }
            }
            if (!have)
                return 0;
            for (int64_t k = 0; k < nc; k++)
                cut[k] = -best[k];
            return cut_finish(lp, cb, cut, -best_rhs, x);
        }
    }
}

static int64_t mir_round(const jaos_model *m, jaos_model *lp, const double *x,
                         const double *ilo, const double *ihi, cutbuf *cb,
                         double *cut, double *best, double *delta,
                         double *agg, int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    int64_t added = 0;
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;

    for (int64_t j = 0; j < nc; j++)
        assert(!m->col_integer[j] ||
               ((!isfinite(ilo[j]) || floor(ilo[j]) == ilo[j]) &&
                (!isfinite(ihi[j]) || floor(ihi[j]) == ihi[j])));
    *work += (m->num_nz + nc + nr) * (MIP_MIR_DELTAS + 1);
    for (int64_t i = 0; i < nr; i++) {
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            memset(agg, 0, (size_t)nc * sizeof *agg);
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++)
                agg[lp->ar_index[k]] += sg * lp->ar_value[k];
            const int pushed = mir_side(m, lp, x, ilo, ihi, agg, sg * bound,
                                        0.0, 0, nullptr, cb, cut, best, delta);
            if (pushed < 0)
                return -1;
            added += pushed;
        }
    }
    return added;
}

static int64_t mir_aggregate_round(const jaos_model *m, jaos_model *lp,
                                   const double *x, const double *ilo,
                                   const double *ihi, cutbuf *cb, double *cut,
                                   double *best, double *delta, double *agg,
                                   double *cmag, bool *used, bool *picked,
                                   int64_t steps, int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    int64_t added = 0;
    if (jm_model_ensure_rowwise(lp) != JAOS_OK)
        return -1;
    if (steps > nr)
        steps = nr;
    for (int64_t i = 0; i < nr; i++) {
        for (int side = 0; side < 2; side++) {
            const double bound = side == 0 ? lp->row_upper[i] : lp->row_lower[i];
            if (!isfinite(bound))
                continue;
            const double sg = side == 0 ? 1.0 : -1.0;
            memset(agg, 0, (size_t)nc * sizeof *agg);
            for (int64_t k = lp->ar_start[i]; k < lp->ar_start[i + 1]; k++)
                agg[lp->ar_index[k]] += sg * lp->ar_value[k];
            memset(used, 0, (size_t)(nr > 0 ? nr : 1) * sizeof *used);
            memset(picked, 0, (size_t)(nc > 0 ? nc : 1) * sizeof *picked);
            memset(cmag, 0, (size_t)(nc > 0 ? nc : 1) * sizeof *cmag);
            used[i] = true;
            double b = sg * bound, mag = 0.0;
            int64_t terms = 0;
            for (int64_t s = 0; s < steps; s++) {

                *work += m->num_nz + nc + nr;

                int64_t pick = -1;
                double pick_a = 0.0;
                for (int64_t j = 0; j < nc; j++) {
                    if (agg[j] == 0.0 || m->col_integer[j] || picked[j])
                        continue;
                    const double lo = ilo[j], hi = ihi[j];
                    if ((isfinite(lo) && x[j] - lo <= MIP_INT_TOL) ||
                        (isfinite(hi) && hi - x[j] <= MIP_INT_TOL))
                        continue;
                    if (fabs(agg[j]) > pick_a) {
                        pick_a = fabs(agg[j]);
                        pick = j;
                    }
                }
                if (pick < 0)
                    break;

                int64_t rrow = -1;
                double lambda = 0.0, rbound = 0.0;
                for (int64_t k = lp->a_start[pick];
                     k < lp->a_start[pick + 1] && rrow < 0; k++) {
                    const int64_t r = lp->a_index[k];
                    if (r >= nr || used[r])
                        continue;
                    const double crj = lp->a_value[k];
                    if (crj == 0.0)
                        continue;
                    double rmax = 0.0;
                    for (int64_t q = lp->ar_start[r]; q < lp->ar_start[r + 1]; q++)
                        if (fabs(lp->ar_value[q]) > rmax)
                            rmax = fabs(lp->ar_value[q]);
                    if (fabs(crj) < MIP_CUT_DROP * rmax)
                        continue;
                    const double lam = agg[pick] / crj;
                    if (!isfinite(lam) || lam == 0.0 ||
                        fabs(lam) > MIP_MIR_LAMBDA ||
                        fabs(lam) < 1.0 / MIP_MIR_LAMBDA)
                        continue;
                    const double bnd = lam > 0.0 ? lp->row_lower[r]
                                                 : lp->row_upper[r];
                    if (!isfinite(bnd))
                        continue;
                    rrow = r;
                    lambda = lam;
                    rbound = bnd;
                }
                if (rrow < 0)
                    break;
                for (int64_t q = lp->ar_start[rrow]; q < lp->ar_start[rrow + 1]; q++) {
                    const int64_t j = lp->ar_index[q];
                    agg[j] -= lambda * lp->ar_value[q];
                    cmag[j] += fabs(lambda * lp->ar_value[q]);
                }
                picked[pick] = true;
                b -= lambda * rbound;
                mag += fabs(lambda * rbound);
                terms++;
                used[rrow] = true;
                const int pushed = mir_side(m, lp, x, ilo, ihi, agg, b, mag,
                                            terms, cmag, cb, cut, best, delta);
                if (pushed < 0)
                    return -1;
                added += pushed;
            }
        }
    }
    return added;
}

static int dive_for_point(const jaos_model *m, const jaos_model *lp,
                          int64_t solves, const double *agree_a,
                          const double *agree_b, double *out, int64_t *work,
                          int64_t *solves_done)
{
    const int64_t nc = m->num_col;
    jaos_model *hv = nullptr;
    if (jaos_model_copy(lp, &hv) != JAOS_OK)
        return -1;
    int rc = 0;
    if (agree_a != nullptr && agree_b != nullptr) {
        *work += nc;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            if (!isfinite(agree_a[j]) || !isfinite(agree_b[j]))
                continue;
            const double a = round(agree_a[j]);
            if (fabs(agree_a[j] - a) > MIP_INT_TOL ||
                fabs(agree_b[j] - a) > MIP_INT_TOL)
                continue;
            if (a < hv->col_lower[j] || a > hv->col_upper[j])
                continue;

            if (jaos_set_col_bounds(hv, j, a, a) != JAOS_OK)
                break;
        }
    }
    for (int64_t s = 0; s < solves; s++) {
        if (jaos_solve(hv) != JAOS_OK)
            break;
        *work += jaos_work_units(hv);
        (*solves_done)++;
        if (jaos_status_of(hv) != JAOS_SOLVE_OPTIMAL)
            break;
        if (jaos_solution(hv, out, nullptr, nullptr, nullptr) != JAOS_OK)
            break;
        int64_t pick = -1;
        double near = 2.0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j])
                continue;
            const double f = out[j] - floor(out[j]);
            const double d = f < 0.5 ? f : 1.0 - f;
            if (d <= MIP_INT_TOL)
                continue;
            if (d < near) {
                near = d;
                pick = j;
            }
        }
        if (pick < 0) {
            rc = 1;
            break;
        }
        double v = round(out[pick]);
        if (v < hv->col_lower[pick])
            v = hv->col_lower[pick];
        if (v > hv->col_upper[pick])
            v = hv->col_upper[pick];
        if (jaos_set_col_bounds(hv, pick, v, v) != JAOS_OK)
            break;
    }
    jaos_model_free(hv);
    return rc;
}

static bool pump_flip(const jaos_model *m, const double *x, double *rnd)
{
    const int64_t nc = m->num_col;
    int64_t chosen[MIP_PUMP_FLIPS];
    int64_t n_chosen = 0;
    bool moved = false;
    for (int64_t k = 0; k < MIP_PUMP_FLIPS; k++) {
        int64_t pick = -1;
        double far = -1.0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j] || !isfinite(x[j]))
                continue;
            if (m->col_upper[j] - m->col_lower[j] < 1.0 - MIP_INT_TOL)
                continue;
            bool taken = false;
            for (int64_t q = 0; q < n_chosen; q++)
                if (chosen[q] == j) {
                    taken = true;
                    break;
                }
            if (taken)
                continue;
            const double d = fabs(x[j] - rnd[j]);
            if (d > far) {
                far = d;
                pick = j;
            }
        }
        if (pick < 0)
            break;
        chosen[n_chosen++] = pick;
        const double lo = m->col_lower[pick], hi = m->col_upper[pick];
        double v = rnd[pick];
        if (isfinite(hi) && v >= hi - MIP_INT_TOL)
            v -= 1.0;
        else if (isfinite(lo) && v <= lo + MIP_INT_TOL)
            v += 1.0;
        else
            v = x[pick] >= v ? v + 1.0 : v - 1.0;
        if (v < lo || v > hi || v == rnd[pick])
            continue;
        rnd[pick] = v;
        moved = true;
    }
    return moved;
}

static inline bool pump_general_col(const jaos_model *m, int64_t j)
{
    return m->col_integer[j] &&
           m->col_upper[j] - m->col_lower[j] > 1.0 + MIP_INT_TOL;
}

static int pump_for_point(const jaos_model *m, const jaos_model *lp,
                          const double *x, int64_t rounds, bool general,
                          double obj_decay, double *out, double *rnd,
                          double *prev, double *prev2, int64_t *work,
                          int64_t *solves_done)
{
    const int64_t nc = m->num_col;
    const int64_t nr0 = lp->num_row;
    jaos_model *pv = nullptr;
    double *sol = nullptr;
    int64_t *gcol = nullptr;
    int64_t g = 0;
    if (jaos_model_copy(lp, &pv) != JAOS_OK)
        return -1;
    int rc = 0;
    if (jaos_set_objective_sense(pv, JAOS_MINIMIZE) != JAOS_OK ||
        jaos_set_objective_offset(pv, 0.0) != JAOS_OK)
        goto out_free;
    if (general) {
        *work += nc;
        for (int64_t j = 0; j < nc; j++)
            if (pump_general_col(m, j))
                g++;
    }
    if (g > 0) {

        *work += nc;
        gcol = malloc((size_t)g * sizeof *gcol);
        double *ac = malloc((size_t)g * sizeof *ac);
        double *alo = malloc((size_t)g * sizeof *alo);
        double *ahi = malloc((size_t)g * sizeof *ahi);
        double *rlo = malloc((size_t)(2 * g) * sizeof *rlo);
        double *rhi = malloc((size_t)(2 * g) * sizeof *rhi);
        int64_t *rs = malloc((size_t)(2 * g + 1) * sizeof *rs);
        int64_t *ri = malloc((size_t)(4 * g) * sizeof *ri);
        double *rv = malloc((size_t)(4 * g) * sizeof *rv);
        bool ok = gcol != nullptr && ac != nullptr && alo != nullptr &&
                  ahi != nullptr && rlo != nullptr && rhi != nullptr &&
                  rs != nullptr && ri != nullptr && rv != nullptr;
        if (ok) {
            int64_t t = 0;
            for (int64_t j = 0; j < nc; j++)
                if (pump_general_col(m, j))
                    gcol[t++] = j;
            for (int64_t q = 0; q < g; q++) {
                ac[q] = 1.0;
                alo[q] = 0.0;
                ahi[q] = INFINITY;
                rlo[2 * q] = -INFINITY;
                rhi[2 * q] = INFINITY;
                rlo[2 * q + 1] = -INFINITY;
                rhi[2 * q + 1] = INFINITY;
                rs[2 * q] = 4 * q;
                rs[2 * q + 1] = 4 * q + 2;
                ri[4 * q] = gcol[q];
                rv[4 * q] = 1.0;
                ri[4 * q + 1] = nc + q;
                rv[4 * q + 1] = -1.0;
                ri[4 * q + 2] = gcol[q];
                rv[4 * q + 2] = 1.0;
                ri[4 * q + 3] = nc + q;
                rv[4 * q + 3] = 1.0;
            }
            rs[2 * g] = 4 * g;
            ok = jaos_add_cols(pv, g, ac, alo, ahi, 0, nullptr, nullptr,
                               nullptr) == JAOS_OK &&
                 jaos_add_rows(pv, 2 * g, rlo, rhi, 4 * g, rs, ri, rv)
                     == JAOS_OK;
            if (ok)
                *work += 2 * (pv->num_nz + pv->num_col);
        }
        free(ac);
        free(alo);
        free(ahi);
        free(rlo);
        free(rhi);
        free(rs);
        free(ri);
        free(rv);
        if (!ok) {
            rc = -1;
            goto out_free;
        }
    }

    sol = out;
    if (g > 0) {
        sol = malloc((size_t)(nc + g) * sizeof *sol);
        if (sol == nullptr) {
            rc = -1;
            goto out_free;
        }
    }
    double cnorm = 0.0;
    if (obj_decay > 0.0) {
        *work += nc;
        for (int64_t j = 0; j < nc; j++)
            cnorm += m->col_cost[j] * m->col_cost[j];
        cnorm = sqrt(cnorm);
    }
    const bool blend = obj_decay > 0.0 && cnorm > 0.0;
    const double sgn = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    double alpha = 1.0;
    memcpy(out, x, (size_t)(nc > 0 ? nc : 1) * sizeof *out);
    for (int64_t r = 0; r < rounds; r++) {

        *work += 2 * nc;
        bool integral = true, same1 = r > 0, same2 = r > 1;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j] || !isfinite(out[j])) {
                rnd[j] = out[j];
                if (m->col_integer[j])
                    integral = false;
                continue;
            }
            double v = round(out[j]);
            if (v < m->col_lower[j])
                v = ceil(m->col_lower[j]);
            if (v > m->col_upper[j])
                v = floor(m->col_upper[j]);
            if (fabs(out[j] - v) > MIP_INT_TOL)
                integral = false;
            if (r > 0 && v != prev[j])
                same1 = false;
            if (r > 1 && v != prev2[j])
                same2 = false;
            rnd[j] = v;
        }
        if (integral) {
            rc = 1;
            break;
        }

        memcpy(prev2, prev, (size_t)(nc > 0 ? nc : 1) * sizeof *prev2);
        memcpy(prev, rnd, (size_t)(nc > 0 ? nc : 1) * sizeof *prev);
        if (same1 || same2) {

            *work += MIP_PUMP_FLIPS * nc;
            if (!pump_flip(m, out, rnd))
                break;
        }
        if (g > 0) {

            *work += 2 * g;
            for (int64_t q = 0; q < g; q++) {
                const double r = rnd[gcol[q]];
                const double lo = isfinite(r) ? r : -INFINITY;
                const double hi = isfinite(r) ? r : INFINITY;
                if (jaos_set_row_bounds(pv, nr0 + 2 * q, -INFINITY, hi)
                        != JAOS_OK ||
                    jaos_set_row_bounds(pv, nr0 + 2 * q + 1, lo, INFINITY)
                        != JAOS_OK)
                    goto out_free;
            }
        }
        double factor = 0.0, a = 0.0;
        if (blend) {

            *work += nc;
            alpha *= obj_decay;
            a = alpha;
            int64_t terms = g;
            for (int64_t j = 0; j < nc; j++) {
                if (!m->col_integer[j])
                    continue;
                if (general && pump_general_col(m, j))
                    continue;
                if ((isfinite(m->col_lower[j]) &&
                     rnd[j] <= m->col_lower[j] + MIP_INT_TOL) ||
                    (isfinite(m->col_upper[j]) &&
                     rnd[j] >= m->col_upper[j] - MIP_INT_TOL))
                    terms++;
            }
            factor = a * sqrt((double)terms) / cnorm;
        }
        for (int64_t j = 0; j < nc; j++) {
            double d = 0.0;
            if (m->col_integer[j] && !(general && pump_general_col(m, j))) {
                if (isfinite(m->col_lower[j]) &&
                    rnd[j] <= m->col_lower[j] + MIP_INT_TOL)
                    d = 1.0;
                else if (isfinite(m->col_upper[j]) &&
                         rnd[j] >= m->col_upper[j] - MIP_INT_TOL)
                    d = -1.0;
            }
            const double c = (1.0 - a) * d + factor * sgn * m->col_cost[j];
            if (jaos_set_col_cost(pv, j, c) != JAOS_OK)
                goto out_free;
        }
        if (blend && g > 0) {
            *work += g;
            for (int64_t q = 0; q < g; q++)
                if (jaos_set_col_cost(pv, nc + q, 1.0 - a) != JAOS_OK)
                    goto out_free;
        }
        if (jaos_solve(pv) != JAOS_OK)
            break;
        *work += jaos_work_units(pv);
        (*solves_done)++;
        if (jaos_status_of(pv) != JAOS_SOLVE_OPTIMAL)
            break;
        if (jaos_solution(pv, sol, nullptr, nullptr, nullptr) != JAOS_OK)
            break;
        if (g > 0)
            memcpy(out, sol, (size_t)nc * sizeof *out);
    }
out_free:
    if (g > 0)
        free(sol);
    free(gcol);
    jaos_model_free(pv);
    return rc;
}

static bool rounded_point(const jaos_model *m, const double *x, double *xr,
                          double *ra, double *obj)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    const double tol = jm_primal_tolerance(m);
    for (int64_t j = 0; j < nc; j++) {
        double v = m->col_integer[j] ? round(x[j]) : x[j];
        const bool semi = semi_live(m, j);
        if (semi && v < m->col_lower[j] - tol) {
            const double l = m->col_lower[j];
            if (v <= 0.5 * l)
                v = 0.0;
            else
                v = m->col_integer[j] ? ceil(l) : l;
        }
        if (!(semi && v == 0.0) &&
            (v < m->col_lower[j] - tol || v > m->col_upper[j] + tol))
            return false;
        xr[j] = v;
    }
    for (int64_t i = 0; i < nr; i++)
        ra[i] = 0.0;
    double z = m->obj_offset;
    for (int64_t j = 0; j < nc; j++) {
        const double v = xr[j];
        if (v == 0.0)
            continue;
        z += m->col_cost[j] * v;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            ra[m->a_index[k]] += m->a_value[k] * v;
    }
    for (int64_t i = 0; i < nr; i++)
        if (!ind_inactive(m, xr, i) &&
            (ra[i] < m->row_lower[i] - tol || ra[i] > m->row_upper[i] + tol))
            return false;
    if (sos_violated(m, xr, 0, nullptr, nullptr) >= 0)
        return false;
    *obj = z;
    return true;
}

static bool incumbent_take_point(incumbent *inc, const jaos_model *lp,
                                 const jaos_model *m, const double *xr,
                                 const double *ra, double obj, double key)
{
    if (!incumbent_take(inc, lp, m->num_row, key))
        return false;
    const int64_t nc = m->num_col, nr = m->num_row;
    if (nc > 0)
        memcpy(inc->x, xr, (size_t)nc * sizeof *inc->x);
    if (nr > 0)
        memcpy(inc->ra, ra, (size_t)nr * sizeof *inc->ra);
    inc->obj = obj;
    return true;
}

constexpr int64_t MIP_LOG_EVERY = 100;

static bool incumbent_announce(const jaos_model *m, const incumbent *inc,
                               int64_t node, double bound, bool by_rounding)
{
    if (m->cfg.incumbent_cb == nullptr)
        return true;
    const jaos_incumbent ev = {
        .node = node,
        .objective = inc->obj,
        .bound = bound,
        .col_value = inc->x,
        .num_col = m->num_col,
        .by_rounding = by_rounding,
    };
    return m->cfg.incumbent_cb(&ev, m->cfg.incumbent_user) !=
           JAOS_CALLBACK_STOP;
}

typedef struct {
    double *x, *key, *obj;
    int64_t n, cap, nc;
} spool;

static bool spool_init(spool *sp, int64_t cap, int64_t nc)
{
    sp->n = 0;
    sp->cap = cap;
    sp->nc = nc;
    sp->x = malloc((size_t)(cap * (nc > 0 ? nc : 1)) * sizeof *sp->x);
    sp->key = malloc((size_t)cap * sizeof *sp->key);
    sp->obj = malloc((size_t)cap * sizeof *sp->obj);
    return sp->x != nullptr && sp->key != nullptr && sp->obj != nullptr;
}

static void spool_free(spool *sp)
{
    free(sp->x); free(sp->key); free(sp->obj);
}

static void spool_offer(spool *sp, const double *x, double key, double obj)
{
    const int64_t nc = sp->nc;
    if (sp->n == sp->cap && !(key < sp->key[sp->n - 1]))
        return;
    for (int64_t i = 0; i < sp->n; i++)
        if (sp->key[i] == key &&
            (nc == 0 || memcmp(sp->x + i * nc, x, (size_t)nc * sizeof *x) == 0))
            return;
    int64_t pos = 0;
    while (pos < sp->n && !(key < sp->key[pos]))
        pos++;
    const int64_t last = sp->n < sp->cap ? sp->n : sp->cap - 1;
    if (last > pos) {
        memmove(sp->key + pos + 1, sp->key + pos, (size_t)(last - pos) * sizeof *sp->key);
        memmove(sp->obj + pos + 1, sp->obj + pos, (size_t)(last - pos) * sizeof *sp->obj);
        if (nc > 0)
            memmove(sp->x + (pos + 1) * nc, sp->x + pos * nc,
                    (size_t)((last - pos) * nc) * sizeof *sp->x);
    }
    sp->key[pos] = key;
    sp->obj[pos] = obj;
    if (nc > 0)
        memcpy(sp->x + pos * nc, x, (size_t)nc * sizeof *x);
    if (sp->n < sp->cap)
        sp->n++;
}

static const char *dive_child_str(int rule)
{
    switch (rule) {
    case JAOS_DIVE_UP:         return "up first";
    case JAOS_DIVE_DOWN:       return "down first";
    case JAOS_DIVE_PSEUDOCOST: return "pseudocost side first";
    default:                   return "nearer side first";
    }
}

static int64_t propagate_node(jaos_model *m, jaos_model *lp, double *plo,
                              double *phi, int64_t rounds, int64_t *work)
{
    const int64_t nc = m->num_col;
    for (int64_t j = 0; j < nc; j++) {
        plo[j] = lp->col_lower[j];
        phi[j] = lp->col_upper[j];
    }
    const int64_t moved = propagate_bounds(m, plo, phi, rounds, work);
    for (int64_t j = 0; moved > 0 && j < nc; j++) {
        if (!m->col_integer[j])
            continue;
        if (plo[j] == lp->col_lower[j] && phi[j] == lp->col_upper[j])
            continue;
        if (jaos_set_col_bounds(lp, j, plo[j], phi[j]) != JAOS_OK)
            return -2;
    }
    return moved;
}

static int64_t propagate_bounds(jaos_model *m, double *plo, double *phi,
                                int64_t rounds, int64_t *work)
{
    const int64_t nc = m->num_col, nr = m->num_row;
    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        return -2;
    int64_t moved = 0;
    for (int64_t r = 0; r < rounds; r++) {
        int64_t moved_here = 0;

        *work += m->num_nz + nc + nr;
        for (int64_t i = 0; i < nr; i++) {
            const double rlo = m->row_lower[i], rhi = m->row_upper[i];
            if (rlo == -INFINITY && rhi == INFINITY)
                continue;
            if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0)
                continue;

            double smin = 0.0, smax = 0.0;
            int64_t nmin = 0, nmax = 0;
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                const int64_t j = m->ar_index[k];
                const double a = m->ar_value[k];
                const double e = a > 0.0 ? plo[j] : phi[j];
                const double f = a > 0.0 ? phi[j] : plo[j];
                if (isinf(e))
                    nmin++;
                else
                    smin += a * e;
                if (isinf(f))
                    nmax++;
                else
                    smax += a * f;
            }

            if (nmin == 0 && rhi < INFINITY &&
                smin - rhi > MIP_PROP_INFEAS * (1.0 + fabs(rhi) + fabs(smin)))
                return -1;
            if (nmax == 0 && rlo > -INFINITY &&
                rlo - smax > MIP_PROP_INFEAS * (1.0 + fabs(rlo) + fabs(smax)))
                return -1;
            for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                const int64_t j = m->ar_index[k];
                if (!m->col_integer[j])
                    continue;
                const double a = m->ar_value[k];
                const double e = a > 0.0 ? plo[j] : phi[j];
                const double f = a > 0.0 ? phi[j] : plo[j];

                const bool rmin_ok = isinf(e) ? nmin == 1 : nmin == 0;
                const bool rmax_ok = isinf(f) ? nmax == 1 : nmax == 0;
                const double rmin = isinf(e) ? smin : smin - a * e;
                const double rmax = isinf(f) ? smax : smax - a * f;
                double ub = INFINITY, lb = -INFINITY;
                if (rmin_ok && rhi < INFINITY) {
                    const double t = (rhi - rmin) / a;
                    if (a > 0.0)
                        ub = t;
                    else
                        lb = t;
                }
                if (rmax_ok && rlo > -INFINITY) {
                    const double t = (rlo - rmax) / a;
                    if (a > 0.0) {
                        if (t > lb)
                            lb = t;
                    } else if (t < ub) {
                        ub = t;
                    }
                }

                if (ub < INFINITY) {
                    const double nh =
                        floor(ub + MIP_PROP_SLACK * (1.0 + fabs(ub)));
                    if (nh < phi[j] - MIP_PROP_MOVE) {
                        phi[j] = nh;
                        moved_here++;
                    }
                }
                if (lb > -INFINITY) {
                    const double nl =
                        ceil(lb - MIP_PROP_SLACK * (1.0 + fabs(lb)));
                    if (nl > plo[j] + MIP_PROP_MOVE) {
                        plo[j] = nl;
                        moved_here++;
                    }
                }
                if (plo[j] > phi[j] + MIP_PROP_MOVE)
                    return -1;
            }
        }
        moved += moved_here;
        if (moved_here == 0)
            break;
    }
    return moved;
}

jaos_status jm_branch_and_bound(jaos_model *m)
{
    const double t0 = now_seconds();
    const int64_t nc = m->num_col, nr = m->num_row;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double gap = m->cfg.mip_gap > 0.0 ? m->cfg.mip_gap : MIP_GAP;
    const int64_t rounds = m->cfg.mip_cut_rounds_set ? m->cfg.mip_cut_rounds
                                                     : MIP_CUT_ROUNDS;
    const int64_t cover_rounds = m->cfg.mip_cover_rounds_set
        ? m->cfg.mip_cover_rounds : MIP_COVER_ROUNDS;
    const int64_t clique_rounds = m->cfg.mip_clique_rounds_set
        ? m->cfg.mip_clique_rounds : MIP_CLIQUE_ROUNDS;
    const int64_t mir_rounds = m->cfg.mip_mir_rounds_set ? m->cfg.mip_mir_rounds
                                                         : MIP_MIR_ROUNDS;
    int64_t root_rounds = rounds > cover_rounds ? rounds : cover_rounds;
    if (mir_rounds > root_rounds)
        root_rounds = mir_rounds;
    if (clique_rounds > root_rounds)
        root_rounds = clique_rounds;
    const int64_t backtrack = m->cfg.mip_dive_backtrack_set
        ? m->cfg.mip_dive_backtrack : MIP_DIVE_BACKTRACK;
    const double dive_gap = m->cfg.mip_dive_gap_set ? m->cfg.mip_dive_gap
                                                    : MIP_DIVE_GAP;
    const bool node_mir = m->cfg.mip_node_mir_set ? m->cfg.mip_node_mir
                                                  : MIP_NODE_MIR;
    const int64_t mir_aggregate = m->cfg.mip_mir_aggregate_set
        ? m->cfg.mip_mir_aggregate : MIP_MIR_AGGREGATE;
    const int64_t dive_heur = m->cfg.mip_dive_heuristic_set
        ? m->cfg.mip_dive_heuristic : MIP_DIVE_HEURISTIC;
    const int64_t dive_heur_depth = m->cfg.mip_dive_heuristic_depth_set
        ? m->cfg.mip_dive_heuristic_depth : MIP_DIVE_HEURISTIC_DEPTH;
    const int64_t rins = m->cfg.mip_rins_set ? m->cfg.mip_rins : MIP_RINS;
    const int64_t feaspump = m->cfg.mip_feaspump_set ? m->cfg.mip_feaspump
                                                     : MIP_FEASPUMP;
    const bool pump_general = m->cfg.mip_pump_general_set
        ? m->cfg.mip_pump_general : MIP_PUMP_GENERAL;
    const double pump_obj = m->cfg.mip_pump_obj_set ? m->cfg.mip_pump_obj
                                                    : MIP_PUMP_OBJ;
    const bool pump_always = m->cfg.mip_pump_always_set
        ? m->cfg.mip_pump_always : MIP_PUMP_ALWAYS;
    const bool rcfix = m->cfg.mip_rcfix_set ? m->cfg.mip_rcfix : MIP_RCFIX;
    const bool tighten = m->cfg.mip_tighten_set ? m->cfg.mip_tighten
                                                : MIP_TIGHTEN;
    const bool probing = m->cfg.mip_probing_set ? m->cfg.mip_probing
                                                : MIP_PROBING;
    const double probing_cap = m->cfg.mip_probing_cap_set
        ? m->cfg.mip_probing_cap : MIP_PROBING_CAP;
    const bool clique_fix_on = m->cfg.mip_clique_fix_set
        ? m->cfg.mip_clique_fix : MIP_CLIQUE_FIX;
    const int64_t propagate = m->cfg.mip_propagate_set ? m->cfg.mip_propagate
                                                       : MIP_PROPAGATE;
    const int64_t propagate_depth = m->cfg.mip_propagate_depth_set
        ? m->cfg.mip_propagate_depth : MIP_PROPAGATE_DEPTH;

    const double cut_key = m->cfg.mip_cutoff_set
        ? sigma * m->cfg.mip_cutoff : INFINITY;
    const double degrade = m->cfg.mip_dive_degrade_set
        ? m->cfg.mip_dive_degrade : MIP_DIVE_DEGRADE;
    const bool dive = m->cfg.mip_dive;

    const bool stack_dive = dive && (backtrack > 0 || dive_gap > 0.0);
    const bool heur = !m->cfg.mip_no_heuristics;
    const jaos_branching rule = (jaos_branching)m->cfg.mip_branching;
    const int64_t reliability = rule == JAOS_BRANCH_PSEUDOCOST
        ? (m->cfg.mip_reliability_set ? m->cfg.mip_reliability : MIP_RELIABILITY)
        : 0;
    const double probe_cap = m->cfg.mip_probe_cap_set ? m->cfg.mip_probe_cap
                                                      : MIP_PROBE_CAP;
    const int dive_child = m->cfg.mip_dive_child;
    const int64_t cut_depth = m->cfg.mip_cut_depth_set ? m->cfg.mip_cut_depth
                                                       : MIP_CUT_DEPTH;
    const bool cut_drop = !m->cfg.mip_no_cut_drop;
    const int64_t node_cut_cap = m->cfg.mip_node_cut_cap_set
        ? m->cfg.mip_node_cut_cap : MIP_NODE_CUT_CAP;
    const int64_t probe_depth = m->cfg.mip_probe_depth_set
        ? m->cfg.mip_probe_depth : MIP_PROBE_DEPTH;
    const int64_t pool_size = m->cfg.mip_pool_size > 0 ? m->cfg.mip_pool_size : 1;
    const double cut_stall = m->cfg.mip_cut_stall_set ? m->cfg.mip_cut_stall
                                                      : MIP_CUT_STALL;
    const double node_cut_stall = m->cfg.mip_node_cut_stall_set
        ? m->cfg.mip_node_cut_stall : MIP_NODE_CUT_STALL;
    const bool root_cut_drop = m->cfg.mip_root_cut_drop_set
        ? m->cfg.mip_root_cut_drop : MIP_ROOT_CUT_DROP;
    const bool cover_lift = m->cfg.mip_cover_lift_set ? m->cfg.mip_cover_lift
                                                      : MIP_COVER_LIFT;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    jaos_model *lp = nullptr;
    bheap heap = {0};
    incumbent inc = {0};
    cutbuf cb = {0}, pool = {0};
    int64_t *act = nullptr;
    int64_t act_n = 0, act_cap = 0;
    int64_t nfixed = nr;
    cutlist in_copy = {0};
    jaos_basis_status *crs = nullptr;
    int64_t crs_cap = 0;
    spool sp = {0};
    int64_t local_cuts = 0;
    bnode *cur = nullptr, *next = nullptr;
    double *x = nullptr, *row = nullptr, *cut = nullptr;
    int64_t row_cap = 0;
    double *xr = nullptr, *ra = nullptr, *x2 = nullptr;
    double *ilo = nullptr, *ihi = nullptr, *pc_sum = nullptr;
    double *plo = nullptr, *phi = nullptr;
    double *rcd = nullptr;
    int64_t *pc_n = nullptr, *cand = nullptr;
    jaos_basis_status *pcs = nullptr, *prs = nullptr;
    int64_t next_id = 0, nodes = 0, solves = 0, cuts = 0, heur_points = 0;
    int64_t probes = 0, capped = 0;
    int64_t dive_points = 0;
    int64_t rins_points = 0;
    int64_t pump_points = 0;
    double rins_key = 0.0;
    bool rins_seen = false;
    int64_t covers = 0;
    int64_t cliques = 0;
    kitem *items = nullptr;
    double *pbuf = nullptr;
    litval *porder = nullptr;
    clique_table ctab = {0};
    int64_t *cstack = nullptr;
    lpair *pextra = nullptr;
    int64_t npextra = 0, pecap = 0;
    int64_t clique_fixed = 0, clique_cut_nodes = 0;
    double *mu = nullptr;
    double *mbest = nullptr, *mdelta = nullptr;
    double *prnd = nullptr;
    double *pprev = nullptr;
    double *pprev2 = nullptr;
    double *magg = nullptr;
    double *mcmag = nullptr;
    bool *mused = nullptr;
    bool *mpicked = nullptr;
    int64_t mirs = 0;
    bnode **dstack = nullptr;
    int64_t dstack_n = 0, dstack_cap = 0, backtracks = 0;
    int64_t first_inc = 0;
    int64_t rcfixed = 0;
    int64_t tightened = 0;
    int64_t work = 0, iters = 0;
    double best_bound = -INFINITY;
    jaos_solve_status outcome = JAOS_SOLVE_NOT_RUN;
    int64_t *fcol = nullptr;
    double *flo = nullptr, *fhi = nullptr;

    free(m->mip_inc_x);
    m->mip_inc_x = nullptr;
    free(m->mip_pool_x);
    m->mip_pool_x = nullptr;
    free(m->mip_pool_obj);
    m->mip_pool_obj = nullptr;
    m->mip_pool_n = 0;
    m->mip_nodes = m->mip_solves = m->mip_cuts = m->mip_heur = 0;
    m->mip_first_inc = 0;
    m->mip_bound = 0.0;
    m->mip_has_incumbent = false;

    m->sol_basis_ok = false;

    if (m->row_ind_col != nullptr && jm_model_ensure_rowwise(m) != JAOS_OK)
        goto done;
    if (jaos_model_copy(m, &lp) != JAOS_OK)
        goto done;
    lp->cfg.node_solve = true;
    free(lp->col_integer);
    lp->col_integer = nullptr;
    free(lp->col_semi);
    lp->col_semi = nullptr;
    lp->num_sos = 0;
    free(lp->row_ind_col);
    lp->row_ind_col = nullptr;
    free(lp->row_ind_val);
    lp->row_ind_val = nullptr;
    lp->cfg.log_cb = nullptr;
    jaos_clear_basis(lp);

    x = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x);
    xr = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *xr);
    x2 = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *x2);
    ra = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *ra);
    ilo = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *ilo);
    ihi = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *ihi);
    pc_sum = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_sum);
    pc_n = calloc((size_t)(nc > 0 ? 2 * nc : 1), sizeof *pc_n);
    cand = malloc((size_t)MIP_STRONG_CANDIDATES * sizeof *cand);
    pcs = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pcs);
    if (propagate > 0) {
        plo = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *plo);
        phi = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *phi);
        if (plo == nullptr || phi == nullptr)
            goto done;
    }
    fcol = malloc((size_t)(2 * nc + 2) * sizeof *fcol);
    flo = malloc((size_t)(2 * nc + 2) * sizeof *flo);
    fhi = malloc((size_t)(2 * nc + 2) * sizeof *fhi);
    if (x == nullptr || xr == nullptr || x2 == nullptr || ra == nullptr ||
        ilo == nullptr ||
        ihi == nullptr || pc_sum == nullptr || pc_n == nullptr ||
        cand == nullptr || pcs == nullptr || fcol == nullptr ||
        flo == nullptr || fhi == nullptr || !spool_init(&sp, pool_size, nc))
        goto done;

    for (int64_t j = 0; j < nc; j++) {
        ilo[j] = m->col_integer[j] ? ceil(m->col_lower[j]) : m->col_lower[j];
        ihi[j] = m->col_integer[j] ? floor(m->col_upper[j]) : m->col_upper[j];
        if (semi_live(m, j))
            ilo[j] = 0.0;
        else if (m->col_integer[j] && ilo[j] > ihi[j])
            outcome = JAOS_SOLVE_INFEASIBLE;
    }
    if (tighten && outcome == JAOS_SOLVE_NOT_RUN) {
        int64_t rows_tightened = 0;
        const int64_t tightened =
            tighten_coefficients(m, lp, ilo, ihi, &rows_tightened, &work);
        if (tightened > 0)
            jm_log(m, JAOS_LOG_SUMMARY,
                   "coefficient tightening: %lld coefficients on %lld rows",
                   (long long)tightened, (long long)rows_tightened);
    }
    if (jm_logging_at(m, JAOS_LOG_SUMMARY)) {
        int64_t nint = 0;
        for (int64_t j = 0; j < nc; j++)
            nint += m->col_integer[j];
        jm_log(m, JAOS_LOG_SUMMARY,
               "branch and bound: %lld integer columns of %lld, %lld rounds "
               "of cuts, %lld of MIR over %lld aggregated rows, cuts to "
               "depth %lld%s, dive %s with %lld "
               "backtracks and a resume gap of %g, rounding %s, %s "
               "branching, reliability %lld, probe cap %gx, cut stall %g "
               "at the root and %g below it, root cuts %s, covers %s",
               (long long)nint, (long long)nc, (long long)rounds,
               (long long)mir_rounds, (long long)mir_aggregate,
               (long long)cut_depth,
               node_mir ? " with MIR" : "",
               dive ? dive_child_str(dive_child) : "off",
               (long long)backtrack, dive_gap, heur ? "on" : "off",
               rule == JAOS_BRANCH_MOST_FRACTIONAL ? "most-fractional"
                                                   : "pseudocost",
               (long long)reliability, probe_cap, cut_stall, node_cut_stall,
               root_cut_drop ? "dropped when slack" : "kept",
               cover_lift ? "lifted" : "extended");
    }

    for (; outcome == JAOS_SOLVE_NOT_RUN;) {

        if (nodes > 0) {
            node_free(cur);
            for (;;) {
                bool resumed = false;
                if (next != nullptr) {
                    cur = next;
                    next = nullptr;
                } else if (dstack_n > 0 &&
                           (backtrack == 0 || backtracks < backtrack) &&
                           resume_within(dstack[dstack_n - 1], &heap, dstack,
                                         dstack_n, dive_gap)) {
                    cur = dstack[--dstack_n];
                    resumed = true;
                } else {
                    while (dstack_n > 0) {
                        if (!heap_push(&heap, dstack[dstack_n - 1]))
                            goto done;
                        dstack_n--;
                    }
                    backtracks = 0;
                    cur = heap_pop(&heap);
                    if (cur == nullptr)
                        break;
                    best_bound = cur->key;
                }

                const double bk = inc.have && inc.key < cut_key ? inc.key
                                                                : cut_key;
                if (bk < INFINITY &&
                    bk - cur->key <= gap * (1.0 + fabs(bk))) {
                    node_free(cur);
                    cur = nullptr;
                    continue;
                }

                if (resumed)
                    backtracks++;
                break;
            }
            if (cur == nullptr) {
                outcome = inc.have ? JAOS_SOLVE_OPTIMAL : JAOS_SOLVE_INFEASIBLE;
                break;
            }
        }

        if (m->cfg.work_limit > 0 && work >= m->cfg.work_limit) {
            outcome = JAOS_SOLVE_WORK_LIMIT;
            break;
        }
        if (m->cfg.time_limit > 0.0 && now_seconds() - t0 >= m->cfg.time_limit) {
            outcome = JAOS_SOLVE_TIME_LIMIT;
            break;
        }
        if (m->cfg.mip_node_limit > 0 && nodes >= m->cfg.mip_node_limit) {
            outcome = JAOS_SOLVE_NODE_LIMIT;
            break;
        }

        if (node_apply(lp, m, ilo, ihi, nodes > 0 ? cur : nullptr, &pool,
                       nfixed, &in_copy) != JAOS_OK)
            goto done;
        const int64_t depth_here = nodes > 0 ? cur->depth : 0;
        bool stalled = false;
        act_n = 0;
        if (nodes > 0 && cur->ncuts > 0) {
            if (!JM_GROW(act, act_cap, cur->ncuts))
                goto done;
            memcpy(act, cur->cuts, (size_t)cur->ncuts * sizeof *act);
            act_n = cur->ncuts;
        }
        nodes++;

        if (nodes > 1 && clique_fix_on && ctab.ne > 0) {
            const int64_t got = clique_fix(m, lp, &ctab, cstack, &work);
            if (got == -2)
                goto done;
            if (got == -1) {
                clique_cut_nodes++;
                jm_log(m, JAOS_LOG_PROGRESS,
                       "node %lld: infeasible by a clique",
                       (long long)nodes);
                continue;
            }
            clique_fixed += got;
        }

        if (propagate > 0 &&
            (propagate_depth < 0 || depth_here <= propagate_depth)) {
            const int64_t got = propagate_node(m, lp, plo, phi, propagate,
                                               &work);
            if (got == -2)
                goto done;
            if (got == -1) {
                jm_log(m, JAOS_LOG_PROGRESS,
                       "node %lld: infeasible by propagation",
                       (long long)nodes);
                continue;
            }
            tightened += got;

            if (nodes == 1 && got > 0) {
                for (int64_t j = 0; j < nc; j++) {
                    if (!m->col_integer[j])
                        continue;
                    ilo[j] = lp->col_lower[j];
                    ihi[j] = lp->col_upper[j];
                }
            }
        }
        jaos_status st = jaos_solve(lp);
        solves++;
        const int64_t node_work = jaos_work_units(lp);
        work += node_work;
        iters += jaos_iterations(lp);
        if (st != JAOS_OK) {
            if (st == JAOS_ERR_NUMERICAL) {
                outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                jm_set_err(m, "node %lld: %s", (long long)nodes,
                           jaos_model_error(lp));
                break;
            }
            goto done;
        }
        jaos_solve_status ns = jaos_status_of(lp);
        if (ns == JAOS_SOLVE_INFEASIBLE)
            continue;
        if (ns == JAOS_SOLVE_UNBOUNDED) {

            outcome = JAOS_SOLVE_UNBOUNDED;
            break;
        }
        if (ns != JAOS_SOLVE_OPTIMAL) {
            outcome = ns;
            break;
        }

        double obj = 0.0;
        if (jaos_objective(lp, &obj) != JAOS_OK ||
            jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
            goto done;
        double key = sigma * obj;

        if (nodes == 1 && probing) {
            if (pbuf == nullptr) {
                pbuf = malloc((size_t)(nc > 0 ? 4 * nc : 1) * sizeof *pbuf);
                porder = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *porder);
                if (pbuf == nullptr || porder == nullptr)
                    goto done;
            }
            const int64_t cap = probing_cap > 0.0
                ? (int64_t)ceil(probing_cap * (double)node_work) : 0;
            const int64_t before = work;
            probe_report pr;
            if (probe_root(m, lp, x, ilo, ihi, pbuf, porder, cap, &pextra,
                           &npextra, &pecap, &pr, &work) != JAOS_OK)
                goto done;
            if (pr.fractional > 0)
                jm_log(m, JAOS_LOG_SUMMARY,
                       "probing: %lld of %lld fractional binaries probed%s, "
                       "%lld fixed, %lld other bounds implied, %lld "
                       "conflicts found, %lld work against the root's "
                       "%lld%s",
                       (long long)pr.probed, (long long)pr.fractional,
                       pr.capped ? " before the cap" : "",
                       (long long)pr.fixed, (long long)pr.implied,
                       (long long)pr.conflicts,
                       (long long)(work - before), (long long)node_work,
                       pr.infeasible ? ", and one fits neither way" : "");
            if (pr.infeasible) {
                outcome = JAOS_SOLVE_INFEASIBLE;
                break;
            }
            if (pr.fixed + pr.implied > 0) {
                st = jaos_solve(lp);
                solves++;
                work += jaos_work_units(lp);
                iters += jaos_iterations(lp);
                if (st != JAOS_OK) {
                    if (st == JAOS_ERR_NUMERICAL) {
                        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                        jm_set_err(m, "root after probing: %s",
                                   jaos_model_error(lp));
                        break;
                    }
                    goto done;
                }
                ns = jaos_status_of(lp);
                if (ns == JAOS_SOLVE_INFEASIBLE) {
                    outcome = JAOS_SOLVE_INFEASIBLE;
                    break;
                }
                if (ns != JAOS_SOLVE_OPTIMAL) {
                    outcome = ns;
                    break;
                }
                if (jaos_objective(lp, &obj) != JAOS_OK ||
                    jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                    goto done;
                key = sigma * obj;
            }
        }

        if (nodes == 1 && (clique_rounds > 0 || clique_fix_on)) {
            if (items == nullptr)
                items = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *items);
            if (items == nullptr)
                goto done;
            if (clique_table_build(m, lp, ilo, ihi, items, pextra, npextra,
                                   &ctab, &work) < 0)
                goto done;
            if (ctab.ne > 0 && clique_fix_on) {
                cstack = malloc((size_t)(2 * nc) * sizeof *cstack);
                if (cstack == nullptr)
                    goto done;
            }
            if (ctab.ne > 0) {
                int64_t lits = 0;
                for (int64_t u = 0; u < 2 * nc; u++)
                    lits += ctab.adj_start[u + 1] > ctab.adj_start[u];
                jm_log(m, JAOS_LOG_SUMMARY,
                       "clique table: %lld conflicts over %lld literals, "
                       "%lld of the conflicts from probing",
                       (long long)ctab.ne, (long long)lits,
                       (long long)npextra);
            }
        }

        const double branch_key = key;
        if (nodes > 1)
            pseudocost_learn(cur, key, nc, pc_sum, pc_n);
        int64_t branch = select_branch(m, x, rule, pc_sum, pc_n);

        if (nodes == 1 && branch >= 0 && root_rounds > 0) {

            const int64_t need = nc + lp->num_row + 1 +
                                 (nc + 4 * nr + 1) * root_rounds;
            double *grown = realloc(row, (size_t)need * sizeof *row);
            if (grown == nullptr)
                goto done;
            row = grown;
            row_cap = need;
            if (cut == nullptr)
                cut = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cut);
            if (cut == nullptr)
                goto done;
            if ((cover_rounds > 0 || clique_rounds > 0) && items == nullptr)
                items = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *items);
            if (clique_rounds > 0 && items == nullptr)
                goto done;
            if (cover_rounds > 0 && mu == nullptr)
                mu = malloc((size_t)(nc + 1) * sizeof *mu);
            if (cover_rounds > 0 && (items == nullptr || mu == nullptr))
                goto done;
            if (mir_rounds > 0 && mbest == nullptr)
                mbest = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mbest);
            if (mir_rounds > 0 && mdelta == nullptr)
                mdelta = malloc((size_t)(MIP_MIR_DELTAS + 1) * sizeof *mdelta);
            if (mir_rounds > 0 && magg == nullptr)
                magg = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *magg);
            if (mir_rounds > 0 && mir_aggregate > 0 && mused == nullptr) {
                mused = malloc((size_t)(nr > 0 ? nr : 1) * sizeof *mused);
                mpicked = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mpicked);
                mcmag = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mcmag);
            }
            if (mir_rounds > 0 && (mbest == nullptr || mdelta == nullptr ||
                                   magg == nullptr ||
                                   (mir_aggregate > 0 &&
                                    (mused == nullptr || mpicked == nullptr ||
                                     mcmag == nullptr))))
                goto done;
            const double key_first = key;
            bool stop = false;
            for (int64_t r = 0; r < root_rounds && !stop; r++) {
                cb.n = cb.nnz = 0;
                int64_t got = 0;
                if (r < rounds) {
                    got = gomory_round(lp, m, x, &cb, row, cut, &work);
                    if (got < 0)
                        goto done;
                }
                if (r < cover_rounds) {
                    const int64_t cv = cover_round(m, lp, x, ilo, ihi, &cb,
                                                   items, cut, mu, cover_lift,
                                                   &work);
                    if (cv < 0)
                        goto done;
                    covers += cv;
                    got += cv;
                }
                if (r < clique_rounds) {
                    const int64_t qv = clique_round(m, &ctab, x, &cb, cut,
                                                    &work);
                    if (qv < 0)
                        goto done;
                    cliques += qv;
                    got += qv;
                }
                if (r < mir_rounds) {
                    const int64_t mv = mir_round(m, lp, x, ilo, ihi, &cb, cut,
                                                 mbest, mdelta, magg, &work);
                    if (mv < 0)
                        goto done;
                    mirs += mv;
                    got += mv;
                    if (mir_aggregate > 0) {
                        const int64_t av = mir_aggregate_round(
                            m, lp, x, ilo, ihi, &cb, cut, mbest, mdelta, magg,
                            mcmag, mused, mpicked, mir_aggregate, &work);
                        if (av < 0)
                            goto done;
                        mirs += av;
                        got += av;
                    }
                }
                if (got == 0)
                    break;
                if (cuts_add(lp, &cb) != JAOS_OK)
                    goto done;

                if (root_cut_drop) {
                    for (int64_t k = 0; k < cb.n; k++) {
                        if (!cutbuf_append(&pool, &cb, k) ||
                            !JM_GROW(act, act_cap, act_n + 1) ||
                            !JM_GROW(in_copy.v, in_copy.cap, in_copy.n + 1))
                            goto done;
                        act[act_n++] = pool.n - 1;
                        in_copy.v[in_copy.n++] = pool.n - 1;
                    }
                }
                cuts += got;
                const double key_before = key;
                st = jaos_solve(lp);
                solves++;
                work += jaos_work_units(lp);
                iters += jaos_iterations(lp);
                if (st != JAOS_OK) {
                    if (st == JAOS_ERR_NUMERICAL) {
                        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                        jm_set_err(m, "root cuts, round %lld: %s",
                                   (long long)(r + 1), jaos_model_error(lp));
                        stop = true;
                        break;
                    }
                    goto done;
                }
                ns = jaos_status_of(lp);
                if (ns != JAOS_SOLVE_OPTIMAL) {
                    outcome = ns;
                    stop = true;
                    break;
                }
                if (jaos_objective(lp, &obj) != JAOS_OK ||
                    jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                    goto done;
                key = sigma * obj;
                branch = select_branch(m, x, rule, pc_sum, pc_n);
                if (branch < 0)
                    break;

                if (cut_stall > 0.0 &&
                    key - key_before < cut_stall * (1.0 + fabs(key_before)))
                    break;
            }
            if (stop)
                break;

            if (node_cut_stall > 0.0 && cuts > 0 &&
                key - key_first < node_cut_stall * (1.0 + fabs(key_first)))
                stalled = true;
        }
        if (nodes == 1) {
            best_bound = key;

            nfixed = root_cut_drop ? nr : lp->num_row;
            jm_log(m, JAOS_LOG_SUMMARY,
                   "root: relaxation %.17g after %lld cuts, %lld of them "
                   "covers, %lld cliques and %lld MIR",
                   obj, (long long)cuts, (long long)covers,
                   (long long)cliques, (long long)mirs);
        }

        if (nodes == 1 && m->mip_start != nullptr) {
            double hobj = 0.0;
            work += m->num_nz + nc + nr;
            if (rounded_point(m, m->mip_start, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    jm_log(m, JAOS_LOG_SUMMARY,
                           "root: incumbent %.17g from the caller's starting "
                           "point", hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key,
                                            true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            } else {
                jm_log(m, JAOS_LOG_SUMMARY,
                       "root: the caller's starting point is not a feasible "
                       "integer point of this model, and is not taken");
            }
        }

        if (dive_heur > 0 && branch >= 0 &&
            (cur == nullptr || cur->depth <= dive_heur_depth)) {
            const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,
                                           xr, &work, &solves);
            if (got < 0)
                goto done;
            double hobj = 0.0;

            if (got == 1)
                work += m->num_nz + nc + nr;
            if (got == 1 && rounded_point(m, xr, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    dive_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by the dive heuristic",
                           (long long)nodes, hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key, true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }

            if (jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                goto done;
        }

        if (nodes == 1 && feaspump > 0 && branch >= 0 &&
            (!inc.have || pump_always)) {

            if (prnd == nullptr) {
                prnd = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *prnd);
                pprev = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pprev);
                pprev2 = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *pprev2);
                if (prnd == nullptr || pprev == nullptr || pprev2 == nullptr)
                    goto done;
            }
            const int got = pump_for_point(m, lp, x, feaspump, pump_general,
                                           pump_obj, xr, prnd, pprev, pprev2,
                                           &work, &solves);
            if (got < 0)
                goto done;
            double hobj = 0.0;
            if (got == 1)
                work += m->num_nz + nc + nr;
            if (got == 1 && rounded_point(m, xr, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);

                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    pump_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "root: incumbent %.17g by the feasibility pump",
                           hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key, true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }

        if (rins > 0 && inc.have && branch >= 0 &&
            (!rins_seen || inc.key != rins_key)) {
            rins_seen = true;
            rins_key = inc.key;
            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves);
            if (got < 0)
                goto done;
            double hobj = 0.0;
            if (got == 1)
                work += m->num_nz + nc + nr;
            if (got == 1 && rounded_point(m, xr, x2, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, x2, hkey, hobj);
                if (hkey < cut_key && hkey < inc.key) {
                    if (!incumbent_take_point(&inc, lp, m, x2, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    rins_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by RINS",
                           (long long)nodes, hobj);
                    if (!incumbent_announce(m, &inc, nodes, sigma * key, true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }

        if (nodes > 1 && branch >= 0 && cur->depth <= cut_depth &&
            !cur->no_cuts) {
            const int64_t need = nc + lp->num_row + 1 + (nc > 0 ? nc : 1);
            if (need > row_cap) {
                double *grown = realloc(row, (size_t)need * sizeof *row);
                if (grown == nullptr)
                    goto done;
                row = grown;
                row_cap = need;
            }
            if (cut == nullptr)
                cut = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *cut);
            if (cut == nullptr)
                goto done;
            cb.n = cb.nnz = 0;
            int64_t got = gomory_round(lp, m, x, &cb, row, cut, &work);
            if (got < 0)
                goto done;

            if (node_mir) {
                if (mbest == nullptr)
                    mbest = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *mbest);
                if (mdelta == nullptr)
                    mdelta = malloc((size_t)(MIP_MIR_DELTAS + 1) * sizeof *mdelta);
                if (magg == nullptr)
                    magg = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *magg);
                if (mbest == nullptr || mdelta == nullptr || magg == nullptr)
                    goto done;
                const int64_t mv = mir_round(m, lp, x, lp->col_lower,
                                             lp->col_upper, &cb, cut, mbest,
                                             mdelta, magg, &work);
                if (mv < 0)
                    goto done;
                got += mv;
            }

            if (got > 0 && node_cut_cap > 0)
                got = cutbuf_keep_best(&cb, node_cut_cap);
            if (got < 0)
                goto done;
            if (got > 0) {
                if (cuts_add(lp, &cb) != JAOS_OK)
                    goto done;
                for (int64_t r = 0; r < got; r++) {
                    if (!cutbuf_append(&pool, &cb, r) ||
                        !JM_GROW(act, act_cap, act_n + 1) ||
                        !JM_GROW(in_copy.v, in_copy.cap, in_copy.n + 1))
                        goto done;
                    act[act_n++] = pool.n - 1;
                    in_copy.v[in_copy.n++] = pool.n - 1;
                }
                cuts += got;
                local_cuts += got;
                const double key_before = key;
                st = jaos_solve(lp);
                solves++;
                work += jaos_work_units(lp);
                iters += jaos_iterations(lp);
                if (st != JAOS_OK) {
                    if (st == JAOS_ERR_NUMERICAL) {
                        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                        jm_set_err(m, "node %lld, cuts: %s", (long long)nodes,
                                   jaos_model_error(lp));
                        break;
                    }
                    goto done;
                }
                ns = jaos_status_of(lp);
                if (ns == JAOS_SOLVE_INFEASIBLE)
                    continue;
                if (ns != JAOS_SOLVE_OPTIMAL) {
                    outcome = ns;
                    break;
                }
                if (jaos_objective(lp, &obj) != JAOS_OK ||
                    jaos_solution(lp, x, nullptr, nullptr, nullptr) != JAOS_OK)
                    goto done;
                key = sigma * obj;
                branch = select_branch(m, x, rule, pc_sum, pc_n);

                if (node_cut_stall > 0.0 &&
                    key - key_before < node_cut_stall * (1.0 + fabs(key_before)))
                    stalled = true;
            }
        }

        if (heur && branch >= 0) {
            double hobj = 0.0;

            work += m->num_nz + nc + nr;
            if (rounded_point(m, x, xr, ra, &hobj)) {
                const double hkey = sigma * hobj;
                spool_offer(&sp, xr, hkey, hobj);
                if (hkey < cut_key && (!inc.have || hkey < inc.key)) {
                    if (!incumbent_take_point(&inc, lp, m, xr, ra, hobj, hkey))
                        goto done;
                    heur_points++;
                    if (first_inc == 0)
                        first_inc = nodes;
                    jm_log(m, JAOS_LOG_PROGRESS,
                           "node %lld: incumbent %.17g by rounding",
                           (long long)nodes, hobj);
                    const double ok = open_key(&heap, dstack, dstack_n);
                    if (!incumbent_announce(m, &inc, nodes,
                            sigma * (ok < key ? ok : key), true)) {
                        outcome = JAOS_SOLVE_INTERRUPTED;
                        break;
                    }
                }
            }
        }

        if (nodes == 1 && rcfix && inc.have && branch >= 0 &&
            inc.key >= key) {
            if (rcd == nullptr) {
                rcd = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *rcd);
                if (rcd == nullptr)
                    goto done;
            }
            if (jaos_solution(lp, nullptr, nullptr, nullptr, rcd) != JAOS_OK)
                goto done;

            work += nc;
            const double room = inc.key - key;
            for (int64_t j = 0; j < nc; j++) {
                if (!m->col_integer[j])
                    continue;
                const double d = sigma * rcd[j];
                const jaos_basis_status bs = lp->sol_col_status[j];
                if (bs == JAOS_BASIS_AT_LOWER && d > 0.0 &&
                    ilo[j] > -INFINITY) {
                    const double t = floor(room / d + MIP_RCFIX_SLACK);
                    const double nh = ilo[j] + t;
                    if (nh < ihi[j]) {
                        ihi[j] = nh;
                        rcfixed++;
                    }
                } else if (bs == JAOS_BASIS_AT_UPPER && d < 0.0 &&
                           ihi[j] < INFINITY) {
                    const double t = floor(room / -d + MIP_RCFIX_SLACK);
                    const double nl = ihi[j] - t;
                    if (nl > ilo[j]) {
                        ilo[j] = nl;
                        rcfixed++;
                    }
                }
            }
            if (rcfixed > 0)
                jm_log(m, JAOS_LOG_SUMMARY,
                       "root: %lld column bounds fixed by their reduced costs",
                       (long long)rcfixed);
        }
        if (nodes % MIP_LOG_EVERY == 0) {
            const double ok = open_key(&heap, dstack, dstack_n);
            jm_log(m, JAOS_LOG_PROGRESS,
                   "node %lld: %lld open, bound %.17g, incumbent %s",
                   (long long)nodes, (long long)(heap.n + dstack_n),
                   sigma * (ok < best_bound ? ok : best_bound),
                   inc.have ? "yes" : "none");
        }
        {
            const double bk = inc.have && inc.key < cut_key ? inc.key
                                                            : cut_key;
            if (bk < INFINITY && bk - key <= gap * (1.0 + fabs(bk)))
                continue;
        }

        int64_t sos_first = -1, sos_last = -1;
        const int64_t sos = branch < 0
            ? sos_violated(m, x, 0, &sos_first, &sos_last) : -1;
        const int64_t ind = branch < 0 && sos < 0 ? indicator_violated(m, x)
                                                   : -1;
        if (branch < 0 && sos < 0 && ind < 0) {
            if (!incumbent_take(&inc, lp, nr, key))
                goto done;
            if (first_inc == 0)
                first_inc = nodes;
            for (int64_t j = 0; j < nc; j++)
                if (m->col_integer[j])
                    inc.x[j] = round(inc.x[j]);
            for (int64_t k = 0; k < m->num_sos; k++)
                for (int64_t t = m->sos_start[k]; t < m->sos_start[k + 1]; t++)
                    if (fabs(inc.x[m->sos_col[t]]) <= MIP_INT_TOL)
                        inc.x[m->sos_col[t]] = 0.0;
            spool_offer(&sp, inc.x, key, obj);
            jm_log(m, JAOS_LOG_PROGRESS, "node %lld: incumbent %.17g, integral",
                   (long long)nodes, obj);
            const double ok = open_key(&heap, dstack, dstack_n);
            if (!incumbent_announce(m, &inc, nodes,
                    sigma * (ok < key ? ok : key), false)) {
                outcome = JAOS_SOLVE_INTERRUPTED;
                break;
            }
            continue;
        }
        const int64_t nrl = lp->num_row;

        if (branch >= 0 && reliability > 0 &&
            (probe_depth < 0 || depth_here <= probe_depth)) {
            jaos_basis_status *grown = realloc(prs, (size_t)(nrl > 0 ? nrl : 1)
                                                        * sizeof *prs);
            if (grown == nullptr)
                goto done;
            prs = grown;

            int64_t cap = 0;
            if (probe_cap > 0.0) {
                const double c = ceil(probe_cap * (double)node_work);
                cap = c < 1.0 ? 1 : (int64_t)c;
            }
            const jaos_status ps = strong_probe(lp, m, x, key, sigma,
                                                reliability, cap, pc_sum, pc_n,
                                                &work, &solves, &probes,
                                                &capped, pcs, prs, cand);
            if (ps != JAOS_OK) {
                if (ps == JAOS_ERR_NUMERICAL) {
                    outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                    jm_set_err(m, "node %lld, strong branching: %s",
                               (long long)nodes, jaos_model_error(lp));
                    break;
                }
                goto done;
            }
            branch = select_branch(m, x, rule, pc_sum, pc_n);
        }

        assert(lp->num_row == nfixed + act_n && act_n == in_copy.n);
        int64_t nr_child = nrl;
        const jaos_basis_status *child_rs = lp->sol_row_status;
        if (cut_drop && act_n > 0) {
            if (nrl > crs_cap) {
                jaos_basis_status *g = realloc(crs, (size_t)nrl * sizeof *crs);
                if (g == nullptr)
                    goto done;
                crs = g;
                crs_cap = nrl;
            }
            memcpy(crs, lp->sol_row_status, (size_t)nfixed * sizeof *crs);
            int64_t kept = 0;
            for (int64_t k = 0; k < act_n; k++) {
                const jaos_basis_status s = lp->sol_row_status[nfixed + k];
                if (s == JAOS_BASIS_BASIC)
                    continue;
                act[kept] = act[k];
                crs[nfixed + kept] = s;
                kept++;
            }
            act_n = kept;
            nr_child = nfixed + kept;
            child_rs = crs;
        }
        const double v = branch >= 0 ? x[branch] : 0.0;
        int64_t nfd = 1, nfu = 1;
        double f = 0.5, frac_d = 0.0, frac_u = 0.0;
        int64_t *const ucol = fcol + nc + 1;
        double *const ulo = flo + nc + 1, *const uhi = fhi + nc + 1;
        if (branch >= 0) {
            const bool sc = semi_broken(m, branch, v) &&
                            !(m->col_integer[branch] &&
                              v - floor(v) > MIP_INT_TOL &&
                              v - floor(v) < 1.0 - MIP_INT_TOL);
            const double sc_lo = !sc ? 0.0
                               : m->col_integer[branch]
                                     ? ceil(m->col_lower[branch])
                                     : m->col_lower[branch];
            f = sc ? v / m->col_lower[branch] : v - floor(v);
            frac_d = f;
            frac_u = 1.0 - f;
            fcol[0] = branch;
            flo[0] = lp->col_lower[branch];
            fhi[0] = sc ? 0.0 : floor(v);
            ucol[0] = branch;
            ulo[0] = sc ? sc_lo : ceil(v);
            uhi[0] = lp->col_upper[branch];
        } else if (ind >= 0) {
            fcol[0] = ind;
            flo[0] = lp->col_lower[ind];
            fhi[0] = 0.0;
            ucol[0] = ind;
            ulo[0] = 1.0;
            uhi[0] = lp->col_upper[ind];
        } else {
            const int64_t b = m->sos_start[sos], e = m->sos_start[sos + 1];
            const int64_t n = e - b;
            double wsum = 0.0, wpos = 0.0;
            for (int64_t t = 0; t < n; t++) {
                const double a = fabs(x[m->sos_col[b + t]]);
                wsum += a;
                wpos += a * (double)t;
            }
            int64_t r = (int64_t)floor(wpos / wsum);
            const int64_t rlo = m->sos_type[sos] == 1 ? sos_first : sos_first + 1;
            const int64_t rhi = sos_last - 1;
            if (r < rlo)
                r = rlo;
            if (r > rhi)
                r = rhi;
            nfd = nfu = 0;
            for (int64_t t = 0; t < n; t++) {
                const int64_t j = m->sos_col[b + t];
                if (t > r) {
                    fcol[nfd] = j;
                    flo[nfd] = 0.0;
                    fhi[nfd] = 0.0;
                    nfd++;
                }
                const bool zero_up = m->sos_type[sos] == 1 ? t <= r : t < r;
                if (zero_up) {
                    ucol[nfu] = j;
                    ulo[nfu] = 0.0;
                    uhi[nfu] = 0.0;
                    nfu++;
                }
            }
        }

        const bool child_no_cuts = stalled || (nodes > 1 && cur->no_cuts);
        bnode *down = node_child(nodes > 0 ? cur : nullptr, nc, nr_child,
                                 lp->sol_col_status, child_rs, nfd, fcol, flo,
                                 fhi, key, next_id++, frac_d, false, act,
                                 act_n, child_no_cuts);
        bnode *up = node_child(nodes > 0 ? cur : nullptr, nc, nr_child,
                               lp->sol_col_status, child_rs, nfu, ucol, ulo,
                               uhi, key, next_id++, frac_u, true, act, act_n,
                               child_no_cuts);
        if (down == nullptr || up == nullptr) {
            node_free(down);
            node_free(up);
            goto done;
        }

        const bool dive_here = dive &&
            (degrade <= 0.0 || cur == nullptr ||
             branch_key - cur->key <= degrade * (1.0 + fabs(cur->key)));
        bnode *first = f < 0.5 ? down : up;
        if (dive_child == JAOS_DIVE_UP) {
            first = up;
        } else if (dive_child == JAOS_DIVE_DOWN) {
            first = down;
        } else if (dive_child == JAOS_DIVE_PSEUDOCOST && branch >= 0) {
            const double gd = f * pseudocost(branch, 0, nc, pc_sum, pc_n);
            const double gu = (1.0 - f) * pseudocost(branch, 1, nc, pc_sum, pc_n);
            if (gd < gu)
                first = down;
            else if (gu < gd)
                first = up;
        }
        bnode *other = first == down ? up : down;
        if (dive_here) {

            if (stack_dive) {
                if (!JM_GROW(dstack, dstack_cap, dstack_n + 1)) {
                    node_free(down);
                    node_free(up);
                    goto done;
                }
                dstack[dstack_n++] = other;
            } else if (!heap_push(&heap, other)) {
                node_free(down);
                node_free(up);
                goto done;
            }
            next = first;
        } else if (!heap_push(&heap, down)) {
            node_free(down);
            node_free(up);
            goto done;
        } else if (!heap_push(&heap, up)) {
            node_free(up);
            goto done;
        }
    }

    rc = JAOS_OK;
    m->solve_status = outcome;

    m->sol_basis_ok = false;
    m->solve_work = work;
    m->solve_iters = iters;
    m->solve_time = now_seconds() - t0;
    m->mip_nodes = nodes;
    m->mip_solves = solves;
    m->mip_cuts = cuts;
    m->mip_heur = heur_points;
    m->mip_first_inc = first_inc;
    m->mip_rcfix_n = rcfixed;
    m->mip_prop_n = tightened;
    {
        const double ok = open_key(&heap, dstack, dstack_n);
        m->mip_bound = sigma * (ok < best_bound ? ok : best_bound);
    }
    if (outcome == JAOS_SOLVE_OPTIMAL)
        m->mip_bound = inc.obj;
    jm_log(m, JAOS_LOG_SUMMARY,
           "branch and bound: %s after %lld nodes, %lld solves, %lld cuts "
           "(%lld below the root), %lld points by rounding, %lld of them "
           "by the dive heuristic, %lld by RINS and %lld by the pump, "
           "%lld probes, "
           "%lld of them capped, %lld columns fixed by cliques and %lld "
           "nodes cut by them",
           jaos_solve_status_str(outcome), (long long)nodes,
           (long long)solves, (long long)cuts, (long long)local_cuts,
           (long long)heur_points, (long long)dive_points,
           (long long)rins_points, (long long)pump_points,
           (long long)probes, (long long)capped, (long long)clique_fixed,
           (long long)clique_cut_nodes);

    m->mip_pool_n = sp.n;
    m->mip_pool_x = sp.x;
    m->mip_pool_obj = sp.obj;
    sp.x = sp.obj = nullptr;
    if (inc.have) {
        m->mip_has_incumbent = true;
        m->mip_inc_obj = inc.obj;
        m->mip_inc_x = inc.x;
        inc.x = nullptr;
    }
    if (outcome == JAOS_SOLVE_OPTIMAL) {
        if (jm_model_ensure_solution_arrays(m) != JAOS_OK) {
            rc = JAOS_ERR_OUT_OF_MEMORY;
            m->solve_status = JAOS_SOLVE_NOT_RUN;
            goto done;
        }

        if (nc > 0) {
            memcpy(m->sol_col, m->mip_inc_x, (size_t)nc * sizeof *m->sol_col);
            memcpy(m->sol_redcost, inc.cd, (size_t)nc * sizeof *m->sol_redcost);
            memcpy(m->sol_col_status, inc.cs, (size_t)nc * sizeof *m->sol_col_status);
        }
        if (nr > 0) {
            memcpy(m->sol_row, inc.ra, (size_t)nr * sizeof *m->sol_row);
            memcpy(m->sol_dual, inc.rd, (size_t)nr * sizeof *m->sol_dual);
            memcpy(m->sol_row_status, inc.rs, (size_t)nr * sizeof *m->sol_row_status);
        }

        m->sol_basis_ok = jm_model_basis_count_ok(m);
        if (!m->sol_basis_ok) {
            int64_t extra = 0;
            if (republish_at_the_incumbent(m, m->mip_inc_x, nc, nr, &extra))
                m->sol_basis_ok = jm_model_basis_count_ok(m);

            m->solve_work += extra;
        }
        jm_model_publish_objective(m);
        assert(!m->sol_basis_ok || jm_model_basis_count_ok(m));
    }

done:
    if (rc != JAOS_OK && m->solve_status != outcome)
        jm_set_err(m, "%s", m->err[0] ? m->err : "out of memory in branch and bound");
    free(x);
    free(xr);
    free(x2);
    free(ra);
    free(ilo);
    free(ihi);
    free(plo);
    free(phi);
    free(rcd);
    free(pc_sum);
    free(pc_n);
    free(cand);
    free(fcol);
    free(flo);
    free(fhi);
    free(pcs);
    free(prs);
    free(row);
    free(cut);
    free(act);
    free(items);
    free(pbuf);
    clique_table_free(&ctab);
    free(cstack);
    free(pextra);
    free(porder);
    free(mu);
    free(mbest);
    free(mdelta);
    free(prnd);
    free(pprev);
    free(pprev2);
    free(magg);
    free(mcmag);
    free(mused);
    free(mpicked);
    free(crs);
    free(in_copy.v);
    spool_free(&sp);
    cutbuf_free(&cb);
    cutbuf_free(&pool);
    while (heap.n > 0)
        node_free(heap_pop(&heap));
    free(heap.v);
    while (dstack_n > 0)
        node_free(dstack[--dstack_n]);
    free(dstack);
    node_free(cur);
    node_free(next);
    incumbent_free(&inc);
    jaos_model_free(lp);
    return rc;
}

jaos_status jaos_mip_result(const jaos_model *m, jaos_mip_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    out->nodes = m->mip_nodes;
    out->lp_solves = m->mip_solves;
    out->has_incumbent = m->mip_has_incumbent;
    out->incumbent = m->mip_has_incumbent ? m->mip_inc_obj : 0.0;
    out->bound = m->mip_bound;
    out->cuts = m->mip_cuts;
    out->heuristic_points = m->mip_heur;
    out->first_incumbent_node = m->mip_first_inc;
    out->fixed_cols = m->mip_rcfix_n;
    out->tightened = m->mip_prop_n;
    return JAOS_OK;
}

jaos_status jaos_mip_pool_count(const jaos_model *m, int64_t *count)
{
    if (m == nullptr || count == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *count = m->mip_pool_n;
    return JAOS_OK;
}

jaos_status jaos_mip_pool_solution(const jaos_model *m, int64_t k,
                                   double *col_value, double *objective)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (k < 0 || k >= m->mip_pool_n || m->mip_pool_x == nullptr) {
        jm_set_err((jaos_model *)m, "the solution pool holds %lld point(s); "
                   "there is no point %lld", (long long)m->mip_pool_n,
                   (long long)k);
        return JAOS_ERR_INVALID_INPUT;
    }
    if (col_value != nullptr && m->num_col > 0)
        memcpy(col_value, m->mip_pool_x + k * m->num_col,
               (size_t)m->num_col * sizeof *col_value);
    if (objective != nullptr)
        *objective = m->mip_pool_obj[k];
    return JAOS_OK;
}

jaos_status jaos_mip_incumbent(const jaos_model *m, double *col_value,
                               double *objective)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!m->mip_has_incumbent || m->mip_inc_x == nullptr) {
        jm_set_err((jaos_model *)m, "no incumbent: the last solve found no "
                   "integer point");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (col_value != nullptr && m->num_col > 0)
        memcpy(col_value, m->mip_inc_x, (size_t)m->num_col * sizeof *col_value);
    if (objective != nullptr)
        *objective = m->mip_inc_obj;
    return JAOS_OK;
}
