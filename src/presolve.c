/* Presolve and postsolve.
 *
 * A reduced problem is built alongside the caller's model, never mutating
 * it. Every reduction that fires pushes a tagged record onto an append-only
 * arena; postsolve replays the arena strictly LIFO to recover the
 * caller's sol_col, sol_row, sol_dual, sol_redcost, sol_col_status and
 * sol_row_status in the model's own, original indices. The reduced index
 * space is an internal detail of this file and of sx; it never escapes
 * either. Singleton columns fire only at cost exactly zero: the row's dual
 * decides a nonzero-cost one. Bound tightening is refused (D97).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A published zero is a zero (D21); presolve's own copy of `published`. */
static double ps_published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

/* Instrument-validation hook: under this build-time guard every postsolve
 * restore index reads one past where it belongs, wrapped by `dim`. */
static int64_t ps_restore_index(int64_t index, int64_t dim)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    return (index + 1) % dim;
#else
    (void)dim;
    return index;
#endif
}

/* Presolve runs on the model as loaded, before scaling: a THIRD tolerance
 * space, and nothing converts into it (docs/tolerances.md, presolve
 * section). */

/* A residue of a running difference is worth nothing below DBL_EPSILON
 * times the traffic that produced it; a few ulps cover the rest (D103). */
#ifndef JAOS_PRESOLVE_ROUND_ULPS_VALUE
#define JAOS_PRESOLVE_ROUND_ULPS_VALUE 8
#endif
constexpr double PRESOLVE_ROUND_ULPS = JAOS_PRESOLVE_ROUND_ULPS_VALUE;

/* Cap on the fixed-point rounds: a safety stop, never above the structural
 * backstop num_row + num_col + 1. Swept in docs/tolerances.md. */
#ifndef JAOS_PRESOLVE_ROUNDS_VALUE
#define JAOS_PRESOLVE_ROUNDS_VALUE 16
#endif
constexpr int64_t JM_PRESOLVE_ROUNDS = JAOS_PRESOLVE_ROUNDS_VALUE;

/* Neumaier-compensated sum in `double`. Not `long double` (D34).
 * `-ffp-contract=off` is what makes the two-term error recovery exact. */
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

/* One row bound, shifted by a removed column's `a * v`, residue kept (D165).
 * An infinite end is left as the uncompensated subtraction left it:
 * `(inf - inf)` is a NaN, and a NaN end makes every comparison against it
 * false. */
static void ps_bound_shift(ps_acc *a, double *cur, double t)
{
    ps_acc_add(a, -t);
    if (!isfinite(a->sum) || !isfinite(a->comp))
        a->comp = 0.0;
    *cur = a->sum + a->comp;
}

/* m stays const throughout this file, so presolve builds its own row-wise
 * mirror instead of calling jm_model_ensure_rowwise. Freed before return. */
typedef struct {
    int64_t *rs;    /* [num_row + 1] */
    int64_t *ridx;  /* [num_nz], column index per row-entry */
    double  *rval;  /* [num_nz] */
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

/* What a row can reach given the current column boxes. `lo_inf`/`hi_inf`
 * count the terms that made an end infinite, kept apart from the FINITE
 * sums. `traffic`: the sum of the terms' magnitudes, a residue's scale
 * (D23). */
typedef struct {
    double lo_sum, hi_sum;
    int64_t lo_inf, hi_inf;
    double traffic;
} ps_range;

/* `skip` names one live column to leave out of the sum, or -1 for none. */
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
            continue;   /* the term is exactly zero, and 0 * inf is a NaN */

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

/* The two ends as numbers, infinite where a term made them so. */
static double ps_min_act(const ps_range *r)
{
    return r->lo_inf > 0 ? -HUGE_VAL : r->lo_sum;
}

static double ps_max_act(const ps_range *r)
{
    return r->hi_inf > 0 ? HUGE_VAL : r->hi_sum;
}

/* Every caller supplies its own scale. */
static double ps_round_tol(double scale)
{
    return PRESOLVE_ROUND_ULPS * DBL_EPSILON * (scale > 1.0 ? scale : 1.0);
}

/* Same shape and, today, the same number as ps_round_tol, and deliberately
 * NOT the same constant: the three activity-range readings must not sit on
 * the EXTRA_CFLAGS hook (docs/tolerances.md). */
static double ps_row_tol(const ps_range *r)
{
    return 8.0 * DBL_EPSILON * (r->traffic > 1.0 ? r->traffic : 1.0);
}

/* The margin the implied free column singleton declines borderline cases
 * by. It is subtracted from the column's own bounds: at equality the family
 * declines. It covers the row sum's residue through the division, in ulps. */
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

/* The window a comparison between two BOUNDS uses. Infinities are skipped:
 * a scale of infinity would make every subsequent comparison pass. */
static double ps_bound_scale(double a, double b)
{
    double s = 1.0;
    if (isfinite(a) && fabs(a) > s)
        s = fabs(a);
    if (isfinite(b) && fabs(b) > s)
        s = fabs(b);
    return s;
}

#ifndef NDEBUG
/* `row_traffic[i]` is the error budget for `cur_rl[i]`/`cur_ru[i]`, so it
 * has to be a number wherever one of those still is; a row whose two ends
 * are both infinite is exempt (D155). */
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
}

void jm_presolve_free(jm_presolve *p)
{
    /* Nothing here aliases the caller's model; jaos_model_free is not used. */
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
    free(p->reduced.start_col_status);
    free(p->reduced.start_row_status);

    free(p->orig_col);
    free(p->orig_row);
    free(p->col_map);
    free(p->row_map);
    free(p->arena);
    memset(p, 0, sizeof *p);
}

/* Pushes one record. Returns false on allocation failure. */
static bool ps_push(jm_presolve *p, jm_presolve_rec rec)
{
    /* `index` is always an ORIGINAL row or column index, never a reduced
     * one. Only the sign is checkable here. The upper bound needs the
     * original dimensions, and `p->orig` is the one field of this struct
     * that presolve may not read: `jm_dual_simplex` sets it and
     * `jaos_internal.h` says nothing inside presolve.c needs to. Reading it
     * here segfaults every test that drives `jm_presolve_run` directly,
     * which is what ASan reported (D223). The replay does bound it, per tag
     * and against the right dimension, at the sites that have `orig`. */
    assert(rec.index >= 0);
    if (!JM_GROW(p->arena, p->arena_cap, p->arena_len + 1))
        return false;
    p->arena[p->arena_len++] = rec;
    return true;
}

/* Empty column's favourable-bound rule; the only family permitted to report
 * unboundedness (D19). `cost` is CANONICAL, sigma*c_j. */
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

    /* "Presolve runs on the model as loaded, before scaling: a THIRD
     * tolerance space, and nothing converts into it." There is no assert
     * for this and `assert(!m->scale_valid)` is NOT it -- that flag says the
     * FACTORS are computed, and `jaos_internal.h` is explicit that the
     * stored matrix is never touched, so presolve reads unscaled data
     * whatever the flag says. It fires on the existing suite, which is how
     * that was found. What the sentence really claims is that nothing here
     * applies `row_scale`/`col_scale`, and that is a property of the whole
     * file rather than of a value in scope (D232). */

    /* The one cost-directional rule is stated for MINIMIZE; a MAXIMIZE model
     * arrives with unflipped costs. */
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    /* All sized by the ORIGINAL dimensions and indexed by original row/col. */
    bool *col_dead = jm_calloc_array(nc, sizeof *col_dead);
    bool *row_dead = jm_calloc_array(nr, sizeof *row_dead);
    bool *row_frozen = jm_calloc_array(nr, sizeof *row_frozen);
    /* A column with an entry in a removed row whose multiplier is not zero
     * by construction. Such a row replays LATER than this column, so its
     * dual still reads zero when a forcing row's derivation would read it. */
    bool *col_pending_dual = jm_calloc_array(nc, sizeof *col_pending_dual);
    /* Magnitude subtracted from each row's bounds so far. */
    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
    double *cur_cl = jm_alloc_array(nc, sizeof *cur_cl);
    double *cur_cu = jm_alloc_array(nc, sizeof *cur_cu);
    /* The objective a reduction sees; implied free pushes cost onto it. */
    double *cur_cost = jm_alloc_array(nc, sizeof *cur_cost);
    double *cur_rl = jm_alloc_array(nr, sizeof *cur_rl);
    double *cur_ru = jm_alloc_array(nr, sizeof *cur_ru);
    /* The residue of the running subtraction (D165). */
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

        /* --- Row pass: empty and singleton rows. --------------------- */
        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || row_frozen[i])
                continue;

            if (row_deg[i] == 0) {
                /* An empty row's activity is exactly zero; its BOUNDS are a
                 * running difference of every removed column's contribution,
                 * so the window is eps times row_traffic[i] (D23). */
                double etol = 0.0;
                assert(ps_traffic_usable(cur_rl[i], cur_ru[i], row_traffic[i]));
                if (row_traffic[i] > 0.0) {
                    /* An infinite window accepts every violation, so the
                     * bound scale stands in. Unreachable since D155, and the
                     * assert is what says so rather than the comment: the
                     * fallback stays as the safe reading, and if it ever runs
                     * a debug build stops instead of silently widening every
                     * window on that row (D235). */
                    assert(isfinite(row_traffic[i]));
                    const double scale = isfinite(row_traffic[i])
                        ? row_traffic[i]
                        : ps_bound_scale(cur_rl[i], cur_ru[i]);
                    /* Eight ulps of the traffic and nothing else (D166). */
                    etol = ps_round_tol(scale);
                }
                if (cur_rl[i] > etol || cur_ru[i] < -etol) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;
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

                /* Checked here, before the fold: the row pass runs first
                 * every round, so a mutual singleton's row never survives to
                 * be seen degree-1 from the column side. */
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
                /* Two roundings meet in new_lo/new_hi: the bounds' own scale
                 * (ps_bound_scale), and the row's traffic carried down by |a|
                 * through the division cur_rl / a. The window covers the
                 * larger. */
                assert(ps_traffic_usable(cur_rl[i], cur_ru[i], row_traffic[i]));
                double bscale = ps_bound_scale(new_lo, new_hi);
                if (row_traffic[i] > 0.0 && isfinite(row_traffic[i])) {
                    const double tscale = row_traffic[i] / fabs(a);
                    if (tscale > bscale)
                        bscale = tscale;
                }
                /* No shift count here (D166). */
                const double btol = ps_round_tol(bscale);

                if (new_lo > new_hi + btol) {
                    /* PAST the opposite bound by more than rounding. */
                    p->outcome = JM_PRESOLVE_INFEASIBLE;
                    goto done;
                }
                if (new_lo > new_hi) {
                    /* ON the opposite bound, within the epsilon: collapsed
                     * to a point. The midpoint is clamped into the column's
                     * box (D158): the residue moves to the row. */
                    const double mid = 0.5 * (new_lo + new_hi);
                    if (cur_cl[j] <= cur_cu[j]) {
                        fold_lo = fold_hi = mid < cur_cl[j] ? cur_cl[j]
                                          : mid > cur_cu[j] ? cur_cu[j]
                                                            : mid;
                        assert(fold_lo >= cur_cl[j] && fold_hi <= cur_cu[j]);
                    } else {
                        /* An INVERTED box is legal input (`jaos.h`), to be
                         * reported infeasible: nothing to clamp into. */
                        fold_lo = fold_hi = mid;
                    }
                }

                /* lo/hi are the bounds the column leaves this fold carrying.
                 * ps_replay_one compares x_j against them to decide whether
                 * THIS row produced the bound x_j rests on. */
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

        /* --- Column pass: fixed, empty, singleton (cost 0). ----------- */
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
                    p->outcome = JM_PRESOLVE_UNBOUNDED;
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
                /* "Singleton columns fire only at cost exactly zero." Both
                 * families below drop the column from the objective, so a
                 * nonzero cost is silently lost. Guarded by the condition
                 * above today; this is what survives a refactor of it. */
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
                    /* Mutual singleton, with the row's CURRENT bounds. */
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
                    /* Bounded, cost-0 singleton column. */
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
                    /* The two ends take DIFFERENT terms, so they are shifted
                     * separately and `row_traffic` takes the larger (D165). */
                    if (lo_absorbs)
                        ps_bound_shift(&cur_rl_acc[i], &cur_rl[i], cmax);
                    if (hi_absorbs)
                        ps_bound_shift(&cur_ru_acc[i], &cur_ru[i], cmin);

                    /* Only the magnitude an absorbing end actually took: an
                     * already-infinite end is not subtracted from, and an
                     * infinite term carries no residue. */
                    double moved = 0.0;
                    if (lo_absorbs && isfinite(cmax) && fabs(cmax) > moved)
                        moved = fabs(cmax);
                    if (hi_absorbs && isfinite(cmin) && fabs(cmin) > moved)
                        moved = fabs(cmin);
                    /* "An already-infinite end is not subtracted from": an
                     * end that did not absorb was never shifted, so it is
                     * still infinite. One way only. The converse is FALSE and
                     * that is measured: `!free_col` means at least one column
                     * bound is finite, not both, so a column boxed at
                     * [0, +inf) makes one of `cmin`/`cmax` infinite and turns
                     * a finite end infinite. Asserting the equality fired 58
                     * times on the gate (D235, `bench/measurements/02-148/`).
                     * `moved` is what carries that: it counts an end only
                     * where the term it took was finite. */
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
                /* Free column in a row with other live entries: the cost-0
                 * families stop here; implied free below takes it. */
            }

            /* --- Implied free column singleton (D105). ----------------
             * When the box the row implies on x_j lies strictly inside the
             * column's own box, the column is substituted out exactly, with
             * the row. The implied box is a predicate, never published
             * (unlike D97). Its cost moves onto the row's other columns.
             * col_pending_dual is NOT set on them: a column removed later
             * carries the shifted cost and reads sol_dual[i] == 0 at replay,
             * and the replay divides the same two numbers, so d_k is exact. */
            if (col_deg[j] == 1 &&
                m->a_start[j + 1] - m->a_start[j] == 1) {
                const int64_t i = m->a_index[m->a_start[j]];
                const double a = m->a_value[m->a_start[j]];

                /* Asked of the ORIGINAL pair too: cur_rl/cur_ru are running
                 * differences, and two bounds that differ can round to the
                 * same double (D156). */
                if (a != 0.0 && !row_dead[i] && !row_frozen[i] &&
                    m->row_lower[i] == m->row_upper[i] &&
                    isfinite(cur_rl[i]) && cur_rl[i] == cur_ru[i]) {
                    const double b = cur_rl[i];
                    const ps_range rg =
                        ps_row_range(&rw, i, cur_cl, cur_cu, col_dead, j);
                    jm_work_add(w, row_deg[i] * JM_WORK_NONZERO);

                    const double minact = ps_min_act(&rg);
                    const double maxact = ps_max_act(&rg);
                    /* a_ij * x_j lies in [b - maxact, b - minact]; an
                     * infinite activity leaves its side open. */
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

        /* --- Activity pass: forcing, redundant. ---------------------- */
        /* One range per row. Degree 0 and 1 are consumed above; a frozen
         * row's bounds stand for a range, not a determined value. */
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

            /* 1. INFEASIBLE: no point of the current boxes satisfies the row.
             *    Its OWN window (D160): `rg.traffic` for the activity sum,
             *    `row_traffic[i]` for the running-difference bound; no
             *    `ps_bound_scale`. */
            assert(ps_traffic_usable(rl, ru, row_traffic[i]));
            /* Two errors in two numbers, so the budgets are ADDED. */
            double iact = rg.traffic > 1.0 ? rg.traffic : 1.0;
            double ibnd = 1.0;
            if (isfinite(row_traffic[i]) && row_traffic[i] > ibnd)
                ibnd = row_traffic[i];
            const double itol = 8.0 * DBL_EPSILON * iact +
                                8.0 * DBL_EPSILON * ibnd;
            if ((isfinite(ru) && min_act > ru + itol) ||
                (isfinite(rl) && max_act < rl - itol)) {
                p->outcome = JM_PRESOLVE_INFEASIBLE;
                goto done;
            }

            /* 2. FORCING: the range touches a row bound, so every live column
             *    is pinned at the bound attaining that extreme. `force_hi`:
             *    min activity at the UPPER bound; `force_lo` the mirror. */
            const bool force_hi = isfinite(ru) && min_act >= ru - rtol;
            const bool force_lo = isfinite(rl) && max_act <= rl + rtol;
            if (force_hi || force_lo) {
                /* Only when every attaining bound is one the CALLER's model
                 * carried: a column pinned at a derived bound is interior in
                 * the caller's box, and no multiplier makes its d_j zero. */
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
                            /* Contributes nothing to the range, but its entry
                             * dies with a row that carries a multiplier. */
                            col_deg[j]--;
                            col_pending_dual[j] = true;
                            continue;
                        }
                        const bool want_lo = force_hi ? (rw.rval[k] > 0.0)
                                                      : (rw.rval[k] < 0.0);
                        const double v = want_lo ? cur_cl[j] : cur_cu[j];
                        assert(isfinite(v));
                        /* "Only when every attaining bound is one the
                         * CALLER's model carried." A column pinned at a
                         * DERIVED bound is interior in the caller's box, so
                         * no multiplier makes its reduced cost zero and the
                         * published point is dual infeasible (D232). */
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
                        /* JM_PS_FIXED_COL, not a tag of its own; `coef` is
                         * what the row's own record needs back. */
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
                    /* Pushed AFTER its columns, so the LIFO walk replays it
                     * BEFORE them: their reduced costs read its multiplier. */
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

            /* 3. REDUNDANT: the whole range lies inside the row's bounds, so
             *    the row never binds; dropped, multiplier zero. */
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

            /* 4. BOUND TIGHTENING is refused (D97). */
        }

        if (!changed)
            break;
        rounds_done++;
    }
    p->counts.rounds = rounds_done;
    /* "Never above the structural backstop." The loop's cap is the smaller of
     * that and `JM_PRESOLVE_ROUNDS`, which is on the `EXTRA_CFLAGS` hook and
     * can be swept; the backstop is what must hold whatever it is set to,
     * because a round that changes nothing breaks the loop and every round
     * that changes something kills at least one row or column (D235). */
    assert(p->counts.rounds <= nr + nc + 1);
    }

#ifndef NDEBUG
    /* Over every row: unguarded `a * v` producers can poison any row. */
    for (int64_t i = 0; i < nr; i++) {
        assert(ps_traffic_usable(cur_rl[i], cur_ru[i], row_traffic[i]));
        /* "Magnitude subtracted from each row's bounds so far." Every writer
         * adds `fabs(t)` or a `moved` built from magnitudes, so the budget
         * only grows. A negative one would hand `ps_round_tol` a window on
         * the wrong side of zero, and a NaN fails this too (D235). */
        assert(row_traffic[i] >= 0.0);
    }
    /* "Boxes only narrow." Past initialisation the one writer of
     * `cur_cl`/`cur_cu` is the singleton-row fold, and it clamps into the
     * column's own box (D158), so a live column never leaves the box the
     * caller gave it. An INVERTED box is legal input (`jaos.h`) and the clamp
     * is skipped there, so it is excluded rather than asserted about (D236). */
    for (int64_t j = 0; j < nc; j++) {
        if (col_dead[j] || m->col_lower[j] > m->col_upper[j])
            continue;
        assert(cur_cl[j] >= m->col_lower[j]);
        assert(cur_cu[j] <= m->col_upper[j]);
    }
#endif

    /* --- Frozen rows, tested for feasibility once the boxes are final. --
     * Once, after the loop: boxes only narrow. */
    for (int64_t i = 0; i < nr; i++) {
        if (row_dead[i] || !row_frozen[i])
            continue;

        const ps_range rg = ps_row_range(&rw, i, cur_cl, cur_cu, col_dead,
                                         -1);
        /* Charged over the entries walked: an emptied row still walks them. */
        jm_work_add(w, (rw.rs[i + 1] - rw.rs[i]) * JM_WORK_NONZERO);

        /* Not ps_row_tol: a frozen row's LIVE traffic is routinely zero.
         * Two budgets, ADDED (D162): `rg.traffic` and `row_traffic[i]`, eight
         * ulps each, no shift count (D166), no `ps_bound_scale` (D161). */
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
            goto done;
        }
    }

    {
    /* --- Final compaction: assign dense reduced indices to survivors. -- */
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

    /* Struct-copy first so cfg, sense, the log callback and the tolerances
     * carry over; every pointer field is overwritten below with presolve's
     * own allocation. */
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

    /* An entry survives only when its column AND its row are alive. */
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
            /* "An entry survives only when its column AND its row are alive."
             * A dead row's `row_map` is -1, so a skip this loop missed lands
             * here as a row index of -1 and the reduced matrix carries it
             * into the solve (D236). */
            assert(p->reduced.a_index[dst] >= 0);
            p->reduced.a_value[dst] = m->a_value[k];
            dst++;
        }
        /* The count pass and this fill pass apply the same "alive column AND
         * alive row" predicate, separately. If they ever disagree, one
         * column's entries land in the next column's slots -- a silently
         * wrong matrix -- and the last column writes past `num_nz` (D232). */
        assert(dst == p->reduced.a_start[rj2 + 1]);
    }

    /* A removed row or column recorded basic undercounts the basic total;
     * build_warm_basis REPAIRS a short count (D144). No per-family fix. */
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

        /* JM_PS_SINGLETON_ROW pairs: the caller's status for the surviving
         * column can be BASIC while its row is gone from reduced space. The
         * row's own supplied status is read. */
        for (int64_t r = 0; r < p->arena_len; r++) {
            const jm_presolve_rec *rec = &p->arena[r];
            if (rec->tag != JM_PS_SINGLETON_ROW)
                continue;
            const int64_t j = rec->index2;
            const int64_t rjj = p->col_map[j];
            if (rjj < 0)
                continue;   /* column j was itself later removed */

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
    /* Recorded on `reduced` so the SOLVED path has presolve's own charge;
     * on the REDUCED path publish() overwrites it with s->work's total. */
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

/* --- Postsolve replay ------------------------------------------------- */

/* One Neumaier step into a replayed row activity: sol_row[i] is the sum,
 * rowc[i] the compensation, folded once after the replay. */
static void ps_row_add(jaos_model *orig, double *rowc, int64_t i, double t)
{
    const double s = orig->sol_row[i];
    const double n = s + t;
    rowc[i] += (fabs(s) >= fabs(t)) ? ((s - n) + t) : ((t - n) + s);
    orig->sol_row[i] = n;
}

/* A removed column's share of every row EXCEPT `skip_row`. Accumulate,
 * never assign: the halves of a dead row's activity arrive in either order. */
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

/* A restored singleton row's status, from its own dual and FINAL activity
 * (D136): a basic variable has a zero dual, a nonbasic one rests on a bound.
 * A step that introduces a row adds exactly one basic variable (D132). */
static jaos_basis_status ps_singleton_row_status(const jaos_model *orig,
                                                 int64_t i)
{
    /* `ps_published` normalises -0.0, so this is not sign-sensitive. */
    if (orig->sol_dual[i] == 0.0)
        return JAOS_BASIS_BASIC;

    const double act = orig->sol_row[i];
    if (act == orig->row_lower[i])
        return JAOS_BASIS_AT_LOWER;
    if (act == orig->row_upper[i])
        return JAOS_BASIS_AT_UPPER;
    return JAOS_BASIS_BASIC;
}

/* The exchange a restored cost-0 bounded column singleton owes: it restores
 * no row (D132), yet an interior value MUST be basic (D133), so row i's
 * logical, basic in the reduced solve, leaves. Read after the replay AND the
 * carry fold (D140). JM_PS_SINGLETON_ROW rewrites the status. */
static void ps_singleton_col_swap(jaos_model *orig, const jm_presolve_rec *rec)
{
    const int64_t j = ps_restore_index(rec->index2, orig->num_col);
    /* A recorded nonbasic status still matches the value the replay wrote;
     * a writer moving sol_col[j] off that bound would misfire this swap. */
    assert(orig->sol_col_status[j] != JAOS_BASIS_AT_LOWER ||
           orig->sol_col[j] == rec->lo);
    assert(orig->sol_col_status[j] != JAOS_BASIS_AT_UPPER ||
           orig->sol_col[j] == rec->hi);
    if (orig->sol_col[j] == rec->lo || orig->sol_col[j] == rec->hi)
        return;                 /* the column rests on a bound; nothing owed */

    const int64_t i = rec->index;   /* the row survives, so it is not restored */
    if (orig->sol_row_status[i] != JAOS_BASIS_BASIC)
        return;                 /* no partner to take out */

    const double act = orig->sol_row[i];
    if (act == orig->row_lower[i])
        orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
    else if (act == orig->row_upper[i])
        orig->sol_row_status[i] = JAOS_BASIS_AT_UPPER;
    /* Otherwise the row is not on a bound and its logical cannot be made
     * nonbasic without claiming one. Left alone deliberately. */
}

static void ps_replay_one(jaos_model *orig, const jm_presolve *p, int64_t r,
                          double *rowc)
{
    const jm_presolve_rec *rec = &p->arena[r];

    /* Every sign rule below is stated for MINIMIZE, the canonical form the
     * checker judges in. sigma canonicalises the QUESTIONS and is applied
     * again on the way out, never to a stored value: d_j = c_j - a_ij*y_i
     * holds in the model's own space, and sol_dual/sol_redcost live there. */
    const double sigma = (orig->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    switch (rec->tag) {
    case JM_PS_FIXED_COL:
    case JM_PS_EMPTY_COL: {
        const int64_t j = ps_restore_index(rec->index, orig->num_col);
        assert(j >= 0 && j < orig->num_col);

        orig->sol_col[j] = ps_published(rec->value);
        /* A fixed column accepts any status (check.c, "fixed -> anything");
         * an empty one sits at the bound the cost picked. */
        orig->sol_col_status[j] =
            (rec->value == orig->col_upper[j] &&
             rec->value != orig->col_lower[j])
                ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;

        /* Over the column's ORIGINAL entries: a row not yet replayed (removed
         * earlier, replays later) still reads a well-defined 0 here. */
        double dw = rec->cost;
        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            assert(i >= 0 && i < orig->num_row);
            dw -= orig->a_value[k] * orig->sol_dual[i];
        }
        orig->sol_redcost[j] = ps_published(dw);

        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            ps_row_add(orig, rowc, i, orig->a_value[k] * rec->value);
        }
        break;
    }

    case JM_PS_EMPTY_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);
        /* Explicit: under JAOS_PRESOLVE_FAULT_OFFBYONE the index can land on
         * a surviving row's already-correct slot, and a fault test relying on
         * the pre-zeroed default could not fail. A zero dual always passes. */
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

        /* x_j's value/status/reduced cost is already final here. Does leaving
         * this row's multiplier at zero satisfy x_j's sign condition? Exact:
         * a value presolve assigned rests on a bound by equality. Otherwise
         * the row absorbs the whole reduced cost, y_i = d'_j / a_ij. */
        const double d0 = orig->sol_redcost[j];
        const double v0 = orig->sol_col[j];
        /* dc is d0 in the checker's canonical minimise space (sigma above). */
        const double dc = sigma * d0;
        const bool zero_works =
            dc == 0.0 ||
            (dc > 0.0 && v0 == orig->col_lower[j]) ||
            (dc < 0.0 && v0 == orig->col_upper[j]);

        /* Several rows can fold into one column, in an order the LIFO replay
         * does not follow, so each record asks: does x_j rest on the bound
         * THIS row produced, on d0's side? Exact: rec->lo/hi is the same
         * computation. An overwritten fold declines (D158). */
        const bool this_row_owns =
            (dc > 0.0 && rec->row_tightens_lo && v0 == rec->lo) ||
            (dc < 0.0 && rec->row_tightens_hi && v0 == rec->hi);

        double y_i;
        if (zero_works || !this_row_owns) {
            y_i = 0.0;
        } else {
            /* x_j rests at a bound the ROW induced, which the ORIGINAL
             * column never had: interior there, so BASIC. */
            y_i = d0 / rec->coef;
            orig->sol_redcost[j] = 0.0;
            orig->sol_col_status[j] = JAOS_BASIS_BASIC;
        }
#if defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
        /* Per-family fault: always take the reduced solve's value instead
         * of the one that makes the full reduced cost satisfy the checker. */
        y_i = orig->sol_redcost[j] / rec->coef;
        orig->sol_redcost[j] = orig->sol_redcost[j] - rec->coef * y_i;
#endif
        orig->sol_dual[i] = ps_published(y_i);

        const double xv = orig->sol_col[j];
        orig->sol_row[i] = ps_published(rec->coef * xv);
        rowc[i] = 0.0;   /* an assignment resets the carry with the sum */
        /* Only the row's own term; later records add theirs through
         * ps_row_add. ps_singleton_row_status decides the status (D136). */
        break;
    }

    case JM_PS_SINGLETON_COL: {
        const int64_t i = rec->index;
        const int64_t j = ps_restore_index(rec->index2, orig->num_col);
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        /* Row i survived, relaxed, but its activity is NOT final mid-LIFO:
         * sol_row[i] holds the columns live when this record was pushed, so
         * x_j is judged against the bounds recorded at that moment. */
        const double rest = orig->sol_row[i] + rowc[i];
        const double rl = rec->row_lo, ru = rec->row_hi;
        double lo_j, hi_j;
        if (rec->coef > 0.0) {
            lo_j = isfinite(rl) ? (rl - rest) / rec->coef : -HUGE_VAL;
            hi_j = isfinite(ru) ? (ru - rest) / rec->coef : HUGE_VAL;
        } else {
            lo_j = isfinite(ru) ? (ru - rest) / rec->coef : -HUGE_VAL;
            hi_j = isfinite(rl) ? (rl - rest) / rec->coef : HUGE_VAL;
        }
        /* Any point of the intersection is optimal (cost 0): the lower end. */
        const double want_lo = rec->lo > lo_j ? rec->lo : lo_j;
        const double want_hi = rec->hi < hi_j ? rec->hi : hi_j;

        /* Empty by an ulp here at most, so the value is clamped into the
         * column's OWN recorded box (D152): the stored end wins. No windowed
         * assert: the residue is the SIMPLEX's (bench/measurements/02-61/). */
        assert(rec->lo <= rec->hi);
        (void)want_hi;   /* the emptiness check that read it was removed */

        const double xv = want_lo < rec->lo ? rec->lo
                        : want_lo > rec->hi ? rec->hi
                                            : want_lo;
        assert(xv >= rec->lo && xv <= rec->hi);

        orig->sol_col[j] = ps_published(xv);
        orig->sol_col_status[j] =
            (xv == rec->lo) ? JAOS_BASIS_AT_LOWER :
            (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;
        /* d_j = 0 - a_ij * y_i: a cost-0 column has no sign requirement. */
        orig->sol_redcost[j] = ps_published(-rec->coef * orig->sol_dual[i]);
        /* Through ps_row_add: re-basing on `rest` discarded the residue. */
        ps_row_add(orig, rowc, i, rec->coef * xv);
        ps_add_to_other_rows(orig, rowc, j, i, xv);
        break;
    }

    case JM_PS_FREE_COL_SINGLETON: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);
        /* This branch publishes a zero reduced cost and a zero dual with no
         * arithmetic behind either. That is sound for a genuinely free
         * column and invents both on a bounded one (D232). */
        assert(!isfinite(orig->col_lower[j]) &&
               !isfinite(orig->col_upper[j]));

        /* Mutual singleton: the row's whole activity is column j's. x_j is
         * free and cost-0, so d_j == 0 exactly forces y_i == 0 exactly; any
         * finite end of the recorded row range is as good a value as any. */
        const double target = isfinite(rec->lo) ? rec->lo :
                              (isfinite(rec->hi) ? rec->hi : 0.0);
        const double xv = target / rec->coef;

        orig->sol_col[j] = ps_published(xv);
        /* JAOS_BASIS_FREE means nonbasic AT ZERO; a nonzero xv is BASIC. */
        orig->sol_col_status[j] = (xv == 0.0) ? JAOS_BASIS_FREE
                                              : JAOS_BASIS_BASIC;
        orig->sol_redcost[j] = 0.0;
        orig->sol_dual[i] = 0.0;
        /* A no-op (row i's other columns were dead), written to stay one. */
        ps_row_add(orig, rowc, i, rec->coef * xv);
        ps_add_to_other_rows(orig, rowc, j, i, xv);
        /* Row-count invariant: this pair restores one row and one column, so
         * exactly one of the two is basic, the other nonbasic at a bound. */
        orig->sol_row_status[i] =
            (orig->sol_col_status[j] == JAOS_BASIS_FREE)
                ? JAOS_BASIS_BASIC : JAOS_BASIS_AT_LOWER;
        break;
    }

    case JM_PS_IMPLIED_FREE_COL: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        /* (sol_row[i], rowc[i]) at THIS moment is exactly the sum over the
         * columns live in row i when this fired, other than j: survivors were
         * seeded, later removals replayed already, earlier removals were
         * subtracted from rec->value when they left. Never sol_row alone. */
        const double xv = (rec->value - (orig->sol_row[i] + rowc[i])) /
                          rec->coef;

        /* x_j is strictly inside its box, so d_j = 0 and the row's multiplier
         * is one division; row i is an equality, no sign condition. It
         * reproduces the forward pass's division bit for bit. */
        const double yi = rec->cost / rec->coef;

        orig->sol_col[j] = ps_published(xv);
        orig->sol_redcost[j] = 0.0;
        orig->sol_dual[i] = ps_published(yi);
        ps_row_add(orig, rowc, i, rec->coef * xv);
        /* One row and one column restored, so exactly one is basic: x_j,
         * being interior. Row i is an equality, so AT_LOWER names it. */
        orig->sol_col_status[j] = JAOS_BASIS_BASIC;
        orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
        break;
    }

    case JM_PS_REDUNDANT_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        /* A row that never binds carries no multiplier. Its activity is NOT
         * assigned here: the slot is already accumulating. BASIC by the
         * row-count invariant (one row, no column). */
        orig->sol_dual[i] = 0.0;
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }

    case JM_PS_FORCING_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        /* Each pinned column has a one-sided sign requirement at an ORIGINAL
         * bound; for the UPPER case y_i = min_j d_j^0 / a_ij capped at 0,
         * the LOWER case mirrors. The columns are the `index2` records just
         * BEFORE this one, so this replays first. */
        double y = 0.0;
        for (int64_t t = 1; t <= rec->index2; t++) {
            /* `r - t` walks the arena backwards from this record. A count
             * that disagrees with what was pushed reads before the arena. */
            assert(r - t >= 0);
            const jm_presolve_rec *cr = &p->arena[r - t];
            assert(cr->tag == JM_PS_FIXED_COL);
            /* The divisor below. The forward pass excludes zero-coefficient
             * columns from the count, so a zero here means the two counts
             * disagree and `y` goes NaN (D232). */
            assert(cr->coef != 0.0);
            const int64_t j = cr->index;
            assert(j >= 0 && j < orig->num_col);

            double d0 = cr->cost;
            for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++)
                d0 -= orig->a_value[k] * orig->sol_dual[orig->a_index[k]];

            /* Canonical through the loop, flipped back once; sigma is +-1. */
            const double lim = sigma * (d0 / cr->coef);
            if (t == 1)
                y = lim;
            else if (rec->row_tightens_hi ? (lim < y) : (lim > y))
                y = lim;
        }
        if (rec->row_tightens_hi ? (y > 0.0) : (y < 0.0))
            y = 0.0;

        orig->sol_dual[i] = ps_published(sigma * y);
        /* Row-count invariant: the row takes the single basic slot. */
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }
    }
}

#ifndef NDEBUG
/* Catches the two producers that ASSIGN `sol_row[i]` (D106). Only on an
 * OPTIMAL solve; elsewhere sol_col and sol_row need not agree. */
static void ps_verify_row_activities(const jaos_model *orig)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    /* The replay is wrong on purpose here, so the check is skipped. */
    (void)orig;
    return;
#else
    if (orig->solve_status != JAOS_SOLVE_OPTIMAL)
        return;
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

    /* Only rows whose logical is BASIC: a nonbasic logical's activity is the
     * tight value. An n-term sum is known to `(n-1)*eps*SUM|t|`. */
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
#endif   /* fault build */
}
#endif   /* NDEBUG */

JAOS_NODISCARD jaos_status jm_postsolve_expand(jm_presolve *p)
{
    /* Entered from `publish`, before it returns, and only when presolve
     * actually reduced. On JM_PRESOLVE_NONE the reduced model IS the
     * caller's and this would replay an arena of records against it twice;
     * on JM_PRESOLVE_SOLVED there is no reduced solve to expand (D223). */
    assert(p->outcome == JM_PRESOLVE_REDUCED);
    jaos_model *orig = p->orig;
    const jaos_model *red = &p->reduced;
    /* "none aliases the caller's model": the reduced model owns fresh
     * arrays, and a shared pointer would have postsolve writing the answer
     * over the data it is reading. */
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

    /* The carry half (see ps_row_add), allocated before any status is
     * published: jaos_status_of would read unzeroed arrays on a later OOM. */
    double *rowc = calloc((size_t)orig->num_row + 1, sizeof *rowc);
    if (rowc == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    orig->solve_status = red->solve_status;
    orig->solve_iters  = red->solve_iters;
    orig->solve_work   = red->solve_work;
    orig->solve_time   = red->solve_time;

    if (red->solve_status != JAOS_SOLVE_OPTIMAL) {
        /* publish()'s non-optimal convention: zeroed, no old answer left. */
        orig->objective = 0.0;
        memset(orig->sol_col, 0, (size_t)orig->num_col * sizeof(double));
        memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_redcost, 0, (size_t)orig->num_col * sizeof(double));

        /* Status only, from red->start_* (publish()'s non-optimal branch
         * copies the interrupted status there and then zeroes sol_*). */
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
            /* Nothing survived to remember: leave orig->start_* as it was. A
             * caller basis can be non-null here on a numerical failure. */
            free(rowc);
            return JAOS_OK;
        }
        /* Arena-keyed corrections: FREE_COL_SINGLETON's column is FREE, and
         * SINGLETON_ROW's row takes its structural tightening direction. */
        for (int64_t r = 0; r < p->arena_len; r++) {
            const jm_presolve_rec *rec = &p->arena[r];
            if (rec->tag == JM_PS_FREE_COL_SINGLETON) {
                orig->sol_col_status[rec->index2] = JAOS_BASIS_FREE;
            } else if (rec->tag == JM_PS_IMPLIED_FREE_COL) {
                orig->sol_col_status[rec->index2] = JAOS_BASIS_BASIC;
                orig->sol_row_status[rec->index] = JAOS_BASIS_AT_LOWER;
            } else if (rec->tag == JM_PS_SINGLETON_ROW) {
                orig->sol_row_status[rec->index] =
                    rec->row_tightens_hi ? JAOS_BASIS_AT_UPPER :
                    rec->row_tightens_lo ? JAOS_BASIS_AT_LOWER :
                                            JAOS_BASIS_BASIC;
            }
        }

        free(rowc);
        /* NUMERICAL_ERROR gets no warm memory (D148): red->start_* still
         * holds the condemned basis jm_presolve_run mapped in. */
        if (red->solve_status != JAOS_SOLVE_NUMERICAL_ERROR)
            (void)jm_model_remember_basis(orig);
        return JAOS_OK;
    }

    /* The objective is set at the end, from the replayed values (D169). */

    /* Zeroed first: a dead row's slot is written only by its own replay and
     * must read a known 0 if another record's replay reads it first. */
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

    /* Dead rows are seeded with the SURVIVING columns' contributions, since
     * those push no record; removed columns add theirs at replay. */
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

    /* Strictly LIFO. */
    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);

    for (int64_t i = 0; i < orig->num_row; i++)
        orig->sol_row[i] = ps_published(orig->sol_row[i] + rowc[i]);
    free(rowc);

    /* Singleton row statuses, with every activity final; forward order, one
     * record per row (D8). ps_restore_index is applied as in the replay. */
    for (int64_t r = 0; r < p->arena_len; r++) {
        const jm_presolve_rec *rec = &p->arena[r];
        if (rec->tag == JM_PS_SINGLETON_ROW) {
            const int64_t i = ps_restore_index(rec->index, orig->num_row);
            orig->sol_row_status[i] = ps_singleton_row_status(orig, i);
        } else if (rec->tag == JM_PS_SINGLETON_COL) {
            ps_singleton_col_swap(orig, rec);
        }
    }

#ifndef NDEBUG
    ps_verify_row_activities(orig);
#endif

    jm_model_publish_objective(orig);
    (void)jm_model_remember_basis(orig);
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p)
{
    jaos_model *orig = p->orig;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    /* The carry half (ps_row_add), allocated before OPTIMAL is published. */
    double *rowc = calloc((size_t)orig->num_row + 1, sizeof *rowc);
    if (rowc == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    orig->solve_status = JAOS_SOLVE_OPTIMAL;
    orig->solve_iters  = 0;
    orig->solve_primal_iters = 0;
    orig->solve_phase1_iters = 0;
    orig->solve_work   = p->reduced.solve_work;
    /* No clock: seconds never enter a baseline (D17). */
    orig->solve_time   = 0.0;
    /* The objective is set at the end, from the replayed values (D169). */
    orig->presolve_num_row = p->reduced.num_row;
    orig->presolve_num_col = p->reduced.num_col;
    orig->presolve_num_nz  = p->reduced.num_nz;

    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
    /* A frozen ROW can survive with every column gone, and nothing else
     * writes its status. Zero is BASIC; the count can be over by one
     * (TODO.md). */
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

    /* The same second pass as jm_postsolve_expand, on BOTH paths. */
    for (int64_t r = 0; r < p->arena_len; r++) {
        const jm_presolve_rec *rec = &p->arena[r];
        if (rec->tag == JM_PS_SINGLETON_ROW) {
            const int64_t i = ps_restore_index(rec->index, orig->num_row);
            orig->sol_row_status[i] = ps_singleton_row_status(orig, i);
        } else if (rec->tag == JM_PS_SINGLETON_COL) {
            ps_singleton_col_swap(orig, rec);
        }
    }

#ifndef NDEBUG
    ps_verify_row_activities(orig);
#endif

    jm_model_publish_objective(orig);
    (void)jm_model_remember_basis(orig);
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

    /* publish()'s non-optimal convention: zeroed; no basis is offered. */
    memset(orig->sol_col, 0, (size_t)orig->num_col * sizeof(double));
    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_redcost, 0, (size_t)orig->num_col * sizeof(double));
    memset(orig->sol_col_status, 0,
           (size_t)orig->num_col * sizeof *orig->sol_col_status);
    memset(orig->sol_row_status, 0,
           (size_t)orig->num_row * sizeof *orig->sol_row_status);
    return JAOS_OK;
}
