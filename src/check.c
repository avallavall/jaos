/* Independent solution checker.
 *
 * Judges a claimed solution against the model exactly as it was loaded —
 * original space, no scaling, no access to any solver bookkeeping.
 *
 * What the independence actually rests on, since this is easy to get wrong:
 *
 *   - Independent inputs. The only things that enter are the model as
 *     loaded and the claimed (x, y). No basis, no factorization, no solver
 *     state. A solver that reasoned its way to a wrong answer cannot pass
 *     that reasoning in here.
 *   - Redundant identities. Primal feasibility, dual sign conditions,
 *     complementary slackness and the primal-dual gap are checked
 *     together, and they are not independent of each other. The dual
 *     objective is accumulated from bounds while activities come from a
 *     separate scatter over the matrix, so a corrupted dot product shows
 *     up as a nonzero gap even if it also corrupts the reduced costs. The
 *     system is overdetermined; one broken kernel cannot satisfy all four.
 *   - Better arithmetic. Accumulation is in long double, so the oracle's
 *     own rounding error is an order below what it is judging. A checker
 *     no more accurate than the solver measures partly itself.
 *
 * What it does NOT protect against, stated so nobody assumes otherwise:
 * a model built wrongly by the loader. Checker and solver read the same
 * a_start/a_index/a_value, and if those are wrong both agree about the
 * wrong problem. Only external ground truth — reference optima — closes
 * that, which is what the Netlib gate is for.
 *
 * The reduced-cost loop here resembles the one in src/simplex.c, and they
 * stay apart: sharing would make this file link against solver internals,
 * the exact coupling it exists to forbid. Once scaling is wired in (Q7)
 * they operate on different matrices anyway.
 *
 * Everything is computed in minimize-canonical form: for maximization the
 * cost and the duals are negated internally (sigma), which turns the sign
 * conditions documented in jaos.h into one set of rules.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

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

/* Sign-condition violation for a multiplier w attached to a value v with
 * bounds [lo, hi], in minimize-canonical form:
 *   at lower  -> w >= 0
 *   at upper  -> w <= 0
 *   interior  -> w == 0   (this is complementary slackness)
 *   fixed     -> anything
 * "At a bound" is judged within tol. Also accumulates the multiplier's
 * contribution to the dual objective: w picks the bound its sign points at;
 * a meaningful multiplier pointing at an infinite bound is itself a dual
 * violation. */
static double sign_condition(double v, double lo, double hi, double w,
                             double tol, long double *dual_obj)
{
    bool at_lo = isfinite(lo) && v <= lo + tol;
    bool at_hi = isfinite(hi) && v >= hi - tol;

    if (fabs(w) <= tol)
        return 0.0; /* negligible multiplier: no condition, no contribution */

    if (w > 0.0) {
        if (!isfinite(lo))
            return w;          /* points at a bound that does not exist */
        *dual_obj += (long double)w * (long double)lo;
        return at_lo ? 0.0 : w;   /* positive w is only justified at lower */
    }
    if (!isfinite(hi))
        return -w;
    *dual_obj += (long double)w * (long double)hi;
    return at_hi ? 0.0 : -w;      /* negative w is only justified at upper */
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
     * is fixed by the data structure, so the result is deterministic (D8).
     * Accumulated wider than the values being judged, so what the report
     * measures is the solver's error and not the checker's. */
    long double *act = jm_calloc_array(m->num_row, sizeof(long double));
    if (act == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t j = 0; j < m->num_col; j++) {
        double xj = col_value[j];
        if (xj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            act[m->a_index[k]] += (long double)m->a_value[k] * (long double)xj;
    }

    /* Primal side. */
    double col_viol = 0.0, row_viol = 0.0;
    long double primal_obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++) {
        col_viol = max2(col_viol, interval_violation(col_value[j],
                                    m->col_lower[j], m->col_upper[j]));
        primal_obj += (long double)m->col_cost[j] * (long double)col_value[j];
    }
    for (int64_t i = 0; i < m->num_row; i++)
        row_viol = max2(row_viol, interval_violation((double)act[i],
                                    m->row_lower[i], m->row_upper[i]));

    out->max_col_violation = col_viol;
    out->max_row_violation = row_viol;
    out->primal_objective = (double)primal_obj;
    out->primal_feasible = col_viol <= tol && row_viol <= tol;

    /* Dual side, minimize-canonical. */
    if (row_dual != nullptr) {
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double dual_viol = 0.0;
        long double dual_obj = sigma * m->obj_offset; /* canonical offset */

        for (int64_t i = 0; i < m->num_row; i++)
            dual_viol = max2(dual_viol,
                sign_condition((double)act[i], m->row_lower[i],
                               m->row_upper[i], sigma * row_dual[i], tol,
                               &dual_obj));

        for (int64_t j = 0; j < m->num_col; j++) {
            /* Reduced cost d_j = c_j - a_j' y, canonicalized. */
            long double dw = m->col_cost[j];
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                dw -= (long double)m->a_value[k] *
                      (long double)row_dual[m->a_index[k]];
            double d = (double)dw;
            dual_viol = max2(dual_viol,
                sign_condition(col_value[j], m->col_lower[j], m->col_upper[j],
                               sigma * d, tol, &dual_obj));
        }

        /* The gap is the identity that catches a corrupted dot product even
         * when the sign conditions it also corrupts still pass, so it is
         * formed at the wider precision and narrowed only at the end. */
        long double true_dual_obj = (long double)sigma * dual_obj;
        long double diff = primal_obj - true_dual_obj;
        if (diff < 0)
            diff = -diff;
        long double scale = primal_obj < 0 ? -primal_obj : primal_obj;
        double gap = (double)(diff / (scale > 1.0L ? scale : 1.0L));

        out->checked_duals = true;
        out->max_dual_violation = dual_viol;
        out->dual_objective = (double)true_dual_obj;
        out->objective_gap = gap;
        out->dual_feasible = dual_viol <= tol && gap <= tol;
    }

    free(act);
    return JAOS_OK;
}
