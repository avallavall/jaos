/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"
#include "jaos_sys.h"

#include <math.h>
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

static ct_node *ct_child(const ct_node *p, int64_t id, int64_t col, double lo,
                         double hi, double key)
{
    ct_node *c = calloc(1, sizeof *c);
    if (c == nullptr)
        return nullptr;
    const int64_t k = p->nfix;
    c->col = jm_alloc_array(k + 1, sizeof *c->col);
    c->lo = jm_alloc_array(k + 1, sizeof *c->lo);
    c->hi = jm_alloc_array(k + 1, sizeof *c->hi);
    if (c->col == nullptr || c->lo == nullptr || c->hi == nullptr) {
        ct_node_free(c);
        return nullptr;
    }
    if (k > 0) {
        memcpy(c->col, p->col, (size_t)k * sizeof *c->col);
        memcpy(c->lo, p->lo, (size_t)k * sizeof *c->lo);
        memcpy(c->hi, p->hi, (size_t)k * sizeof *c->hi);
    }
    c->col[k] = col;
    c->lo[k] = lo;
    c->hi[k] = hi;
    c->nfix = k + 1;
    c->key = key;
    c->id = id;
    c->depth = p->depth + 1;
    c->bcol = -1;
    return c;
}

static jaos_status ct_apply(jaos_model *rel, const jaos_model *m,
                            const double *ilo, const double *ihi,
                            const ct_node *n)
{
    for (int64_t j = 0; j < m->num_col; j++)
        if (m->col_integer[j] &&
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
    return JAOS_OK;
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
    ct_node *down = ct_child(cur, (*next_id)++, best, lo, mid, cur->key);
    ct_node *up = ct_child(cur, (*next_id)++, best, mid + 1.0, hi, cur->key);
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
    rel->cfg.log_cb = nullptr;
    rel->cfg.progress_cb = nullptr;
    rel->cfg.incumbent_cb = nullptr;
    return rel;
}

static void ct_budget(jaos_model *sub, const jaos_model *m, int64_t work)
{
    if (m->cfg.work_limit <= 0)
        return;
    const int64_t left = m->cfg.work_limit - work;
    sub->cfg.work_limit = left > 0 ? left : 1;
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
        if (!m->col_integer[j])
            continue;
        const double f = x[j] - floor(x[j]);
        if (f <= CT_INT_TOL || f >= 1.0 - CT_INT_TOL)
            continue;
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
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!m->col_integer[j])
            continue;
        double v = jm_round(x[j]);
        if (v < m->col_lower[j])
            v = ceil(m->col_lower[j]);
        if (v > m->col_upper[j])
            v = floor(m->col_upper[j]);
        const jaos_status st = jaos_set_col_bounds(fin, j, v, v);
        if (st != JAOS_OK) {
            jaos_model_free(fin);
            return st;
        }
    }
    ct_budget(fin, m, work);
    const jaos_status st = jaos_solve(fin);
    if (st != JAOS_OK) {
        jaos_model_free(fin);
        return st;
    }
    *out = fin;
    return JAOS_OK;
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
        if (m->col_integer[j])
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

static bool ct_closed(const ct_best *b, double key, double gap)
{
    return b->key < INFINITY && b->key - key <= gap * (1.0 + fabs(b->key));
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

static jaos_status ct_feasible(const jaos_model *m, int64_t work,
                               jaos_model **out)
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
    for (int64_t i = 0; i < m->num_row; i++) {
        double act = 0.0, comp = 0.0;
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            jm_obj_add(&act, &comp, m->ar_value[p] * m->sol_col[m->ar_index[p]]);
        for (int64_t p = m->rq_start != nullptr ? m->rq_start[i] : 0;
             m->rq_start != nullptr && p < m->rq_start[i + 1]; p++) {
            const double v = m->rq_v[p] * m->sol_col[m->rq_i[p]] *
                             m->sol_col[m->rq_j[p]];
            jm_obj_add(&act, &comp, m->rq_i[p] == m->rq_j[p] ? 0.5 * v : v);
        }
        const double a = act + comp;
        m->sol_row[i] = a == 0.0 ? 0.0 : a;
    }
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

jaos_status jm_conic_branch_and_bound(jaos_model *m)
{
    const double t0 = jm_monotonic_seconds();
    const int64_t nc = m->num_col;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double gap = m->cfg.mip_gap > 0.0 ? m->cfg.mip_gap
                                            : jm_mip_default(JM_DEF_GAP);
    const int64_t dive = m->cfg.mip_dive_heuristic_set
                             ? m->cfg.mip_dive_heuristic
                             : (int64_t)jm_mip_default(JM_DEF_DIVE_HEURISTIC);

    if (m->num_sos > 0 || m->row_ind_col != nullptr) {
        jm_set_err(m, "the model has cones or quadratic rows and %s; JAOS's "
                      "conic tree takes integer columns only",
                   m->num_sos > 0 ? "SOS sets" : "indicator rows");
        return JAOS_ERR_INVALID_INPUT;
    }
    for (int64_t j = 0; j < nc; j++)
        if (m->col_semi != nullptr && m->col_semi[j]) {
            jm_set_err(m, "column %lld is semi-continuous in a model with "
                          "cones or quadratic rows; JAOS's conic tree takes "
                          "integer columns only", (long long)j);
            return JAOS_ERR_INVALID_INPUT;
        }
    if (m->cfg.node_cb != nullptr) {
        jm_set_err(m, "a node callback runs in the tree of linear and "
                      "quadratic models; a model with cones or quadratic rows "
                      "is solved by its own tree, which does not call it");
        return JAOS_ERR_INVALID_INPUT;
    }

    free(m->mip_inc_x);
    m->mip_inc_x = nullptr;
    free(m->mip_pool_x);
    m->mip_pool_x = nullptr;
    free(m->mip_pool_obj);
    m->mip_pool_obj = nullptr;
    m->mip_pool_n = 0;
    m->mip_has_incumbent = false;
    m->sol_basis_ok = false;
    m->farkas_ok = false;
    m->ray_ok = false;
    m->cone_ok = false;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    jaos_model *rel = ct_relaxation(m);
    double *ilo = jm_alloc_array(nc > 0 ? nc : 1, sizeof *ilo);
    double *ihi = jm_alloc_array(nc > 0 ? nc : 1, sizeof *ihi);
    double *x = jm_alloc_array(nc > 0 ? nc : 1, sizeof *x);
    double *xc = jm_alloc_array(nc > 0 ? nc : 1, sizeof *xc);
    double *xd = jm_alloc_array(nc > 0 ? nc : 1, sizeof *xd);
    ct_pcost pc = {jm_calloc_array(2 * nc + 1, sizeof *pc.sum),
                   jm_calloc_array(2 * nc + 1, sizeof *pc.cnt)};
    ct_heap heap = {0};
    ct_node *cur = calloc(1, sizeof *cur), *next = nullptr, **sv = nullptr;
    int64_t sn = 0, scap = 0;
    jaos_solve_status outcome = JAOS_SOLVE_NOT_RUN;
    int64_t work = 0, iters = 0, nodes = 0, solves = 0, next_id = 1;
    int64_t roughs = 0, splits = 0;
    ct_best inc = {
        .key = m->cfg.mip_cutoff_set ? sigma * m->cfg.mip_cutoff : INFINITY,
        .sigma = sigma,
    };
    double best_bound = -INFINITY;
    if (rel == nullptr || ilo == nullptr || ihi == nullptr || x == nullptr ||
        xc == nullptr || xd == nullptr || cur == nullptr || pc.sum == nullptr ||
        pc.cnt == nullptr)
        goto done;
    rel->cfg.node_solve = true;
    cur->key = -INFINITY;
    cur->bcol = -1;

    int64_t nint = 0;
    for (int64_t j = 0; j < nc; j++) {
        ilo[j] = m->col_lower[j];
        ihi[j] = m->col_upper[j];
        if (!m->col_integer[j])
            continue;
        nint++;
        ilo[j] = ceil(m->col_lower[j] - CT_INT_TOL);
        ihi[j] = floor(m->col_upper[j] + CT_INT_TOL);
        if (ilo[j] > ihi[j])
            outcome = JAOS_SOLVE_INFEASIBLE;
    }
    jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: %lld integer "
           "columns of %lld, depth first until an incumbent and best bound "
           "first with a plunge after it, the most fractional column "
           "branched; at the root, %s and a dive of up to %lld solves while "
           "no incumbent is known",
           (long long)nint, (long long)nc,
           m->cfg.mip_no_heuristics ? "no rounding" : "the relaxation rounded",
           (long long)dive);

    if (m->mip_start != nullptr && outcome == JAOS_SOLVE_NOT_RUN) {
        jaos_model *fin = nullptr;
        rc = ct_fixed(m, m->mip_start, work, &fin);
        if (rc != JAOS_OK)
            goto done;
        rc = JAOS_ERR_OUT_OF_MEMORY;
        solves++;
        work += jaos_work_units(fin);
        iters += jaos_iterations(fin);
        double obj = 0.0;
        if (jaos_status_of(fin) == JAOS_SOLVE_OPTIMAL &&
            jaos_objective(fin, &obj) == JAOS_OK && sigma * obj < inc.key) {
            jaos_model_free(inc.model);
            inc.model = fin;
            fin = nullptr;
            inc.key = sigma * obj;
            inc.obj = obj;
            jm_log(m, JAOS_LOG_PROGRESS, "the start is an incumbent at %.12g",
                   obj);
        }
        jaos_model_free(fin);
    }

    while (outcome == JAOS_SOLVE_NOT_RUN) {
        if (nodes > 0) {
            ct_node_free(cur);
            cur = next;
            next = nullptr;
            if (cur != nullptr && ct_closed(&inc, cur->key, gap)) {
                ct_node_free(cur);
                cur = nullptr;
            }
            while (cur == nullptr && sn > 0) {
                cur = sv[--sn];
                if (ct_closed(&inc, cur->key, gap)) {
                    ct_node_free(cur);
                    cur = nullptr;
                }
            }
            if (cur == nullptr) {
                while ((cur = ct_pop(&heap)) != nullptr) {
                    if (!ct_closed(&inc, cur->key, gap))
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

        jaos_status st = ct_apply(rel, m, ilo, ihi, cur);
        if (st != JAOS_OK) {
            rc = st;
            goto done;
        }
        nodes++;
        ct_budget(rel, m, work);
        st = jaos_solve(rel);
        solves++;
        work += jaos_work_units(rel);
        iters += jaos_iterations(rel);
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
                const int64_t members = m->num_cone > 0
                                            ? m->cone_start[m->num_cone] : 0;
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
            jaos_model *f = nullptr;
            st = ct_feasible(m, work, &f);
            if (st != JAOS_OK) {
                rc = st;
                goto done;
            }
            work += jaos_work_units(f);
            iters += jaos_iterations(f);
            solves += f->mip_solves;
            const jaos_solve_status fs = jaos_status_of(f);
            outcome = fs == JAOS_SOLVE_OPTIMAL ? JAOS_SOLVE_UNBOUNDED : fs;
            if (fs == JAOS_SOLVE_NUMERICAL_ERROR)
                jm_set_err(m, "the relaxation is unbounded, and the search "
                              "for an integer point ends as a numerical "
                              "error: %s", jaos_model_error(f));
            jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: the "
                   "relaxation is unbounded, and the search for an integer "
                   "point ends %s", jaos_solve_status_str(fs));
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
            jm_set_err(m, "node %lld: %s", (long long)nodes,
                       jaos_model_error(rel));
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
        if (ct_closed(&inc, key, gap))
            continue;

        const int64_t j = ct_branch_col(m, x, &pc);
        const bool rounding = j >= 0 && nodes == 1 &&
                              !m->cfg.mip_no_heuristics;
        if (j < 0 || rounding) {
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
            const bool solved = jaos_status_of(fin) == JAOS_SOLVE_OPTIMAL &&
                                jaos_objective(fin, &fobj) == JAOS_OK;
            if (!solved && j < 0) {
                const int split = ct_split(&heap, cur, rel, m, &next_id);
                if (split != 0) {
                    jaos_model_free(fin);
                    if (split < 0)
                        goto done;
                    splits++;
                    continue;
                }
                jm_set_err(m, "node %lld: the relaxation is integral, and "
                              "with its integer columns fixed the model "
                              "ends %s: %s", (long long)nodes,
                           jaos_solve_status_str(jaos_status_of(fin)),
                           jaos_model_error(fin));
                jaos_model_free(fin);
                outcome = JAOS_SOLVE_NUMERICAL_ERROR;
                break;
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
            if (j < 0)
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
            if (fin != nullptr && jaos_status_of(fin) == JAOS_SOLVE_OPTIMAL &&
                jaos_objective(fin, &fobj) == JAOS_OK)
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
        if (ct_closed(&inc, key, gap))
            continue;

        const double lo = rel->col_lower[j], hi = rel->col_upper[j];
        ct_node *down = ct_child(cur, next_id++, j, lo, floor(x[j]), key);
        ct_node *up = ct_child(cur, next_id++, j, ceil(x[j]), hi, key);
        if (down != nullptr) {
            down->bcol = j;
            down->bdir = 0;
            down->bfrac = x[j] - floor(x[j]);
        }
        if (up != nullptr) {
            up->bcol = j;
            up->bdir = 1;
            up->bfrac = ceil(x[j]) - x[j];
        }
        const bool down_first = x[j] - floor(x[j]) < 0.5;
        ct_node *other = down_first ? up : down;
        bool kept = down != nullptr && up != nullptr;
        if (kept && inc.model == nullptr) {
            kept = JM_GROW(sv, scap, sn + 1);
            if (kept)
                sv[sn++] = other;
        } else if (kept) {
            kept = ct_push(&heap, other);
        }
        if (!kept) {
            ct_node_free(down);
            ct_node_free(up);
            goto done;
        }
        next = down_first ? down : up;
    }
    if (outcome == JAOS_SOLVE_NOT_RUN)
        outcome = inc.model != nullptr ? JAOS_SOLVE_OPTIMAL
                                       : JAOS_SOLVE_INFEASIBLE;

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
        m->mip_bound = sigma * (open < INFINITY ? open : best_bound);
    }
    if (inc.model != nullptr) {
        m->mip_has_incumbent = true;
        m->mip_inc_obj = inc.obj;
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
        if (nc > 0)
            memcpy(m->mip_pool_x, m->mip_inc_x, (size_t)nc * sizeof *m->mip_pool_x);
        m->mip_pool_obj[0] = inc.obj;
        m->mip_pool_n = 1;
    }
    if (outcome == JAOS_SOLVE_OPTIMAL) {
        m->mip_bound = inc.obj;
        rc = ct_publish(m, inc.model);
        if (rc != JAOS_OK) {
            m->solve_status = JAOS_SOLVE_NOT_RUN;
            goto done;
        }
    }
    jm_log(m, JAOS_LOG_SUMMARY, "conic branch and bound: %s after %lld nodes "
           "and %lld solves, %lld work units; %lld relaxations the checker "
           "refused were branched on without a bound, and %lld nodes the "
           "relaxation failed on were split", jaos_solve_status_str(outcome),
           (long long)nodes, (long long)solves, (long long)work,
           (long long)roughs, (long long)splits);

done:
    if (rc == JAOS_ERR_OUT_OF_MEMORY && m->err[0] == '\0')
        jm_set_err(m, "out of memory in the conic branch and bound");
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
    free(pc.sum);
    free(pc.cnt);
    jaos_model_free(rel);
    jaos_model_free(inc.model);
    return rc;
}
