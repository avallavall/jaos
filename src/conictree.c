/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"
#include "jaos_sys.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr double CT_INT_TOL = 1e-6;
constexpr double CT_PC_EPS = 1e-6;

typedef struct {
    double key;
    int64_t id, depth, nfix;
    int64_t *col;
    double *lo, *hi;
    int64_t bcol;
    int bdir;
    double bfrac;
} ct_node;

typedef struct {
    ct_node **v;
    int64_t n, cap;
} ct_heap;

static bool ct_before(const ct_node *a, const ct_node *b)
{
    return a->key != b->key ? a->key < b->key : a->id < b->id;
}

static void ct_node_free(ct_node *n)
{
    if (n == nullptr)
        return;
    free(n->col);
    free(n->lo);
    free(n->hi);
    free(n);
}

static bool ct_push(ct_heap *h, ct_node *x)
{
    if (!JM_GROW(h->v, h->cap, h->n + 1))
        return false;
    int64_t at = h->n++;
    h->v[at] = x;
    while (at > 0) {
        const int64_t up = (at - 1) / 2;
        if (!ct_before(h->v[at], h->v[up]))
            break;
        ct_node *t = h->v[at];
        h->v[at] = h->v[up];
        h->v[up] = t;
        at = up;
    }
    return true;
}

static ct_node *ct_pop(ct_heap *h)
{
    if (h->n == 0)
        return nullptr;
    ct_node *top = h->v[0];
    h->v[0] = h->v[--h->n];
    int64_t at = 0;
    for (;;) {
        int64_t best = at;
        const int64_t l = 2 * at + 1, r = l + 1;
        if (l < h->n && ct_before(h->v[l], h->v[best]))
            best = l;
        if (r < h->n && ct_before(h->v[r], h->v[best]))
            best = r;
        if (best == at)
            break;
        ct_node *t = h->v[at];
        h->v[at] = h->v[best];
        h->v[best] = t;
        at = best;
    }
    return top;
}

static ct_node *ct_child(const ct_node *p, int64_t id, int64_t n,
                         const int64_t *col, const double *lo,
                         const double *hi, double key)
{
    ct_node *c = calloc(1, sizeof *c);
    if (c == nullptr)
        return nullptr;
    const int64_t k = p->nfix;
    c->col = jm_alloc_array(k + n, sizeof *c->col);
    c->lo = jm_alloc_array(k + n, sizeof *c->lo);
    c->hi = jm_alloc_array(k + n, sizeof *c->hi);
    if (c->col == nullptr || c->lo == nullptr || c->hi == nullptr) {
        ct_node_free(c);
        return nullptr;
    }
    if (k > 0) {
        memcpy(c->col, p->col, (size_t)k * sizeof *c->col);
        memcpy(c->lo, p->lo, (size_t)k * sizeof *c->lo);
        memcpy(c->hi, p->hi, (size_t)k * sizeof *c->hi);
    }
    memcpy(c->col + k, col, (size_t)n * sizeof *c->col);
    memcpy(c->lo + k, lo, (size_t)n * sizeof *c->lo);
    memcpy(c->hi + k, hi, (size_t)n * sizeof *c->hi);
    c->nfix = k + n;
    c->key = key;
    c->id = id;
    c->depth = p->depth + 1;
    c->bcol = -1;
    return c;
}

static bool ct_semi(const jaos_model *m, int64_t j)
{
    return m->col_semi != nullptr && m->col_semi[j] && m->col_lower[j] > 0.0;
}

static bool ct_semi_broken(const jaos_model *m, int64_t j, double x)
{
    return ct_semi(m, j) && x > CT_INT_TOL && x < m->col_lower[j] - CT_INT_TOL;
}

static bool ct_disc(const jaos_model *m, int64_t j)
{
    return m->col_integer[j] || ct_semi(m, j);
}

static int64_t ct_sos_broken(const jaos_model *m, const double *x,
                             int64_t *first, int64_t *last)
{
    for (int64_t k = 0; k < m->num_sos; k++) {
        const int64_t b = m->sos_start[k], e = m->sos_start[k + 1];
        int64_t cnt = 0, lo = -1, hi = -1;
        for (int64_t t = b; t < e; t++) {
            if (fabs(x[m->sos_col[t]]) <= CT_INT_TOL)
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

static double ct_row_value(const jaos_model *m, const double *x, int64_t i)
{
    double act = 0.0, comp = 0.0;
    for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
        jm_obj_add(&act, &comp, m->ar_value[p] * x[m->ar_index[p]]);
    for (int64_t p = m->rq_start != nullptr ? m->rq_start[i] : 0;
         m->rq_start != nullptr && p < m->rq_start[i + 1]; p++) {
        const double v = m->rq_v[p] * x[m->rq_i[p]] * x[m->rq_j[p]];
        jm_obj_add(&act, &comp, m->rq_i[p] == m->rq_j[p] ? 0.5 * v : v);
    }
    const double a = act + comp;
    return a == 0.0 ? 0.0 : a;
}

static int64_t ct_ind_broken(const jaos_model *m, const jaos_model *rel,
                             const double *x)
{
    if (m->row_ind_col == nullptr)
        return -1;
    const double tol = jm_primal_tolerance(m);
    for (int64_t i = 0; i < m->num_row; i++) {
        const int64_t z = m->row_ind_col[i];
        if (z < 0 || rel->col_lower[z] == rel->col_upper[z] ||
            fabs(x[z] - (double)m->row_ind_val[i]) > 0.5)
            continue;
        const double act = ct_row_value(m, x, i);
        if (act < m->row_lower[i] - tol || act > m->row_upper[i] + tol)
            return i;
    }
    return -1;
}

static jaos_status ct_indicators(jaos_model *rel, const jaos_model *m)
{
    if (m->row_ind_col == nullptr)
        return JAOS_OK;
    for (int64_t i = 0; i < m->num_row; i++) {
        const int64_t z = m->row_ind_col[i];
        if (z < 0)
            continue;
        const bool on = rel->col_lower[z] == rel->col_upper[z] &&
                        rel->col_lower[z] == (double)m->row_ind_val[i];
        const double lo = on ? m->row_lower[i] : -INFINITY;
        const double hi = on ? m->row_upper[i] : INFINITY;
        if (rel->row_lower[i] == lo && rel->row_upper[i] == hi)
            continue;
        const jaos_status st = jaos_set_row_bounds(rel, i, lo, hi);
        if (st != JAOS_OK)
            return st;
    }
    return JAOS_OK;
}

static jaos_status ct_apply(jaos_model *rel, const jaos_model *m,
                            const double *ilo, const double *ihi,
                            const ct_node *n)
{
    for (int64_t k = 0; k < m->num_sos; k++)
        for (int64_t t = m->sos_start[k]; t < m->sos_start[k + 1]; t++) {
            const int64_t j = m->sos_col[t];
            if (ct_disc(m, j) || (rel->col_lower[j] == m->col_lower[j] &&
                                  rel->col_upper[j] == m->col_upper[j]))
                continue;
            const jaos_status st =
                jaos_set_col_bounds(rel, j, m->col_lower[j], m->col_upper[j]);
            if (st != JAOS_OK)
                return st;
        }
    for (int64_t j = 0; j < m->num_col; j++)
        if (ct_disc(m, j) &&
            (rel->col_lower[j] != ilo[j] || rel->col_upper[j] != ihi[j])) {
            const jaos_status st = jaos_set_col_bounds(rel, j, ilo[j], ihi[j]);
            if (st != JAOS_OK)
                return st;
        }
    for (int64_t k = 0; k < n->nfix; k++) {
        const jaos_status st =
            jaos_set_col_bounds(rel, n->col[k], n->lo[k], n->hi[k]);
        if (st != JAOS_OK)
            return st;
    }
    return ct_indicators(rel, m);
}

static int ct_split(ct_heap *h, const ct_node *cur, const jaos_model *rel,
                    const jaos_model *m, int64_t *next_id)
{
    int64_t best = -1;
    double width = 0.0;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!m->col_integer[j])
            continue;
        const double w = rel->col_upper[j] - rel->col_lower[j];
        if (w > width) {
            width = w;
            best = j;
        }
    }
    if (best < 0)
        return 0;
    const double lo = rel->col_lower[best], hi = rel->col_upper[best];
    const double mid = isfinite(lo) && isfinite(hi) ? floor(0.5 * (lo + hi))
                     : isfinite(lo)                 ? lo
                     : isfinite(hi)                 ? hi - 1.0
                                                    : 0.0;
    const double up_lo = mid + 1.0;
    ct_node *down = ct_child(cur, (*next_id)++, 1, &best, &lo, &mid, cur->key);
    ct_node *up = ct_child(cur, (*next_id)++, 1, &best, &up_lo, &hi, cur->key);
    if (down == nullptr || up == nullptr || !ct_push(h, down)) {
        ct_node_free(down);
        ct_node_free(up);
        return -1;
    }
    if (!ct_push(h, up)) {
        ct_node_free(up);
        return -1;
    }
    return 1;
}

static jaos_model *ct_relaxation(const jaos_model *m)
{
    jaos_model *rel = nullptr;
    if (jaos_model_copy(m, &rel) != JAOS_OK)
        return nullptr;
    free(rel->col_integer);
    rel->col_integer = nullptr;
    free(rel->col_semi);
    rel->col_semi = nullptr;
    rel->num_sos = 0;
    free(rel->row_ind_col);
    rel->row_ind_col = nullptr;
    free(rel->row_ind_val);
    rel->row_ind_val = nullptr;
    rel->cfg.log_cb = nullptr;
    rel->cfg.progress_cb = nullptr;
    rel->cfg.incumbent_cb = nullptr;
    return rel;
}

static void ct_budget_share(jaos_model *sub, const jaos_model *m, int64_t work,
                            int64_t share)
{
    if (m->cfg.work_limit <= 0)
        return;
    const int64_t left = (m->cfg.work_limit - work) / share;
    sub->cfg.work_limit = left > 0 ? left : 1;
}

static void ct_budget(jaos_model *sub, const jaos_model *m, int64_t work)
{
    ct_budget_share(sub, m, work, 1);
}

typedef struct {
    double *sum;
    int64_t *cnt;
} ct_pcost;

static int64_t ct_branch_col(const jaos_model *m, const double *x,
                             const ct_pcost *pc)
{
    const int64_t nc = m->num_col;
    double avg[2] = {1.0, 1.0};
    for (int d = 0; d < 2; d++) {
        double s = 0.0;
        int64_t n = 0;
        for (int64_t j = 0; j < nc; j++)
            if (pc->cnt[d * nc + j] > 0) {
                s += pc->sum[d * nc + j] / (double)pc->cnt[d * nc + j];
                n++;
            }
        if (n > 0 && s > 0.0)
            avg[d] = s / (double)n;
    }
    const bool pcost = m->cfg.mip_branching != JAOS_BRANCH_MOST_FRACTIONAL;
    int64_t best = -1;
    double score = -1.0;
    for (int64_t j = 0; j < nc; j++) {
        double f = m->col_integer[j] ? x[j] - floor(x[j]) : 0.0;
        if (f <= CT_INT_TOL || f >= 1.0 - CT_INT_TOL) {
            if (!ct_semi_broken(m, j, x[j]))
                continue;
            f = x[j] / m->col_lower[j];
        }
        double s = f < 1.0 - f ? f : 1.0 - f;
        if (pcost) {
            double est[2];
            for (int d = 0; d < 2; d++)
                est[d] = pc->cnt[d * nc + j] > 0
                             ? pc->sum[d * nc + j] /
                                   (double)pc->cnt[d * nc + j]
                             : avg[d];
            s = fmax(f * est[0], CT_PC_EPS) *
                fmax((1.0 - f) * est[1], CT_PC_EPS);
        }
        if (s > score) {
            score = s;
            best = j;
        }
    }
    return best;
}

static void ct_learn(ct_pcost *pc, int64_t nc, const ct_node *n, double key)
{
    if (n->bcol < 0 || !(n->bfrac > 0.0) || !isfinite(key) ||
        !isfinite(n->key))
        return;
    const double gain = (key - n->key) / n->bfrac;
    const int64_t k = (int64_t)n->bdir * nc + n->bcol;
    pc->sum[k] += gain > 0.0 ? gain : 0.0;
    pc->cnt[k]++;
}

static jaos_status ct_fixed(const jaos_model *m, const double *x, int64_t work,
                            jaos_model **out)
{
    *out = nullptr;
    jaos_model *fin = ct_relaxation(m);
    if (fin == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    jaos_status st = JAOS_OK;
    for (int64_t k = 0; st == JAOS_OK && k < m->num_sos; k++)
        for (int64_t t = m->sos_start[k];
             st == JAOS_OK && t < m->sos_start[k + 1]; t++)
            if (fabs(x[m->sos_col[t]]) <= CT_INT_TOL)
                st = jaos_set_col_bounds(fin, m->sos_col[t], 0.0, 0.0);
    for (int64_t j = 0; st == JAOS_OK && j < m->num_col; j++) {
        if (ct_semi(m, j) && x[j] < 0.5 * m->col_lower[j]) {
            st = jaos_set_col_bounds(fin, j, 0.0, 0.0);
            continue;
        }
        if (!m->col_integer[j])
            continue;
        double v = jm_round(x[j]);
        if (v < m->col_lower[j])
            v = ceil(m->col_lower[j]);
        if (v > m->col_upper[j])
            v = floor(m->col_upper[j]);
        st = jaos_set_col_bounds(fin, j, v, v);
    }
    if (st == JAOS_OK)
        st = ct_indicators(fin, m);
    if (st != JAOS_OK) {
        jaos_model_free(fin);
        return st;
    }
    ct_budget(fin, m, work);
    st = jaos_solve(fin);
    if (st != JAOS_OK) {
        jaos_model_free(fin);
        return st;
    }
    *out = fin;
    return JAOS_OK;
}

static bool ct_answer(const jaos_model *m, jaos_model *fin, double *obj)
{
    return fin != nullptr && jaos_status_of(fin) == JAOS_SOLVE_OPTIMAL &&
           jaos_objective(fin, obj) == JAOS_OK &&
           ct_sos_broken(m, fin->sol_col, nullptr, nullptr) < 0;
}

typedef struct {
    double dist;
    int64_t col;
} ct_frac;

static int ct_frac_cmp(const void *a, const void *b)
{
    const ct_frac *p = a, *q = b;
    if (p->dist != q->dist)
        return p->dist < q->dist ? -1 : 1;
    return p->col < q->col ? -1 : p->col > q->col;
}

static jaos_status ct_dive(jaos_model *m, const double *ilo, const double *ihi,
                           int64_t steps, int64_t *work, int64_t *iters,
                           int64_t *solves, double *x, bool *found)
{
    *found = false;
    const int64_t nc = m->num_col;
    jaos_model *d = ct_relaxation(m);
    ct_frac *fr = jm_alloc_array(nc > 0 ? nc : 1, sizeof *fr);
    jaos_status st = d != nullptr && fr != nullptr ? JAOS_OK
                                                   : JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t j = 0; st == JAOS_OK && j < nc; j++)
        if (ct_disc(m, j))
            st = jaos_set_col_bounds(d, j, ilo[j], ihi[j]);
    if (st == JAOS_OK)
        d->cfg.node_solve = true;
    int64_t done = 0, fixed = 0;
    jaos_solve_status last = JAOS_SOLVE_OPTIMAL;
    for (int64_t step = 0; st == JAOS_OK; step++) {
        int64_t nf = 0;
        for (int64_t j = 0; j < nc; j++) {
            if (!m->col_integer[j] || d->col_lower[j] == d->col_upper[j])
                continue;
            const double f = x[j] - floor(x[j]);
            const double dist = f < 1.0 - f ? f : 1.0 - f;
            if (dist > CT_INT_TOL)
                fr[nf++] = (ct_frac){dist, j};
        }
        if (nf == 0 || step == steps) {
            *found = true;
            break;
        }
        qsort(fr, (size_t)nf, sizeof *fr, ct_frac_cmp);
        for (int64_t k = 0; k < (nf + 1) / 2 && st == JAOS_OK; k++) {
            const int64_t j = fr[k].col;
            double v = jm_round(x[j]);
            if (v < d->col_lower[j])
                v = ceil(d->col_lower[j]);
            if (v > d->col_upper[j])
                v = floor(d->col_upper[j]);
            st = jaos_set_col_bounds(d, j, v, v);
            fixed++;
        }
        if (st == JAOS_OK)
            st = ct_indicators(d, m);
        ct_budget(d, m, *work);
        if (st == JAOS_OK)
            st = jaos_solve(d);
        if (st != JAOS_OK)
            break;
        done++;
        (*solves)++;
        *work += jaos_work_units(d);
        *iters += jaos_iterations(d);
        last = jaos_status_of(d);
        if (last != JAOS_SOLVE_OPTIMAL)
            break;
        st = jaos_solution(d, x, nullptr, nullptr, nullptr);
    }
    if (st == JAOS_OK && *found)
        jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: the dive fixed "
               "%lld integer columns in %lld solves and rounds its last "
               "point", (long long)fixed, (long long)done);
    else if (st == JAOS_OK)
        jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: the dive fixed "
               "%lld integer columns in %lld solves, the last of which ends "
               "%s", (long long)fixed, (long long)done,
               jaos_solve_status_str(last));
    free(fr);
    jaos_model_free(d);
    return st;
}

typedef struct {
    jaos_model *model;
    double key, obj, sigma;
    int64_t first, heur;
} ct_best;

static bool ct_closed(const ct_best *b, double key, double gap,
                      double shift)
{
    return b->key < INFINITY && b->key - key <= gap * (shift + fabs(b->key));
}

static int ct_offer(jaos_model *m, ct_best *b, jaos_model *fin, double fobj,
                    int64_t node, double bound, bool heuristic,
                    ct_node **sv, int64_t *sn, ct_heap *heap, double *xc)
{
    if (b->sigma * fobj >= b->key) {
        jaos_model_free(fin);
        return 0;
    }
    jaos_model_free(b->model);
    b->model = fin;
    b->key = b->sigma * fobj;
    b->obj = fobj;
    if (b->first == 0)
        b->first = node;
    b->heur += heuristic;
    while (*sn > 0) {
        ct_node *held = sv[--*sn];
        if (!ct_push(heap, held)) {
            ct_node_free(held);
            return -1;
        }
    }
    jm_log(m, JAOS_LOG_PROGRESS, "node %lld: %s at %.12g", (long long)node,
           heuristic ? "a rounding is an incumbent" : "an incumbent", fobj);
    if (m->cfg.incumbent_cb == nullptr)
        return 0;
    const int64_t nc = m->num_col;
    for (int64_t t = 0; t < nc; t++)
        xc[t] = m->col_integer[t] ? jm_round(fin->sol_col[t]) : fin->sol_col[t];
    const jaos_incumbent ev = {
        .node = node,
        .objective = fobj,
        .bound = bound,
        .col_value = xc,
        .num_col = nc,
        .by_rounding = heuristic,
    };
    return m->cfg.incumbent_cb(&ev, m->cfg.incumbent_user) ==
                   JAOS_CALLBACK_STOP ? 1 : 0;
}

static jaos_status ct_feasible(const jaos_model *m, const jaos_model *at,
                               const double *ilo, const double *ihi,
                               int64_t work, jaos_model **out)
{
    *out = nullptr;
    jaos_model *f = nullptr;
    if (jaos_model_copy(m, &f) != JAOS_OK)
        return JAOS_ERR_OUT_OF_MEMORY;
    f->cfg.log_cb = nullptr;
    f->cfg.progress_cb = nullptr;
    f->cfg.incumbent_cb = nullptr;
    jaos_status st = jaos_set_quadratic(f, 0, nullptr, nullptr, nullptr);
    for (int64_t j = 0; j < m->num_col && st == JAOS_OK; j++) {
        st = jaos_set_col_cost(f, j, 0.0);
        if (st == JAOS_OK)
            st = jaos_set_col_quadratic(f, j, 0.0);
    }
    if (st == JAOS_OK)
        st = jaos_set_objective_offset(f, 0.0);
    if (st == JAOS_OK && at != nullptr)
        st = jm_piece_bounds(f, m, at->col_lower, at->col_upper, ilo, ihi);
    ct_budget(f, m, work);
    if (st == JAOS_OK)
        st = jaos_solve(f);
    if (st != JAOS_OK) {
        jaos_model_free(f);
        return st;
    }
    *out = f;
    return JAOS_OK;
}

static void ct_activities(jaos_model *m)
{
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row[i] = ct_row_value(m, m->sol_col, i);
}

static jaos_status ct_publish(jaos_model *m, const jaos_model *best)
{
    jaos_status st = jm_model_ensure_solution_arrays(m);
    if (st != JAOS_OK)
        return st;
    const int64_t nc = m->num_col, nr = m->num_row;
    const int64_t members = m->num_cone > 0 ? m->cone_start[m->num_cone] : 0;
    if (members > 0 && m->sol_cone == nullptr) {
        m->sol_cone = jm_calloc_array(members, sizeof *m->sol_cone);
        if (m->sol_cone == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (nc > 0) {
        memcpy(m->sol_col, best->sol_col, (size_t)nc * sizeof *m->sol_col);
        memcpy(m->sol_redcost, best->sol_redcost,
               (size_t)nc * sizeof *m->sol_redcost);
        memcpy(m->sol_col_status, best->sol_col_status,
               (size_t)nc * sizeof *m->sol_col_status);
    }
    if (nr > 0) {
        memcpy(m->sol_dual, best->sol_dual, (size_t)nr * sizeof *m->sol_dual);
        memcpy(m->sol_row_status, best->sol_row_status,
               (size_t)nr * sizeof *m->sol_row_status);
    }
    if (members > 0)
        memcpy(m->sol_cone, best->sol_cone, (size_t)members * sizeof *m->sol_cone);
    for (int64_t j = 0; j < nc; j++)
        if (m->col_integer[j])
            m->sol_col[j] = jm_round(m->sol_col[j]);
    st = jm_model_ensure_rowwise(m);
    if (st != JAOS_OK)
        return st;
    ct_activities(m);
    m->cone_ok = members > 0 && best->cone_ok;
    m->sol_basis_ok = false;
    jm_model_publish_objective(m);
    return JAOS_OK;
}

constexpr int64_t CT_BATCH_MAX = 64;

typedef struct {
    jaos_model **rels;
    jaos_status *st;
    int64_t nb, stride, first;
} ct_crew;

static void ct_crew_run(void *arg)
{
    const ct_crew *c = arg;
    for (int64_t b = c->first; b < c->nb; b += c->stride)
        c->st[b] = jaos_solve(c->rels[b]);
}

static void ct_solve_batch(jaos_model **rels, jaos_status *st, int64_t nb,
                           int64_t threads)
{
    const int64_t w = threads < nb ? threads : nb;
    ct_crew crew[CT_BATCH_MAX];
    jm_thread th[CT_BATCH_MAX];
    for (int64_t k = 0; k < w; k++)
        crew[k] = (ct_crew){rels, st, nb, w, k};
    for (int64_t k = 1; k < w; k++)
        if (!jm_thread_start(&th[k], ct_crew_run, &crew[k]))
            ct_crew_run(&crew[k]);
    ct_crew_run(&crew[0]);
    for (int64_t k = 1; k < w; k++)
        jm_thread_join(&th[k]);
}

jaos_status jm_conic_branch_and_bound(jaos_model *m)
{
    const double t0 = jm_monotonic_seconds();
    m->mip_started = t0;
    const int64_t nc = m->num_col;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double gap = m->cfg.mip_gap_set ? m->cfg.mip_gap
                                          : jm_mip_default(JM_DEF_GAP);
    const double gap_shift = m->cfg.mip_gap_rule == JAOS_GAP_RELATIVE ? 0.0
                                                                      : 1.0;
    const int64_t dive = m->cfg.mip_dive_heuristic_set
                             ? m->cfg.mip_dive_heuristic
                             : (int64_t)jm_mip_default(JM_DEF_DIVE_HEURISTIC);

    if (m->cfg.node_cb != nullptr) {
        jm_set_err(m, "a node callback runs in the tree of linear and "
                      "quadratic models; a model with cones or quadratic rows "
                      "is solved by its own tree, which does not call it");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (m->col_integer == nullptr) {
        m->col_integer = jm_calloc_array(nc > 0 ? nc : 1, sizeof(bool));
        if (m->col_integer == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (m->row_ind_col != nullptr && jm_model_ensure_rowwise(m) != JAOS_OK)
        return JAOS_ERR_OUT_OF_MEMORY;

    free(m->mip_inc_x);
    m->mip_inc_x = nullptr;
    free(m->mip_pool_x);
    m->mip_pool_x = nullptr;
    free(m->mip_pool_obj);
    m->mip_pool_obj = nullptr;
    m->mip_pool_n = 0;
    m->mip_has_incumbent = false;
    m->mip_start_taken = false;
    m->sol_basis_ok = false;
    m->farkas_ok = false;
    m->ray_ok = false;
    m->cone_ok = false;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    const int64_t per_round = m->cfg.mip_tree_batch < 1 ? 1
                        : m->cfg.mip_tree_batch > CT_BATCH_MAX
                              ? CT_BATCH_MAX : m->cfg.mip_tree_batch;
    jaos_model *rels[CT_BATCH_MAX] = {0};
    ct_node *batch[CT_BATCH_MAX] = {0};
    jaos_status bst[CT_BATCH_MAX] = {0};
    rels[0] = ct_relaxation(m);
    double *ilo = jm_alloc_array(nc > 0 ? nc : 1, sizeof *ilo);
    double *ihi = jm_alloc_array(nc > 0 ? nc : 1, sizeof *ihi);
    double *x = jm_alloc_array(nc > 0 ? nc : 1, sizeof *x);
    double *xc = jm_alloc_array(nc > 0 ? nc : 1, sizeof *xc);
    double *xd = jm_alloc_array(nc > 0 ? nc : 1, sizeof *xd);
    int64_t *fcol = jm_alloc_array(2 * nc + 2, sizeof *fcol);
    double *flo = jm_alloc_array(2 * nc + 2, sizeof *flo);
    double *fhi = jm_alloc_array(2 * nc + 2, sizeof *fhi);
    ct_pcost pc = {jm_calloc_array(2 * nc + 1, sizeof *pc.sum),
                   jm_calloc_array(2 * nc + 1, sizeof *pc.cnt)};
    ct_heap heap = {0};
    ct_node *cur = calloc(1, sizeof *cur), *next = nullptr, **sv = nullptr;
    int64_t sn = 0, scap = 0;
    jaos_solve_status outcome = JAOS_SOLVE_NOT_RUN;
    int64_t work = 0, iters = 0, nodes = 0, solves = 0, next_id = 1;
    int64_t roughs = 0, splits = 0, parked = 0, open_splits = 0;
    double parked_key = INFINITY, stopped_key = INFINITY;
    char why[sizeof m->err] = "";
    ct_best inc = {
        .key = m->cfg.mip_cutoff_set ? sigma * m->cfg.mip_cutoff : INFINITY,
        .sigma = sigma,
    };
    double best_bound = -INFINITY;
    if (rels[0] == nullptr || ilo == nullptr || ihi == nullptr ||
        x == nullptr || xc == nullptr || xd == nullptr || cur == nullptr ||
        fcol == nullptr || flo == nullptr || fhi == nullptr ||
        pc.sum == nullptr || pc.cnt == nullptr)
        goto done;
    rels[0]->cfg.node_solve = true;
    cur->key = -INFINITY;
    cur->bcol = -1;

    int64_t nint = 0, nsemi = 0, nind = 0;
    for (int64_t j = 0; j < nc; j++) {
        ilo[j] = m->col_lower[j];
        ihi[j] = m->col_upper[j];
        if (m->col_integer[j]) {
            nint++;
            ilo[j] = ceil(m->col_lower[j] - CT_INT_TOL);
            ihi[j] = floor(m->col_upper[j] + CT_INT_TOL);
        }
        if (ct_semi(m, j)) {
            nsemi++;
            ilo[j] = 0.0;
        } else if (m->col_integer[j] && ilo[j] > ihi[j]) {
            outcome = JAOS_SOLVE_INFEASIBLE;
        }
    }
    for (int64_t i = 0; m->row_ind_col != nullptr && i < m->num_row; i++)
        nind += m->row_ind_col[i] >= 0;
    jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: %lld integer "
           "columns of %lld, depth first until an incumbent and best bound "
           "first with a plunge after it, the most fractional column "
           "branched, %lld open nodes to a round on up to %lld threads; at "
           "the root, %s and a dive of up to %lld solves while no incumbent "
           "is known",
           (long long)nint, (long long)nc, (long long)per_round,
           (long long)jaos_threads_of(m),
           m->cfg.mip_no_heuristics ? "no rounding" : "the relaxation rounded",
           (long long)dive);
    if (nsemi > 0 || m->num_sos > 0 || nind > 0)
        jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: %lld "
               "semi-continuous columns branched on beside the integer "
               "columns, and %lld SOS sets and %lld indicator rows branched on "
               "once those hold",
               (long long)nsemi, (long long)m->num_sos, (long long)nind);

    if (m->mip_start != nullptr && outcome == JAOS_SOLVE_NOT_RUN) {
        bool open_int = false;
        for (int64_t j = 0; j < nc; j++)
            open_int = open_int ||
                       (m->col_integer[j] && isnan(m->mip_start[j]));
        double *filled = nullptr;
        int got = 1;
        if (open_int) {
            filled = jm_alloc_array(nc > 0 ? nc : 1, sizeof *filled);
            if (filled == nullptr)
                goto done;
            int64_t spent = 0;
            got = jm_mip_start_complete(m, work, filled, &spent);
            work += spent;
            if (got < 0) {
                free(filled);
                goto done;
            }
            jm_log(m, JAOS_LOG_SUMMARY, "the caller's partial start %s",
                   got == 1 ? "was completed by a tree over the columns it "
                              "leaves open"
                            : "could not be completed");
        }
        jaos_model *fin = nullptr;
        if (got == 1) {
            rc = ct_fixed(m, filled != nullptr ? filled : m->mip_start, work,
                          &fin);
            free(filled);
            if (rc != JAOS_OK)
                goto done;
            rc = JAOS_ERR_OUT_OF_MEMORY;
            solves++;
            work += jaos_work_units(fin);
            iters += jaos_iterations(fin);
        } else {
            free(filled);
        }
        double obj = 0.0;
        if (ct_answer(m, fin, &obj) && sigma * obj < inc.key) {
            jaos_model_free(inc.model);
            inc.model = fin;
            fin = nullptr;
            inc.key = sigma * obj;
            inc.obj = obj;
            m->mip_start_taken = true;
            jm_log(m, JAOS_LOG_PROGRESS, "the start is an incumbent at %.12g",
                   obj);
        }
        jaos_model_free(fin);
    }

    while (outcome == JAOS_SOLVE_NOT_RUN) {
        if (nodes > 0) {
            cur = next;
            next = nullptr;
            if (cur != nullptr && ct_closed(&inc, cur->key, gap, gap_shift)) {
                ct_node_free(cur);
                cur = nullptr;
            }
            while (cur == nullptr && sn > 0) {
                cur = sv[--sn];
                if (ct_closed(&inc, cur->key, gap, gap_shift)) {
                    ct_node_free(cur);
                    cur = nullptr;
                }
            }
            if (cur == nullptr) {
                while ((cur = ct_pop(&heap)) != nullptr) {
                    if (!ct_closed(&inc, cur->key, gap, gap_shift))
                        break;
                    ct_node_free(cur);
                }
                if (cur == nullptr) {
                    outcome = inc.model != nullptr ? JAOS_SOLVE_OPTIMAL
                                                   : JAOS_SOLVE_INFEASIBLE;
                    break;
                }
                best_bound = cur->key;
            }
        }
        if (m->cfg.work_limit > 0 && work >= m->cfg.work_limit) {
            outcome = JAOS_SOLVE_WORK_LIMIT;
            break;
        }
        if (m->cfg.time_limit > 0.0 &&
            jm_monotonic_seconds() - t0 >= m->cfg.time_limit) {
            outcome = JAOS_SOLVE_TIME_LIMIT;
            break;
        }
        if (m->cfg.mip_node_limit > 0 && nodes >= m->cfg.mip_node_limit) {
            outcome = JAOS_SOLVE_NODE_LIMIT;
            break;
        }

        int64_t nb = 0;
        batch[nb++] = cur;
        cur = nullptr;
        while (nb < per_round && (m->cfg.mip_node_limit <= 0 ||
                                 nodes + nb < m->cfg.mip_node_limit)) {
            ct_node *c = nullptr;
            while (c == nullptr && sn > 0) {
                c = sv[--sn];
                if (ct_closed(&inc, c->key, gap, gap_shift)) {
                    ct_node_free(c);
                    c = nullptr;
                }
            }
            while (c == nullptr && (c = ct_pop(&heap)) != nullptr)
                if (ct_closed(&inc, c->key, gap, gap_shift)) {
                    ct_node_free(c);
                    c = nullptr;
                }
            if (c == nullptr)
                break;
            batch[nb++] = c;
        }
        for (int64_t b = 0; b < nb; b++) {
            if (rels[b] == nullptr) {
                rels[b] = ct_relaxation(m);
                if (rels[b] == nullptr)
                    goto done;
                rels[b]->cfg.node_solve = true;
            }
            const jaos_status st = ct_apply(rels[b], m, ilo, ihi, batch[b]);
            if (st != JAOS_OK) {
                rc = st;
                goto done;
            }
            ct_budget_share(rels[b], m, work, nb);
        }
        ct_solve_batch(rels, bst, nb, jaos_threads_of(m));
        for (int64_t b = 0; b < nb; b++) {
            solves++;
            work += jaos_work_units(rels[b]);
            iters += jaos_iterations(rels[b]);
        }

        int64_t at = 0;
        for (; at < nb && outcome == JAOS_SOLVE_NOT_RUN; at++) {
            const int64_t b = at;
            cur = batch[b];
            jaos_model *rel = rels[b];
            nodes++;
            jaos_status st = bst[b];
            if (st != JAOS_OK) {
                jm_set_err(m, "node %lld: %s", (long long)nodes,
                           jaos_model_error(rel));
                rc = st;
                goto done;
            }
            const jaos_solve_status ns = jaos_status_of(rel);
            if (ns == JAOS_SOLVE_INFEASIBLE) {
                if (nodes == 1 && rel->farkas_ok) {
                    st = jm_model_ensure_solution_arrays(m);
                    if (st != JAOS_OK) {
                        rc = st;
                        goto done;
                    }
                    const int64_t members =
                        m->num_cone > 0 ? m->cone_start[m->num_cone] : 0;
                    if (members > 0 && m->sol_cone == nullptr &&
                        (m->sol_cone = jm_calloc_array(members,
                                                       sizeof *m->sol_cone)) ==
                            nullptr)
                        goto done;
                    memcpy(m->sol_farkas, rel->sol_farkas,
                           (size_t)m->num_row * sizeof *m->sol_farkas);
                    if (members > 0)
                        memcpy(m->sol_cone, rel->sol_cone,
                               (size_t)members * sizeof *m->sol_cone);
                    m->farkas_ok = true;
                    m->cone_ok = members > 0 && rel->cone_ok;
                }
                continue;
            }
            if (ns == JAOS_SOLVE_UNBOUNDED) {
                const bool pieces = m->num_sos > 0 || nind > 0;
                int64_t nd = 0, nu = 0;
                if (pieces &&
                    jm_open_piece(m, rel->col_lower, rel->col_upper, fcol, flo,
                                  fhi, &nd, fcol + nc + 1, flo + nc + 1,
                                  fhi + nc + 1, &nu)) {
                    for (int side = 0; side < 2; side++) {
                        const int64_t n = side == 0 ? nd : nu;
                        const int64_t *col = fcol + (side == 0 ? 0 : nc + 1);
                        const double *lo = flo + (side == 0 ? 0 : nc + 1);
                        const double *hi = fhi + (side == 0 ? 0 : nc + 1);
                        bool crossed = false;
                        for (int64_t k = 0; k < n; k++)
                            crossed = crossed || lo[k] > hi[k];
                        if (crossed)
                            continue;
                        ct_node *c = ct_child(cur, next_id++, n, col, lo, hi,
                                              cur->key);
                        if (c == nullptr || !ct_push(&heap, c)) {
                            ct_node_free(c);
                            goto done;
                        }
                    }
                    open_splits++;
                    continue;
                }
                jaos_model *f = nullptr;
                st = ct_feasible(m, pieces ? rel : nullptr, ilo, ihi, work, &f);
                if (st != JAOS_OK) {
                    rc = st;
                    goto done;
                }
                work += jaos_work_units(f);
                iters += jaos_iterations(f);
                solves += f->mip_solves;
                const jaos_solve_status fs = jaos_status_of(f);
                jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: the "
                       "relaxation is unbounded, and the search for an integer "
                       "point%s ends %s",
                       pieces ? " in the node's box, where every SOS set and "
                                "indicator row is settled," : "",
                       jaos_solve_status_str(fs));
                if (pieces && fs == JAOS_SOLVE_INFEASIBLE) {
                    jaos_model_free(f);
                    continue;
                }
                outcome = fs == JAOS_SOLVE_OPTIMAL ? JAOS_SOLVE_UNBOUNDED : fs;
                if (fs == JAOS_SOLVE_NUMERICAL_ERROR)
                    jm_set_err(m, "the relaxation is unbounded, and the search "
                                  "for an integer point ends as a numerical "
                                  "error: %s", jaos_model_error(f));
                jaos_model_free(f);
                break;
            }
            if (ns == JAOS_SOLVE_NUMERICAL_ERROR) {
                const int split = ct_split(&heap, cur, rel, m, &next_id);
                if (split < 0)
                    goto done;
                if (split > 0) {
                    splits++;
                    continue;
                }
                if (parked++ == 0)
                    snprintf(why, sizeof why, "node %lld: %s", (long long)nodes,
                             jaos_model_error(rel));
                if (cur->key < parked_key)
                    parked_key = cur->key;
                continue;
            }
            if (ns != JAOS_SOLVE_OPTIMAL) {
                outcome = ns;
                break;
            }

            double obj = 0.0;
            if (jaos_objective(rel, &obj) != JAOS_OK ||
                jaos_solution(rel, x, nullptr, nullptr, nullptr) != JAOS_OK)
                goto done;
            roughs += rel->conic_rough;
            const double key = rel->conic_rough ? cur->key : sigma * obj;
            if (!rel->conic_rough)
                ct_learn(&pc, nc, cur, key);
            if (nodes == 1)
                best_bound = key;
            if (m->cfg.progress_cb != nullptr) {
                const double lb = inc.model != nullptr && inc.key < best_bound
                                      ? inc.key : best_bound;
                const jaos_progress ev = {
                    .iterations = iters,
                    .work_units = work,
                    .nodes = nodes,
                    .bound = sigma * lb,
                    .has_incumbent = inc.model != nullptr,
                    .incumbent = inc.model != nullptr ? inc.obj : 0.0,
                };
                if (m->cfg.progress_cb(&ev, m->cfg.progress_user) ==
                    JAOS_CALLBACK_STOP) {
                    outcome = JAOS_SOLVE_INTERRUPTED;
                    break;
                }
            }
            if (ct_closed(&inc, key, gap, gap_shift))
                continue;

            const int64_t j = ct_branch_col(m, x, &pc);
            int64_t sfirst = -1, slast = -1;
            const int64_t sos =
                j < 0 ? ct_sos_broken(m, x, &sfirst, &slast) : -1;
            const int64_t indrow =
                j < 0 && sos < 0 ? ct_ind_broken(m, rel, x) : -1;
            const bool leaf = j < 0 && sos < 0 && indrow < 0;
            const bool rounding = !leaf && nodes == 1 &&
                                  !m->cfg.mip_no_heuristics;
            if (leaf || rounding) {
                jaos_model *fin = nullptr;
                st = ct_fixed(m, x, work, &fin);
                if (st != JAOS_OK) {
                    rc = st;
                    goto done;
                }
                solves++;
                work += jaos_work_units(fin);
                iters += jaos_iterations(fin);
                double fobj = 0.0;
                const bool solved = ct_answer(m, fin, &fobj);
                if (!solved && leaf) {
                    const int split = ct_split(&heap, cur, rel, m, &next_id);
                    if (split != 0) {
                        jaos_model_free(fin);
                        if (split < 0)
                            goto done;
                        splits++;
                        continue;
                    }
                    if (parked++ == 0)
                        snprintf(why, sizeof why, "node %lld: the relaxation "
                                 "is integral, and with its integer columns "
                                 "fixed the model ends %s: %s", (long long)nodes,
                                 jaos_solve_status_str(jaos_status_of(fin)),
                                 jaos_model_error(fin));
                    if (key < parked_key)
                        parked_key = key;
                    jaos_model_free(fin);
                    continue;
                }
                int r = 0;
                if (solved)
                    r = ct_offer(m, &inc, fin, fobj, nodes, sigma * best_bound,
                                 rounding, sv, &sn, &heap, xc);
                else
                    jaos_model_free(fin);
                if (r < 0)
                    goto done;
                if (r > 0) {
                    outcome = JAOS_SOLVE_INTERRUPTED;
                    break;
                }
                if (leaf)
                    continue;
            }
            if (nodes == 1 && inc.model == nullptr && dive > 0) {
                memcpy(xd, x, (size_t)nc * sizeof *xd);
                bool found = false;
                st = ct_dive(m, ilo, ihi, dive, &work, &iters, &solves, xd,
                             &found);
                if (st != JAOS_OK) {
                    rc = st;
                    goto done;
                }
                jaos_model *fin = nullptr;
                if (found) {
                    st = ct_fixed(m, xd, work, &fin);
                    if (st != JAOS_OK) {
                        rc = st;
                        goto done;
                    }
                    solves++;
                    work += jaos_work_units(fin);
                    iters += jaos_iterations(fin);
                }
                double fobj = 0.0;
                int r = 0;
                if (ct_answer(m, fin, &fobj))
                    r = ct_offer(m, &inc, fin, fobj, nodes, sigma * best_bound,
                                 true, sv, &sn, &heap, xc);
                else
                    jaos_model_free(fin);
                if (r < 0)
                    goto done;
                if (r > 0) {
                    outcome = JAOS_SOLVE_INTERRUPTED;
                    break;
                }
            }
            if (ct_closed(&inc, key, gap, gap_shift))
                continue;

            int64_t *const ucol = fcol + nc + 1;
            double *const ulo = flo + nc + 1, *const uhi = fhi + nc + 1;
            int64_t nd = 1, nu = 1;
            double f = 0.5, fu = 0.5;
            if (j >= 0) {
                const double v = x[j];
                const double g = v - floor(v);
                const bool sc = ct_semi_broken(m, j, v) &&
                                !(m->col_integer[j] && g > CT_INT_TOL &&
                                  g < 1.0 - CT_INT_TOL);
                f = sc ? v / m->col_lower[j] : g;
                fu = sc ? 1.0 - f : ceil(v) - v;
                fcol[0] = ucol[0] = j;
                flo[0] = rel->col_lower[j];
                fhi[0] = sc ? 0.0 : floor(v);
                ulo[0] = !sc ? ceil(v)
                       : m->col_integer[j] ? ceil(m->col_lower[j])
                                           : m->col_lower[j];
                uhi[0] = rel->col_upper[j];
            } else if (indrow >= 0) {
                const int64_t z = m->row_ind_col[indrow];
                const double val = (double)m->row_ind_val[indrow];
                const double cut = rel->col_lower[z] < val ? val - 1.0 : val;
                fcol[0] = ucol[0] = z;
                flo[0] = rel->col_lower[z];
                fhi[0] = cut;
                ulo[0] = cut + 1.0;
                uhi[0] = rel->col_upper[z];
            } else {
                const int64_t b = m->sos_start[sos];
                const int64_t n = m->sos_start[sos + 1] - b;
                double wsum = 0.0, wpos = 0.0;
                for (int64_t t = 0; t < n; t++) {
                    const double a = fabs(x[m->sos_col[b + t]]);
                    wsum += a;
                    wpos += a * (double)t;
                }
                int64_t r = (int64_t)floor(wpos / wsum);
                const int64_t rlo = m->sos_type[sos] == 1 ? sfirst : sfirst + 1;
                if (r < rlo)
                    r = rlo;
                if (r > slast - 1)
                    r = slast - 1;
                nd = nu = 0;
                for (int64_t t = 0; t < n; t++) {
                    const int64_t c = m->sos_col[b + t];
                    const double zlo = fmax(0.0, rel->col_lower[c]);
                    const double zhi = fmin(0.0, rel->col_upper[c]);
                    if (t > r) {
                        fcol[nd] = c;
                        flo[nd] = zlo;
                        fhi[nd] = zhi;
                        nd++;
                    }
                    if (m->sos_type[sos] == 1 ? t <= r : t < r) {
                        ucol[nu] = c;
                        ulo[nu] = zlo;
                        uhi[nu] = zhi;
                        nu++;
                    }
                }
            }
            bool down_ok = true, up_ok = true;
            for (int64_t k = 0; k < nd; k++)
                down_ok = down_ok && flo[k] <= fhi[k];
            for (int64_t k = 0; k < nu; k++)
                up_ok = up_ok && ulo[k] <= uhi[k];
            ct_node *down = down_ok ? ct_child(cur, next_id++, nd, fcol, flo,
                                               fhi, key)
                                    : nullptr;
            ct_node *up = up_ok ? ct_child(cur, next_id++, nu, ucol, ulo, uhi,
                                           key)
                                : nullptr;
            if ((down_ok && down == nullptr) || (up_ok && up == nullptr)) {
                ct_node_free(down);
                ct_node_free(up);
                goto done;
            }
            if (j >= 0 && down != nullptr) {
                down->bcol = j;
                down->bdir = 0;
                down->bfrac = f;
            }
            if (j >= 0 && up != nullptr) {
                up->bcol = j;
                up->bdir = 1;
                up->bfrac = fu;
            }
            const bool down_first = f < 0.5;
            ct_node *first = down_first ? down : up;
            ct_node *other = down_first ? up : down;
            if (first == nullptr) {
                first = other;
                other = nullptr;
            }
            if (other != nullptr) {
                bool kept = inc.model == nullptr ? JM_GROW(sv, scap, sn + 1)
                                                 : ct_push(&heap, other);
                if (kept && inc.model == nullptr)
                    sv[sn++] = other;
                if (!kept) {
                    ct_node_free(down);
                    ct_node_free(up);
                    goto done;
                }
            }
            if (first == nullptr)
                continue;
            if (next != nullptr) {
                const bool kept = inc.model == nullptr
                                      ? JM_GROW(sv, scap, sn + 1)
                                      : ct_push(&heap, next);
                if (kept && inc.model == nullptr)
                    sv[sn++] = next;
                if (!kept) {
                    ct_node_free(first);
                    goto done;
                }
            }
            next = first;
        }
        for (int64_t b = at; b < nb; b++)
            if (batch[b]->key < stopped_key)
                stopped_key = batch[b]->key;
        for (int64_t b = 0; b < nb; b++) {
            if (batch[b] == cur)
                cur = nullptr;
            ct_node_free(batch[b]);
            batch[b] = nullptr;
        }
    }
    if (outcome == JAOS_SOLVE_NOT_RUN)
        outcome = inc.model != nullptr ? JAOS_SOLVE_OPTIMAL
                                       : JAOS_SOLVE_INFEASIBLE;
    if (parked > 0 &&
        (outcome == JAOS_SOLVE_INFEASIBLE ||
         (outcome == JAOS_SOLVE_OPTIMAL &&
          !ct_closed(&inc, parked_key, gap, gap_shift)))) {
        outcome = JAOS_SOLVE_NUMERICAL_ERROR;
        jm_set_err(m, "%s", why);
    }

    rc = JAOS_OK;
    m->solve_status = outcome;
    m->solve_work = work;
    m->solve_iters = iters;
    m->solve_time = jm_monotonic_seconds() - t0;
    m->mip_nodes = nodes;
    m->mip_solves = solves;
    m->mip_cuts = 0;
    m->mip_heur = inc.heur;
    m->mip_first_inc = inc.first;
    m->mip_rcfix_n = m->mip_prop_n = 0;
    m->mip_sym_gen = m->mip_sym_orbits = 0;
    {
        double open = cur != nullptr && outcome != JAOS_SOLVE_OPTIMAL
                          ? cur->key : INFINITY;
        if (next != nullptr && next->key < open)
            open = next->key;
        for (int64_t k = 0; k < sn; k++)
            if (sv[k]->key < open)
                open = sv[k]->key;
        for (int64_t k = 0; k < heap.n; k++)
            if (heap.v[k]->key < open)
                open = heap.v[k]->key;
        if (parked_key < open)
            open = parked_key;
        if (stopped_key < open)
            open = stopped_key;
        m->mip_bound = sigma * (open < INFINITY ? open : best_bound);
    }
    if (inc.model != nullptr) {
        m->mip_has_incumbent = true;
        m->mip_inc_x = jm_alloc_array(nc > 0 ? nc : 1, sizeof *m->mip_inc_x);
        m->mip_pool_x = jm_alloc_array(nc > 0 ? nc : 1, sizeof *m->mip_pool_x);
        m->mip_pool_obj = jm_alloc_array(1, sizeof *m->mip_pool_obj);
        if (m->mip_inc_x == nullptr || m->mip_pool_x == nullptr ||
            m->mip_pool_obj == nullptr) {
            rc = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        for (int64_t j = 0; j < nc; j++)
            m->mip_inc_x[j] = m->col_integer[j]
                                  ? jm_round(inc.model->sol_col[j])
                                  : inc.model->sol_col[j];
        m->mip_inc_obj = jm_model_objective_at(m, m->mip_inc_x);
        if (nc > 0)
            memcpy(m->mip_pool_x, m->mip_inc_x, (size_t)nc * sizeof *m->mip_pool_x);
        m->mip_pool_obj[0] = m->mip_inc_obj;
        m->mip_pool_n = 1;
        if (outcome == JAOS_SOLVE_OPTIMAL ||
            sigma * m->mip_bound > sigma * m->mip_inc_obj)
            m->mip_bound = m->mip_inc_obj;
    }
    if (outcome == JAOS_SOLVE_OPTIMAL) {
        rc = ct_publish(m, inc.model);
        if (rc != JAOS_OK) {
            m->solve_status = JAOS_SOLVE_NOT_RUN;
            goto done;
        }
        jm_mip_take_published(m);
    }
    jm_mip_progress_end(m, nodes, work, iters);
    jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: %s after %lld nodes "
           "and %lld solves, %lld work units; %lld relaxations the checker "
           "refused were branched on without a bound, %lld nodes the "
           "relaxation failed on were split, and %lld with every integer "
           "column fixed were set aside with their bound",
           jaos_solve_status_str(outcome), (long long)nodes, (long long)solves,
           (long long)work, (long long)roughs, (long long)splits,
           (long long)parked);
    if (open_splits > 0)
        jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: %lld nodes with "
               "an unbounded relaxation were split on an SOS set or an "
               "indicator row the node's box leaves open",
               (long long)open_splits);

done:
    if (rc == JAOS_ERR_OUT_OF_MEMORY && m->err[0] == '\0')
        jm_set_err(m, "out of memory in the conic branch and bound");
    for (int64_t b = 0; b < CT_BATCH_MAX; b++) {
        if (batch[b] == cur)
            cur = nullptr;
        ct_node_free(batch[b]);
        jaos_model_free(rels[b]);
    }
    ct_node_free(cur);
    ct_node_free(next);
    while (sn > 0)
        ct_node_free(sv[--sn]);
    free(sv);
    while (heap.n > 0)
        ct_node_free(ct_pop(&heap));
    free(heap.v);
    free(ilo);
    free(ihi);
    free(x);
    free(xc);
    free(xd);
    free(fcol);
    free(flo);
    free(fhi);
    free(pc.sum);
    free(pc.cnt);
    jaos_model_free(inc.model);
    return rc;
}
