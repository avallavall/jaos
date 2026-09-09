/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr int64_t SYM_MAX_DEPTH = 64;
constexpr int64_t SYM_LEAF_CAP = 64;

typedef struct {
    int64_t n, nc, nr;
    int64_t *adj_start, *adj, *lab;
    int64_t *color0;
    int64_t *order, *key_start, *key;
    int64_t *cells, *inv, *perm, *uf;
    int64_t budget;
    int64_t *gen;
    int64_t ngen, gcap;
} sg;

static int64_t *g_cmp_key;
static int64_t *g_cmp_key_start;
static int64_t *g_cmp_color;

static int vertex_cmp(const void *pa, const void *pb)
{
    const int64_t a = *(const int64_t *)pa, b = *(const int64_t *)pb;
    if (g_cmp_color[a] != g_cmp_color[b])
        return g_cmp_color[a] < g_cmp_color[b] ? -1 : 1;
    const int64_t la = g_cmp_key_start[a + 1] - g_cmp_key_start[a];
    const int64_t lb = g_cmp_key_start[b + 1] - g_cmp_key_start[b];
    if (la != lb)
        return la < lb ? -1 : 1;
    const int64_t *ka = g_cmp_key + g_cmp_key_start[a];
    const int64_t *kb = g_cmp_key + g_cmp_key_start[b];
    for (int64_t t = 0; t < la; t++)
        if (ka[t] != kb[t])
            return ka[t] < kb[t] ? -1 : 1;
    return a < b ? -1 : a > b;
}

static bool vertex_same(int64_t a, int64_t b)
{
    if (g_cmp_color[a] != g_cmp_color[b])
        return false;
    const int64_t la = g_cmp_key_start[a + 1] - g_cmp_key_start[a];
    const int64_t lb = g_cmp_key_start[b + 1] - g_cmp_key_start[b];
    if (la != lb)
        return false;
    return memcmp(g_cmp_key + g_cmp_key_start[a], g_cmp_key + g_cmp_key_start[b],
                  (size_t)la * sizeof(int64_t)) == 0;
}

static int i64_cmp(const void *pa, const void *pb)
{
    const int64_t a = *(const int64_t *)pa, b = *(const int64_t *)pb;
    return a < b ? -1 : a > b;
}

static int64_t sg_count_colors(const sg *g, const int64_t *color)
{
    int64_t k = 0;
    for (int64_t v = 0; v < g->n; v++)
        if (color[v] + 1 > k)
            k = color[v] + 1;
    return k;
}

static bool sg_refine(sg *g, int64_t *color)
{
    const int64_t n = g->n;
    int64_t ncol = sg_count_colors(g, color);
    for (;;) {
        if (g->budget <= 0)
            return false;
        const int64_t stride = ncol + 1;
        for (int64_t v = 0; v < n; v++) {
            int64_t *k = g->key + g->adj_start[v];
            for (int64_t p = g->adj_start[v]; p < g->adj_start[v + 1]; p++)
                k[p - g->adj_start[v]] = g->lab[p] * stride + color[g->adj[p]];
            const int64_t d = g->adj_start[v + 1] - g->adj_start[v];
            if (d > 1)
                qsort(k, (size_t)d, sizeof *k, i64_cmp);
            g->order[v] = v;
        }
        g->budget -= g->adj_start[n] + n;
        g_cmp_key = g->key;
        g_cmp_key_start = g->adj_start;
        g_cmp_color = color;
        qsort(g->order, (size_t)n, sizeof *g->order, vertex_cmp);
        int64_t rank = 0;
        g->inv[g->order[0]] = 0;
        for (int64_t t = 1; t < n; t++) {
            if (!vertex_same(g->order[t - 1], g->order[t]))
                rank++;
            g->inv[g->order[t]] = rank;
        }
        const int64_t got = rank + 1;
        for (int64_t v = 0; v < n; v++)
            color[v] = g->inv[v];
        if (got == ncol)
            return true;
        ncol = got;
    }
}

static int64_t sg_first_cell(sg *g, const int64_t *color, int64_t *members)
{
    const int64_t n = g->n;
    for (int64_t v = 0; v < n; v++)
        g->cells[v] = 0;
    for (int64_t v = 0; v < n; v++)
        g->cells[color[v]]++;
    int64_t c = -1;
    for (int64_t k = 0; k < n; k++)
        if (g->cells[k] > 1) {
            c = k;
            break;
        }
    if (c < 0)
        return 0;
    int64_t m = 0;
    for (int64_t v = 0; v < n; v++)
        if (color[v] == c)
            members[m++] = v;
    return m;
}

static void sg_individualize(const sg *g, int64_t *color, int64_t v)
{
    const int64_t cv = color[v];
    for (int64_t u = 0; u < g->n; u++)
        color[u] = 2 * color[u] + (color[u] == cv && u != v ? 1 : 0);
}

static bool sg_is_automorphism(sg *g, const int64_t *perm);
static int64_t uf_find(int64_t *uf, int64_t v);

static int sg_find_under(sg *g, int64_t *color, const int64_t *inv1,
                         int64_t *leaves_left)
{
    const int64_t n = g->n;
    if (!sg_refine(g, color))
        return -1;
    int64_t *mem = malloc((size_t)n * sizeof *mem);
    if (mem == nullptr)
        return -1;
    const int64_t m = sg_first_cell(g, color, mem);
    if (m == 0) {
        free(mem);
        for (int64_t v = 0; v < n; v++)
            g->perm[v] = inv1[color[v]];
        (*leaves_left)--;
        if (sg_is_automorphism(g, g->perm))
            return 1;
        return *leaves_left > 0 ? 0 : -1;
    }
    int64_t *save = malloc((size_t)n * sizeof *save);
    if (save == nullptr) {
        free(mem);
        return -1;
    }
    memcpy(save, color, (size_t)n * sizeof *save);
    int r = 0;
    for (int64_t t = 0; t < m && r == 0; t++) {
        const int64_t u = mem[t];
        bool seen = false;
        for (int64_t q = 0; q < t && !seen; q++)
            seen = uf_find(g->uf, mem[q]) == uf_find(g->uf, u);
        if (seen)
            continue;
        memcpy(color, save, (size_t)n * sizeof *color);
        sg_individualize(g, color, u);
        r = sg_find_under(g, color, inv1, leaves_left);
    }
    free(save);
    free(mem);
    return r;
}

static bool sg_has_edge(const sg *g, int64_t u, int64_t v, int64_t lab)
{
    int64_t lo = g->adj_start[u], hi = g->adj_start[u + 1];
    while (lo < hi) {
        const int64_t mid = lo + (hi - lo) / 2;
        if (g->adj[mid] < v || (g->adj[mid] == v && g->lab[mid] < lab))
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < g->adj_start[u + 1] && g->adj[lo] == v && g->lab[lo] == lab;
}

static bool sg_is_automorphism(sg *g, const int64_t *perm)
{
    const int64_t n = g->n;
    bool identity = true;
    for (int64_t v = 0; v < n; v++) {
        if (perm[v] != v)
            identity = false;
        if (g->color0[perm[v]] != g->color0[v])
            return false;
        if ((v < g->nc) != (perm[v] < g->nc))
            return false;
    }
    if (identity)
        return false;
    for (int64_t v = 0; v < n; v++)
        for (int64_t p = g->adj_start[v]; p < g->adj_start[v + 1]; p++)
            if (!sg_has_edge(g, perm[v], perm[g->adj[p]], g->lab[p]))
                return false;
    g->budget -= g->adj_start[n];
    return true;
}

static int64_t uf_find_impl(int64_t *uf, int64_t v)
{
    while (uf[v] != v) {
        uf[v] = uf[uf[v]];
        v = uf[v];
    }
    return v;
}

static int64_t uf_find(int64_t *uf, int64_t v)
{
    return uf_find_impl(uf, v);
}

static void uf_union(int64_t *uf, int64_t a, int64_t b)
{
    a = uf_find(uf, a);
    b = uf_find(uf, b);
    if (a == b)
        return;
    if (a < b)
        uf[b] = a;
    else
        uf[a] = b;
}

static bool sg_add_generator(sg *g, const int64_t *perm)
{
    if (!JM_GROW(g->gen, g->gcap, (g->ngen + 1) * g->n))
        return false;
    memcpy(g->gen + g->ngen * g->n, perm, (size_t)g->n * sizeof *perm);
    g->ngen++;
    for (int64_t v = 0; v < g->n; v++)
        uf_union(g->uf, v, perm[v]);
    return true;
}

static int dbl_cmp(const void *pa, const void *pb)
{
    const double a = *(const double *)pa, b = *(const double *)pb;
    return a < b ? -1 : a > b;
}

static int64_t rank_of(const double *sorted, int64_t n, double v)
{
    int64_t lo = 0, hi = n;
    while (lo < hi) {
        const int64_t mid = lo + (hi - lo) / 2;
        if (sorted[mid] < v)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

typedef struct {
    double a, b, c, e;
    int64_t d;
    int64_t who;
} ckey;

static int ckey_cmp(const void *pa, const void *pb)
{
    const ckey *p = pa, *q = pb;
    if (p->a != q->a) return p->a < q->a ? -1 : 1;
    if (p->b != q->b) return p->b < q->b ? -1 : 1;
    if (p->c != q->c) return p->c < q->c ? -1 : 1;
    if (p->e != q->e) return p->e < q->e ? -1 : 1;
    if (p->d != q->d) return p->d < q->d ? -1 : 1;
    return p->who < q->who ? -1 : p->who > q->who;
}

static bool ckey_same(const ckey *p, const ckey *q)
{
    return p->a == q->a && p->b == q->b && p->c == q->c && p->e == q->e &&
           p->d == q->d;
}

static jaos_status sg_build(sg *g, const jaos_model *m)
{
    const int64_t nc = m->num_col, nr = m->num_row, nz = m->num_nz;
    const int64_t n = nc + nr;
    g->n = n;
    g->nc = nc;
    g->nr = nr;
    g->adj_start = jm_calloc_array(n + 1, sizeof *g->adj_start);
    g->adj = malloc((size_t)(2 * nz > 0 ? 2 * nz : 1) * sizeof *g->adj);
    g->lab = malloc((size_t)(2 * nz > 0 ? 2 * nz : 1) * sizeof *g->lab);
    g->color0 = malloc((size_t)(n > 0 ? n : 1) * sizeof *g->color0);
    g->order = malloc((size_t)(n > 0 ? n : 1) * sizeof *g->order);
    g->key = malloc((size_t)(2 * nz > 0 ? 2 * nz : 1) * sizeof *g->key);
    g->cells = malloc((size_t)(n > 0 ? n : 1) * sizeof *g->cells);
    g->inv = malloc((size_t)(n > 0 ? n : 1) * sizeof *g->inv);
    g->perm = malloc((size_t)(n > 0 ? n : 1) * sizeof *g->perm);
    g->uf = malloc((size_t)(n > 0 ? n : 1) * sizeof *g->uf);
    double *vals = malloc((size_t)(nz > 0 ? nz : 1) * sizeof *vals);
    ckey *keys = malloc((size_t)(n > 0 ? n : 1) * sizeof *keys);
    int64_t *indmark = jm_calloc_array(nc > 0 ? nc : 1, sizeof *indmark);
    if (g->adj_start == nullptr || g->adj == nullptr || g->lab == nullptr ||
        g->color0 == nullptr || g->order == nullptr || g->key == nullptr ||
        g->cells == nullptr || g->inv == nullptr || g->perm == nullptr ||
        g->uf == nullptr || vals == nullptr || keys == nullptr ||
        indmark == nullptr) {
        free(vals);
        free(keys);
        free(indmark);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (m->row_ind_col != nullptr)
        for (int64_t i = 0; i < nr; i++)
            if (m->row_ind_col[i] >= 0)
                indmark[m->row_ind_col[i]] = m->row_ind_col[i] + 1;
    memcpy(vals, m->a_value, (size_t)nz * sizeof *vals);
    qsort(vals, (size_t)nz, sizeof *vals, dbl_cmp);
    int64_t nv = 0;
    for (int64_t k = 0; k < nz; k++)
        if (nv == 0 || vals[k] != vals[nv - 1])
            vals[nv++] = vals[k];
    for (int64_t j = 0; j < nc; j++)
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            g->adj_start[j + 1]++;
            g->adj_start[nc + m->a_index[p] + 1]++;
        }
    for (int64_t v = 0; v < n; v++)
        g->adj_start[v + 1] += g->adj_start[v];
    for (int64_t v = 0; v < n; v++)
        g->cells[v] = g->adj_start[v];
    for (int64_t j = 0; j < nc; j++)
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const int64_t i = nc + m->a_index[p];
            const int64_t l = rank_of(vals, nv, m->a_value[p]);
            g->adj[g->cells[j]] = i;
            g->lab[g->cells[j]] = l;
            g->cells[j]++;
            g->adj[g->cells[i]] = j;
            g->lab[g->cells[i]] = l;
            g->cells[i]++;
        }
    for (int64_t j = 0; j < nc; j++) {
        keys[j] = (ckey){ .a = m->col_cost[j], .b = m->col_lower[j],
                          .c = m->col_upper[j],
                          .e = m->col_quad != nullptr ? m->col_quad[j] : 0.0,
                          .d = (m->col_integer != nullptr && m->col_integer[j])
                               + 2 * (m->col_semi != nullptr && m->col_semi[j])
                               + 4 * indmark[j],
                          .who = j };
    }
    qsort(keys, (size_t)nc, sizeof *keys, ckey_cmp);
    int64_t rank = 0;
    for (int64_t t = 0; t < nc; t++) {
        if (t > 0 && !ckey_same(&keys[t - 1], &keys[t]))
            rank++;
        g->color0[keys[t].who] = rank;
    }
    const int64_t col_colors = nc > 0 ? rank + 1 : 0;
    for (int64_t i = 0; i < nr; i++)
        keys[i] = (ckey){ .a = m->row_lower[i], .b = m->row_upper[i], .c = 0.0,
                          .e = 0.0,
                          .d = m->row_ind_col != nullptr ? m->row_ind_col[i] : -1,
                          .who = i };
    qsort(keys, (size_t)nr, sizeof *keys, ckey_cmp);
    rank = 0;
    for (int64_t t = 0; t < nr; t++) {
        if (t > 0 && !ckey_same(&keys[t - 1], &keys[t]))
            rank++;
        g->color0[nc + keys[t].who] = col_colors + rank;
    }
    for (int64_t v = 0; v < n; v++) {
        const int64_t d = g->adj_start[v + 1] - g->adj_start[v];
        int64_t *a = g->adj + g->adj_start[v], *l = g->lab + g->adj_start[v];
        for (int64_t x = 1; x < d; x++) {
            const int64_t av = a[x], lv = l[x];
            int64_t y = x;
            while (y > 0 && (a[y - 1] > av || (a[y - 1] == av && l[y - 1] > lv))) {
                a[y] = a[y - 1];
                l[y] = l[y - 1];
                y--;
            }
            a[y] = av;
            l[y] = lv;
        }
        g->uf[v] = v;
    }
    free(vals);
    free(keys);
    free(indmark);
    return JAOS_OK;
}

static void sg_free(sg *g)
{
    free(g->adj_start); free(g->adj); free(g->lab); free(g->color0);
    free(g->order); free(g->key); free(g->cells); free(g->inv);
    free(g->perm); free(g->uf); free(g->gen);
    memset(g, 0, sizeof *g);
}

void jm_symmetry_free(jm_symmetry *s)
{
    free(s->gen);
    free(s->orbit);
    memset(s, 0, sizeof *s);
}

jaos_status jm_symmetry_find(const jaos_model *m, int64_t work_cap,
                             jm_symmetry *out, int64_t *work)
{
    memset(out, 0, sizeof *out);
    out->nc = m->num_col;
    if (m->num_col == 0 || m->num_sos > 0)
        return JAOS_OK;
    sg g = {0};
    jaos_status st = sg_build(&g, m);
    if (st != JAOS_OK) {
        sg_free(&g);
        return st;
    }
    const int64_t n = g.n;
    g.budget = work_cap;
    int64_t *color = malloc((size_t)n * sizeof *color);
    int64_t *members = malloc((size_t)n * sizeof *members);
    int64_t *leaf1 = malloc((size_t)n * sizeof *leaf1);
    int64_t *inv1 = malloc((size_t)n * sizeof *inv1);
    int64_t *saved = nullptr, *cellv = nullptr, *cellstart = nullptr;
    int64_t *chosen = nullptr;
    st = JAOS_ERR_OUT_OF_MEMORY;
    if (color == nullptr || members == nullptr || leaf1 == nullptr ||
        inv1 == nullptr)
        goto out;
    memcpy(color, g.color0, (size_t)n * sizeof *color);
    if (!sg_refine(&g, color)) {
        st = JAOS_OK;
        goto out;
    }
    saved = malloc((size_t)(SYM_MAX_DEPTH * n) * sizeof *saved);
    cellv = malloc((size_t)(SYM_MAX_DEPTH * n) * sizeof *cellv);
    cellstart = jm_calloc_array(SYM_MAX_DEPTH + 1, sizeof *cellstart);
    chosen = malloc((size_t)SYM_MAX_DEPTH * sizeof *chosen);
    if (saved == nullptr || cellv == nullptr || cellstart == nullptr ||
        chosen == nullptr)
        goto out;
    int64_t depth = 0;
    bool stored = true;
    for (;;) {
        const int64_t mcount = sg_first_cell(&g, color, members);
        if (mcount == 0)
            break;
        if (depth < SYM_MAX_DEPTH && stored) {
            memcpy(saved + depth * n, color, (size_t)n * sizeof *color);
            memcpy(cellv + cellstart[depth], members,
                   (size_t)mcount * sizeof *members);
            cellstart[depth + 1] = cellstart[depth] + mcount;
            chosen[depth] = members[0];
            depth++;
        } else {
            stored = false;
        }
        sg_individualize(&g, color, members[0]);
        if (!sg_refine(&g, color)) {
            st = JAOS_OK;
            goto out;
        }
    }
    for (int64_t v = 0; v < n; v++) {
        leaf1[v] = color[v];
        inv1[color[v]] = v;
    }
    for (int64_t lev = depth - 1; lev >= 0 && g.budget > 0; lev--) {
        const int64_t m0 = cellstart[lev], m1 = cellstart[lev + 1];
        for (int64_t t = m0; t < m1 && g.budget > 0; t++) {
            const int64_t w = cellv[t];
            if (w == chosen[lev])
                continue;
            bool seen = false;
            for (int64_t u = m0; u < t && !seen; u++)
                seen = uf_find(g.uf, cellv[u]) == uf_find(g.uf, w);
            if (seen)
                continue;
            memcpy(color, saved + lev * n, (size_t)n * sizeof *color);
            sg_individualize(&g, color, w);
            int64_t leaves_left = SYM_LEAF_CAP;
            const int r = sg_find_under(&g, color, inv1, &leaves_left);
            if (r == 1 && !sg_add_generator(&g, g.perm))
                goto out;
            if (g.budget <= 0)
                break;
        }
    }
    st = JAOS_OK;
    if (g.ngen > 0) {
        const int64_t nc = g.nc;
        out->gen = malloc((size_t)(g.ngen * nc) * sizeof *out->gen);
        out->orbit = malloc((size_t)nc * sizeof *out->orbit);
        if (out->gen == nullptr || out->orbit == nullptr) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto out;
        }
        for (int64_t k = 0; k < g.ngen; k++)
            memcpy(out->gen + k * nc, g.gen + k * n, (size_t)nc * sizeof *out->gen);
        out->ngen = g.ngen;
        for (int64_t j = 0; j < nc; j++)
            g.cells[j] = 0;
        for (int64_t j = 0; j < nc; j++) {
            out->orbit[j] = uf_find(g.uf, j);
            g.cells[out->orbit[j]]++;
        }
        for (int64_t j = 0; j < nc; j++) {
            if (g.cells[j] > 1)
                out->norbit++;
            if (g.cells[j] > out->largest)
                out->largest = g.cells[j];
        }
    }
out:
    *work += work_cap - (g.budget > 0 ? g.budget : 0);
    free(color); free(members); free(leaf1); free(inv1);
    free(saved); free(cellv); free(cellstart); free(chosen);
    sg_free(&g);
    if (st != JAOS_OK)
        jm_symmetry_free(out);
    return st;
}
