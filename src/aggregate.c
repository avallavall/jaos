/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef JAOS_AGG_ROW_MAX_VALUE
#define JAOS_AGG_ROW_MAX_VALUE 3
#endif
constexpr int64_t AGG_ROW_MAX = JAOS_AGG_ROW_MAX_VALUE;

#ifndef JAOS_AGG_FILL_MAX_VALUE
#define JAOS_AGG_FILL_MAX_VALUE 8
#endif
constexpr int64_t AGG_FILL_MAX = JAOS_AGG_FILL_MAX_VALUE;

#ifndef JAOS_AGG_PIVOT_REL_VALUE
#define JAOS_AGG_PIVOT_REL_VALUE 0.5
#endif
constexpr double AGG_PIVOT_REL = JAOS_AGG_PIVOT_REL_VALUE;

#ifndef JAOS_AGG_PASSES_VALUE
#define JAOS_AGG_PASSES_VALUE 8
#endif
constexpr int64_t AGG_PASSES = JAOS_AGG_PASSES_VALUE;

#ifndef JAOS_AGG_IMPLIED_FREE_ULPS_VALUE
#define JAOS_AGG_IMPLIED_FREE_ULPS_VALUE 0
#endif
constexpr double AGG_IMPLIED_FREE_ULPS = JAOS_AGG_IMPLIED_FREE_ULPS_VALUE;
constexpr double AGG_CANCEL_ULPS = 8.0;

typedef struct {
    double sum, comp;
} ag_acc;

static void ag_acc_add(ag_acc *a, double t)
{
    const double s = a->sum + t;
    a->comp += (fabs(a->sum) >= fabs(t)) ? ((a->sum - s) + t)
                                         : ((t - s) + a->sum);
    a->sum = s;
}

static double ag_acc_value(const ag_acc *a)
{
    return a->sum + a->comp;
}

static double ag_published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

typedef struct {
    int64_t len, cap;
    int64_t *row, *col;
    double *val;
    int64_t *rnext, *rprev, *cnext, *cprev;
    int64_t *rhead, *rtail, *chead, *ctail;
    int64_t *rlen, *clen;
} ag_mat;

static void ag_mat_free(ag_mat *a)
{
    free(a->row); free(a->col); free(a->val);
    free(a->rnext); free(a->rprev); free(a->cnext); free(a->cprev);
    free(a->rhead); free(a->rtail); free(a->chead); free(a->ctail);
    free(a->rlen); free(a->clen);
    memset(a, 0, sizeof *a);
}

static bool ag_grow_one(void **p, int64_t cap, size_t size)
{
    void *q = realloc(*p, (size_t)cap * size);
    if (q == nullptr)
        return false;
    *p = q;
    return true;
}

static bool ag_grow(ag_mat *a, int64_t need)
{
    if (need <= a->cap)
        return true;
    int64_t cap = a->cap < 16 ? 16 : a->cap;
    while (cap < need)
        cap *= 2;
    if (!ag_grow_one((void **)&a->row, cap, sizeof *a->row) ||
        !ag_grow_one((void **)&a->col, cap, sizeof *a->col) ||
        !ag_grow_one((void **)&a->val, cap, sizeof *a->val) ||
        !ag_grow_one((void **)&a->rnext, cap, sizeof *a->rnext) ||
        !ag_grow_one((void **)&a->rprev, cap, sizeof *a->rprev) ||
        !ag_grow_one((void **)&a->cnext, cap, sizeof *a->cnext) ||
        !ag_grow_one((void **)&a->cprev, cap, sizeof *a->cprev))
        return false;
    a->cap = cap;
    return true;
}

static int64_t ag_add(ag_mat *a, int64_t r, int64_t c, double v)
{
    if (!ag_grow(a, a->len + 1))
        return -1;
    const int64_t e = a->len++;
    a->row[e] = r;
    a->col[e] = c;
    a->val[e] = v;
    a->rnext[e] = -1;
    a->rprev[e] = a->rtail[r];
    if (a->rtail[r] >= 0)
        a->rnext[a->rtail[r]] = e;
    else
        a->rhead[r] = e;
    a->rtail[r] = e;
    a->cnext[e] = -1;
    a->cprev[e] = a->ctail[c];
    if (a->ctail[c] >= 0)
        a->cnext[a->ctail[c]] = e;
    else
        a->chead[c] = e;
    a->ctail[c] = e;
    a->rlen[r]++;
    a->clen[c]++;
    return e;
}

static void ag_unlink(ag_mat *a, int64_t e)
{
    const int64_t r = a->row[e], c = a->col[e];
    if (a->rprev[e] >= 0)
        a->rnext[a->rprev[e]] = a->rnext[e];
    else
        a->rhead[r] = a->rnext[e];
    if (a->rnext[e] >= 0)
        a->rprev[a->rnext[e]] = a->rprev[e];
    else
        a->rtail[r] = a->rprev[e];
    if (a->cprev[e] >= 0)
        a->cnext[a->cprev[e]] = a->cnext[e];
    else
        a->chead[c] = a->cnext[e];
    if (a->cnext[e] >= 0)
        a->cprev[a->cnext[e]] = a->cprev[e];
    else
        a->ctail[c] = a->cprev[e];
    a->rlen[r]--;
    a->clen[c]--;
}

static int64_t ag_find(const ag_mat *a, int64_t r, int64_t c, jm_work *w)
{
    if (a->rlen[r] <= a->clen[c]) {
        for (int64_t e = a->rhead[r]; e >= 0; e = a->rnext[e]) {
            jm_work_add(w, JM_WORK_NONZERO);
            if (a->col[e] == c)
                return e;
        }
    } else {
        for (int64_t e = a->chead[c]; e >= 0; e = a->cnext[e]) {
            jm_work_add(w, JM_WORK_NONZERO);
            if (a->row[e] == r)
                return e;
        }
    }
    return -1;
}

static bool ag_mat_build(const jaos_model *m, ag_mat *a)
{
    const int64_t nr = m->num_row, nc = m->num_col;
    memset(a, 0, sizeof *a);
    a->rhead = jm_alloc_array(nr, sizeof *a->rhead);
    a->rtail = jm_alloc_array(nr, sizeof *a->rtail);
    a->chead = jm_alloc_array(nc, sizeof *a->chead);
    a->ctail = jm_alloc_array(nc, sizeof *a->ctail);
    a->rlen = jm_calloc_array(nr, sizeof *a->rlen);
    a->clen = jm_calloc_array(nc, sizeof *a->clen);
    if ((nr > 0 && (!a->rhead || !a->rtail || !a->rlen)) ||
        (nc > 0 && (!a->chead || !a->ctail || !a->clen)) ||
        !ag_grow(a, m->num_nz + 1))
        return false;
    for (int64_t i = 0; i < nr; i++)
        a->rhead[i] = a->rtail[i] = -1;
    for (int64_t j = 0; j < nc; j++)
        a->chead[j] = a->ctail[j] = -1;
    for (int64_t j = 0; j < nc; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            if (m->a_value[k] != 0.0 &&
                ag_add(a, m->a_index[k], j, m->a_value[k]) < 0)
                return false;
    return true;
}

static bool ag_push_rec(jm_presolve *p, jm_agg_rec rec)
{
    if (p->agg_len == p->agg_cap) {
        const int64_t cap = p->agg_cap < 16 ? 16 : 2 * p->agg_cap;
        jm_agg_rec *q = realloc(p->agg, (size_t)cap * sizeof *q);
        if (q == nullptr)
            return false;
        p->agg = q;
        p->agg_cap = cap;
    }
    p->agg[p->agg_len++] = rec;
    return true;
}

static bool ag_push_entry(jm_presolve *p, int64_t index, double value)
{
    if (p->agg_plen == p->agg_pcap) {
        const int64_t cap = p->agg_pcap < 64 ? 64 : 2 * p->agg_pcap;
        int64_t *qi = realloc(p->agg_pidx, (size_t)cap * sizeof *qi);
        if (qi == nullptr)
            return false;
        p->agg_pidx = qi;
        double *qv = realloc(p->agg_pval, (size_t)cap * sizeof *qv);
        if (qv == nullptr)
            return false;
        p->agg_pval = qv;
        p->agg_pcap = cap;
    }
    p->agg_pidx[p->agg_plen] = index;
    p->agg_pval[p->agg_plen] = value;
    p->agg_plen++;
    return true;
}

static bool ag_implied_free(const ag_mat *a, int64_t e, double b,
                            const double *cl, const double *cu, jm_work *w)
{
    const int64_t i = a->row[e], j = a->col[e];
    const double aij = a->val[e];
    ag_acc lo = {0.0, 0.0}, hi = {0.0, 0.0}, tr = {0.0, 0.0};
    int64_t lo_inf = 0, hi_inf = 0;
    for (int64_t f = a->rhead[i]; f >= 0; f = a->rnext[f]) {
        jm_work_add(w, JM_WORK_NONZERO);
        if (f == e)
            continue;
        const int64_t l = a->col[f];
        const double v = a->val[f];
        const double t_lo = v > 0.0 ? cl[l] : cu[l];
        const double t_hi = v > 0.0 ? cu[l] : cl[l];
        if (isfinite(t_lo)) {
            ag_acc_add(&lo, v * t_lo);
            ag_acc_add(&tr, fabs(v * t_lo));
        } else {
            lo_inf++;
        }
        if (isfinite(t_hi)) {
            ag_acc_add(&hi, v * t_hi);
            ag_acc_add(&tr, fabs(v * t_hi));
        } else {
            hi_inf++;
        }
    }
    const double minact = lo_inf > 0 ? -HUGE_VAL : ag_acc_value(&lo);
    const double maxact = hi_inf > 0 ? HUGE_VAL : ag_acc_value(&hi);
    const double loside = isfinite(maxact) ? b - maxact : -HUGE_VAL;
    const double upside = isfinite(minact) ? b - minact : HUGE_VAL;
    double ilo, iup;
    if (aij > 0.0) {
        ilo = loside / aij;
        iup = upside / aij;
    } else {
        ilo = upside / aij;
        iup = loside / aij;
    }
    double scale = fabs(b);
    const double traffic = ag_acc_value(&tr);
    if (isfinite(traffic) && traffic > scale)
        scale = traffic;
    if (scale < 1.0)
        scale = 1.0;
    const double margin =
        AGG_IMPLIED_FREE_ULPS * DBL_EPSILON * scale / fabs(aij);
    const bool lo_ok = !isfinite(cl[j]) || ilo >= cl[j] + margin;
    const bool up_ok = !isfinite(cu[j]) || iup <= cu[j] - margin;
    return lo_ok && up_ok;
}

static void ag_shift(ag_acc *acc, double *cur, double t)
{
    ag_acc_add(acc, -t);
    if (!isfinite(acc->sum) || !isfinite(acc->comp))
        acc->comp = 0.0;
    *cur = acc->sum + acc->comp;
}

static void ag_fresh(jaos_model *r)
{
    r->rowwise_valid = false;
    r->ar_start = nullptr;
    r->ar_index = nullptr;
    r->ar_value = nullptr;
    r->scale_valid = false;
    r->scale_clamped = false;
    r->row_scale = nullptr;
    r->col_scale = nullptr;
    r->sol_col = nullptr;
    r->sol_row = nullptr;
    r->sol_dual = nullptr;
    r->sol_redcost = nullptr;
    r->sol_col_status = nullptr;
    r->sol_row_status = nullptr;
    r->sol_farkas = nullptr;
    r->farkas_ok = false;
    r->sol_ray = nullptr;
    r->ray_ok = false;
    r->sol_basis_ok = false;
    r->start_col_status = nullptr;
    r->start_row_status = nullptr;
    r->solve_status = JAOS_SOLVE_NOT_RUN;
    r->objective = 0.0;
    r->solve_work = 0;
    r->solve_iters = 0;
    r->solve_primal_iters = 0;
    r->solve_phase1_iters = 0;
    r->solve_time = 0.0;
    r->err[0] = '\0';
}

static void *ag_dup(const void *src, int64_t n, size_t size)
{
    void *d = jm_alloc_array(n > 0 ? n : 1, size);
    if (d != nullptr && n > 0)
        memcpy(d, src, (size_t)n * size);
    return d;
}

static bool ag_identity(jm_presolve *p, const jaos_model *m)
{
    const int64_t nr = m->num_row, nc = m->num_col;
    free(p->col_map);
    free(p->row_map);
    free(p->orig_col);
    free(p->orig_row);
    p->col_map = jm_alloc_array(nc > 0 ? nc : 1, sizeof *p->col_map);
    p->row_map = jm_alloc_array(nr > 0 ? nr : 1, sizeof *p->row_map);
    p->orig_col = jm_alloc_array(nc > 0 ? nc : 1, sizeof *p->orig_col);
    p->orig_row = jm_alloc_array(nr > 0 ? nr : 1, sizeof *p->orig_row);
    jaos_model s = *m;
    ag_fresh(&s);
    s.col_cost = ag_dup(m->col_cost, nc, sizeof(double));
    s.col_lower = ag_dup(m->col_lower, nc, sizeof(double));
    s.col_upper = ag_dup(m->col_upper, nc, sizeof(double));
    s.row_lower = ag_dup(m->row_lower, nr, sizeof(double));
    s.row_upper = ag_dup(m->row_upper, nr, sizeof(double));
    s.a_start = ag_dup(m->a_start, nc + 1, sizeof(int64_t));
    s.a_index = ag_dup(m->a_index, m->num_nz, sizeof(int64_t));
    s.a_value = ag_dup(m->a_value, m->num_nz, sizeof(double));
    p->stage1 = s;
    if (!p->col_map || !p->row_map || !p->orig_col || !p->orig_row ||
        !s.col_cost || !s.col_lower || !s.col_upper || !s.row_lower ||
        !s.row_upper || !s.a_start || !s.a_index || !s.a_value)
        return false;
    for (int64_t j = 0; j < nc; j++)
        p->col_map[j] = p->orig_col[j] = j;
    for (int64_t i = 0; i < nr; i++)
        p->row_map[i] = p->orig_row[i] = i;
    return true;
}

JAOS_NODISCARD jaos_status jm_aggregate(jm_presolve *p, const jaos_model *src,
                                        jm_work *w)
{
#if defined(JAOS_AGG_OFF)
    (void)p;
    (void)src;
    (void)w;
    return JAOS_OK;
#else
    assert(!p->aggregated);
    assert(src == &p->reduced ? p->outcome == JM_PRESOLVE_REDUCED
                              : p->outcome == JM_PRESOLVE_NONE);
    const jaos_model *m = src;
    const int64_t nr = m->num_row, nc = m->num_col;

    ag_mat a;
    double *cost = jm_alloc_array(nc, sizeof *cost);
    double *rl = jm_alloc_array(nr, sizeof *rl);
    double *ru = jm_alloc_array(nr, sizeof *ru);
    ag_acc *rl_acc = jm_calloc_array(nr, sizeof *rl_acc);
    ag_acc *ru_acc = jm_calloc_array(nr, sizeof *ru_acc);
    bool *row_dead = jm_calloc_array(nr, sizeof *row_dead);
    bool *col_dead = jm_calloc_array(nc, sizeof *col_dead);
    jaos_status ret = JAOS_OK;
    ag_acc offset = {0.0, 0.0};

    if (!ag_mat_build(m, &a) || (nc > 0 && (!cost || !col_dead)) ||
        (nr > 0 && (!rl || !ru || !rl_acc || !ru_acc || !row_dead))) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    jm_work_add(w, m->num_nz * JM_WORK_NONZERO);

    for (int64_t j = 0; j < nc; j++)
        cost[j] = m->col_cost[j];
    for (int64_t i = 0; i < nr; i++) {
        rl[i] = m->row_lower[i];
        ru[i] = m->row_upper[i];
        rl_acc[i].sum = rl[i];
        ru_acc[i].sum = ru[i];
    }

    int64_t live_col = nc;
    for (int64_t pass = 0; pass < AGG_PASSES; pass++) {
        bool changed = false;
        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || !isfinite(rl[i]) || rl[i] != ru[i])
                continue;
            const int64_t deg = a.rlen[i];
            if (deg < 2 || deg > AGG_ROW_MAX || live_col <= 1)
                continue;
            const double b = rl[i];

            double rowmax = 0.0;
            for (int64_t f = a.rhead[i]; f >= 0; f = a.rnext[f])
                if (fabs(a.val[f]) > rowmax)
                    rowmax = fabs(a.val[f]);
            jm_work_add(w, deg * JM_WORK_NONZERO);

            int64_t best = -1, best_mark = INT64_MAX;
            for (int64_t f = a.rhead[i]; f >= 0; f = a.rnext[f]) {
                const int64_t j = a.col[f];
                if (fabs(a.val[f]) < AGG_PIVOT_REL * rowmax)
                    continue;
                const int64_t mark = (a.clen[j] - 1) * (deg - 1);
                if (mark > AGG_FILL_MAX || mark >= best_mark)
                    continue;
                if (!ag_implied_free(&a, f, b, m->col_lower, m->col_upper,
                                     w))
                    continue;
                best = f;
                best_mark = mark;
            }
            if (best < 0)
                continue;

            const int64_t j = a.col[best];
            const double aij = a.val[best];
            jm_agg_rec rec = {
                .row = i, .col = j, .coef = aij, .rhs = b, .cost = cost[j],
                .row_at = p->agg_plen,
            };
            for (int64_t f = a.rhead[i]; f >= 0; f = a.rnext[f]) {
                if (f == best)
                    continue;
                if (!ag_push_entry(p, a.col[f], a.val[f])) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup;
                }
            }
            rec.row_n = p->agg_plen - rec.row_at;
            rec.col_at = p->agg_plen;
            for (int64_t g = a.chead[j]; g >= 0; g = a.cnext[g]) {
                if (g == best)
                    continue;
                if (!ag_push_entry(p, a.row[g], a.val[g])) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup;
                }
            }
            rec.col_n = p->agg_plen - rec.col_at;
            if (!ag_push_rec(p, rec)) {
                ret = JAOS_ERR_OUT_OF_MEMORY;
                goto cleanup;
            }

            const double yc = cost[j] / aij;
            if (yc != 0.0) {
                ag_acc_add(&offset, yc * b);
                for (int64_t f = a.rhead[i]; f >= 0; f = a.rnext[f])
                    if (f != best)
                        cost[a.col[f]] -= yc * a.val[f];
            }
            cost[j] = 0.0;

            int64_t g = a.chead[j];
            while (g >= 0) {
                const int64_t gnext = a.cnext[g];
                if (g == best) {
                    g = gnext;
                    continue;
                }
                const int64_t r = a.row[g];
                const double fr = a.val[g] / aij;
                if (isfinite(rl[r]) && rl[r] == ru[r]) {
                    ag_shift(&rl_acc[r], &rl[r], fr * b);
                    ru_acc[r] = rl_acc[r];
                    ru[r] = rl[r];
                } else {
                    if (isfinite(rl[r]))
                        ag_shift(&rl_acc[r], &rl[r], fr * b);
                    if (isfinite(ru[r]))
                        ag_shift(&ru_acc[r], &ru[r], fr * b);
                }
                for (int64_t f = a.rhead[i]; f >= 0; f = a.rnext[f]) {
                    if (f == best)
                        continue;
                    const int64_t l = a.col[f];
                    const double delta = -fr * a.val[f];
                    const int64_t h = ag_find(&a, r, l, w);
                    if (h >= 0) {
                        const double old = a.val[h];
                        const double nv = old + delta;
                        const double big = fabs(old) > fabs(delta)
                                               ? fabs(old) : fabs(delta);
                        if (fabs(nv) <= AGG_CANCEL_ULPS * DBL_EPSILON * big)
                            ag_unlink(&a, h);
                        else
                            a.val[h] = nv;
                    } else if (ag_add(&a, r, l, delta) < 0) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup;
                    }
                    jm_work_add(w, JM_WORK_NONZERO);
                }
                ag_unlink(&a, g);
                g = gnext;
            }
            for (int64_t f = a.rhead[i]; f >= 0;) {
                const int64_t fnext = a.rnext[f];
                ag_unlink(&a, f);
                f = fnext;
            }
            row_dead[i] = true;
            col_dead[j] = true;
            live_col--;
            p->counts.aggregated_col++;
            changed = true;
        }
        if (!changed)
            break;
    }

    if (p->agg_len == 0)
        goto cleanup;

    {
    int64_t rcol = 0, rrow = 0, rnz = 0;
    for (int64_t j = 0; j < nc; j++)
        if (!col_dead[j]) {
            rcol++;
            rnz += a.clen[j];
        }
    for (int64_t i = 0; i < nr; i++)
        if (!row_dead[i])
            rrow++;

    p->agg_col_map = jm_alloc_array(nc, sizeof *p->agg_col_map);
    p->agg_row_map = jm_alloc_array(nr, sizeof *p->agg_row_map);
    p->agg_orig_col = jm_alloc_array(rcol, sizeof *p->agg_orig_col);
    p->agg_orig_row = jm_alloc_array(rrow, sizeof *p->agg_orig_row);
    jaos_model r = *m;
    ag_fresh(&r);
    r.num_col = rcol;
    r.num_row = rrow;
    r.num_nz = rnz;
    r.col_cost = jm_alloc_array(rcol, sizeof(double));
    r.col_lower = jm_alloc_array(rcol, sizeof(double));
    r.col_upper = jm_alloc_array(rcol, sizeof(double));
    r.row_lower = jm_alloc_array(rrow, sizeof(double));
    r.row_upper = jm_alloc_array(rrow, sizeof(double));
    r.a_start = jm_alloc_array(rcol + 1, sizeof(int64_t));
    r.a_index = jm_alloc_array(rnz, sizeof(int64_t));
    r.a_value = jm_alloc_array(rnz, sizeof(double));
    if ((nc > 0 && !p->agg_col_map) || (nr > 0 && !p->agg_row_map) ||
        (rcol > 0 && (!p->agg_orig_col || !r.col_cost || !r.col_lower ||
                      !r.col_upper)) ||
        (rrow > 0 && (!p->agg_orig_row || !r.row_lower || !r.row_upper)) ||
        !r.a_start || (rnz > 0 && (!r.a_index || !r.a_value))) {
        free(r.col_cost); free(r.col_lower); free(r.col_upper);
        free(r.row_lower); free(r.row_upper);
        free(r.a_start); free(r.a_index); free(r.a_value);
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }

    int64_t rj = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (col_dead[j]) {
            p->agg_col_map[j] = -1;
            continue;
        }
        p->agg_col_map[j] = rj;
        p->agg_orig_col[rj] = j;
        rj++;
    }
    int64_t ri = 0;
    for (int64_t i = 0; i < nr; i++) {
        if (row_dead[i]) {
            p->agg_row_map[i] = -1;
            continue;
        }
        p->agg_row_map[i] = ri;
        p->agg_orig_row[ri] = i;
        r.row_lower[ri] = rl[i];
        r.row_upper[ri] = ru[i];
        ri++;
    }

    r.a_start[0] = 0;
    for (int64_t k = 0; k < rcol; k++) {
        const int64_t j = p->agg_orig_col[k];
        r.col_cost[k] = cost[j];
        r.col_lower[k] = m->col_lower[j];
        r.col_upper[k] = m->col_upper[j];
        r.a_start[k + 1] = r.a_start[k] + a.clen[j];
    }
    assert(r.a_start[rcol] == rnz);
    for (int64_t k = 0; k < rcol; k++)
        a.clen[p->agg_orig_col[k]] = r.a_start[k];
    for (int64_t i = 0; i < nr; i++) {
        if (row_dead[i])
            continue;
        const int64_t at = p->agg_row_map[i];
        for (int64_t e = a.rhead[i]; e >= 0; e = a.rnext[e]) {
            const int64_t j = a.col[e];
            assert(!col_dead[j]);
            const int64_t dst = a.clen[j]++;
            r.a_index[dst] = at;
            r.a_value[dst] = a.val[e];
        }
    }
    jm_work_add(w, rnz * JM_WORK_NONZERO);

    r.obj_offset = m->obj_offset + ag_acc_value(&offset);

    if (src == &p->reduced) {
        p->stage1 = p->reduced;
    } else if (ag_identity(p, m)) {
        p->outcome = JM_PRESOLVE_REDUCED;
    } else {
        free(p->stage1.col_cost); free(p->stage1.col_lower);
        free(p->stage1.col_upper); free(p->stage1.row_lower);
        free(p->stage1.row_upper); free(p->stage1.a_start);
        free(p->stage1.a_index); free(p->stage1.a_value);
        memset(&p->stage1, 0, sizeof p->stage1);
        free(r.col_cost); free(r.col_lower); free(r.col_upper);
        free(r.row_lower); free(r.row_upper);
        free(r.a_start); free(r.a_index); free(r.a_value);
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    p->reduced = r;
    p->aggregated = true;
    }

cleanup:
    ag_mat_free(&a);
    free(cost); free(rl); free(ru); free(rl_acc); free(ru_acc);
    free(row_dead); free(col_dead);
    return ret;
#endif
}

static double ag_row_rest(const jm_presolve *p, const jm_agg_rec *rec,
                          double base, const double *x)
{
    ag_acc s = {base, 0.0};
    for (int64_t k = rec->row_at; k < rec->row_at + rec->row_n; k++) {
        const double t = -p->agg_pval[k] * x[p->agg_pidx[k]];
        ag_acc_add(&s, t);
        const double e = jm_two_product_residue(-p->agg_pval[k],
                                                x[p->agg_pidx[k]], t);
        if (e != 0.0)
            ag_acc_add(&s, e);
    }
    return ag_acc_value(&s);
}

static double ag_col_rest(const jm_presolve *p, const jm_agg_rec *rec,
                          double c, const double *y)
{
    ag_acc s = {c, 0.0};
    for (int64_t k = rec->col_at; k < rec->col_at + rec->col_n; k++) {
        const double t = -p->agg_pval[k] * y[p->agg_pidx[k]];
        ag_acc_add(&s, t);
        const double e = jm_two_product_residue(-p->agg_pval[k],
                                                y[p->agg_pidx[k]], t);
        if (e != 0.0)
            ag_acc_add(&s, e);
    }
    return ag_acc_value(&s);
}

JAOS_NODISCARD jaos_status jm_aggregate_expand(jm_presolve *p)
{
    assert(p->aggregated);
    jaos_model *mid = &p->stage1;
    const jaos_model *red = &p->reduced;

    jaos_status est = jm_model_ensure_solution_arrays(mid);
    if (est != JAOS_OK)
        return est;

    const int64_t nr = mid->num_row, nc = mid->num_col;
    mid->solve_status = red->solve_status;
    mid->solve_iters = red->solve_iters;
    mid->solve_work = red->solve_work;
    mid->solve_time = red->solve_time;
    mid->objective = 0.0;
    mid->farkas_ok = false;
    mid->ray_ok = false;
    mid->sol_basis_ok = red->sol_basis_ok;

    memset(mid->sol_col, 0, (size_t)nc * sizeof(double));
    memset(mid->sol_row, 0, (size_t)nr * sizeof(double));
    memset(mid->sol_dual, 0, (size_t)nr * sizeof(double));
    memset(mid->sol_redcost, 0, (size_t)nc * sizeof(double));
    memset(mid->sol_farkas, 0, (size_t)nr * sizeof(double));
    memset(mid->sol_ray, 0, (size_t)nc * sizeof(double));

    for (int64_t ri = 0; ri < red->num_row; ri++) {
        const int64_t i = p->agg_orig_row[ri];
        mid->sol_dual[i] = red->sol_dual[ri];
        mid->sol_row_status[i] = red->sol_row_status[ri];
        mid->sol_farkas[i] = red->sol_farkas[ri];
    }
    for (int64_t rj = 0; rj < red->num_col; rj++) {
        const int64_t j = p->agg_orig_col[rj];
        mid->sol_col[j] = red->sol_col[rj];
        mid->sol_redcost[j] = red->sol_redcost[rj];
        mid->sol_col_status[j] = red->sol_col_status[rj];
        mid->sol_ray[j] = red->sol_ray[rj];
    }

    if (red->solve_status != JAOS_SOLVE_OPTIMAL) {
        memset(mid->sol_col, 0, (size_t)nc * sizeof(double));
        memset(mid->sol_dual, 0, (size_t)nr * sizeof(double));
        memset(mid->sol_redcost, 0, (size_t)nc * sizeof(double));
        const bool lift_farkas =
            red->solve_status == JAOS_SOLVE_INFEASIBLE && red->farkas_ok;
        const bool lift_ray =
            red->solve_status == JAOS_SOLVE_UNBOUNDED && red->ray_ok;
        for (int64_t t = p->agg_len - 1; t >= 0; t--) {
            const jm_agg_rec *rec = &p->agg[t];
            if (lift_farkas)
                mid->sol_farkas[rec->row] = ag_published(
                    ag_col_rest(p, rec, 0.0, mid->sol_farkas) / rec->coef);
            if (lift_ray)
                mid->sol_ray[rec->col] = ag_published(
                    ag_row_rest(p, rec, 0.0, mid->sol_ray) / rec->coef);
        }
        mid->farkas_ok = lift_farkas;
        mid->ray_ok = lift_ray;
        if (red->start_col_status != nullptr &&
            red->start_row_status != nullptr) {
            for (int64_t ri = 0; ri < red->num_row; ri++)
                mid->sol_row_status[p->agg_orig_row[ri]] =
                    red->start_row_status[ri];
            for (int64_t rj = 0; rj < red->num_col; rj++)
                mid->sol_col_status[p->agg_orig_col[rj]] =
                    red->start_col_status[rj];
            for (int64_t t = 0; t < p->agg_len; t++) {
                mid->sol_col_status[p->agg[t].col] = JAOS_BASIS_BASIC;
                mid->sol_row_status[p->agg[t].row] = JAOS_BASIS_AT_LOWER;
            }
            est = jm_model_remember_basis(mid);
            if (est != JAOS_OK)
                return est;
        } else {
            free(mid->start_col_status);
            free(mid->start_row_status);
            mid->start_col_status = nullptr;
            mid->start_row_status = nullptr;
        }
        return JAOS_OK;
    }

    for (int64_t t = p->agg_len - 1; t >= 0; t--) {
        const jm_agg_rec *rec = &p->agg[t];
        const double xv = ag_row_rest(p, rec, rec->rhs, mid->sol_col) /
                          rec->coef;
        const double yv = ag_col_rest(p, rec, rec->cost, mid->sol_dual) /
                          rec->coef;
        mid->sol_col[rec->col] = ag_published(xv);
        mid->sol_redcost[rec->col] = 0.0;
        mid->sol_dual[rec->row] = ag_published(yv);
        mid->sol_col_status[rec->col] = JAOS_BASIS_BASIC;
        mid->sol_row_status[rec->row] = JAOS_BASIS_AT_LOWER;
    }

    ag_acc *act = jm_calloc_array(nr, sizeof *act);
    if (nr > 0 && act == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t j = 0; j < nc; j++) {
        const double xv = mid->sol_col[j];
        if (xv == 0.0)
            continue;
        for (int64_t k = mid->a_start[j]; k < mid->a_start[j + 1]; k++) {
            const double t = mid->a_value[k] * xv;
            ag_acc_add(&act[mid->a_index[k]], t);
            const double e = jm_two_product_residue(mid->a_value[k], xv, t);
            if (e != 0.0)
                ag_acc_add(&act[mid->a_index[k]], e);
        }
    }
    for (int64_t i = 0; i < nr; i++)
        mid->sol_row[i] = ag_published(ag_acc_value(&act[i]));
    free(act);

    jm_model_publish_objective(mid);
    est = jm_model_remember_basis(mid);
    if (est != JAOS_OK)
        return est;
    mid->sol_basis_ok = true;
    return JAOS_OK;
}
