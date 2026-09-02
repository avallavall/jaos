/* Independent solution checker.
 *
 * Judges a claimed solution against the model exactly as it was loaded —
 * original space, no scaling, no access to any solver bookkeeping (D18).
 * It does NOT protect against a model built wrongly by the loader: checker
 * and solver read the same a_start/a_index/a_value. The reduced-cost loop
 * here resembles the one in src/simplex.c, and they stay apart: sharing
 * would make this file link against solver internals.
 *
 * Everything is computed in minimize-canonical form: for maximization the
 * cost and the duals are negated internally (sigma).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double max2(double a, double b) { return a > b ? a : b; }

/* Violation of "v must lie in [lo, hi]" as a raw magnitude. */
static double interval_violation(double v, double lo, double hi)
{
    double viol = 0.0;
    if (isfinite(lo))
        viol = max2(viol, lo - v);
    if (isfinite(hi))
        viol = max2(viol, v - hi);
    return viol;
}

/* What the dual walk accumulates. `pos`/`neg` are magnitudes. */
typedef struct {
    long double dual_obj;
    long double pos, neg;

    /* The same two sums restricted to bounds the model declared (D91). An
     * implied bound is sound but slack, so its term is live at an optimum.
     * The verdict reads these; `pos`/`neg` bound the suboptimality. */
    long double pos_model, neg_model;

    /* The largest multiplier whose term the dual objective could not take,
     * and how many there were. No verdict reads them (D47). */
    double dropped_max;
    int64_t dropped_n;

    /* The largest suboptimality any one dropped term certifies: a lower
     * bound on `P - P*` (D73). Zero means nothing was certified. */
    long double certified;

    /* Dropped columns that can move without limit while every row stays
     * inside its bounds, and whose multiplier this checker calls zero.
     * Counted rather than certified (D73). */
    int64_t rays;
} dual_acc;

static void split_term(long double t, long double *pos, long double *neg)
{
    if (t > 0.0L)
        *pos += t;
    else
        *neg -= t;    /* kept as a magnitude, so both halves are >= 0 */
}

/* Records a term the dual objective could not take. Every nonzero multiplier
 * pointing at an infinite bound counts, with no magnitude exemption (D47). */
static void note_dropped(dual_acc *a, double w)
{
    a->dropped_n++;
    if (fabs(w) > a->dropped_max)
        a->dropped_max = fabs(w);
}

/* How far column j can travel in direction `dir` before some row leaves its
 * bounds, with every other variable held exactly where it is. It travels
 * less far than the simplex direction, so what it certifies is a lower
 * bound (D73). Only ever called where the column's own opposite bound is
 * infinite. Room is clamped at zero because the point being judged may sit
 * a tolerance outside a bound. Returns HUGE_VALL when nothing blocks. */
static long double certified_step(const jaos_model *m, int64_t j, double dir,
                                  const long double *act)
{
    /* Both sentences above, checked rather than written (D219). The caller
     * reaches here only where `drops` held, and `drops` is the same test on
     * the bound this direction travels away from. */
    assert(!isfinite(dir < 0.0 ? m->col_lower[j] : m->col_upper[j]));
    long double t = HUGE_VALL;
    for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
        int64_t i = m->a_index[k];
        long double per_t = (long double)m->a_value[k] * dir;
        if (per_t == 0.0L)
            continue;

        long double room, limit;
        if (per_t > 0.0L) {
            if (!isfinite(m->row_upper[i]))
                continue;
            room = (long double)m->row_upper[i] - act[i];
            limit = (room > 0.0L ? room : 0.0L) / per_t;
        } else {
            if (!isfinite(m->row_lower[i]))
                continue;
            room = act[i] - (long double)m->row_lower[i];
            limit = (room > 0.0L ? room : 0.0L) / -per_t;
        }
        if (limit < t)
            t = limit;
    }
    /* "Room is clamped at zero", so what this returns is a distance and
     * never a negative one; a negative would turn a certified suboptimality
     * into a claim that the point is better than optimal (D219). */
    assert(t >= 0.0L);
    return t;
}

/* The cap on propagation rounds. Not a quality knob: the loop exits as soon
 * as a round bounds nothing new. Sweep in docs/tolerances.md (D91). */
constexpr int64_t IMPLIED_ROUNDS = 64;

/* The bounds the constraints imply for a variable the model left unbounded
 * (D87). The bound is implied, not imposed: every feasible point already
 * satisfies it, so adding it changes neither the feasible region nor `P*`.
 * Where the rows imply nothing the bound stays infinite and the term is
 * still dropped. Terms that are infinite are counted rather than summed. */
static void implied_bounds(const jaos_model *m, double *cl, double *cu,
                           long double *lo_sum, long double *up_sum,
                           int64_t *lo_inf, int64_t *up_inf)
{
    const int64_t nr = m->num_row, nc = m->num_col;

    for (int64_t j = 0; j < nc; j++) {
        cl[j] = m->col_lower[j];
        cu[j] = m->col_upper[j];
    }

    /* Iterated (D91). Only ever tightened, never loosened: the sequence is
     * monotone, so stopping early is safe. */
    for (int64_t pass = 0; pass < IMPLIED_ROUNDS; pass++) {
    bool moved = false;

    for (int64_t i = 0; i < nr; i++) {
        lo_sum[i] = 0.0L;
        up_sum[i] = 0.0L;
        lo_inf[i] = 0;
        up_inf[i] = 0;
    }

    /* Each row's activity range over the column boxes, infinities counted. */
    for (int64_t j = 0; j < nc; j++) {
        const double xl = cl[j], xu = cu[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double aij = m->a_value[k];
            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            if (isfinite(t_lo))
                lo_sum[i] += (long double)aij * t_lo;
            else
                lo_inf[i]++;
            if (isfinite(t_up))
                up_sum[i] += (long double)aij * t_up;
            else
                up_inf[i]++;
        }
    }

    for (int64_t j = 0; j < nc; j++) {
        const bool want_lo = !isfinite(cl[j]), want_up = !isfinite(cu[j]);
        if (!want_lo && !want_up)
            continue;

        const double xl = cl[j], xu = cu[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double aij = m->a_value[k];
            if (aij == 0.0)
                continue;

            /* This column's own share of each end of the row's range, and
             * whether the rest of the row is finite once it is taken out. */
            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            const bool rest_lo_finite =
                lo_inf[i] == (isfinite(t_lo) ? 0 : 1);
            const bool rest_up_finite =
                up_inf[i] == (isfinite(t_up) ? 0 : 1);
            const long double rest_lo =
                lo_sum[i] - (isfinite(t_lo) ? (long double)aij * t_lo : 0.0L);
            const long double rest_up =
                up_sum[i] - (isfinite(t_up) ? (long double)aij * t_up : 0.0L);

            /* a_ij x_j <= ru - min(rest)  and  a_ij x_j >= rl - max(rest).
             * Dividing by a_ij swaps the two when it is negative. */
            if (rest_lo_finite && isfinite(m->row_upper[i])) {
                const long double lim =
                    ((long double)m->row_upper[i] - rest_lo) / aij;
                if (aij > 0.0) {
                    if (want_up && (double)lim < cu[j]) {
                        cu[j] = (double)lim;
                        moved = true;
                    }
                } else if (want_lo && (double)lim > cl[j]) {
                    cl[j] = (double)lim;
                    moved = true;
                }
            }
            if (rest_up_finite && isfinite(m->row_lower[i])) {
                const long double lim =
                    ((long double)m->row_lower[i] - rest_up) / aij;
                if (aij > 0.0) {
                    if (want_lo && (double)lim > cl[j]) {
                        cl[j] = (double)lim;
                        moved = true;
                    }
                } else if (want_up && (double)lim < cu[j]) {
                    cu[j] = (double)lim;
                    moved = true;
                }
            }
        }
    }

    if (!moved)
        break;   /* a round that bounded nothing new cannot enable another */
    }

    /* "Only ever tightened, never loosened", checked rather than written
     * (D219). A loosened bound would enlarge the feasible region the gap is
     * measured over, and the verdict would certify a point the model does
     * not contain. Holds by construction: a bound is written only where it
     * was infinite, and then only to a strictly tighter value. */
#ifndef NDEBUG
    for (int64_t j = 0; j < nc; j++) {
        assert(cl[j] >= m->col_lower[j]);
        assert(cu[j] <= m->col_upper[j]);
    }
#endif
}

/* Sign-condition violation for a multiplier w attached to a value v with
 * bounds [lo, hi], in minimize-canonical form:
 *   at lower  -> w >= 0
 *   at upper  -> w <= 0
 *   interior  -> w == 0   (this is complementary slackness)
 *   fixed     -> anything
 * "At a bound" is judged within tol * scale, and a multiplier at or below
 * tol is held to no condition at all. `scale` is what the value being
 * tested is made of (D23). A value waived here at a distance d with a
 * multiplier w contributes w * d to the gap, which is checked separately.
 * Also accumulates the multiplier's contribution to the dual objective: w
 * picks the bound its sign points at, and every multiplier contributes,
 * including the ones exempt from the condition (D22), split by sign (D24). */
static double sign_condition(double v, double lo, double hi, double w,
                             double tol, double scale, dual_acc *a,
                             bool lo_implied, bool hi_implied)
{
    double window = tol * scale;
    bool at_lo = isfinite(lo) && v <= lo + window;
    bool at_hi = isfinite(hi) && v >= hi - window;

    bool negligible = fabs(w) <= tol;

    if (w > 0.0) {
        if (!isfinite(lo)) {
            note_dropped(a, w);
            return negligible ? 0.0 : w;
        }
        a->dual_obj += (long double)w * lo;
        const long double t = (long double)w * ((long double)v - lo);
        split_term(t, &a->pos, &a->neg);
        if (!lo_implied)
            split_term(t, &a->pos_model, &a->neg_model);
        return (negligible || at_lo) ? 0.0 : w;
    }
    if (w < 0.0) {
        if (!isfinite(hi)) {
            note_dropped(a, w);
            return negligible ? 0.0 : -w;
        }
        a->dual_obj += (long double)w * hi;
        const long double t = (long double)w * ((long double)v - hi);
        split_term(t, &a->pos, &a->neg);
        if (!hi_implied)
            split_term(t, &a->pos_model, &a->neg_model);
        return (negligible || at_hi) ? 0.0 : -w;
    }
    return 0.0;   /* a zero multiplier contributes to neither */
}

jaos_status jaos_check_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, double tol,
    jaos_check_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!(isfinite(tol) && tol >= 0.0))
        return JAOS_ERR_INVALID_INPUT;
    if (m->num_col > 0 && col_value == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t j = 0; j < m->num_col; j++)
        if (isnan(col_value[j]))
            return JAOS_ERR_INVALID_INPUT;
    if (row_dual != nullptr)
        for (int64_t i = 0; i < m->num_row; i++)
            if (isnan(row_dual[i]))
                return JAOS_ERR_INVALID_INPUT;

    memset(out, 0, sizeof *out);

    /* Row activities from the CSC copy: column-order scatter-add. The order
     * is fixed by the data structure, so the result is deterministic (D8). */
    long double *act = jm_calloc_array(m->num_row, sizeof(long double));
    /* The sum of the magnitudes of each activity's terms: its scale. */
    long double *traffic = jm_calloc_array(m->num_row, sizeof(long double));
    if (act == nullptr || traffic == nullptr) {
        free(act);
        free(traffic);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        double xj = col_value[j];
        if (xj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            long double term = (long double)m->a_value[k] * xj;
            act[m->a_index[k]] += term;
            traffic[m->a_index[k]] += fabsl(term);
        }
    }

    /* Primal side. */
    double col_viol = 0.0, row_viol = 0.0, row_viol_rel = 0.0;
    long double primal_obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++) {
        col_viol = max2(col_viol, interval_violation(col_value[j],
                                    m->col_lower[j], m->col_upper[j]));
        primal_obj += (long double)m->col_cost[j] * col_value[j];
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        double viol = interval_violation((double)act[i], m->row_lower[i],
                                         m->row_upper[i]);
        row_viol = max2(row_viol, viol);
        /* The same residue against what the row is made of. It decides
         * nothing: D24 keeps the predicate absolute. */
        row_viol_rel = max2(row_viol_rel,
                            viol / max2(1.0, (double)traffic[i]));
    }

    out->max_col_violation = col_viol;
    out->max_row_violation = row_viol;
    out->max_row_violation_relative = row_viol_rel;
    out->primal_objective = (double)primal_obj;
    out->primal_feasible = col_viol <= tol && row_viol <= tol;

    /* Dual side, minimize-canonical. */
    if (row_dual != nullptr) {
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double dual_viol = 0.0;
        dual_acc a = {0};
        a.dual_obj = sigma * m->obj_offset;   /* canonical offset */

        /* What the constraints imply about the variables the model left
         * unbounded (D87). Allocation failure is not fatal: the bounds fall
         * back to the model's own. A row's own range comes free with them. */
        double *icl = jm_calloc_array(m->num_col, sizeof(double));
        double *icu = jm_calloc_array(m->num_col, sizeof(double));
        long double *rlo = jm_calloc_array(m->num_row, sizeof(long double));
        long double *rup = jm_calloc_array(m->num_row, sizeof(long double));
        int64_t *rli = jm_calloc_array(m->num_row, sizeof(int64_t));
        int64_t *rui = jm_calloc_array(m->num_row, sizeof(int64_t));
        const bool implied = icl != nullptr && icu != nullptr &&
                             rlo != nullptr && rup != nullptr &&
                             rli != nullptr && rui != nullptr;
        if (implied)
            implied_bounds(m, icl, icu, rlo, rup, rli, rui);

        for (int64_t i = 0; i < m->num_row; i++) {
            double rl = m->row_lower[i], ru = m->row_upper[i];
            bool rl_imp = false, ru_imp = false;
            if (implied) {
                if (!isfinite(rl) && rli[i] == 0) {
                    rl = (double)rlo[i];
                    rl_imp = true;
                }
                if (!isfinite(ru) && rui[i] == 0) {
                    ru = (double)rup[i];
                    ru_imp = true;
                }
            }
            dual_viol = max2(dual_viol,
                sign_condition((double)act[i], rl, ru, sigma * row_dual[i],
                               tol, max2(1.0, (double)traffic[i]), &a,
                               rl_imp, ru_imp));
        }

        for (int64_t j = 0; j < m->num_col; j++) {
            /* Reduced cost d_j = c_j - a_j' y, canonicalized. */
            long double dw = m->col_cost[j];
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                dw -= (long double)m->a_value[k] * row_dual[m->a_index[k]];
            double d = (double)dw;
            /* A column value is one published number, so its scale is its
             * own magnitude. */
            dual_viol = max2(dual_viol,
                sign_condition(col_value[j],
                               implied ? icl[j] : m->col_lower[j],
                               implied ? icu[j] : m->col_upper[j],
                               sigma * d, tol, max2(1.0, fabs(col_value[j])),
                               &a,
                               implied && !isfinite(m->col_lower[j]),
                               implied && !isfinite(m->col_upper[j])));

            /* Where that call had to drop a term, this says what the drop
             * is worth. Rows are not done: no single-entity direction (D73). */
            const double w = sigma * d;
            const bool drops = (w > 0.0 && !isfinite(m->col_lower[j])) ||
                               (w < 0.0 && !isfinite(m->col_upper[j]));
            if (drops) {
                long double t = certified_step(m, j, w > 0.0 ? -1.0 : 1.0,
                                               act);
                if (isinf((double)t) && fabs(w) <= tol) {
                    /* An unbounded ray whose rate this checker calls zero.
                     * Counted, not certified (see dual_acc). */
                    a.rays++;
                } else {
                    long double gain = (long double)fabs(w) * t;
                    if (gain > a.certified)
                        a.certified = gain;
                }
            }
        }

        /* Two gaps (D91). The one the verdict reads is over bounds the model
         * declared, where every term must vanish at an optimum. The other
         * includes the implied bounds and is what bounds the suboptimality. */
        /* All four are magnitudes, which is what makes their difference the
         * gap rather than a sum with a sign error in it (D219). `split_term`
         * negates the negative half on the way in, so a sign fault there
         * would read as a plausible smaller gap and nothing else. */
        assert(a.pos >= 0.0L && a.neg >= 0.0L);
        assert(a.pos_model >= 0.0L && a.neg_model >= 0.0L);
        long double true_dual_obj = sigma * a.dual_obj;
        long double scale = 1.0L + fabsl(primal_obj) + fabsl(true_dual_obj);
        double gap = (double)(fabsl(a.pos_model - a.neg_model) / scale);

        out->checked_duals = true;
        out->max_dual_violation = dual_viol;
        out->dual_objective = (double)true_dual_obj;
        out->objective_gap = gap;

        /* The suboptimality bound, relative to the objective (D47). */
        out->relative_suboptimality =
            (double)(a.pos / (1.0L + fabsl(primal_obj)));
        /* Whether the identity the two halves come from was complete (D47). */
        out->max_dropped_multiplier = a.dropped_max;
        out->dropped_terms = a.dropped_n;
        out->gap_certified = a.dropped_n == 0;
        out->certified_suboptimality = (double)a.certified;
        out->unquantified_rays = a.rays;
        /* In the objective's own units: they are for P - P* <= gap_positive. */
        out->gap_positive = (double)a.pos;
        out->gap_negative = (double)a.neg;
        out->dual_feasible = dual_viol <= tol && gap <= tol;

        free(icl);
        free(icu);
        free(rlo);
        free(rup);
        free(rli);
        free(rui);
    }

    free(act);
    free(traffic);
    return JAOS_OK;
}

/* Judges a claimed infeasibility certificate against the model as loaded:
 * original space, the model's own bounds, no solver bookkeeping (D18).
 * The claim is an impossibility: the smallest value the row bounds allow
 * y'(Ax) to take still exceeds the largest value the column bounds allow
 * (A'y)'x to take — and the two are the same number for any x, so no
 * feasible x exists. A ray that needs an infinite bound side makes its
 * sum infinite and the proof dies, whatever produced the ray (D254). */
jaos_status jaos_check_certificate(const jaos_model *m,
    const double *row_ray, double tol, jaos_certificate_report *out)
{
    if (m == nullptr || row_ray == nullptr || out == nullptr ||
        !isfinite(tol) || tol < 0.0)
        return JAOS_ERR_INVALID_INPUT;
    out->sup_columns = 0.0;
    out->inf_rows = 0.0;
    out->gap = 0.0;
    out->certified = false;

    for (int64_t i = 0; i < m->num_row; i++)
        if (!isfinite(row_ray[i]))
            return JAOS_ERR_INVALID_INPUT;

    bool bounded = true;
    long double sup_cols = 0.0L;
    for (int64_t j = 0; j < m->num_col && bounded; j++) {
        long double a = 0.0L, traffic = 0.0L;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const long double t =
                (long double)m->a_value[p] * row_ray[m->a_index[p]];
            a += t;
            traffic += fabsl(t);
        }
        /* How precisely this sum can be placed is set by the terms that
         * went into it (the same rule the bound-rest test above states):
         * below tol times its own traffic, (A'y)_j is a zero the
         * arithmetic cannot distinguish — read literally against an
         * infinite bound it would turn roundoff on a structurally zero
         * column into an infinite sup (D254). */
        if (fabsl(a) <= tol * traffic)
            continue;
        if (a > 0.0L) {
            if (isfinite(m->col_upper[j]))
                sup_cols += a * (long double)m->col_upper[j];
            else
                bounded = false;
        } else if (a < 0.0L) {
            if (isfinite(m->col_lower[j]))
                sup_cols += a * (long double)m->col_lower[j];
            else
                bounded = false;
        }
    }

    long double inf_rows = 0.0L;
    for (int64_t i = 0; i < m->num_row && bounded; i++) {
        const double y = row_ray[i];
        if (y > 0.0) {
            if (isfinite(m->row_lower[i]))
                inf_rows += (long double)y * m->row_lower[i];
            else
                bounded = false;
        } else if (y < 0.0) {
            if (isfinite(m->row_upper[i]))
                inf_rows += (long double)y * m->row_upper[i];
            else
                bounded = false;
        }
    }

    if (!bounded) {
        /* The sup (or inf) really is infinite; published as such, and
         * the gap with it, so the report says which side died. */
        out->sup_columns = INFINITY;
        out->inf_rows = -INFINITY;
        out->gap = -INFINITY;
        return JAOS_OK;
    }

    out->sup_columns = (double)sup_cols;
    out->inf_rows = (double)inf_rows;
    out->gap = (double)(inf_rows - sup_cols);
    out->certified = out->gap >
        tol * (1.0 + fabs(out->sup_columns) + fabs(out->inf_rows));
    return JAOS_OK;
}
