/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double ps_published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

static int64_t ps_restore_index(int64_t index, int64_t dim)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    return (index + 1) % dim;
#else
    (void)dim;
    return index;
#endif
}

#ifndef JAOS_PRESOLVE_ROUND_ULPS_VALUE
#define JAOS_PRESOLVE_ROUND_ULPS_VALUE 8
#endif
constexpr double PRESOLVE_ROUND_ULPS = JAOS_PRESOLVE_ROUND_ULPS_VALUE;

#ifndef JAOS_PRESOLVE_ROUNDS_VALUE
#define JAOS_PRESOLVE_ROUNDS_VALUE 16
#endif
constexpr int64_t JM_PRESOLVE_ROUNDS = JAOS_PRESOLVE_ROUNDS_VALUE;

typedef struct {
    double sum, comp;
} ps_acc;

static void ps_acc_add(ps_acc *a, double t)
{
    const double s = a->sum + t;
    a->comp += (fabs(a->sum) >= fabs(t)) ? ((a->sum - s) + t)
                                         : ((t - s) + a->sum);
    a->sum = s;
}

static double ps_acc_value(const ps_acc *a)
{
    return a->sum + a->comp;
}

static void ps_bound_shift(ps_acc *a, double *cur, double t)
{
    ps_acc_add(a, -t);
    if (!isfinite(a->sum) || !isfinite(a->comp))
        a->comp = 0.0;
    *cur = a->sum + a->comp;
}

typedef struct {
    int64_t *rs;
    int64_t *ridx;
    double  *rval;
} ps_rowwise;

static bool ps_build_rowwise(const jaos_model *m, ps_rowwise *rw)
{
    rw->rs = jm_calloc_array(m->num_row + 1, sizeof *rw->rs);
    rw->ridx = jm_alloc_array(m->num_nz, sizeof *rw->ridx);
    rw->rval = jm_alloc_array(m->num_nz, sizeof *rw->rval);
    int64_t *cursor = jm_alloc_array(m->num_row, sizeof *cursor);
    if (!rw->rs || !rw->ridx || !rw->rval || !cursor) {
        free(rw->rs); free(rw->ridx); free(rw->rval); free(cursor);
        rw->rs = nullptr; rw->ridx = nullptr; rw->rval = nullptr;
        return false;
    }
    for (int64_t k = 0; k < m->num_nz; k++)
        rw->rs[m->a_index[k] + 1]++;
    for (int64_t i = 0; i < m->num_row; i++)
        rw->rs[i + 1] += rw->rs[i];
    memcpy(cursor, rw->rs, (size_t)m->num_row * sizeof *cursor);
    for (int64_t j = 0; j < m->num_col; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const int64_t dst = cursor[i]++;
            rw->ridx[dst] = j;
            rw->rval[dst] = m->a_value[k];
        }
    free(cursor);
    return true;
}

static void ps_free_rowwise(ps_rowwise *rw)
{
    free(rw->rs); free(rw->ridx); free(rw->rval);
}

typedef struct {
    double lo_sum, hi_sum;
    int64_t lo_inf, hi_inf;
    double traffic;
} ps_range;

static ps_range ps_row_range(const ps_rowwise *rw, int64_t i,
                             const double *cl, const double *cu,
                             const bool *col_dead, int64_t skip)
{
    ps_acc lo = {0.0, 0.0}, hi = {0.0, 0.0}, tr = {0.0, 0.0};
    ps_range r = {0.0, 0.0, 0, 0, 0.0};

    for (int64_t k = rw->rs[i]; k < rw->rs[i + 1]; k++) {
        const int64_t j = rw->ridx[k];
        if (col_dead[j] || j == skip)
            continue;
        const double a = rw->rval[k];
        if (a == 0.0)
            continue;

        const double t_lo = a > 0.0 ? cl[j] : cu[j];
        const double t_hi = a > 0.0 ? cu[j] : cl[j];
        if (isfinite(t_lo)) {
            ps_acc_add(&lo, a * t_lo);
            ps_acc_add(&tr, fabs(a * t_lo));
        } else {
            r.lo_inf++;
        }
        if (isfinite(t_hi)) {
            ps_acc_add(&hi, a * t_hi);
            ps_acc_add(&tr, fabs(a * t_hi));
        } else {
            r.hi_inf++;
        }
    }

    r.lo_sum = ps_acc_value(&lo);
    r.hi_sum = ps_acc_value(&hi);
    r.traffic = ps_acc_value(&tr);
    return r;
}

static double ps_min_act(const ps_range *r)
{
    return r->lo_inf > 0 ? -HUGE_VAL : r->lo_sum;
}

static double ps_max_act(const ps_range *r)
{
    return r->hi_inf > 0 ? HUGE_VAL : r->hi_sum;
}

static double ps_round_tol(double scale)
{
    return PRESOLVE_ROUND_ULPS * DBL_EPSILON * (scale > 1.0 ? scale : 1.0);
}

static double ps_row_tol(const ps_range *r)
{
    return 8.0 * DBL_EPSILON * (r->traffic > 1.0 ? r->traffic : 1.0);
}

#ifndef JAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE
#define JAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE 8
#endif
constexpr double PRESOLVE_IMPLIED_FREE_ULPS =
    JAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE;

static double ps_implied_free_margin(double scale)
{
    return PRESOLVE_IMPLIED_FREE_ULPS * DBL_EPSILON *
           (scale > 1.0 ? scale : 1.0);
}

static double ps_bound_scale(double a, double b)
{
    double s = 1.0;
    if (isfinite(a) && fabs(a) > s)
        s = fabs(a);
    if (isfinite(b) && fabs(b) > s)
        s = fabs(b);
    return s;
}

bool jm_box_inverted(double lower, double upper)
{
    return lower > upper + ps_round_tol(ps_bound_scale(lower, upper));
}

#ifndef NDEBUG

static bool ps_traffic_usable(double rl, double ru, double traffic)
{
    if (!isfinite(rl) && !isfinite(ru))
        return true;
    return isfinite(traffic);
}
#endif

void jm_presolve_init(jm_presolve *p)
{
    memset(p, 0, sizeof *p);

    p->proof_index = -1;
}

void jm_presolve_free(jm_presolve *p)
{

    free(p->reduced.col_cost);
    free(p->reduced.col_lower);
    free(p->reduced.col_upper);
    free(p->reduced.row_lower);
    free(p->reduced.row_upper);
    free(p->reduced.a_start);
    free(p->reduced.a_index);
    free(p->reduced.a_value);
    free(p->reduced.ar_start);
    free(p->reduced.ar_index);
    free(p->reduced.ar_value);
    free(p->reduced.row_scale);
    free(p->reduced.col_scale);
    free(p->reduced.sol_col);
    free(p->reduced.sol_row);
    free(p->reduced.sol_dual);
    free(p->reduced.sol_redcost);
    free(p->reduced.sol_col_status);
    free(p->reduced.sol_row_status);
    free(p->reduced.sol_farkas);
    free(p->reduced.sol_ray);
    free(p->reduced.start_col_status);
    free(p->reduced.start_row_status);

    free(p->orig_col);
    free(p->orig_row);
    free(p->col_map);
    free(p->row_map);
    free(p->arena);
    memset(p, 0, sizeof *p);
}

static bool ps_push(jm_presolve *p, jm_presolve_rec rec)
{

    assert(rec.index >= 0);
    if (!JM_GROW(p->arena, p->arena_cap, p->arena_len + 1))
        return false;
    p->arena[p->arena_len++] = rec;
    return true;
}

static bool ps_empty_col_value(double cl, double cu, double cost,
                               double *out_v)
{
    if (cost > 0.0) {
        if (isfinite(cl)) { *out_v = cl; return true; }
        return false;
    }
    if (cost < 0.0) {
        if (isfinite(cu)) { *out_v = cu; return true; }
        return false;
    }
    if (isfinite(cl)) { *out_v = cl; return true; }
    if (isfinite(cu)) { *out_v = cu; return true; }
    *out_v = 0.0;
    return true;
}

JAOS_NODISCARD jaos_status jm_presolve_run(const jaos_model *m, jm_presolve *p,
                                           jm_work *w)
{
    const int64_t nr = m->num_row, nc = m->num_col;

    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    bool *col_dead = jm_calloc_array(nc, sizeof *col_dead);
    bool *row_dead = jm_calloc_array(nr, sizeof *row_dead);
    bool *row_frozen = jm_calloc_array(nr, sizeof *row_frozen);

    bool *col_pending_dual = jm_calloc_array(nc, sizeof *col_pending_dual);

    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
    double *cur_cl = jm_alloc_array(nc, sizeof *cur_cl);
    double *cur_cu = jm_alloc_array(nc, sizeof *cur_cu);

    double *cur_cost = jm_alloc_array(nc, sizeof *cur_cost);
    double *cur_rl = jm_alloc_array(nr, sizeof *cur_rl);
    double *cur_ru = jm_alloc_array(nr, sizeof *cur_ru);

    ps_acc *cur_rl_acc = jm_calloc_array(nr, sizeof *cur_rl_acc);
    ps_acc *cur_ru_acc = jm_calloc_array(nr, sizeof *cur_ru_acc);
    int64_t *col_deg = jm_alloc_array(nc, sizeof *col_deg);
    int64_t *row_deg = jm_alloc_array(nr, sizeof *row_deg);
    ps_rowwise rw = {0};

    bool ok = col_dead && row_dead && row_frozen && col_pending_dual &&
              row_traffic && cur_cl && cur_cu && cur_cost &&
              cur_rl && cur_ru && cur_rl_acc && cur_ru_acc &&
              col_deg && row_deg && ps_build_rowwise(m, &rw);

    jaos_status ret = JAOS_OK;
    if (!ok) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    for (int64_t j = 0; j < nc; j++) {
        cur_cl[j] = m->col_lower[j];
        cur_cu[j] = m->col_upper[j];
        cur_cost[j] = m->col_cost[j];
        col_deg[j] = m->a_start[j + 1] - m->a_start[j];
    }
    for (int64_t i = 0; i < nr; i++) {
        cur_rl[i] = m->row_lower[i];
        cur_ru[i] = m->row_upper[i];
        cur_rl_acc[i].sum = cur_rl[i];
        cur_ru_acc[i].sum = cur_ru[i];
        row_deg[i] = rw.rs[i + 1] - rw.rs[i];
    }

    p->outcome = JM_PRESOLVE_NONE;

    {
    int64_t cap = nr + nc + 1;
    if (JM_PRESOLVE_ROUNDS < cap)
        cap = JM_PRESOLVE_ROUNDS;
    int64_t rounds_done = 0;

    for (int64_t round = 0; round < cap; round++) {
        bool changed = false;

        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || row_frozen[i])
                continue;

            if (row_deg[i] == 0) {

                double etol = 0.0;
                assert(ps_traffic_usable(cur_rl[i], cur_ru[i], row_traffic[i]));
                if (row_traffic[i] > 0.0) {

                    assert(isfinite(row_traffic[i]));
                    const double scale = isfinite(row_traffic[i])
                        ? row_traffic[i]
                        : ps_bound_scale(cur_rl[i], cur_ru[i]);

                    etol = ps_round_tol(scale);
                }
                if (cur_rl[i] > etol || cur_ru[i] < -etol) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;

                    p->proof_index = i;
                    p->proof_sign = cur_rl[i] > etol ? 1.0 : -1.0;
                    goto done;
                }
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_EMPTY_ROW, .index = i })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                row_dead[i] = true;
                p->counts.empty_row++;
                changed = true;
                continue;
            }

            if (row_deg[i] == 1) {
                int64_t j = -1;
                double a = 0.0;
                for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++) {
                    if (!col_dead[rw.ridx[k]]) {
                        j = rw.ridx[k];
                        a = rw.rval[k];
                        break;
                    }
                }
                assert(j >= 0);

                if (col_deg[j] == 1 && cur_cost[j] == 0.0 &&
                    !isfinite(cur_cl[j]) && !isfinite(cur_cu[j])) {
                    jm_work_add(w, JM_WORK_NONZERO);
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_FREE_COL_SINGLETON,
                            .index = i, .index2 = j, .coef = a,
                            .lo = cur_rl[i], .hi = cur_ru[i] })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    col_dead[j] = true;
                    row_dead[i] = true;
                    p->counts.free_col_singleton++;
                    changed = true;
                    continue;
                }

                double implied_lo, implied_hi;
                if (a > 0.0) {
                    implied_lo = isfinite(cur_rl[i]) ? cur_rl[i] / a : -HUGE_VAL;
                    implied_hi = isfinite(cur_ru[i]) ? cur_ru[i] / a : HUGE_VAL;
                } else {
                    implied_lo = isfinite(cur_ru[i]) ? cur_ru[i] / a : -HUGE_VAL;
                    implied_hi = isfinite(cur_rl[i]) ? cur_rl[i] / a : HUGE_VAL;
                }

                const bool tightens_lo = implied_lo > cur_cl[j];
                const bool tightens_hi = implied_hi < cur_cu[j];
                const double new_lo = implied_lo > cur_cl[j] ? implied_lo
                                                             : cur_cl[j];
                const double new_hi = implied_hi < cur_cu[j] ? implied_hi
                                                             : cur_cu[j];

                jm_work_add(w, JM_WORK_NONZERO);

                double fold_lo = new_lo, fold_hi = new_hi;

                assert(ps_traffic_usable(cur_rl[i], cur_ru[i], row_traffic[i]));
                double bscale = ps_bound_scale(new_lo, new_hi);
                if (row_traffic[i] > 0.0 && isfinite(row_traffic[i])) {
                    const double tscale = row_traffic[i] / fabs(a);
                    if (tscale > bscale)
                        bscale = tscale;
                }

                const double btol = ps_round_tol(bscale);

                if (new_lo > new_hi + btol) {

                    p->outcome = JM_PRESOLVE_INFEASIBLE;
                    if (implied_lo > cur_cu[j]) {
                        p->proof_index = i;
                        p->proof_sign = a > 0.0 ? 1.0 : -1.0;
                    } else if (implied_hi < cur_cl[j]) {
                        p->proof_index = i;
                        p->proof_sign = a > 0.0 ? -1.0 : 1.0;
                    }
                    goto done;
                }
                if (new_lo > new_hi) {

                    const double mid = 0.5 * (new_lo + new_hi);
                    if (cur_cl[j] <= cur_cu[j]) {
                        fold_lo = fold_hi = mid < cur_cl[j] ? cur_cl[j]
                                          : mid > cur_cu[j] ? cur_cu[j]
                                                            : mid;
                        assert(fold_lo >= cur_cl[j] && fold_hi <= cur_cu[j]);
                    } else {

                        fold_lo = fold_hi = mid;
                    }
                }

                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_SINGLETON_ROW,
                        .index = i, .index2 = j, .coef = a,
                        .lo = fold_lo, .hi = fold_hi,
                        .row_tightens_lo = tightens_lo,
                        .row_tightens_hi = tightens_hi })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                cur_cl[j] = fold_lo;
                cur_cu[j] = fold_hi;
                row_dead[i] = true;
                col_deg[j]--;
                col_pending_dual[j] = true;
                p->counts.singleton_row++;
                changed = true;
            }
        }

        for (int64_t j = 0; j < nc; j++) {
            if (col_dead[j])
                continue;

            if (cur_cl[j] == cur_cu[j]) {
                const double v = cur_cl[j];
                p->reduced.obj_offset += cur_cost[j] * v;
                jm_work_add(w, (m->a_start[j + 1] - m->a_start[j]) *
                               JM_WORK_NONZERO);
                for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                    const int64_t i = m->a_index[k];
                    if (row_dead[i])
                        continue;
                    const double t = m->a_value[k] * v;
                    ps_bound_shift(&cur_rl_acc[i], &cur_rl[i], t);
                    ps_bound_shift(&cur_ru_acc[i], &cur_ru[i], t);
                    row_traffic[i] += fabs(t);
                    row_deg[i]--;
                }
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_FIXED_COL, .index = j,
                        .value = v, .cost = cur_cost[j] })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                col_dead[j] = true;
                p->counts.fixed_col++;
                changed = true;
                continue;
            }

            if (col_deg[j] == 0) {
                double v;
                if (!ps_empty_col_value(cur_cl[j], cur_cu[j],
                                        sigma * cur_cost[j], &v)) {
                    p->outcome = JM_PRESOLVE_NONE;
                    p->proof_index = -1;
                    goto done;
                }
                p->reduced.obj_offset += cur_cost[j] * v;
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_EMPTY_COL, .index = j,
                        .value = v, .cost = cur_cost[j] })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                col_dead[j] = true;
                p->counts.empty_col++;
                changed = true;
                continue;
            }

            if (col_deg[j] == 1 && cur_cost[j] == 0.0) {

                assert(cur_cost[j] == 0.0);
                int64_t i = -1;
                double a = 0.0;
                for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                    if (!row_dead[m->a_index[k]]) {
                        i = m->a_index[k];
                        a = m->a_value[k];
                        break;
                    }
                }
                assert(i >= 0);

                const bool free_col = !isfinite(cur_cl[j]) &&
                                      !isfinite(cur_cu[j]);

                if (free_col && !row_frozen[i] && row_deg[i] == 1) {

                    jm_work_add(w, JM_WORK_NONZERO);
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_FREE_COL_SINGLETON,
                            .index = i, .index2 = j, .coef = a,
                            .lo = cur_rl[i], .hi = cur_ru[i] })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    col_dead[j] = true;
                    row_dead[i] = true;
                    p->counts.free_col_singleton++;
                    changed = true;
                    continue;
                }
                if (!free_col) {

                    jm_work_add(w, JM_WORK_NONZERO);
                    const double c1 = a * cur_cl[j], c2 = a * cur_cu[j];
                    const double cmin = c1 < c2 ? c1 : c2;
                    const double cmax = c1 > c2 ? c1 : c2;
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_SINGLETON_COL,
                            .index = i, .index2 = j, .coef = a,
                            .lo = cur_cl[j], .hi = cur_cu[j],
                            .row_lo = cur_rl[i], .row_hi = cur_ru[i] })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    const bool lo_absorbs = isfinite(cur_rl[i]);
                    const bool hi_absorbs = isfinite(cur_ru[i]);

                    if (lo_absorbs)
                        ps_bound_shift(&cur_rl_acc[i], &cur_rl[i], cmax);
                    if (hi_absorbs)
                        ps_bound_shift(&cur_ru_acc[i], &cur_ru[i], cmin);

                    double moved = 0.0;
                    if (lo_absorbs && isfinite(cmax) && fabs(cmax) > moved)
                        moved = fabs(cmax);
                    if (hi_absorbs && isfinite(cmin) && fabs(cmin) > moved)
                        moved = fabs(cmin);

                    assert(lo_absorbs || !isfinite(cur_rl[i]));
                    assert(hi_absorbs || !isfinite(cur_ru[i]));
                    assert(moved >= 0.0 && isfinite(moved));
                    row_traffic[i] += moved;
                    col_dead[j] = true;
                    row_deg[i]--;
                    row_frozen[i] = true;
                    p->counts.singleton_col++;
                    changed = true;
                    continue;
                }

            }

            if (col_deg[j] == 1 &&
                m->a_start[j + 1] - m->a_start[j] == 1) {
                const int64_t i = m->a_index[m->a_start[j]];
                const double a = m->a_value[m->a_start[j]];

                if (a != 0.0 && !row_dead[i] && !row_frozen[i] &&
                    m->row_lower[i] == m->row_upper[i] &&
                    isfinite(cur_rl[i]) && cur_rl[i] == cur_ru[i]) {
                    const double b = cur_rl[i];
                    const ps_range rg =
                        ps_row_range(&rw, i, cur_cl, cur_cu, col_dead, j);
                    jm_work_add(w, row_deg[i] * JM_WORK_NONZERO);

                    const double minact = ps_min_act(&rg);
                    const double maxact = ps_max_act(&rg);

                    const double loside = isfinite(maxact) ? b - maxact
                                                           : -HUGE_VAL;
                    const double upside = isfinite(minact) ? b - minact
                                                           : HUGE_VAL;
                    double ilo, iup;
                    if (a > 0.0) {
                        ilo = loside / a;
                        iup = upside / a;
                    } else {
                        ilo = upside / a;
                        iup = loside / a;
                    }

                    const double margin = ps_implied_free_margin(
                        ps_bound_scale(b, rg.traffic) / fabs(a));
                    const bool lo_ok = !isfinite(cur_cl[j]) ||
                                       ilo >= cur_cl[j] + margin;
                    const bool up_ok = !isfinite(cur_cu[j]) ||
                                       iup <= cur_cu[j] - margin;

                    if (lo_ok && up_ok) {
                        const double yi = cur_cost[j] / a;
                        if (!ps_push(p, (jm_presolve_rec){
                                .tag = JM_PS_IMPLIED_FREE_COL,
                                .index = i, .index2 = j, .coef = a,
                                .value = b, .cost = cur_cost[j],
                                .lo = ilo, .hi = iup })) {
                            ret = JAOS_ERR_OUT_OF_MEMORY;
                            goto cleanup_scratch;
                        }
                        p->reduced.obj_offset += yi * b;
                        jm_work_add(w, row_deg[i] * JM_WORK_NONZERO);
                        for (int64_t kk = rw.rs[i]; kk < rw.rs[i + 1]; kk++) {
                            const int64_t k2 = rw.ridx[kk];
                            if (k2 == j || col_dead[k2])
                                continue;
                            cur_cost[k2] -= yi * rw.rval[kk];
                            col_deg[k2]--;
                        }
                        col_dead[j] = true;
                        row_dead[i] = true;
                        p->counts.implied_free_col++;
                        changed = true;
                        continue;
                    }
                }
            }
        }

        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || row_frozen[i] || row_deg[i] < 2)
                continue;

            const ps_range rg =
                ps_row_range(&rw, i, cur_cl, cur_cu, col_dead, -1);
            jm_work_add(w, row_deg[i] * JM_WORK_NONZERO);

            const double rtol = ps_row_tol(&rg);
            const double min_act = ps_min_act(&rg);
            const double max_act = ps_max_act(&rg);
            const double rl = cur_rl[i], ru = cur_ru[i];

            assert(ps_traffic_usable(rl, ru, row_traffic[i]));

            double iact = rg.traffic > 1.0 ? rg.traffic : 1.0;
            double ibnd = 1.0;
            if (isfinite(row_traffic[i]) && row_traffic[i] > ibnd)
                ibnd = row_traffic[i];
            const double itol = 8.0 * DBL_EPSILON * iact +
                                8.0 * DBL_EPSILON * ibnd;
            if ((isfinite(ru) && min_act > ru + itol) ||
                (isfinite(rl) && max_act < rl - itol)) {
                p->outcome = JM_PRESOLVE_INFEASIBLE;

                p->proof_index = i;
                p->proof_sign =
                    (isfinite(ru) && min_act > ru + itol) ? -1.0 : 1.0;
                goto done;
            }

            const bool force_hi = isfinite(ru) && min_act >= ru - rtol;
            const bool force_lo = isfinite(rl) && max_act <= rl + rtol;
            if (force_hi || force_lo) {

                bool at_own_bounds = true;
                for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++) {
                    const int64_t j = rw.ridx[k];
                    if (col_dead[j] || rw.rval[k] == 0.0)
                        continue;
                    if (col_pending_dual[j]) {
                        at_own_bounds = false;
                        break;
                    }
                    const bool want_lo =
                        force_hi ? (rw.rval[k] > 0.0) : (rw.rval[k] < 0.0);
                    if (want_lo ? (cur_cl[j] != m->col_lower[j])
                                : (cur_cu[j] != m->col_upper[j])) {
                        at_own_bounds = false;
                        break;
                    }
                }

                if (at_own_bounds) {
                    int64_t nfix = 0;
                    bool oom = false;
                    for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++) {
                        const int64_t j = rw.ridx[k];
                        if (col_dead[j])
                            continue;
                        if (rw.rval[k] == 0.0) {

                            col_deg[j]--;
                            col_pending_dual[j] = true;
                            continue;
                        }
                        const bool want_lo = force_hi ? (rw.rval[k] > 0.0)
                                                      : (rw.rval[k] < 0.0);
                        const double v = want_lo ? cur_cl[j] : cur_cu[j];
                        assert(isfinite(v));

                        assert(v == m->col_lower[j] || v == m->col_upper[j]);

                        p->reduced.obj_offset += cur_cost[j] * v;
                        jm_work_add(w, (m->a_start[j + 1] - m->a_start[j]) *
                                       JM_WORK_NONZERO);
                        for (int64_t kk = m->a_start[j];
                             kk < m->a_start[j + 1]; kk++) {
                            const int64_t ii = m->a_index[kk];
                            if (row_dead[ii])
                                continue;
                            const double t = m->a_value[kk] * v;
                            ps_bound_shift(&cur_rl_acc[ii], &cur_rl[ii], t);
                            ps_bound_shift(&cur_ru_acc[ii], &cur_ru[ii], t);
                            row_traffic[ii] += fabs(t);
                            row_deg[ii]--;
                        }

                        if (!ps_push(p, (jm_presolve_rec){
                                .tag = JM_PS_FIXED_COL, .index = j,
                                .value = v, .cost = cur_cost[j],
                                .coef = rw.rval[k] })) {
                            oom = true;
                            break;
                        }
                        col_dead[j] = true;
                        nfix++;
                    }
                    if (oom) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }

                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_FORCING_ROW, .index = i,
                            .index2 = nfix,
                            .row_tightens_hi = force_hi })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    row_dead[i] = true;
                    p->counts.forcing_row++;
                    changed = true;
                    continue;
                }
            }

            if ((!isfinite(rl) || min_act >= rl - rtol) &&
                (!isfinite(ru) || max_act <= ru + rtol)) {
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_REDUNDANT_ROW, .index = i })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++)
                    if (!col_dead[rw.ridx[k]])
                        col_deg[rw.ridx[k]]--;
                row_dead[i] = true;
                p->counts.redundant_row++;
                changed = true;
                continue;
            }

        }

        if (!changed)
            break;
        rounds_done++;
    }
    p->counts.rounds = rounds_done;

    assert(p->counts.rounds <= nr + nc + 1);
    }

#ifndef NDEBUG

    for (int64_t i = 0; i < nr; i++) {
        assert(ps_traffic_usable(cur_rl[i], cur_ru[i], row_traffic[i]));

        assert(row_traffic[i] >= 0.0);
    }

    for (int64_t j = 0; j < nc; j++) {
        if (col_dead[j] || m->col_lower[j] > m->col_upper[j])
            continue;
        assert(cur_cl[j] >= m->col_lower[j]);
        assert(cur_cu[j] <= m->col_upper[j]);
    }
#endif

    for (int64_t i = 0; i < nr; i++) {
        if (row_dead[i] || !row_frozen[i])
            continue;

        const ps_range rg = ps_row_range(&rw, i, cur_cl, cur_cu, col_dead,
                                         -1);

        jm_work_add(w, (rw.rs[i + 1] - rw.rs[i]) * JM_WORK_NONZERO);

        double ract = 1.0;
        if (isfinite(rg.traffic) && rg.traffic > ract)
            ract = rg.traffic;
        double rbnd = 1.0;
        if (isfinite(row_traffic[i]) && row_traffic[i] > rbnd)
            rbnd = row_traffic[i];
        const double rtol = ps_round_tol(ract) + ps_round_tol(rbnd);
        const double min_act = ps_min_act(&rg);
        const double max_act = ps_max_act(&rg);
        if ((isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol) ||
            (isfinite(cur_rl[i]) && max_act < cur_rl[i] - rtol)) {
            p->outcome = JM_PRESOLVE_INFEASIBLE;

            p->proof_index = i;
            p->proof_sign =
                (isfinite(cur_ru[i]) && min_act > cur_ru[i] + rtol) ? -1.0
                                                                    : 1.0;
            goto done;
        }
    }

    {

    int64_t rcol = 0, rrow = 0;
    for (int64_t j = 0; j < nc; j++)
        if (!col_dead[j])
            rcol++;
    for (int64_t i = 0; i < nr; i++)
        if (!row_dead[i])
            rrow++;

    p->col_map  = jm_alloc_array(nc, sizeof *p->col_map);
    p->row_map  = jm_alloc_array(nr, sizeof *p->row_map);
    p->orig_col = jm_alloc_array(rcol, sizeof *p->orig_col);
    p->orig_row = jm_alloc_array(rrow, sizeof *p->orig_row);
    if (!p->col_map || !p->row_map || (rcol > 0 && !p->orig_col) ||
        (rrow > 0 && !p->orig_row)) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    int64_t rj = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (col_dead[j]) { p->col_map[j] = -1; continue; }
        p->col_map[j] = rj;
        p->orig_col[rj] = j;
        rj++;
    }
    int64_t ri = 0;
    for (int64_t i = 0; i < nr; i++) {
        if (row_dead[i]) { p->row_map[i] = -1; continue; }
        p->row_map[i] = ri;
        p->orig_row[ri] = i;
        ri++;
    }
    assert(rj == rcol && ri == rrow);

    if (rcol == nc && rrow == nr) {
        p->outcome = JM_PRESOLVE_NONE;
        goto cleanup_scratch;
    }

    const double accumulated_offset = p->reduced.obj_offset;
    p->reduced = *m;
    p->reduced.obj_offset = m->obj_offset + accumulated_offset;
    p->reduced.num_col = rcol;
    p->reduced.num_row = rrow;
    p->reduced.rowwise_valid = false;
    p->reduced.ar_start = nullptr;
    p->reduced.ar_index = nullptr;
    p->reduced.ar_value = nullptr;
    p->reduced.scale_valid = false;
    p->reduced.scale_clamped = false;
    p->reduced.row_scale = nullptr;
    p->reduced.col_scale = nullptr;
    p->reduced.sol_col = nullptr;
    p->reduced.sol_row = nullptr;
    p->reduced.sol_dual = nullptr;
    p->reduced.sol_redcost = nullptr;
    p->reduced.sol_col_status = nullptr;
    p->reduced.sol_row_status = nullptr;
    p->reduced.sol_farkas = nullptr;
    p->reduced.farkas_ok = false;
    p->reduced.sol_ray = nullptr;
    p->reduced.ray_ok = false;
    p->reduced.start_col_status = nullptr;
    p->reduced.start_row_status = nullptr;
    p->reduced.solve_status = JAOS_SOLVE_NOT_RUN;
    p->reduced.objective = 0.0;
    p->reduced.solve_work = 0;
    p->reduced.solve_iters = 0;
    p->reduced.solve_primal_iters = 0;
    p->reduced.solve_phase1_iters = 0;
    p->reduced.solve_time = 0.0;
    p->reduced.err[0] = '\0';

    p->reduced.col_cost  = jm_alloc_array(rcol, sizeof(double));
    p->reduced.col_lower = jm_alloc_array(rcol, sizeof(double));
    p->reduced.col_upper = jm_alloc_array(rcol, sizeof(double));
    p->reduced.row_lower = jm_alloc_array(rrow, sizeof(double));
    p->reduced.row_upper = jm_alloc_array(rrow, sizeof(double));
    p->reduced.a_start   = jm_alloc_array(rcol + 1, sizeof(int64_t));
    if ((rcol > 0 && (!p->reduced.col_cost || !p->reduced.col_lower ||
                      !p->reduced.col_upper)) ||
        (rrow > 0 && (!p->reduced.row_lower || !p->reduced.row_upper)) ||
        !p->reduced.a_start) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    for (int64_t ri2 = 0; ri2 < rrow; ri2++) {
        const int64_t i = p->orig_row[ri2];
        p->reduced.row_lower[ri2] = cur_rl[i];
        p->reduced.row_upper[ri2] = cur_ru[i];
    }

    p->reduced.a_start[0] = 0;
    for (int64_t rj2 = 0; rj2 < rcol; rj2++) {
        const int64_t j = p->orig_col[rj2];
        p->reduced.col_cost[rj2]  = cur_cost[j];
        p->reduced.col_lower[rj2] = cur_cl[j];
        p->reduced.col_upper[rj2] = cur_cu[j];
        int64_t n = 0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            if (!row_dead[m->a_index[k]])
                n++;
        p->reduced.a_start[rj2 + 1] = p->reduced.a_start[rj2] + n;
    }
    p->reduced.num_nz = p->reduced.a_start[rcol];

    p->reduced.a_index = jm_alloc_array(p->reduced.num_nz, sizeof(int64_t));
    p->reduced.a_value = jm_alloc_array(p->reduced.num_nz, sizeof(double));
    if (p->reduced.num_nz > 0 &&
        (!p->reduced.a_index || !p->reduced.a_value)) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    for (int64_t rj2 = 0; rj2 < rcol; rj2++) {
        const int64_t j = p->orig_col[rj2];
        int64_t dst = p->reduced.a_start[rj2];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (row_dead[i])
                continue;
            p->reduced.a_index[dst] = p->row_map[i];

            assert(p->reduced.a_index[dst] >= 0);
            p->reduced.a_value[dst] = m->a_value[k];
            dst++;
        }

        assert(dst == p->reduced.a_start[rj2 + 1]);
    }

    if (m->start_col_status != nullptr && m->start_row_status != nullptr) {
        p->reduced.start_col_status =
            jm_alloc_array(rcol, sizeof *p->reduced.start_col_status);
        p->reduced.start_row_status =
            jm_alloc_array(rrow, sizeof *p->reduced.start_row_status);
        if ((rcol > 0 && !p->reduced.start_col_status) ||
            (rrow > 0 && !p->reduced.start_row_status)) {
            ret = JAOS_ERR_OUT_OF_MEMORY;
            goto cleanup_scratch;
        }
        for (int64_t j = 0; j < nc; j++) {
            const int64_t rjj = p->col_map[j];
            if (rjj >= 0)
                p->reduced.start_col_status[rjj] = m->start_col_status[j];
        }
        for (int64_t i = 0; i < nr; i++) {
            const int64_t rii = p->row_map[i];
            if (rii >= 0)
                p->reduced.start_row_status[rii] = m->start_row_status[i];
        }

        for (int64_t r = 0; r < p->arena_len; r++) {
            const jm_presolve_rec *rec = &p->arena[r];
            if (rec->tag != JM_PS_SINGLETON_ROW)
                continue;
            const int64_t j = rec->index2;
            const int64_t rjj = p->col_map[j];
            if (rjj < 0)
                continue;

            const jaos_basis_status rst = m->start_row_status[rec->index];
            if (rst == JAOS_BASIS_AT_LOWER || rst == JAOS_BASIS_AT_UPPER) {
                const bool row_at_hi = (rst == JAOS_BASIS_AT_UPPER);
                const bool x_at_lo = (rec->coef > 0.0) ? !row_at_hi
                                                        : row_at_hi;
                p->reduced.start_col_status[rjj] =
                    x_at_lo ? JAOS_BASIS_AT_LOWER : JAOS_BASIS_AT_UPPER;
            } else if (m->start_col_status[j] == JAOS_BASIS_BASIC &&
                      m->sol_col != nullptr) {
                if (m->sol_col[j] == cur_cl[j])
                    p->reduced.start_col_status[rjj] = JAOS_BASIS_AT_LOWER;
                else if (m->sol_col[j] == cur_cu[j])
                    p->reduced.start_col_status[rjj] = JAOS_BASIS_AT_UPPER;
            }
        }
    }

    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;
    }

done:

    p->reduced.solve_work = (w != nullptr) ? w->units : 0;

cleanup_scratch:
    free(col_dead); free(row_dead); free(row_frozen);
    free(col_pending_dual); free(row_traffic);
    free(cur_cl); free(cur_cu); free(cur_cost);
    free(cur_rl); free(cur_ru);
    free(cur_rl_acc); free(cur_ru_acc);
    free(col_deg); free(row_deg);
    ps_free_rowwise(&rw);
    return ret;
}

static void ps_row_add(jaos_model *orig, double *rowc, int64_t i, double t)
{
    const double s = orig->sol_row[i];
    const double n = s + t;
    rowc[i] += (fabs(s) >= fabs(t)) ? ((s - n) + t) : ((t - n) + s);
    orig->sol_row[i] = n;
}

static void ps_add_to_other_rows(jaos_model *orig, double *rowc, int64_t j,
                                 int64_t skip_row, double xv)
{
    for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
        const int64_t i = orig->a_index[k];
        if (i == skip_row)
            continue;
        ps_row_add(orig, rowc, i, orig->a_value[k] * xv);
    }
}

static jaos_basis_status ps_fixed_status(const jaos_model *orig, int64_t j,
                                         double value, double dc)
{
    if (orig->col_lower[j] == orig->col_upper[j])
        return dc < 0.0 ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;
    return (value == orig->col_upper[j] && value != orig->col_lower[j])
               ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;
}

static double ps_reduced_cost_now(const jaos_model *orig, int64_t j,
                                  double cost)
{
    double dw = cost;
    for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
        const int64_t i = orig->a_index[k];
        assert(i >= 0 && i < orig->num_row);
        dw -= orig->a_value[k] * orig->sol_dual[i];
    }
    return dw;
}

static void ps_replay_one(jaos_model *orig, const jm_presolve *p, int64_t r,
                          double *rowc)
{
    const jm_presolve_rec *rec = &p->arena[r];

    const double sigma = (orig->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    switch (rec->tag) {
    case JM_PS_FIXED_COL:
    case JM_PS_EMPTY_COL: {
        const int64_t j = ps_restore_index(rec->index, orig->num_col);
        assert(j >= 0 && j < orig->num_col);

        orig->sol_col[j] = ps_published(rec->value);

        const double dw = ps_reduced_cost_now(orig, j, rec->cost);

        if (rec->coef == 0.0) {
            orig->sol_col_status[j] =
                ps_fixed_status(orig, j, rec->value, sigma * dw);
        } else {

#ifndef NDEBUG
            int64_t k = r + 1;
            while (k < p->arena_len && p->arena[k].tag == JM_PS_FIXED_COL &&
                   p->arena[k].coef != 0.0)
                k++;
            assert(k < p->arena_len &&
                   p->arena[k].tag == JM_PS_FORCING_ROW &&
                   p->arena[k].index2 >= k - r);
#endif
        }

        orig->sol_redcost[j] =
            (orig->sol_col_status[j] == JAOS_BASIS_BASIC) ? 0.0
                                                          : ps_published(dw);

        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            ps_row_add(orig, rowc, i, orig->a_value[k] * rec->value);
        }
        break;
    }

    case JM_PS_EMPTY_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        orig->sol_row[i] = 0.0;
        rowc[i] = 0.0;
        orig->sol_dual[i] = 0.0;
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }

    case JM_PS_SINGLETON_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        const double d0 = orig->sol_redcost[j];
        const double v0 = orig->sol_col[j];

        const double dc = sigma * d0;
        const bool nonbasic_j = orig->sol_col_status[j] != JAOS_BASIS_BASIC;
        const bool sign_decides = rec->lo == rec->hi;
        const bool held_by_own_bound =
            (v0 == orig->col_lower[j] && (!sign_decides || dc >= 0.0)) ||
            (v0 == orig->col_upper[j] && (!sign_decides || dc <= 0.0));
        const bool owns_lo = rec->row_tightens_lo && v0 == rec->lo &&
                             (!sign_decides || dc >= 0.0);
        const bool owns_hi = rec->row_tightens_hi && v0 == rec->hi &&
                             (!sign_decides || dc <= 0.0);

        assert(rec->coef != 0.0);

        double y_i;
        if (nonbasic_j && !held_by_own_bound && (owns_lo || owns_hi)) {

            const bool from_lo = owns_lo;

            assert(from_lo ? v0 > orig->col_lower[j]
                           : v0 < orig->col_upper[j]);
            orig->sol_row_status[i] =
                (from_lo == (rec->coef > 0.0)) ? JAOS_BASIS_AT_LOWER
                                               : JAOS_BASIS_AT_UPPER;
            y_i = d0 / rec->coef;
            orig->sol_redcost[j] = 0.0;
            orig->sol_col_status[j] = JAOS_BASIS_BASIC;
        } else {
            y_i = 0.0;
            orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        }
#if defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

        y_i = orig->sol_redcost[j] / rec->coef;
        orig->sol_redcost[j] = orig->sol_redcost[j] - rec->coef * y_i;
#endif
        orig->sol_dual[i] = ps_published(y_i);

        const double xv = orig->sol_col[j];
        orig->sol_row[i] = ps_published(rec->coef * xv);
        rowc[i] = 0.0;

        break;
    }

    case JM_PS_SINGLETON_COL: {
        const int64_t i = rec->index;
        const int64_t j = ps_restore_index(rec->index2, orig->num_col);
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        const double rest = orig->sol_row[i] + rowc[i];

        assert(isfinite(rest));
        const double rl = rec->row_lo, ru = rec->row_hi;
        double lo_j, hi_j;
        if (rec->coef > 0.0) {
            lo_j = isfinite(rl) ? (rl - rest) / rec->coef : -HUGE_VAL;
            hi_j = isfinite(ru) ? (ru - rest) / rec->coef : HUGE_VAL;
        } else {
            lo_j = isfinite(ru) ? (ru - rest) / rec->coef : -HUGE_VAL;
            hi_j = isfinite(rl) ? (rl - rest) / rec->coef : HUGE_VAL;
        }

        const double want_lo = rec->lo > lo_j ? rec->lo : lo_j;
        const double want_hi = rec->hi < hi_j ? rec->hi : hi_j;
        assert(rec->lo <= rec->hi);

        const jaos_basis_status rs = orig->sol_row_status[i];
        double xv;
        if (rs == JAOS_BASIS_AT_LOWER || rs == JAOS_BASIS_AT_UPPER) {
            const bool take_hi = (rs == JAOS_BASIS_AT_LOWER) == (rec->coef > 0.0);
            xv = take_hi ? rec->hi : rec->lo;

            assert(isfinite(xv));
            orig->sol_col_status[j] = take_hi ? JAOS_BASIS_AT_UPPER
                                              : JAOS_BASIS_AT_LOWER;
        } else if (want_lo == rec->lo && isfinite(rec->lo)) {

            xv = rec->lo;
            orig->sol_col_status[j] = JAOS_BASIS_AT_LOWER;
        } else if (want_lo == -HUGE_VAL) {

            assert(isfinite(rec->hi) && isfinite(want_hi));
            if (want_hi == rec->hi) {
                xv = rec->hi;
                orig->sol_col_status[j] = JAOS_BASIS_AT_UPPER;
            } else {
                xv = want_hi;
                orig->sol_col_status[j] = JAOS_BASIS_BASIC;
                orig->sol_row_status[i] = (rec->coef > 0.0)
                                              ? JAOS_BASIS_AT_UPPER
                                              : JAOS_BASIS_AT_LOWER;
            }
        } else if (want_lo >= rec->hi) {

            xv = rec->hi;
            orig->sol_col_status[j] = JAOS_BASIS_AT_UPPER;
        } else {
            xv = want_lo;
            orig->sol_col_status[j] = JAOS_BASIS_BASIC;

            orig->sol_row_status[i] = (rec->coef > 0.0) ? JAOS_BASIS_AT_LOWER
                                                        : JAOS_BASIS_AT_UPPER;
        }
        assert(xv >= rec->lo && xv <= rec->hi);

        orig->sol_col[j] = ps_published(xv);

        orig->sol_redcost[j] = ps_published(-rec->coef * orig->sol_dual[i]);

        if (orig->col_lower[j] == orig->col_upper[j] &&
            orig->sol_col_status[j] != JAOS_BASIS_BASIC)
            orig->sol_col_status[j] =
                ps_fixed_status(orig, j, xv, sigma * orig->sol_redcost[j]);

        ps_row_add(orig, rowc, i, rec->coef * xv);
        ps_add_to_other_rows(orig, rowc, j, i, xv);
        break;
    }

    case JM_PS_FREE_COL_SINGLETON: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        assert(!isfinite(orig->col_lower[j]) &&
               !isfinite(orig->col_upper[j]));

        const double target = isfinite(rec->lo) ? rec->lo :
                              (isfinite(rec->hi) ? rec->hi : 0.0);
        const double xv = target / rec->coef;

        orig->sol_col[j] = ps_published(xv);

        orig->sol_col_status[j] = (xv == 0.0) ? JAOS_BASIS_FREE
                                              : JAOS_BASIS_BASIC;
        orig->sol_redcost[j] = 0.0;
        orig->sol_dual[i] = 0.0;

        ps_row_add(orig, rowc, i, rec->coef * xv);
        ps_add_to_other_rows(orig, rowc, j, i, xv);

        orig->sol_row_status[i] =
            (orig->sol_col_status[j] == JAOS_BASIS_FREE) ? JAOS_BASIS_BASIC
            : isfinite(rec->lo) ? JAOS_BASIS_AT_LOWER : JAOS_BASIS_AT_UPPER;
        break;
    }

    case JM_PS_IMPLIED_FREE_COL: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        const double xv = (rec->value - (orig->sol_row[i] + rowc[i])) /
                          rec->coef;

        const double yi = rec->cost / rec->coef;

        orig->sol_col[j] = ps_published(xv);
        orig->sol_redcost[j] = 0.0;
        orig->sol_dual[i] = ps_published(yi);
        ps_row_add(orig, rowc, i, rec->coef * xv);

        orig->sol_col_status[j] = JAOS_BASIS_BASIC;
        orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
        break;
    }

    case JM_PS_REDUNDANT_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        orig->sol_dual[i] = 0.0;
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }

    case JM_PS_FORCING_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        double y = 0.0;
        int64_t pick = -1;
        for (int64_t t = 1; t <= rec->index2; t++) {

            assert(r - t >= 0);
            const jm_presolve_rec *cr = &p->arena[r - t];
            assert(cr->tag == JM_PS_FIXED_COL);

            assert(cr->coef != 0.0);
            const int64_t j = cr->index;
            assert(j >= 0 && j < orig->num_col);

            orig->sol_col_status[j] = ps_fixed_status(orig, j, cr->value, 0.0);

            const double d0 = ps_reduced_cost_now(orig, j, cr->cost);

            const double lim = sigma * (d0 / cr->coef);
            if (t == 1 || (rec->row_tightens_hi ? (lim < y) : (lim > y))) {
                y = lim;
                pick = j;
            }
        }
        if (rec->row_tightens_hi ? (y > 0.0) : (y < 0.0))
            y = 0.0;

        orig->sol_dual[i] = ps_published(sigma * y);

        if (y == 0.0) {
            orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        } else {
            assert(pick >= 0);
            orig->sol_col_status[pick] = JAOS_BASIS_BASIC;
            orig->sol_row_status[i] = rec->row_tightens_hi
                                          ? JAOS_BASIS_AT_UPPER
                                          : JAOS_BASIS_AT_LOWER;
        }

        for (int64_t t = 1; t <= rec->index2; t++) {
            const jm_presolve_rec *cr = &p->arena[r - t];
            const int64_t j = cr->index;
            if (orig->col_lower[j] != orig->col_upper[j] ||
                orig->sol_col_status[j] == JAOS_BASIS_BASIC)
                continue;
            orig->sol_col_status[j] = ps_fixed_status(
                orig, j, cr->value,
                sigma * ps_reduced_cost_now(orig, j, cr->cost));
        }
        break;
    }
    }
}

static void ps_final_boxes(const jm_presolve *p, double *lo, double *hi)
{
    const jaos_model *orig = p->orig;
    for (int64_t j = 0; j < orig->num_col; j++) {
        lo[j] = orig->col_lower[j];
        hi[j] = orig->col_upper[j];
    }
    for (int64_t r = 0; r < p->arena_len; r++) {
        const jm_presolve_rec *rec = &p->arena[r];
        switch (rec->tag) {
        case JM_PS_SINGLETON_ROW:
            lo[rec->index2] = rec->lo;
            hi[rec->index2] = rec->hi;
            break;
        case JM_PS_FIXED_COL:
        case JM_PS_EMPTY_COL:
            lo[rec->index] = rec->value;
            hi[rec->index] = rec->value;
            break;
        default:
            break;
        }
    }
}

static double ps_column_dot(const jaos_model *orig, const double *y,
                            int64_t j)
{
    double g = 0.0;
    for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++)
        g += orig->a_value[k] * y[orig->a_index[k]];
    return g;
}

JAOS_NODISCARD static bool ps_lift_farkas(const jm_presolve *p, double *y)
{
    const jaos_model *orig = p->orig;
    const int64_t nr = orig->num_row, nc = orig->num_col;

    double *lo = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *lo);
    double *hi = malloc((size_t)(nc > 0 ? nc : 1) * sizeof *hi);
    bool *absorbed = calloc((size_t)(nc > 0 ? nc : 1), sizeof *absorbed);
    if (lo == nullptr || hi == nullptr || absorbed == nullptr) {
        free(lo);
        free(hi);
        free(absorbed);
        return false;
    }
    ps_final_boxes(p, lo, hi);

    for (int64_t r = p->arena_len - 1; r >= 0; r--) {
        const jm_presolve_rec *rec = &p->arena[r];
        switch (rec->tag) {
        case JM_PS_SINGLETON_ROW: {
            const int64_t i = ps_restore_index(rec->index, nr);
            const int64_t j = rec->index2;
            assert(i >= 0 && i < nr && j >= 0 && j < nc);
            if (absorbed[j])
                break;
            const double g = ps_column_dot(orig, y, j);
            if (g == 0.0)
                break;
            const bool up = g > 0.0;
            const double used = up ? hi[j] : lo[j];
            const double own = up ? orig->col_upper[j] : orig->col_lower[j];
            if (used == own)
                break;
            const bool this_row_owns =
                up ? (rec->row_tightens_hi && rec->hi == used)
                   : (rec->row_tightens_lo && rec->lo == used);
            if (!this_row_owns)
                break;

            assert(rec->coef != 0.0);
            y[i] = ps_published(-g / rec->coef);
            absorbed[j] = true;
            break;
        }

        case JM_PS_FORCING_ROW: {
            const int64_t i = ps_restore_index(rec->index, nr);
            assert(i >= 0 && i < nr);
            double yi = 0.0;
            for (int64_t t = 1; t <= rec->index2; t++) {
                assert(r - t >= 0);
                const jm_presolve_rec *cr = &p->arena[r - t];
                assert(cr->tag == JM_PS_FIXED_COL);
                assert(cr->coef != 0.0);
                const int64_t j = cr->index;
                assert(j >= 0 && j < nc);
                const double lim = -ps_column_dot(orig, y, j) / cr->coef;
                if (t == 1)
                    yi = lim;
                else if (rec->row_tightens_hi ? (lim < yi) : (lim > yi))
                    yi = lim;
            }
            if (rec->row_tightens_hi ? (yi > 0.0) : (yi < 0.0))
                yi = 0.0;
            y[i] = ps_published(yi);
            break;
        }

        default:
            break;
        }
    }

    free(lo);
    free(hi);
    free(absorbed);
    return true;
}

static void ps_ray_move(const jaos_model *orig, double *move, int64_t j,
                        double dj)
{
    for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++)
        move[orig->a_index[k]] += orig->a_value[k] * dj;
}

JAOS_NODISCARD static bool ps_lift_ray(const jm_presolve *p, double *d)
{
    const jaos_model *orig = p->orig;
    const int64_t nr = orig->num_row, nc = orig->num_col;

    double *move = calloc((size_t)(nr > 0 ? nr : 1), sizeof *move);
    if (move == nullptr)
        return false;
    for (int64_t j = 0; j < nc; j++)
        if (d[j] != 0.0)
            ps_ray_move(orig, move, j, d[j]);

    for (int64_t r = p->arena_len - 1; r >= 0; r--) {
        const jm_presolve_rec *rec = &p->arena[r];
        switch (rec->tag) {
        case JM_PS_SINGLETON_COL: {
            const int64_t i = rec->index;
            const int64_t j = ps_restore_index(rec->index2, nc);
            assert(i >= 0 && i < nr && j >= 0 && j < nc);
            const double mi = move[i];
            if (mi == 0.0)
                break;
            const double side = mi > 0.0 ? orig->row_upper[i]
                                         : orig->row_lower[i];
            if (!isfinite(side))
                break;
            assert(rec->coef != 0.0);
            const double dj = -mi / rec->coef;
            const double open = dj > 0.0 ? orig->col_upper[j]
                                         : orig->col_lower[j];
            if (isfinite(open))
                break;
            d[j] = ps_published(dj);
            ps_ray_move(orig, move, j, dj);
            break;
        }

        case JM_PS_IMPLIED_FREE_COL:
        case JM_PS_FREE_COL_SINGLETON: {
            const int64_t i = ps_restore_index(rec->index, nr);
            const int64_t j = rec->index2;
            assert(i >= 0 && i < nr && j >= 0 && j < nc);
            const double mi = move[i];
            if (mi == 0.0)
                break;
            assert(rec->coef != 0.0);
            const double dj = -mi / rec->coef;

            const double open = dj > 0.0 ? orig->col_upper[j]
                                         : orig->col_lower[j];
            if (isfinite(open))
                break;
            d[j] = ps_published(dj);
            ps_ray_move(orig, move, j, dj);
            break;
        }

        default:
            break;
        }
    }

    free(move);
    return true;
}

#ifndef NDEBUG

static void ps_verify_row_activities(const jaos_model *orig)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    (void)orig;
    return;
#else
    if (orig->solve_status != JAOS_SOLVE_OPTIMAL)
        return;

    for (int64_t j = 0; j < orig->num_col; j++)
        assert(orig->sol_col_status[j] != JAOS_BASIS_FREE ||
               (!isfinite(orig->col_lower[j]) && !isfinite(orig->col_upper[j])));
    for (int64_t i = 0; i < orig->num_row; i++)
        assert(orig->sol_row_status[i] != JAOS_BASIS_FREE ||
               (!isfinite(orig->row_lower[i]) && !isfinite(orig->row_upper[i])));

    if (orig->num_row <= 0 || orig->sol_row == nullptr ||
        orig->sol_col == nullptr)
        return;

    double *act = calloc((size_t)orig->num_row, sizeof *act);
    double *traffic = calloc((size_t)orig->num_row, sizeof *traffic);
    int64_t *nnz = calloc((size_t)orig->num_row, sizeof *nnz);
    if (act == nullptr || traffic == nullptr || nnz == nullptr) {
        free(act);
        free(traffic);
        free(nnz);
        return;
    }

    for (int64_t j = 0; j < orig->num_col; j++) {
        const double xv = orig->sol_col[j];
        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            const double t = orig->a_value[k] * xv;
            act[i] += t;
            traffic[i] += fabs(t);
            nnz[i]++;
        }
    }

    for (int64_t i = 0; i < orig->num_row; i++) {
        if (orig->sol_row_status[i] != JAOS_BASIS_BASIC)
            continue;
        const double w = ps_round_tol(traffic[i]);
        const double window = nnz[i] > 1 ? w * (double)(nnz[i] - 1) : w;
        assert(fabs(orig->sol_row[i] - act[i]) <= window);
    }

    free(act);
    free(traffic);
    free(nnz);
#endif
}
#endif

JAOS_NODISCARD jaos_status jm_postsolve_expand(jm_presolve *p)
{

    assert(p->outcome == JM_PRESOLVE_REDUCED);
    jaos_model *orig = p->orig;
    const jaos_model *red = &p->reduced;

    assert(red->a_start != orig->a_start && red->a_index != orig->a_index &&
           red->a_value != orig->a_value);
    assert(red->col_lower != orig->col_lower &&
           red->col_upper != orig->col_upper);
    assert(red->row_lower != orig->row_lower &&
           red->row_upper != orig->row_upper);
    assert(red->col_cost != orig->col_cost);

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    double *rowc = calloc((size_t)orig->num_row + 1, sizeof *rowc);
    if (rowc == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    orig->solve_status = red->solve_status;
    orig->solve_iters  = red->solve_iters;
    orig->solve_work   = red->solve_work;
    orig->solve_time   = red->solve_time;

    if (red->solve_status != JAOS_SOLVE_OPTIMAL) {

        orig->objective = 0.0;
        memset(orig->sol_col, 0, (size_t)orig->num_col * sizeof(double));
        memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_redcost, 0, (size_t)orig->num_col * sizeof(double));

        memset(orig->sol_farkas, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_ray, 0, (size_t)orig->num_col * sizeof(double));
        if (red->solve_status == JAOS_SOLVE_INFEASIBLE && red->farkas_ok) {
            for (int64_t ri = 0; ri < red->num_row; ri++)
                orig->sol_farkas[p->orig_row[ri]] = red->sol_farkas[ri];
            orig->farkas_ok = ps_lift_farkas(p, orig->sol_farkas);
        }
        if (red->solve_status == JAOS_SOLVE_UNBOUNDED && red->ray_ok) {
            for (int64_t rj = 0; rj < red->num_col; rj++)
                orig->sol_ray[p->orig_col[rj]] = red->sol_ray[rj];
            orig->ray_ok = ps_lift_ray(p, orig->sol_ray);
        }

        for (int64_t i = 0; i < orig->num_row; i++) {
            const int64_t ri = p->row_map[i];
            orig->sol_row_status[i] =
                (ri >= 0 && red->start_row_status != nullptr)
                    ? red->start_row_status[ri] : JAOS_BASIS_BASIC;
        }
        for (int64_t j = 0; j < orig->num_col; j++) {
            const int64_t rj = p->col_map[j];
            orig->sol_col_status[j] =
                (rj >= 0 && red->start_col_status != nullptr)
                    ? red->start_col_status[rj] : JAOS_BASIS_AT_LOWER;
        }
        if (red->start_col_status == nullptr ||
            red->start_row_status == nullptr) {

            free(rowc);
            return JAOS_OK;
        }

        for (int64_t r = 0; r < p->arena_len; r++) {
            const jm_presolve_rec *rec = &p->arena[r];
            if (rec->tag == JM_PS_FREE_COL_SINGLETON) {
                orig->sol_col_status[rec->index2] = JAOS_BASIS_FREE;
            } else if (rec->tag == JM_PS_IMPLIED_FREE_COL) {
                orig->sol_col_status[rec->index2] = JAOS_BASIS_BASIC;
                orig->sol_row_status[rec->index] = JAOS_BASIS_AT_LOWER;
            } else if (rec->tag == JM_PS_SINGLETON_ROW) {
                const int64_t i = ps_restore_index(rec->index, orig->num_row);
                const int64_t j = rec->index2;
                const bool tightens =
                    rec->row_tightens_hi || rec->row_tightens_lo;
                if (tightens &&
                    orig->sol_col_status[j] != JAOS_BASIS_BASIC) {
                    orig->sol_row_status[i] = rec->row_tightens_hi
                        ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;
                    orig->sol_col_status[j] = JAOS_BASIS_BASIC;
                } else {
                    orig->sol_row_status[i] = JAOS_BASIS_BASIC;
                }
            }
        }
#if !defined(NDEBUG) && !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)

        if (red->sol_basis_ok) {
            int64_t nb = 0;
            for (int64_t j = 0; j < orig->num_col; j++)
                nb += (orig->sol_col_status[j] == JAOS_BASIS_BASIC);
            for (int64_t i = 0; i < orig->num_row; i++)
                nb += (orig->sol_row_status[i] == JAOS_BASIS_BASIC);
            assert(nb == orig->num_row);
        }
#endif

        free(rowc);

        if (red->solve_status != JAOS_SOLVE_NUMERICAL_ERROR) {
            (void)jm_model_remember_basis(orig);
            orig->sol_basis_ok = red->sol_basis_ok;
        }
        return JAOS_OK;
    }

    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));

    for (int64_t i = 0; i < orig->num_row; i++) {
        const int64_t ri = p->row_map[i];
        if (ri < 0)
            continue;
        orig->sol_dual[i] = red->sol_dual[ri];
        orig->sol_row_status[i] = red->sol_row_status[ri];
        orig->sol_row[i] = red->sol_row[ri];
    }

    for (int64_t j = 0; j < orig->num_col; j++) {
        const int64_t rj = p->col_map[j];
        if (rj < 0)
            continue;
        orig->sol_col[j] = red->sol_col[rj];
        orig->sol_col_status[j] = red->sol_col_status[rj];
        orig->sol_redcost[j] = red->sol_redcost[rj];
    }

    for (int64_t j = 0; j < orig->num_col; j++) {
        if (p->col_map[j] < 0)
            continue;
        const double xv = orig->sol_col[j];
        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            if (p->row_map[i] < 0)
                ps_row_add(orig, rowc, i, orig->a_value[k] * xv);
        }
    }

    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);

    for (int64_t i = 0; i < orig->num_row; i++)
        orig->sol_row[i] = ps_published(orig->sol_row[i] + rowc[i]);
    free(rowc);

#ifndef NDEBUG
    ps_verify_row_activities(orig);
#endif

    jm_model_publish_objective(orig);
    (void)jm_model_remember_basis(orig);
    orig->sol_basis_ok = true;
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p)
{
    jaos_model *orig = p->orig;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    double *rowc = calloc((size_t)orig->num_row + 1, sizeof *rowc);
    if (rowc == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    orig->solve_status = JAOS_SOLVE_OPTIMAL;
    orig->solve_iters  = 0;
    orig->solve_primal_iters = 0;
    orig->solve_phase1_iters = 0;
    orig->solve_work   = p->reduced.solve_work;

    orig->solve_time   = 0.0;

    orig->presolve_num_row = p->reduced.num_row;
    orig->presolve_num_col = p->reduced.num_col;
    orig->presolve_num_nz  = p->reduced.num_nz;

    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));

    memset(orig->sol_row_status, 0,
           (size_t)orig->num_row * sizeof *orig->sol_row_status);
    memset(orig->sol_col_status, 0,
           (size_t)orig->num_col * sizeof *orig->sol_col_status);
    static_assert(JAOS_BASIS_BASIC == 0,
                  "the two memsets above publish BASIC by writing zero");

    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);

    for (int64_t i = 0; i < orig->num_row; i++)
        orig->sol_row[i] = ps_published(orig->sol_row[i] + rowc[i]);
    free(rowc);

#ifndef NDEBUG
    ps_verify_row_activities(orig);
#endif

    jm_model_publish_objective(orig);
    (void)jm_model_remember_basis(orig);

    orig->sol_basis_ok = jm_model_basis_count_ok(orig);
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_infeasible_or_unbounded(jm_presolve *p,
    jaos_solve_status status)
{
    jaos_model *orig = p->orig;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    orig->solve_status = status;
    orig->solve_iters  = 0;
    orig->solve_primal_iters = 0;
    orig->solve_phase1_iters = 0;
    orig->solve_work   = p->reduced.solve_work;
    orig->solve_time   = 0.0;
    orig->objective = 0.0;
    orig->presolve_num_row = p->reduced.num_row;
    orig->presolve_num_col = p->reduced.num_col;
    orig->presolve_num_nz  = p->reduced.num_nz;

    memset(orig->sol_col, 0, (size_t)orig->num_col * sizeof(double));
    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_redcost, 0, (size_t)orig->num_col * sizeof(double));
    memset(orig->sol_col_status, 0,
           (size_t)orig->num_col * sizeof *orig->sol_col_status);
    memset(orig->sol_row_status, 0,
           (size_t)orig->num_row * sizeof *orig->sol_row_status);

    memset(orig->sol_farkas, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_ray, 0, (size_t)orig->num_col * sizeof(double));
    if (p->proof_index >= 0) {
        if (status == JAOS_SOLVE_INFEASIBLE) {
            assert(p->proof_index < orig->num_row);
            orig->sol_farkas[p->proof_index] = p->proof_sign;
            orig->farkas_ok = ps_lift_farkas(p, orig->sol_farkas);
        } else {
            assert(p->proof_index < orig->num_col);
            orig->sol_ray[p->proof_index] = p->proof_sign;
            orig->ray_ok = ps_lift_ray(p, orig->sol_ray);
        }
    }
    return JAOS_OK;
}
