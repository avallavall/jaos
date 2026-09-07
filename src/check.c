/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double max2(double a, double b) { return a > b ? a : b; }

static double interval_violation(double v, double lo, double hi)
{
    double viol = 0.0;
    if (isfinite(lo))
        viol = max2(viol, lo - v);
    if (isfinite(hi))
        viol = max2(viol, v - hi);
    return viol;
}

typedef struct {
    double dual_obj, dual_objc;
    double pos, posc, neg, negc;

    double pos_model, pos_modelc, neg_model, neg_modelc;

    double dropped_max;
    int64_t dropped_n;

    double certified;

    int64_t rays;
} dual_acc;

static double acc_value(double sum, double comp)
{
    return (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

static double acc_difference(double sum_a, double comp_a,
                             double sum_b, double comp_b)
{
    if (!(isfinite(sum_a) && isfinite(comp_a) &&
          isfinite(sum_b) && isfinite(comp_b)))
        return sum_a - sum_b;
    return (sum_a - sum_b) + (comp_a - comp_b);
}

static double two_sum_residue(double a, double b, double s)
{
    if (!isfinite(s))
        return 0.0;
    const double bb = s - a;
    return (a - (s - bb)) + (b - bb);
}

static void split_term(double t, double e, double *pos, double *posc,
                       double *neg, double *negc)
{
    if (t > 0.0) {
        jm_obj_add(pos, posc, t);
        if (e != 0.0)
            jm_obj_add(pos, posc, e);
    } else {

        jm_obj_add(neg, negc, -t);
        if (e != 0.0)
            jm_obj_add(neg, negc, -e);
    }
}

static void add_product(double *sum, double *comp, double a, double b)
{
    const double p = a * b;
    const double e = jm_two_product_residue(a, b, p);
    jm_obj_add(sum, comp, p);
    if (e != 0.0)
        jm_obj_add(sum, comp, e);
}

static double bound_term(double w, double v, double b, double *e)
{
    const double dv = v - b;
    const double dve = two_sum_residue(v, -b, dv);
    const double t = w * dv;

    if (!isfinite(t)) {
        *e = 0.0;
        return t;
    }
    *e = jm_two_product_residue(w, dv, t);
    if (dve != 0.0)
        *e += w * dve;
    return t;
}

static void note_dropped(dual_acc *a, double w)
{
    a->dropped_n++;
    if (fabs(w) > a->dropped_max)
        a->dropped_max = fabs(w);
}

static double certified_step(const jaos_model *m, int64_t j, double dir,
                             const double *act)
{

    assert(!isfinite(dir < 0.0 ? m->col_lower[j] : m->col_upper[j]));
    double t = HUGE_VAL;
    for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
        int64_t i = m->a_index[k];

        const double per_t = m->a_value[k] * dir;
        if (per_t == 0.0)
            continue;

        double room, limit;
        if (per_t > 0.0) {
            if (!isfinite(m->row_upper[i]))
                continue;
            room = m->row_upper[i] - act[i];
            limit = (room > 0.0 ? room : 0.0) / per_t;
        } else {
            if (!isfinite(m->row_lower[i]))
                continue;
            room = act[i] - m->row_lower[i];
            limit = (room > 0.0 ? room : 0.0) / -per_t;
        }
        if (limit < t)
            t = limit;
    }

    assert(t >= 0.0);
    return t;
}

constexpr int64_t IMPLIED_ROUNDS = 64;

static void implied_bounds(const jaos_model *m, double *cl, double *cu,
                           double *lo_sum, double *lo_comp,
                           double *up_sum, double *up_comp,
                           int64_t *lo_inf, int64_t *up_inf)
{
    const int64_t nr = m->num_row, nc = m->num_col;

    for (int64_t j = 0; j < nc; j++) {
        cl[j] = m->col_lower[j];
        cu[j] = m->col_upper[j];
    }

    for (int64_t pass = 0; pass < IMPLIED_ROUNDS; pass++) {
    bool moved = false;

    for (int64_t i = 0; i < nr; i++) {
        lo_sum[i] = 0.0;
        lo_comp[i] = 0.0;
        up_sum[i] = 0.0;
        up_comp[i] = 0.0;
        lo_inf[i] = 0;
        up_inf[i] = 0;
    }

    for (int64_t j = 0; j < nc; j++) {
        const double xl = cl[j], xu = cu[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double aij = m->a_value[k];
            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            if (isfinite(t_lo)) {
                const double p = aij * t_lo;
                const double e = jm_two_product_residue(aij, t_lo, p);
                jm_obj_add(&lo_sum[i], &lo_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&lo_sum[i], &lo_comp[i], e);
            } else {
                lo_inf[i]++;
            }
            if (isfinite(t_up)) {
                const double p = aij * t_up;
                const double e = jm_two_product_residue(aij, t_up, p);
                jm_obj_add(&up_sum[i], &up_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&up_sum[i], &up_comp[i], e);
            } else {
                up_inf[i]++;
            }
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

            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            const bool rest_lo_finite =
                lo_inf[i] == (isfinite(t_lo) ? 0 : 1);
            const bool rest_up_finite =
                up_inf[i] == (isfinite(t_up) ? 0 : 1);

            const double own_lo = isfinite(t_lo) ? aij * t_lo : 0.0;
            const double own_up = isfinite(t_up) ? aij * t_up : 0.0;
            const double own_lo_e = isfinite(t_lo)
                ? jm_two_product_residue(aij, t_lo, own_lo) : 0.0;
            const double own_up_e = isfinite(t_up)
                ? jm_two_product_residue(aij, t_up, own_up) : 0.0;
            const double rest_lo =
                acc_difference(lo_sum[i], lo_comp[i], own_lo, own_lo_e);
            const double rest_up =
                acc_difference(up_sum[i], up_comp[i], own_up, own_up_e);

            if (rest_lo_finite && isfinite(m->row_upper[i])) {
                const double lim = (m->row_upper[i] - rest_lo) / aij;
                if (!isfinite(lim)) {

                } else if (aij > 0.0) {
                    if (want_up && lim < cu[j]) {
                        cu[j] = lim;
                        moved = true;
                    }
                } else if (want_lo && lim > cl[j]) {
                    cl[j] = lim;
                    moved = true;
                }
            }
            if (rest_up_finite && isfinite(m->row_lower[i])) {
                const double lim = (m->row_lower[i] - rest_up) / aij;
                if (!isfinite(lim)) {

                } else if (aij > 0.0) {
                    if (want_lo && lim > cl[j]) {
                        cl[j] = lim;
                        moved = true;
                    }
                } else if (want_up && lim < cu[j]) {
                    cu[j] = lim;
                    moved = true;
                }
            }
        }
    }

    if (!moved)
        break;
    }

#ifndef NDEBUG
    for (int64_t j = 0; j < nc; j++) {
        assert(cl[j] >= m->col_lower[j]);
        assert(cu[j] <= m->col_upper[j]);
    }
#endif
}

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
        const double c = w * lo;
        jm_obj_add(&a->dual_obj, &a->dual_objc, c);
        const double ce = jm_two_product_residue(w, lo, c);
        if (ce != 0.0)
            jm_obj_add(&a->dual_obj, &a->dual_objc, ce);
        double e;
        const double t = bound_term(w, v, lo, &e);
        split_term(t, e, &a->pos, &a->posc, &a->neg, &a->negc);
        if (!lo_implied)
            split_term(t, e, &a->pos_model, &a->pos_modelc,
                       &a->neg_model, &a->neg_modelc);
        return (negligible || at_lo) ? 0.0 : w;
    }
    if (w < 0.0) {
        if (!isfinite(hi)) {
            note_dropped(a, w);
            return negligible ? 0.0 : -w;
        }
        const double c = w * hi;
        jm_obj_add(&a->dual_obj, &a->dual_objc, c);
        const double ce = jm_two_product_residue(w, hi, c);
        if (ce != 0.0)
            jm_obj_add(&a->dual_obj, &a->dual_objc, ce);
        double e;
        const double t = bound_term(w, v, hi, &e);
        split_term(t, e, &a->pos, &a->posc, &a->neg, &a->negc);
        if (!hi_implied)
            split_term(t, e, &a->pos_model, &a->pos_modelc,
                       &a->neg_model, &a->neg_modelc);
        return (negligible || at_hi) ? 0.0 : -w;
    }
    return 0.0;
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

    double *acts = jm_calloc_array(m->num_row, sizeof(double));
    double *actc = jm_calloc_array(m->num_row, sizeof(double));

    double *traffics = jm_calloc_array(m->num_row, sizeof(double));
    double *trafficc = jm_calloc_array(m->num_row, sizeof(double));
    if (acts == nullptr || actc == nullptr ||
        traffics == nullptr || trafficc == nullptr) {
        free(acts);
        free(actc);
        free(traffics);
        free(trafficc);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        const double xj = col_value[j];
        if (xj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double t = m->a_value[k] * xj;
            const double e = jm_two_product_residue(m->a_value[k], xj, t);
            jm_obj_add(&acts[i], &actc[i], t);
            if (e != 0.0)
                jm_obj_add(&acts[i], &actc[i], e);

            jm_obj_add(&traffics[i], &trafficc[i], fabs(t));
        }
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        acts[i] = acc_value(acts[i], actc[i]);
        traffics[i] = acc_value(traffics[i], trafficc[i]);
    }
    free(actc);
    free(trafficc);

    const double *const act = acts;
    const double *const traffic = traffics;

    double col_viol = 0.0, row_viol = 0.0, row_viol_rel = 0.0;
    double primal_obj = m->obj_offset, primal_objc = 0.0;
    for (int64_t j = 0; j < m->num_col; j++) {
        double cv = interval_violation(col_value[j], m->col_lower[j],
                                       m->col_upper[j]);
        if (m->col_semi != nullptr && m->col_semi[j] &&
            m->col_lower[j] > 0.0 && fabs(col_value[j]) < cv)
            cv = fabs(col_value[j]);
        col_viol = max2(col_viol, cv);
        const double c = m->col_cost[j], x = col_value[j];

        const double t = c * x;
        jm_obj_add(&primal_obj, &primal_objc, t);
        const double e = jm_two_product_residue(c, x, t);
        if (e != 0.0)
            jm_obj_add(&primal_obj, &primal_objc, e);
    }

    const double pobj = acc_value(primal_obj, primal_objc);
    for (int64_t i = 0; i < m->num_row; i++) {
        double viol = interval_violation(act[i], m->row_lower[i],
                                         m->row_upper[i]);
        row_viol = max2(row_viol, viol);

        row_viol_rel = max2(row_viol_rel,
                            viol / max2(1.0, traffic[i]));
    }

    double int_viol = 0.0;
    if (m->col_integer != nullptr)
        for (int64_t j = 0; j < m->num_col; j++)
            if (m->col_integer[j]) {
                const double f = fabs(col_value[j] - round(col_value[j]));
                if (f > int_viol)
                    int_viol = f;
            }
    out->max_integrality_violation = int_viol;

    out->max_col_violation = col_viol;
    out->max_row_violation = row_viol;
    out->max_row_violation_relative = row_viol_rel;
    out->primal_objective = pobj;
    out->primal_feasible = col_viol <= tol && row_viol <= tol &&
                           int_viol <= tol;

    if (row_dual != nullptr) {
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double dual_viol = 0.0;
        dual_acc a = {0};
        a.dual_obj = sigma * m->obj_offset;

        double *icl = jm_calloc_array(m->num_col, sizeof(double));
        double *icu = jm_calloc_array(m->num_col, sizeof(double));

        double *rlo = jm_calloc_array(m->num_row, sizeof(double));
        double *rloc = jm_calloc_array(m->num_row, sizeof(double));
        double *rup = jm_calloc_array(m->num_row, sizeof(double));
        double *rupc = jm_calloc_array(m->num_row, sizeof(double));
        int64_t *rli = jm_calloc_array(m->num_row, sizeof(int64_t));
        int64_t *rui = jm_calloc_array(m->num_row, sizeof(int64_t));
        const bool implied = icl != nullptr && icu != nullptr &&
                             rlo != nullptr && rloc != nullptr &&
                             rup != nullptr && rupc != nullptr &&
                             rli != nullptr && rui != nullptr;
        if (implied)
            implied_bounds(m, icl, icu, rlo, rloc, rup, rupc, rli, rui);

        for (int64_t i = 0; i < m->num_row; i++) {
            double rl = m->row_lower[i], ru = m->row_upper[i];
            bool rl_imp = false, ru_imp = false;
            if (implied) {
                if (!isfinite(rl) && rli[i] == 0) {
                    rl = acc_value(rlo[i], rloc[i]);
                    rl_imp = true;
                }
                if (!isfinite(ru) && rui[i] == 0) {
                    ru = acc_value(rup[i], rupc[i]);
                    ru_imp = true;
                }
            }
            dual_viol = max2(dual_viol,
                sign_condition(act[i], rl, ru, sigma * row_dual[i],
                               tol, max2(1.0, traffic[i]), &a,
                               rl_imp, ru_imp));
        }

        for (int64_t j = 0; j < m->num_col; j++) {

            double dw = m->col_cost[j], dwc = 0.0;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                const double aij = m->a_value[k];
                const double y = row_dual[m->a_index[k]];
                const double p = aij * y;
                const double e = jm_two_product_residue(aij, y, p);
                jm_obj_add(&dw, &dwc, -p);
                if (e != 0.0)
                    jm_obj_add(&dw, &dwc, -e);
            }
            const double d = acc_value(dw, dwc);

            dual_viol = max2(dual_viol,
                sign_condition(col_value[j],
                               implied ? icl[j] : m->col_lower[j],
                               implied ? icu[j] : m->col_upper[j],
                               sigma * d, tol, max2(1.0, fabs(col_value[j])),
                               &a,
                               implied && !isfinite(m->col_lower[j]),
                               implied && !isfinite(m->col_upper[j])));

            const double w = sigma * d;
            const bool drops = (w > 0.0 && !isfinite(m->col_lower[j])) ||
                               (w < 0.0 && !isfinite(m->col_upper[j]));
            if (drops) {
                const double t = certified_step(m, j, w > 0.0 ? -1.0 : 1.0,
                                                act);
                if (isinf(t) && fabs(w) <= tol) {

                    a.rays++;
                } else {
                    const double gain = fabs(w) * t;
                    if (gain > a.certified)
                        a.certified = gain;
                }
            }
        }

        assert(a.pos >= 0.0 && a.neg >= 0.0);
        assert(a.pos_model >= 0.0 && a.neg_model >= 0.0);
        const double true_dual_obj = sigma * acc_value(a.dual_obj, a.dual_objc);
        const double scale = 1.0 + fabs(pobj) + fabs(true_dual_obj);

        const double gap =
            fabs(acc_difference(a.pos_model, a.pos_modelc,
                                a.neg_model, a.neg_modelc)) / scale;

        out->checked_duals = true;
        out->max_dual_violation = dual_viol;
        out->dual_objective = true_dual_obj;
        out->objective_gap = gap;

        out->relative_suboptimality =
            acc_value(a.pos, a.posc) / (1.0 + fabs(pobj));

        out->max_dropped_multiplier = a.dropped_max;
        out->dropped_terms = a.dropped_n;
        out->gap_certified = a.dropped_n == 0;
        out->certified_suboptimality = a.certified;
        out->unquantified_rays = a.rays;

        out->gap_positive = acc_value(a.pos, a.posc);
        out->gap_negative = acc_value(a.neg, a.negc);
        out->dual_feasible = dual_viol <= tol && gap <= tol;

        free(icl);
        free(icu);
        free(rlo);
        free(rloc);
        free(rup);
        free(rupc);
        free(rli);
        free(rui);
    }

    free(acts);
    free(traffics);
    return JAOS_OK;
}

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
    double sup_cols = 0.0, sup_colsc = 0.0;
    for (int64_t j = 0; j < m->num_col && bounded; j++) {
        double asum = 0.0, acomp = 0.0, traffic = 0.0, trafficc = 0.0;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const double aij = m->a_value[p];
            const double y = row_ray[m->a_index[p]];
            const double t = aij * y;
            const double e = jm_two_product_residue(aij, y, t);
            jm_obj_add(&asum, &acomp, t);
            if (e != 0.0)
                jm_obj_add(&asum, &acomp, e);

            jm_obj_add(&traffic, &trafficc, fabs(t));
        }
        const double a = acc_value(asum, acomp);
        const double traf = acc_value(traffic, trafficc);

        if (fabs(a) <= tol * traf)
            continue;
        if (a > 0.0) {
            if (isfinite(m->col_upper[j]))
                add_product(&sup_cols, &sup_colsc, a, m->col_upper[j]);
            else
                bounded = false;
        } else if (a < 0.0) {
            if (isfinite(m->col_lower[j]))
                add_product(&sup_cols, &sup_colsc, a, m->col_lower[j]);
            else
                bounded = false;
        }
    }

    double inf_rows = 0.0, inf_rowsc = 0.0;
    for (int64_t i = 0; i < m->num_row && bounded; i++) {
        const double y = row_ray[i];
        if (y > 0.0) {
            if (isfinite(m->row_lower[i]))
                add_product(&inf_rows, &inf_rowsc, y, m->row_lower[i]);
            else
                bounded = false;
        } else if (y < 0.0) {
            if (isfinite(m->row_upper[i]))
                add_product(&inf_rows, &inf_rowsc, y, m->row_upper[i]);
            else
                bounded = false;
        }
    }

    if (!bounded) {

        out->sup_columns = INFINITY;
        out->inf_rows = -INFINITY;
        out->gap = -INFINITY;
        return JAOS_OK;
    }

    out->sup_columns = acc_value(sup_cols, sup_colsc);
    out->inf_rows = acc_value(inf_rows, inf_rowsc);

    out->gap = acc_difference(inf_rows, inf_rowsc, sup_cols, sup_colsc);
    out->certified = out->gap >
        tol * (1.0 + fabs(out->sup_columns) + fabs(out->inf_rows));
    return JAOS_OK;
}

jaos_status jaos_check_ray(const jaos_model *m, const double *col_ray,
                           double tol, jaos_ray_report *out)
{
    if (m == nullptr || col_ray == nullptr || out == nullptr ||
        !isfinite(tol) || tol < 0.0)
        return JAOS_ERR_INVALID_INPUT;
    out->rate = 0.0;
    out->max_col_escape = 0.0;
    out->max_row_escape = 0.0;
    out->certified = false;

    for (int64_t j = 0; j < m->num_col; j++)
        if (!isfinite(col_ray[j]))
            return JAOS_ERR_INVALID_INPUT;

    for (int64_t j = 0; j < m->num_col; j++) {
        const double d = col_ray[j];
        if (d > 0.0 && isfinite(m->col_upper[j])) {
            if (d > out->max_col_escape)
                out->max_col_escape = d;
        } else if (d < 0.0 && isfinite(m->col_lower[j])) {
            if (-d > out->max_col_escape)
                out->max_col_escape = -d;
        }
    }

    const size_t nrow = (size_t)(m->num_row > 0 ? m->num_row : 1);
    double *move = calloc(nrow, sizeof *move);
    double *movec = calloc(nrow, sizeof *movec);
    double *traf = calloc(nrow, sizeof *traf);
    double *trafc = calloc(nrow, sizeof *trafc);
    if (move == nullptr || movec == nullptr ||
        traf == nullptr || trafc == nullptr) {
        free(move);
        free(movec);
        free(traf);
        free(trafc);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        const double d = col_ray[j];
        if (d == 0.0)
            continue;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const int64_t i = m->a_index[p];
            const double aij = m->a_value[p];
            add_product(&move[i], &movec[i], aij, d);
            jm_obj_add(&traf[i], &trafc[i], fabs(aij * d));
        }
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        const double r = acc_value(move[i], movec[i]);
        if (fabs(r) <= tol * acc_value(traf[i], trafc[i]))
            continue;
        if (r > 0.0 && isfinite(m->row_upper[i])) {
            if (r > out->max_row_escape)
                out->max_row_escape = r;
        } else if (r < 0.0 && isfinite(m->row_lower[i])) {
            if (-r > out->max_row_escape)
                out->max_row_escape = -r;
        }
    }
    free(move);
    free(movec);
    free(traf);
    free(trafc);

    double rate = 0.0, ratec = 0.0, ctraf = 0.0, ctrafc = 0.0;
    for (int64_t j = 0; j < m->num_col; j++) {
        const double c = m->col_cost[j], d = col_ray[j];
        const double t = c * d;
        const double e = jm_two_product_residue(c, d, t);
        jm_obj_add(&rate, &ratec, t);
        if (e != 0.0)
            jm_obj_add(&rate, &ratec, e);
        jm_obj_add(&ctraf, &ctrafc, fabs(t));
    }
    out->rate = acc_value(rate, ratec);
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    const bool improves =
        sigma * out->rate < -tol * (1.0 + acc_value(ctraf, ctrafc));
    out->certified = improves && out->max_col_escape == 0.0 &&
                     out->max_row_escape == 0.0;
    return JAOS_OK;
}
