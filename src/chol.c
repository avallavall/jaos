/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double CHOL_PIVOT_REL  = 1e-14;
constexpr double CHOL_PIVOT_HUGE = 1e128;
constexpr double TINY            = 1e-300;

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
    int64_t n;
    ilist *var;
    ilist *elt;
    int8_t *state;
    int64_t *deg;
    int64_t *head, *next, *prev;
    int64_t *mark, *wstamp;
    int64_t *w;
    int64_t *lp;
    int64_t lp_n;
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
    free(g->head);
    free(g->next);
    free(g->prev);
    free(g->mark);
    free(g->wstamp);
    free(g->w);
    free(g->lp);
    memset(g, 0, sizeof *g);
}

static void bucket_remove(md *g, int64_t i)
{
    int64_t d = g->deg[i];
    if (g->prev[i] >= 0)
        g->next[g->prev[i]] = g->next[i];
    else
        g->head[d] = g->next[i];
    if (g->next[i] >= 0)
        g->prev[g->next[i]] = g->prev[i];
    g->next[i] = g->prev[i] = -1;
}

static void bucket_insert(md *g, int64_t i, int64_t d)
{
    g->deg[i] = d;
    g->prev[i] = -1;
    g->next[i] = g->head[d];
    if (g->head[d] >= 0)
        g->prev[g->head[d]] = i;
    g->head[d] = i;
}

static bool md_build(md *g, int64_t n, const int64_t *start,
                     const int64_t *index)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->var    = jm_calloc_array(n, sizeof *g->var);
    g->elt    = jm_calloc_array(n, sizeof *g->elt);
    g->state  = jm_calloc_array(n, sizeof *g->state);
    g->deg    = jm_alloc_array(n, sizeof *g->deg);
    g->head   = jm_alloc_array(n + 1, sizeof *g->head);
    g->next   = jm_alloc_array(n, sizeof *g->next);
    g->prev   = jm_alloc_array(n, sizeof *g->prev);
    g->mark   = jm_alloc_array(n, sizeof *g->mark);
    g->wstamp = jm_alloc_array(n, sizeof *g->wstamp);
    g->w      = jm_alloc_array(n, sizeof *g->w);
    g->lp     = jm_alloc_array(n, sizeof *g->lp);
    if (g->var == nullptr || g->elt == nullptr || g->state == nullptr ||
        g->deg == nullptr || g->head == nullptr || g->next == nullptr ||
        g->prev == nullptr || g->mark == nullptr || g->wstamp == nullptr ||
        g->w == nullptr || g->lp == nullptr)
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
    for (int64_t d = 0; d <= n; d++)
        g->head[d] = -1;
    for (int64_t i = 0; i < n; i++)
        bucket_insert(g, i, g->var[i].n);
    return true;
}

static int64_t md_pick(md *g, int64_t *mindeg)
{
    while (g->head[*mindeg] < 0)
        (*mindeg)++;
    int64_t best = -1;
    for (int64_t i = g->head[*mindeg]; i >= 0; i = g->next[i])
        if (best < 0 || i < best)
            best = i;
    return best;
}

static bool md_order(md *g, int64_t *perm, jm_work *w)
{
    int64_t n = g->n;
    int64_t mindeg = 0;
    int64_t touched = 0;

    for (int64_t k = 0; k < n; k++) {
        int64_t p = md_pick(g, &mindeg);
        perm[k] = p;
        bucket_remove(g, p);

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
            bucket_remove(g, i);
            bucket_insert(g, i, d);
            if (d < mindeg)
                mindeg = d;
        }
    }
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
    free(c->fill);
    free(c->x);
    free(c->s);
    free(c->path);
    free(c->mark);
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

jaos_status jm_chol_symbolic(jm_chol *c, int64_t n, const int64_t *start,
                             const int64_t *index, jm_work *w)
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
    if (c->perm == nullptr || c->inv == nullptr || c->parent == nullptr ||
        c->l_start == nullptr || c->a_start == nullptr || c->fill == nullptr ||
        c->x == nullptr || c->s == nullptr || c->path == nullptr ||
        c->mark == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    md g;
    bool ok = md_build(&g, n, start, index) && md_order(&g, c->perm, w);
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
        for (int64_t q = top; q < n; q++)
            c->fill[c->s[q]]++;
    }
    jm_work_add(w, (reach + c->a_start[n]) * JM_WORK_NONZERO);

    c->l_start[0] = 0;
    for (int64_t k = 0; k < n; k++)
        c->l_start[k + 1] = c->l_start[k] + c->fill[k];
    c->nnz = c->l_start[n];
    c->l_index = jm_alloc_array(c->nnz, sizeof *c->l_index);
    c->l_value = jm_alloc_array(c->nnz, sizeof *c->l_value);
    if (c->l_index == nullptr || c->l_value == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = 0; k < n; k++)
        c->l_index[c->l_start[k]] = k;
    c->symbolic = true;
    return JAOS_OK;
}

jaos_status jm_chol_numeric(jm_chol *c, const double *value, jm_work *w)
{
    if (!c->symbolic || (c->n > 0 && value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    int64_t n = c->n;
    int64_t gathered = 0, eliminated = 0;

    jm_work_add(w, JM_WORK_FACTOR);
    c->replaced = 0;
    for (int64_t k = 0; k < n; k++) {
        c->fill[k] = c->l_start[k] + 1;
        c->mark[k] = -1;
        c->x[k] = 0.0;
    }

    for (int64_t k = 0; k < n; k++) {
        int64_t top = ereach(c, k);
        double diag = 0.0;
        for (int64_t p = c->a_start[k]; p < c->a_start[k + 1]; p++) {
            int64_t i = c->a_index[p];
            double v = value[c->a_src[p]];
            if (i == k)
                diag += v;
            else
                c->x[i] += v;
        }
        gathered += c->a_start[k + 1] - c->a_start[k];
        double d = diag;
        for (int64_t q = top; q < n; q++) {
            int64_t i = c->s[q];
            double lki = c->x[i] / c->l_value[c->l_start[i]];
            c->x[i] = 0.0;
            int64_t p0 = c->l_start[i] + 1, p1 = c->fill[i];
            for (int64_t p = p0; p < p1; p++)
                c->x[c->l_index[p]] -= c->l_value[p] * lki;
            eliminated += p1 - p0;
            d -= lki * lki;
            c->l_index[p1] = k;
            c->l_value[p1] = lki;
            c->fill[i] = p1 + 1;
        }
        gathered += n - top;
        double low = fabs(diag) * CHOL_PIVOT_REL;
        if (low < TINY)
            low = TINY;
        if (!(d > low)) {
            d = CHOL_PIVOT_HUGE;
            c->replaced++;
        }
        c->l_value[c->l_start[k]] = sqrt(d);
    }
    for (int64_t k = 0; k < n; k++)
        assert(c->fill[k] == c->l_start[k + 1]);
    jm_work_add(w, gathered * JM_WORK_NONZERO + eliminated * JM_WORK_ELIMINATED);
    return JAOS_OK;
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
