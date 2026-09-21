/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"
#include "jaos_sys.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double CHOL_PIVOT_REL  = 1e-14;
constexpr double CHOL_PIVOT_HUGE = 1e128;
constexpr double TINY            = 1e-300;
constexpr double CHOL_DENSE      = 10.0;
constexpr int64_t CHOL_DENSE_MIN = 16;
#ifndef JAOS_CHOL_BLOCK_VALUE
#define JAOS_CHOL_BLOCK_VALUE 32
#endif
constexpr int64_t CHOL_BLOCK = JAOS_CHOL_BLOCK_VALUE;
#ifndef JAOS_CHOL_BLOCK_WORK_VALUE
#define JAOS_CHOL_BLOCK_WORK_VALUE 1000000
#endif
constexpr int64_t CHOL_BLOCK_WORK = JAOS_CHOL_BLOCK_WORK_VALUE;
constexpr int64_t CHOL_THREADS_MAX = 64;

typedef struct {
    int64_t *idx;
    int64_t n, cap;
} ilist;

static bool ilist_push(ilist *l, int64_t v)
{
    if (!JM_GROW(l->idx, l->cap, l->n + 1))
        return false;
    l->idx[l->n++] = v;
    return true;
}

enum { MD_VAR, MD_ELT, MD_DEAD };

typedef struct {
    int64_t n, live;
    ilist *var;
    ilist *elt;
    int8_t *state;
    int64_t *deg;
    int64_t *heap, *hat, hn;
    int64_t *mark, *wstamp;
    int64_t *w;
    int64_t *lp;
    int64_t lp_n;
    int64_t *last, nlast;
} md;

static void md_free(md *g)
{
    if (g->var != nullptr)
        for (int64_t i = 0; i < g->n; i++)
            free(g->var[i].idx);
    if (g->elt != nullptr)
        for (int64_t i = 0; i < g->n; i++)
            free(g->elt[i].idx);
    free(g->var);
    free(g->elt);
    free(g->state);
    free(g->deg);
    free(g->heap);
    free(g->hat);
    free(g->mark);
    free(g->wstamp);
    free(g->w);
    free(g->lp);
    free(g->last);
    memset(g, 0, sizeof *g);
}

static bool md_before(const md *g, int64_t a, int64_t b)
{
    return g->deg[a] != g->deg[b] ? g->deg[a] < g->deg[b] : a < b;
}

static void heap_swap(md *g, int64_t a, int64_t b)
{
    const int64_t x = g->heap[a], y = g->heap[b];
    g->heap[a] = y;
    g->hat[y] = a;
    g->heap[b] = x;
    g->hat[x] = b;
}

static void heap_up(md *g, int64_t at)
{
    while (at > 0) {
        const int64_t up = (at - 1) / 2;
        if (!md_before(g, g->heap[at], g->heap[up]))
            break;
        heap_swap(g, at, up);
        at = up;
    }
}

static void heap_down(md *g, int64_t at)
{
    for (;;) {
        int64_t best = at;
        const int64_t l = 2 * at + 1, r = l + 1;
        if (l < g->hn && md_before(g, g->heap[l], g->heap[best]))
            best = l;
        if (r < g->hn && md_before(g, g->heap[r], g->heap[best]))
            best = r;
        if (best == at)
            break;
        heap_swap(g, at, best);
        at = best;
    }
}

static void heap_insert(md *g, int64_t i, int64_t d)
{
    g->deg[i] = d;
    g->heap[g->hn] = i;
    g->hat[i] = g->hn++;
    heap_up(g, g->hat[i]);
}

static void heap_set(md *g, int64_t i, int64_t d)
{
    const int64_t old = g->deg[i];
    g->deg[i] = d;
    if (d < old)
        heap_up(g, g->hat[i]);
    else if (d > old)
        heap_down(g, g->hat[i]);
}

static int64_t heap_pop(md *g)
{
    const int64_t top = g->heap[0];
    g->hn--;
    if (g->hn > 0) {
        g->heap[0] = g->heap[g->hn];
        g->hat[g->heap[0]] = 0;
        heap_down(g, 0);
    }
    g->hat[top] = -1;
    return top;
}

static bool md_build(md *g, int64_t n, const int64_t *start,
                     const int64_t *index, int64_t dense)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->var    = jm_calloc_array(n, sizeof *g->var);
    g->elt    = jm_calloc_array(n, sizeof *g->elt);
    g->state  = jm_calloc_array(n, sizeof *g->state);
    g->deg    = jm_alloc_array(n, sizeof *g->deg);
    g->heap   = jm_alloc_array(n, sizeof *g->heap);
    g->hat    = jm_alloc_array(n, sizeof *g->hat);
    g->mark   = jm_alloc_array(n, sizeof *g->mark);
    g->wstamp = jm_alloc_array(n, sizeof *g->wstamp);
    g->w      = jm_alloc_array(n, sizeof *g->w);
    g->lp     = jm_alloc_array(n, sizeof *g->lp);
    g->last   = jm_alloc_array(n, sizeof *g->last);
    if (g->var == nullptr || g->elt == nullptr || g->state == nullptr ||
        g->deg == nullptr || g->heap == nullptr || g->hat == nullptr ||
        g->mark == nullptr || g->wstamp == nullptr || g->w == nullptr ||
        g->lp == nullptr || g->last == nullptr)
        return false;

    for (int64_t i = 0; i < n; i++) {
        g->mark[i] = -1;
        g->wstamp[i] = -1;
    }
    for (int64_t j = 0; j < n; j++)
        for (int64_t p = start[j]; p < start[j + 1]; p++) {
            int64_t i = index[p];
            if (i == j)
                continue;
            if (!ilist_push(&g->var[j], i) || !ilist_push(&g->var[i], j))
                return false;
        }
    for (int64_t i = 0; i < n; i++) {
        ilist *l = &g->var[i];
        int64_t m = 0;
        for (int64_t k = 0; k < l->n; k++) {
            int64_t v = l->idx[k];
            if (g->mark[v] != i) {
                g->mark[v] = i;
                l->idx[m++] = v;
            }
        }
        l->n = m;
    }
    for (int64_t i = 0; i < n; i++)
        g->mark[i] = -1;
    for (int64_t i = 0; i < n; i++)
        if (g->var[i].n > dense) {
            g->state[i] = MD_DEAD;
            g->last[g->nlast++] = i;
        }
    for (int64_t i = 0; g->nlast > 0 && i < n; i++) {
        ilist *l = &g->var[i];
        int64_t m = 0;
        for (int64_t k = 0; g->state[i] != MD_DEAD && k < l->n; k++)
            if (g->state[l->idx[k]] != MD_DEAD)
                l->idx[m++] = l->idx[k];
        l->n = m;
    }
    g->live = n - g->nlast;
    for (int64_t i = 0; i < n; i++)
        if (g->state[i] != MD_DEAD)
            heap_insert(g, i, g->var[i].n);
    return true;
}

static bool md_order(md *g, int64_t *perm, jm_work *w)
{
    int64_t n = g->live;
    int64_t touched = 0;

    for (int64_t k = 0; k < n; k++) {
        int64_t p = heap_pop(g);
        perm[k] = p;

        g->lp_n = 0;
        g->mark[p] = k;
        ilist *ap = &g->var[p];
        for (int64_t q = 0; q < ap->n; q++) {
            int64_t v = ap->idx[q];
            touched++;
            if (g->state[v] != MD_VAR || g->mark[v] == k)
                continue;
            g->mark[v] = k;
            g->lp[g->lp_n++] = v;
        }
        ilist *ep = &g->elt[p];
        for (int64_t q = 0; q < ep->n; q++) {
            int64_t e = ep->idx[q];
            touched++;
            if (g->state[e] != MD_ELT)
                continue;
            ilist *le = &g->var[e];
            for (int64_t r = 0; r < le->n; r++) {
                int64_t v = le->idx[r];
                touched++;
                if (g->state[v] != MD_VAR || g->mark[v] == k)
                    continue;
                g->mark[v] = k;
                g->lp[g->lp_n++] = v;
            }
            g->state[e] = MD_DEAD;
            free(le->idx);
            le->idx = nullptr;
            le->n = le->cap = 0;
        }
        free(ep->idx);
        ep->idx = nullptr;
        ep->n = ep->cap = 0;

        if (!JM_GROW(ap->idx, ap->cap, g->lp_n))
            return false;
        if (g->lp_n > 0)
            memcpy(ap->idx, g->lp, sizeof(int64_t) * (size_t)g->lp_n);
        ap->n = g->lp_n;
        g->state[p] = MD_ELT;

        for (int64_t q = 0; q < g->lp_n; q++) {
            int64_t i = g->lp[q];
            ilist *ei = &g->elt[i];
            for (int64_t r = 0; r < ei->n; r++) {
                int64_t e = ei->idx[r];
                touched++;
                if (g->state[e] != MD_ELT)
                    continue;
                if (g->wstamp[e] != k) {
                    g->wstamp[e] = k;
                    g->w[e] = g->var[e].n;
                }
                g->w[e]--;
            }
        }

        for (int64_t q = 0; q < g->lp_n; q++) {
            int64_t i = g->lp[q];
            ilist *ei = &g->elt[i];
            int64_t m = 0;
            int64_t ext = 0;
            for (int64_t r = 0; r < ei->n; r++) {
                int64_t e = ei->idx[r];
                touched++;
                if (g->state[e] != MD_ELT)
                    continue;
                if (g->w[e] == 0) {
                    g->state[e] = MD_DEAD;
                    free(g->var[e].idx);
                    g->var[e].idx = nullptr;
                    g->var[e].n = g->var[e].cap = 0;
                    continue;
                }
                ext += g->w[e];
                ei->idx[m++] = e;
            }
            ei->n = m;
            if (!ilist_push(ei, p))
                return false;

            ilist *ai = &g->var[i];
            m = 0;
            for (int64_t r = 0; r < ai->n; r++) {
                int64_t v = ai->idx[r];
                touched++;
                if (g->state[v] != MD_VAR || g->mark[v] == k)
                    continue;
                ai->idx[m++] = v;
            }
            ai->n = m;

            int64_t d = ai->n + (g->lp_n - 1) + ext;
            int64_t bound = g->deg[i] + (g->lp_n - 1);
            if (d > bound)
                d = bound;
            if (d > n - k - 1)
                d = n - k - 1;
            if (d < 0)
                d = 0;
            heap_set(g, i, d);
        }
    }
    for (int64_t t = 0; t < g->nlast; t++)
        perm[n + t] = g->last[t];
    jm_work_add(w, touched * JM_WORK_NONZERO);
    return true;
}

void jm_chol_init(jm_chol *c) { memset(c, 0, sizeof *c); }

void jm_chol_free(jm_chol *c)
{
    free(c->perm);
    free(c->inv);
    free(c->parent);
    free(c->a_start);
    free(c->a_index);
    free(c->a_src);
    free(c->l_start);
    free(c->l_index);
    free(c->l_value);
    free(c->d);
    free(c->fill);
    free(c->x);
    free(c->s);
    free(c->path);
    free(c->mark);
    free(c->row_work);
    memset(c, 0, sizeof *c);
}

static int64_t ereach(const jm_chol *c, int64_t k)
{
    int64_t n = c->n;
    int64_t top = n;
    c->mark[k] = k;
    for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
        int64_t i = c->a_index[p];
        int64_t len = 0;
        for (; c->mark[i] != k; i = c->parent[i]) {
            c->path[len++] = i;
            c->mark[i] = k;
        }
        while (len > 0)
            c->s[--top] = c->path[--len];
    }
    return top;
}

int64_t jm_chol_dense_limit(int64_t n)
{
    const double limit = CHOL_DENSE * sqrt((double)n);
    return limit > (double)CHOL_DENSE_MIN ? (int64_t)limit : CHOL_DENSE_MIN;
}

jaos_status jm_chol_symbolic(jm_chol *c, int64_t n, const int64_t *start,
                             const int64_t *index, jm_work *w)
{
    return jm_chol_symbolic_dense(c, n, start, index, n, w);
}

jaos_status jm_chol_symbolic_dense(jm_chol *c, int64_t n,
                                   const int64_t *start, const int64_t *index,
                                   int64_t dense, jm_work *w)
{
    if (n < 0 || (n > 0 && (start == nullptr || index == nullptr)))
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t j = 0; j < n; j++) {
        if (start[j] > start[j + 1])
            return JAOS_ERR_INVALID_INPUT;
        for (int64_t p = start[j]; p < start[j + 1]; p++)
            if (index[p] < 0 || index[p] >= n)
                return JAOS_ERR_INVALID_INPUT;
    }

    jm_chol_free(c);
    c->n = n;
    c->perm   = jm_alloc_array(n, sizeof *c->perm);
    c->inv    = jm_alloc_array(n, sizeof *c->inv);
    c->parent = jm_alloc_array(n, sizeof *c->parent);
    c->l_start = jm_alloc_array(n + 1, sizeof *c->l_start);
    c->a_start = jm_alloc_array(n + 1, sizeof *c->a_start);
    c->fill = jm_alloc_array(n, sizeof *c->fill);
    c->x    = jm_alloc_array(n, sizeof *c->x);
    c->s    = jm_alloc_array(n, sizeof *c->s);
    c->path = jm_alloc_array(n, sizeof *c->path);
    c->mark = jm_alloc_array(n, sizeof *c->mark);
    c->row_work = jm_alloc_array(n, sizeof *c->row_work);
    if (c->perm == nullptr || c->inv == nullptr || c->parent == nullptr ||
        c->l_start == nullptr || c->a_start == nullptr || c->fill == nullptr ||
        c->x == nullptr || c->s == nullptr || c->path == nullptr ||
        c->mark == nullptr || c->row_work == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    md g;
    bool ok = md_build(&g, n, start, index, dense) && md_order(&g, c->perm, w);
    md_free(&g);
    if (!ok)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = 0; k < n; k++)
        c->inv[c->perm[k]] = k;

    int64_t nnz = start[n];
    c->a_index = jm_alloc_array(nnz, sizeof *c->a_index);
    c->a_src   = jm_alloc_array(nnz, sizeof *c->a_src);
    if (c->a_index == nullptr || c->a_src == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = 0; k <= n; k++)
        c->a_start[k] = 0;
    for (int64_t j = 0; j < n; j++)
        for (int64_t p = start[j]; p < start[j + 1]; p++)
            if (c->inv[index[p]] <= c->inv[j])
                c->a_start[c->inv[j] + 1]++;
    for (int64_t k = 0; k < n; k++)
        c->a_start[k + 1] += c->a_start[k];
    for (int64_t k = 0; k < n; k++)
        c->fill[k] = c->a_start[k];
    for (int64_t j = 0; j < n; j++)
        for (int64_t p = start[j]; p < start[j + 1]; p++) {
            int64_t pi = c->inv[index[p]], pj = c->inv[j];
            if (pi > pj)
                continue;
            c->a_index[c->fill[pj]] = pi;
            c->a_src[c->fill[pj]] = p;
            c->fill[pj]++;
        }

    for (int64_t k = 0; k < n; k++) {
        c->parent[k] = -1;
        c->mark[k] = -1;
    }
    for (int64_t k = 0; k < n; k++)
        for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
            int64_t i = c->a_index[p];
            while (i != -1 && i < k) {
                int64_t up = c->mark[i];
                c->mark[i] = k;
                if (up == -1)
                    c->parent[i] = k;
                i = up;
            }
        }

    for (int64_t k = 0; k < n; k++) {
        c->fill[k] = 1;
        c->mark[k] = -1;
    }
    int64_t reach = 0;
    for (int64_t k = 0; k < n; k++) {
        int64_t top = ereach(c, k);
        reach += n - top;
        int64_t row = 0;
        for (int64_t q = top; q < n; q++)
            row += c->fill[c->s[q]]++ - 1;
        c->row_work[k] = row;
    }
    jm_work_add(w, (reach + c->a_start[n]) * JM_WORK_NONZERO);

    c->l_start[0] = 0;
    for (int64_t k = 0; k < n; k++)
        c->l_start[k + 1] = c->l_start[k] + c->fill[k];
    c->nnz = c->l_start[n];
    c->l_index = jm_alloc_array(c->nnz, sizeof *c->l_index);
    c->l_value = jm_alloc_array(c->nnz, sizeof *c->l_value);
    c->d       = jm_alloc_array(n, sizeof *c->d);
    if (c->l_index == nullptr || c->l_value == nullptr || c->d == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = 0; k < n; k++)
        c->l_index[c->l_start[k]] = k;
    c->symbolic = true;
    return JAOS_OK;
}

static void chol_pivot(jm_chol *c, int64_t k, double diag, double d)
{
    double low = fabs(diag) * CHOL_PIVOT_REL;
    if (low < TINY)
        low = TINY;
    if (!(d > low)) {
        d = CHOL_PIVOT_HUGE;
        c->replaced++;
    }
    c->l_value[c->l_start[k]] = sqrt(d);
}

static void chol_row(jm_chol *c, const double *value, int64_t k,
                     int64_t *gathered, int64_t *eliminated)
{
    const int64_t n = c->n;
    const int64_t top = ereach(c, k);
    double diag = 0.0;
    for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
        const int64_t i = c->a_index[p];
        const double v = value[c->a_src[p]];
        if (i == k)
            diag += v;
        else
            c->x[i] += v;
    }
    *gathered += c->a_start[k + 1] - c->a_start[k] + n - top;
    double d = diag;
    for (int64_t q = top; q < n; q++) {
        const int64_t i = c->s[q];
        const double lki = c->x[i] / c->l_value[c->l_start[i]];
        c->x[i] = 0.0;
        const int64_t p0 = c->l_start[i] + 1, p1 = c->fill[i];
        for (int64_t p = p0; p < p1; p++)
            c->x[c->l_index[p]] -= c->l_value[p] * lki;
        *eliminated += p1 - p0;
        d -= lki * lki;
        c->l_index[p1] = k;
        c->l_value[p1] = lki;
        c->fill[i] = p1 + 1;
    }
    chol_pivot(c, k, diag, d);
}

typedef struct {
    int64_t k0, k1;
    int64_t off[CHOL_BLOCK], len[CHOL_BLOCK], who[CHOL_BLOCK];
    double diag[CHOL_BLOCK];
    double xb[CHOL_BLOCK * CHOL_BLOCK];
} chol_block;

typedef struct {
    jm_chol *c;
    const double *value;
    chol_block *b;
    int64_t id, lanes;
    int64_t *mark, *s, *path;
    double *x;
    int64_t *rs, *rp;
    double *rl;
    int64_t cap_s, cap_p, cap_l, used;
    int64_t gathered, eliminated;
    bool failed;
} chol_lane;

static bool chol_front(chol_lane *t, int64_t k)
{
    jm_chol *c = t->c;
    chol_block *b = t->b;
    const int64_t n = c->n, k0 = b->k0, at = k - k0;
    int64_t top = n;
    t->mark[k] = k;
    for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
        int64_t i = c->a_index[p];
        int64_t len = 0;
        for (; t->mark[i] != k; i = c->parent[i]) {
            t->path[len++] = i;
            t->mark[i] = k;
        }
        while (len > 0)
            t->s[--top] = t->path[--len];
    }
    const int64_t r = n - top;
    if (!JM_GROW(t->rs, t->cap_s, t->used + r) ||
        !JM_GROW(t->rp, t->cap_p, t->used + r) ||
        !JM_GROW(t->rl, t->cap_l, t->used + r))
        return false;
    int64_t *rs = t->rs + t->used, *rp = t->rp + t->used;
    double *rl = t->rl + t->used;
    double diag = 0.0;
    for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
        const int64_t i = c->a_index[p];
        const double v = t->value[c->a_src[p]];
        if (i == k)
            diag += v;
        else
            t->x[i] += v;
    }
    t->gathered += c->a_start[k + 1] - c->a_start[k] + r;
    for (int64_t q = 0; q < r; q++) {
        const int64_t i = t->s[top + q];
        rs[q] = i;
        rp[q] = 0;
        rl[q] = 0.0;
        if (i >= k0)
            continue;
        const double lki = t->x[i] / c->l_value[c->l_start[i]];
        t->x[i] = 0.0;
        const int64_t p0 = c->l_start[i] + 1, p1 = c->fill[i];
        for (int64_t p = p0; p < p1; p++)
            t->x[c->l_index[p]] -= c->l_value[p] * lki;
        t->eliminated += p1 - p0;
        rl[q] = lki;
        rp[q] = p1;
    }
    for (int64_t j = k0; j < k; j++) {
        b->xb[at * CHOL_BLOCK + (j - k0)] = t->x[j];
        t->x[j] = 0.0;
    }
    b->off[at] = t->used;
    b->len[at] = r;
    b->who[at] = t->id;
    b->diag[at] = diag;
    t->used += r;
    return true;
}

static void chol_lane_run(void *arg)
{
    chol_lane *t = arg;
    for (int64_t k = t->b->k0 + t->id; k < t->b->k1 && !t->failed;
         k += t->lanes)
        t->failed = !chol_front(t, k);
}

static void chol_back(jm_chol *c, const chol_block *b, const chol_lane *lane,
                      int64_t k, int64_t *eliminated)
{
    const int64_t k0 = b->k0, at = k - k0;
    const chol_lane *t = &lane[b->who[at]];
    const int64_t *rs = t->rs + b->off[at], *rp = t->rp + b->off[at];
    const double *rl = t->rl + b->off[at];
    for (int64_t j = k0; j < k; j++)
        c->x[j] = b->xb[at * CHOL_BLOCK + (j - k0)];
    double d = b->diag[at];
    for (int64_t q = 0; q < b->len[at]; q++) {
        const int64_t i = rs[q];
        const int64_t p1 = c->fill[i];
        double lki;
        int64_t p0;
        if (i < k0) {
            lki = rl[q];
            p0 = rp[q];
        } else {
            lki = c->x[i] / c->l_value[c->l_start[i]];
            c->x[i] = 0.0;
            p0 = c->l_start[i] + 1;
        }
        for (int64_t p = p0; p < p1; p++)
            c->x[c->l_index[p]] -= c->l_value[p] * lki;
        *eliminated += p1 - p0;
        d -= lki * lki;
        c->l_index[p1] = k;
        c->l_value[p1] = lki;
        c->fill[i] = p1 + 1;
    }
    chol_pivot(c, k, b->diag[at], d);
}

jaos_status jm_chol_numeric(jm_chol *c, const double *value, jm_work *w)
{
    if (!c->symbolic || (c->n > 0 && value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    const int64_t n = c->n;
    int64_t gathered = 0, eliminated = 0;

    jm_work_add(w, JM_WORK_FACTOR);
    c->replaced = 0;
    for (int64_t k = 0; k < n; k++) {
        c->fill[k] = c->l_start[k] + 1;
        c->mark[k] = -1;
        c->x[k] = 0.0;
    }

    const int64_t lanes = c->threads > CHOL_THREADS_MAX ? CHOL_THREADS_MAX
                          : c->threads > 1 ? c->threads : 1;
    chol_lane lane[CHOL_THREADS_MAX] = {};
    chol_block *b = nullptr;
    int64_t *lmark = nullptr, *lsp = nullptr;
    double *lx = nullptr;
    jaos_status st = JAOS_OK;
    if (lanes > 1) {
        b = jm_calloc_array(1, sizeof *b);
        lmark = jm_alloc_array(lanes * n, sizeof *lmark);
        lsp = jm_alloc_array(2 * lanes * n, sizeof *lsp);
        lx = jm_calloc_array(lanes * n, sizeof *lx);
        if (b == nullptr || lmark == nullptr || lsp == nullptr ||
            lx == nullptr) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto out;
        }
        for (int64_t i = 0; i < lanes * n; i++)
            lmark[i] = -1;
        for (int64_t t = 0; t < lanes; t++)
            lane[t] = (chol_lane){ .c = c, .value = value, .b = b, .id = t,
                                   .lanes = lanes, .mark = lmark + t * n,
                                   .s = lsp + 2 * t * n,
                                   .path = lsp + (2 * t + 1) * n,
                                   .x = lx + t * n };
    }
    for (int64_t k0 = 0; k0 < n;) {
        const int64_t k1 = n - k0 > CHOL_BLOCK ? k0 + CHOL_BLOCK : n;
        int64_t work = 0;
        for (int64_t k = k0; lanes > 1 && k < k1; k++)
            work += c->row_work[k];
        if (lanes == 1 || work < CHOL_BLOCK_WORK) {
            for (int64_t k = k0; k < k1; k++)
                chol_row(c, value, k, &gathered, &eliminated);
            k0 = k1;
            continue;
        }
        b->k0 = k0;
        b->k1 = k1;
        for (int64_t t = 0; t < lanes; t++)
            lane[t].used = 0;
        jm_thread th[CHOL_THREADS_MAX];
        bool started[CHOL_THREADS_MAX] = {};
        for (int64_t t = 1; t < lanes; t++)
            started[t] = jm_thread_start(&th[t], chol_lane_run, &lane[t]);
        chol_lane_run(&lane[0]);
        for (int64_t t = 1; t < lanes; t++) {
            if (started[t])
                jm_thread_join(&th[t]);
            else
                chol_lane_run(&lane[t]);
        }
        for (int64_t t = 0; t < lanes; t++)
            if (lane[t].failed) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto out;
            }
        for (int64_t k = k0; k < k1; k++)
            chol_back(c, b, lane, k, &eliminated);
        k0 = k1;
    }
    for (int64_t t = 0; t < lanes; t++) {
        gathered += lane[t].gathered;
        eliminated += lane[t].eliminated;
    }
out:
    for (int64_t t = 0; t < lanes; t++) {
        free(lane[t].rs);
        free(lane[t].rp);
        free(lane[t].rl);
    }
    free(b);
    free(lmark);
    free(lsp);
    free(lx);
    if (st != JAOS_OK)
        return st;
    for (int64_t k = 0; k < n; k++)
        assert(c->fill[k] == c->l_start[k + 1]);
    jm_work_add(w, gathered * JM_WORK_NONZERO + eliminated * JM_WORK_ELIMINATED);
    return JAOS_OK;
}

jaos_status jm_ldlt_numeric(jm_chol *c, const double *value,
                            const int8_t *sign, double floor, jm_work *w)
{
    if (!c->symbolic || (c->n > 0 && value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    if (!(floor > 0.0))
        return JAOS_ERR_INVALID_INPUT;
    const int64_t n = c->n;
    int64_t gathered = 0, eliminated = 0;

    jm_work_add(w, JM_WORK_FACTOR);
    c->replaced = 0;
    for (int64_t k = 0; k < n; k++) {
        c->fill[k] = c->l_start[k] + 1;
        c->mark[k] = -1;
        c->x[k] = 0.0;
        c->l_value[c->l_start[k]] = 1.0;
    }

    for (int64_t k = 0; k < n; k++) {
        const int64_t top = ereach(c, k);
        double diag = 0.0;
        for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
            const int64_t i = c->a_index[p];
            const double v = value[c->a_src[p]];
            if (i == k)
                diag += v;
            else
                c->x[i] += v;
        }
        gathered += c->a_start[k + 1] - c->a_start[k];

        double d = diag;
        for (int64_t q = top; q < n; q++) {
            const int64_t i = c->s[q];

            const double xi = c->x[i];
            const double lki = xi / c->d[i];
            c->x[i] = 0.0;
            const int64_t p0 = c->l_start[i] + 1, p1 = c->fill[i];
            for (int64_t p = p0; p < p1; p++)
                c->x[c->l_index[p]] -= c->l_value[p] * xi;
            eliminated += p1 - p0;
            d -= lki * xi;
            c->l_index[p1] = k;
            c->l_value[p1] = lki;
            c->fill[i] = p1 + 1;
        }
        gathered += n - top;

        const double want = sign == nullptr ? 1.0
                                            : (double)sign[c->perm[k]];
        if (!(d * want >= floor)) {
            d = want * floor;
            c->replaced++;
        }
        c->d[k] = d;
    }
    for (int64_t k = 0; k < n; k++)
        assert(c->fill[k] == c->l_start[k + 1]);
    jm_work_add(w, gathered * JM_WORK_NONZERO +
                       eliminated * JM_WORK_ELIMINATED);
    return JAOS_OK;
}

void jm_ldlt_solve(const jm_chol *c, double *b, jm_work *w)
{
    const int64_t n = c->n;
    double *y = c->x;
    for (int64_t k = 0; k < n; k++)
        y[k] = b[c->perm[k]];
    for (int64_t k = 0; k < n; k++) {
        const int64_t p0 = c->l_start[k], p1 = c->l_start[k + 1];
        const double yk = y[k];
        for (int64_t p = p0 + 1; p < p1; p++)
            y[c->l_index[p]] -= c->l_value[p] * yk;
    }
    for (int64_t k = 0; k < n; k++)
        y[k] /= c->d[k];
    for (int64_t k = n - 1; k >= 0; k--) {
        const int64_t p0 = c->l_start[k], p1 = c->l_start[k + 1];
        double s = y[k];
        for (int64_t p = p0 + 1; p < p1; p++)
            s -= c->l_value[p] * y[c->l_index[p]];
        y[k] = s;
    }
    for (int64_t k = 0; k < n; k++)
        b[c->perm[k]] = y[k];
    jm_work_add(w, (2 * c->nnz + 3 * n) * JM_WORK_NONZERO);
}

void jm_chol_solve(const jm_chol *c, double *b, jm_work *w)
{
    int64_t n = c->n;
    double *y = c->x;
    for (int64_t k = 0; k < n; k++)
        y[k] = b[c->perm[k]];
    for (int64_t k = 0; k < n; k++) {
        int64_t p0 = c->l_start[k], p1 = c->l_start[k + 1];
        double yk = y[k] / c->l_value[p0];
        y[k] = yk;
        for (int64_t p = p0 + 1; p < p1; p++)
            y[c->l_index[p]] -= c->l_value[p] * yk;
    }
    for (int64_t k = n - 1; k >= 0; k--) {
        int64_t p0 = c->l_start[k], p1 = c->l_start[k + 1];
        double s = y[k];
        for (int64_t p = p0 + 1; p < p1; p++)
            s -= c->l_value[p] * y[c->l_index[p]];
        y[k] = s / c->l_value[p0];
    }
    for (int64_t k = 0; k < n; k++)
        b[c->perm[k]] = y[k];
    jm_work_add(w, (2 * c->nnz + 2 * n) * JM_WORK_NONZERO);
}
