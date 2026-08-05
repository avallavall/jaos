/* Independent solution checker.
 *
 * Judges a claimed solution against the model exactly as it was loaded —
 * original space, no scaling, no access to any solver bookkeeping. The test
 * suite runs it after every solve, so no solver bug can approve itself
 * ("check the thing, not the wrapper").
 *
 * DO NOT factor the reduced-cost loop below together with the identical
 * one in src/simplex.c. The duplication is the point: the moment these two
 * share an implementation, a solver whose pricing is wrong computes the
 * same wrong number here and the check passes. Independence is worth more
 * than the dozen lines it costs.
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
                             double tol, double *dual_obj)
{
    bool at_lo = isfinite(lo) && v <= lo + tol;
    bool at_hi = isfinite(hi) && v >= hi - tol;

    if (fabs(w) <= tol)
        return 0.0; /* negligible multiplier: no condition, no contribution */

    if (w > 0.0) {
        if (!isfinite(lo))
            return w;          /* points at a bound that does not exist */
        *dual_obj += w * lo;
        return at_lo ? 0.0 : w;   /* positive w is only justified at lower */
    }
    if (!isfinite(hi))
        return -w;
    *dual_obj += w * hi;
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
     * is fixed by the data structure, so the result is deterministic (D8). */
    double *act = jm_calloc_array(m->num_row, sizeof(double));
    if (act == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t j = 0; j < m->num_col; j++) {
        double xj = col_value[j];
        if (xj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            act[m->a_index[k]] += m->a_value[k] * xj;
    }

    /* Primal side. */
    double col_viol = 0.0, row_viol = 0.0, primal_obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++) {
        col_viol = max2(col_viol, interval_violation(col_value[j],
                                    m->col_lower[j], m->col_upper[j]));
        primal_obj += m->col_cost[j] * col_value[j];
    }
    for (int64_t i = 0; i < m->num_row; i++)
        row_viol = max2(row_viol, interval_violation(act[i],
                                    m->row_lower[i], m->row_upper[i]));

    out->max_col_violation = col_viol;
    out->max_row_violation = row_viol;
    out->primal_objective = primal_obj;
    out->primal_feasible = col_viol <= tol && row_viol <= tol;

    /* Dual side, minimize-canonical. */
    if (row_dual != nullptr) {
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double dual_viol = 0.0;
        double dual_obj = sigma * m->obj_offset; /* canonical offset */

        for (int64_t i = 0; i < m->num_row; i++)
            dual_viol = max2(dual_viol,
                sign_condition(act[i], m->row_lower[i], m->row_upper[i],
                               sigma * row_dual[i], tol, &dual_obj));

        for (int64_t j = 0; j < m->num_col; j++) {
            /* Reduced cost d_j = c_j - a_j' y, canonicalized. */
            double d = m->col_cost[j];
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                d -= m->a_value[k] * row_dual[m->a_index[k]];
            dual_viol = max2(dual_viol,
                sign_condition(col_value[j], m->col_lower[j], m->col_upper[j],
                               sigma * d, tol, &dual_obj));
        }

        double true_dual_obj = sigma * dual_obj; /* back to the model's sense */
        double gap = fabs(primal_obj - true_dual_obj)
                     / max2(1.0, fabs(primal_obj));

        out->checked_duals = true;
        out->max_dual_violation = dual_viol;
        out->dual_objective = true_dual_obj;
        out->objective_gap = gap;
        out->dual_feasible = dual_viol <= tol && gap <= tol;
    }

    free(act);
    return JAOS_OK;
}
