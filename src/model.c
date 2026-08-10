/* Model lifecycle: create, load, query, free — and the CSR mirror.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double jaos_infinity(void)
{
    return INFINITY;
}

jaos_status jaos_model_new(jaos_model **out)
{
    if (out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *out = nullptr;
    jaos_model *m = jm_calloc_array(1, sizeof *m);
    if (m == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    /* All-zero is a valid empty model: 0 cols, 0 rows, minimize. */
    *out = m;
    return JAOS_OK;
}

static void model_release_arrays(jaos_model *m)
{
    free(m->col_cost);
    free(m->col_lower);
    free(m->col_upper);
    free(m->row_lower);
    free(m->row_upper);
    free(m->a_start);
    free(m->a_index);
    free(m->a_value);
    free(m->ar_start);
    free(m->ar_index);
    free(m->ar_value);
    free(m->row_scale);
    free(m->col_scale);
    free(m->sol_col);
    free(m->sol_row);
    free(m->sol_dual);
    free(m->sol_redcost);
    free(m->sol_col_status);
    free(m->sol_row_status);
    memset(m, 0, sizeof *m);
}

void jaos_model_free(jaos_model *m)
{
    if (m == nullptr)
        return;
    model_release_arrays(m);
    free(m);
}

int64_t jaos_num_col(const jaos_model *m) { return m ? m->num_col : 0; }
int64_t jaos_num_row(const jaos_model *m) { return m ? m->num_row : 0; }
int64_t jaos_num_nz(const jaos_model *m)  { return m ? m->num_nz : 0; }

const char *jaos_model_error(const jaos_model *m)
{
    return m ? m->err : "";
}

jaos_status jaos_set_work_limit(jaos_model *m, int64_t units)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->work_limit = units;
    return JAOS_OK;
}

jaos_status jaos_set_time_limit(jaos_model *m, double seconds)
{
    if (m == nullptr || isnan(seconds))
        return JAOS_ERR_INVALID_INPUT;
    m->time_limit = seconds;
    return JAOS_OK;
}

/* The two tolerances the caller owns. Rejected rather than clamped when they
 * are not usable numbers: a solver that silently substitutes its own value
 * for the one it was given reports success for a run the caller cannot
 * reason about. Zero restores the default, which is the only way to say
 * "whatever you would have done" once a value has been set. */
static jaos_status set_tolerance(jaos_model *m, double value, double *slot,
                                 const char *what)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!(isfinite(value) && value >= 0.0)) {
        jm_set_err(m, "%s tolerance must be finite and non-negative", what);
        return JAOS_ERR_INVALID_INPUT;
    }
    *slot = value;
    return JAOS_OK;
}

jaos_status jaos_set_primal_tolerance(jaos_model *m, double tol)
{
    return set_tolerance(m, tol, m ? &m->primal_tol : nullptr, "primal");
}

jaos_status jaos_set_dual_tolerance(jaos_model *m, double tol)
{
    return set_tolerance(m, tol, m ? &m->dual_tol : nullptr, "dual");
}

jaos_status jaos_solve(jaos_model *m)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->err[0] = '\0';
    return jm_dual_simplex(m);
}

jaos_solve_status jaos_status_of(const jaos_model *m)
{
    return m ? m->solve_status : JAOS_SOLVE_NOT_RUN;
}

jaos_status jaos_objective(const jaos_model *m, double *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (m->solve_status != JAOS_SOLVE_OPTIMAL)
        return JAOS_ERR_INVALID_INPUT;
    *out = m->objective;
    return JAOS_OK;
}

int64_t jaos_work_units(const jaos_model *m) { return m ? m->solve_work : 0; }
int64_t jaos_iterations(const jaos_model *m) { return m ? m->solve_iters : 0; }

jaos_status jaos_solution(const jaos_model *m, double *col_value,
    double *row_activity, double *row_dual, double *col_dual)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* Same rule as jaos_objective, for the same reason: a solve that
     * found no optimum has no solution to hand out, and a buffer of
     * zeros cannot be told apart from an answer that is genuinely
     * zero. */
    if (m->solve_status != JAOS_SOLVE_OPTIMAL || m->sol_col == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (col_value)
        memcpy(col_value, m->sol_col, (size_t)m->num_col * sizeof(double));
    if (row_activity)
        memcpy(row_activity, m->sol_row, (size_t)m->num_row * sizeof(double));
    if (row_dual)
        memcpy(row_dual, m->sol_dual, (size_t)m->num_row * sizeof(double));
    if (col_dual)
        memcpy(col_dual, m->sol_redcost, (size_t)m->num_col * sizeof(double));
    return JAOS_OK;
}

jaos_status jaos_basis(const jaos_model *m, jaos_basis_status *col_status,
    jaos_basis_status *row_status)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (m->solve_status != JAOS_SOLVE_OPTIMAL || m->sol_col_status == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (col_status)
        memcpy(col_status, m->sol_col_status,
               (size_t)m->num_col * sizeof *col_status);
    if (row_status)
        memcpy(row_status, m->sol_row_status,
               (size_t)m->num_row * sizeof *row_status);
    return JAOS_OK;
}

void jm_set_err(jaos_model *m, const char *fmt, ...)
{
    if (m == nullptr)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->err, sizeof m->err, fmt, ap);
    va_end(ap);
}

/* --------------------------------------------------------------------- */
/* Loading                                                               */
/* --------------------------------------------------------------------- */

static bool any_nan(const double *v, int64_t n)
{
    for (int64_t i = 0; i < n; i++)
        if (isnan(v[i]))
            return true;
    return false;
}

static bool all_finite(const double *v, int64_t n)
{
    for (int64_t i = 0; i < n; i++)
        if (!isfinite(v[i]))
            return false;
    return true;
}

/* Insertion sort of one column's (index, value) pairs by row index.
 * Quadratic per column, which is fine for correctness-first; replace only
 * with a measurement in hand (D17). */
static void sort_column(int64_t *idx, double *val, int64_t n)
{
    for (int64_t i = 1; i < n; i++) {
        int64_t ki = idx[i];
        double  kv = val[i];
        int64_t j = i;
        while (j > 0 && idx[j - 1] > ki) {
            idx[j] = idx[j - 1];
            val[j] = val[j - 1];
            j--;
        }
        idx[j] = ki;
        val[j] = kv;
    }
}

jaos_status jaos_load_lp(jaos_model *m,
    int64_t num_col, int64_t num_row,
    jaos_obj_sense sense, double obj_offset,
    const double *col_cost,
    const double *col_lower, const double *col_upper,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value)
{
    if (m == nullptr || num_col < 0 || num_row < 0 || num_nz < 0)
        return JAOS_ERR_INVALID_INPUT;
    if (sense != JAOS_MINIMIZE && sense != JAOS_MAXIMIZE)
        return JAOS_ERR_INVALID_INPUT;
    if (!isfinite(obj_offset))
        return JAOS_ERR_INVALID_INPUT;

    if (num_col > 0 &&
        (col_cost == nullptr || col_lower == nullptr || col_upper == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    if (num_row > 0 && (row_lower == nullptr || row_upper == nullptr))
        return JAOS_ERR_INVALID_INPUT;

    /* Costs must be finite; bounds may be +-inf but never NaN. */
    if (num_col > 0 && (!all_finite(col_cost, num_col) ||
                        any_nan(col_lower, num_col) || any_nan(col_upper, num_col)))
        return JAOS_ERR_INVALID_INPUT;
    if (num_row > 0 && (any_nan(row_lower, num_row) || any_nan(row_upper, num_row)))
        return JAOS_ERR_INVALID_INPUT;

    /* Matrix structure. a_start == NULL is allowed only as "no matrix". */
    if (a_start == nullptr) {
        if (num_nz != 0)
            return JAOS_ERR_INVALID_INPUT;
    } else {
        if (a_start[0] != 0 || a_start[num_col] != num_nz)
            return JAOS_ERR_INVALID_INPUT;
        for (int64_t j = 0; j < num_col; j++)
            if (a_start[j] > a_start[j + 1])
                return JAOS_ERR_INVALID_INPUT;
    }
    if (num_nz > 0 && (a_index == nullptr || a_value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t k = 0; k < num_nz; k++) {
        if (a_index[k] < 0 || a_index[k] >= num_row)
            return JAOS_ERR_INVALID_INPUT;
        if (!isfinite(a_value[k]))
            return JAOS_ERR_INVALID_INPUT;
    }

    /* Count the entries that survive the explicit-zero drop. */
    int64_t kept = 0;
    for (int64_t k = 0; k < num_nz; k++)
        if (a_value[k] != 0.0)
            kept++;

    /* Build the new copy fully before touching the model, so a failed load
     * leaves the model exactly as it was. */
    double  *cost = jm_alloc_array(num_col, sizeof(double));
    double  *cl   = jm_alloc_array(num_col, sizeof(double));
    double  *cu   = jm_alloc_array(num_col, sizeof(double));
    double  *rl   = jm_alloc_array(num_row, sizeof(double));
    double  *ru   = jm_alloc_array(num_row, sizeof(double));
    int64_t *as   = jm_alloc_array(num_col + 1, sizeof(int64_t));
    int64_t *ai   = jm_alloc_array(kept, sizeof(int64_t));
    double  *av   = jm_alloc_array(kept, sizeof(double));

    if (!cost || !cl || !cu || !rl || !ru || !as || !ai || !av) {
        free(cost); free(cl); free(cu); free(rl); free(ru);
        free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    if (num_col > 0) {
        memcpy(cost, col_cost,  (size_t)num_col * sizeof(double));
        memcpy(cl,   col_lower, (size_t)num_col * sizeof(double));
        memcpy(cu,   col_upper, (size_t)num_col * sizeof(double));
    }
    if (num_row > 0) {
        memcpy(rl, row_lower, (size_t)num_row * sizeof(double));
        memcpy(ru, row_upper, (size_t)num_row * sizeof(double));
    }

    /* Copy column by column, dropping zeros, then sort and reject
     * duplicates. */
    jaos_status err = JAOS_OK;
    int64_t pos = 0;
    as[0] = 0;
    for (int64_t j = 0; j < num_col; j++) {
        int64_t lo = a_start ? a_start[j] : 0;
        int64_t hi = a_start ? a_start[j + 1] : 0;
        int64_t col_begin = pos;
        for (int64_t k = lo; k < hi; k++) {
            if (a_value[k] != 0.0) {
                ai[pos] = a_index[k];
                av[pos] = a_value[k];
                pos++;
            }
        }
        sort_column(ai + col_begin, av + col_begin, pos - col_begin);
        for (int64_t k = col_begin + 1; k < pos; k++) {
            if (ai[k] == ai[k - 1]) {
                err = JAOS_ERR_INVALID_INPUT;
                break;
            }
        }
        if (err != JAOS_OK)
            break;
        as[j + 1] = pos;
    }
    if (err != JAOS_OK) {
        free(cost); free(cl); free(cu); free(rl); free(ru);
        free(as); free(ai); free(av);
        return err;
    }

    /* Budgets and tolerances are solver configuration, not problem data:
     * loading a new problem into the same model must not silently discard
     * them. Every one of these has to be listed here, and a setting that is
     * added without being added to this list is lost by anyone who
     * configures before loading — which is the natural order to write it in
     * and is how the primal tolerance was found to be dropped. */
    int64_t keep_work_limit = m->work_limit;
    double keep_time_limit = m->time_limit;
    double keep_primal_tol = m->primal_tol;
    double keep_dual_tol = m->dual_tol;

    model_release_arrays(m);
    m->work_limit = keep_work_limit;
    m->time_limit = keep_time_limit;
    m->primal_tol = keep_primal_tol;
    m->dual_tol = keep_dual_tol;
    m->num_col = num_col;
    m->num_row = num_row;
    m->num_nz  = kept;
    m->sense = sense;
    m->obj_offset = obj_offset;
    m->col_cost = cost;
    m->col_lower = cl;
    m->col_upper = cu;
    m->row_lower = rl;
    m->row_upper = ru;
    m->a_start = as;
    m->a_index = ai;
    m->a_value = av;
    /* rowwise_valid is false after the memset in model_release_arrays. */
    return JAOS_OK;
}

/* --------------------------------------------------------------------- */
/* CSR mirror                                                            */
/* --------------------------------------------------------------------- */

jaos_status jm_model_ensure_rowwise(jaos_model *m)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (m->rowwise_valid)
        return JAOS_OK;

    int64_t *rs = jm_calloc_array(m->num_row + 1, sizeof(int64_t));
    int64_t *ri = jm_alloc_array(m->num_nz, sizeof(int64_t));
    double  *rv = jm_alloc_array(m->num_nz, sizeof(double));
    if (!rs || !ri || !rv) {
        free(rs); free(ri); free(rv);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    /* Counting transpose: count entries per row, prefix-sum, scatter.
     * Scattering in column order makes every CSR row sorted by column
     * index automatically. */
    for (int64_t k = 0; k < m->num_nz; k++)
        rs[m->a_index[k] + 1]++;
    for (int64_t i = 0; i < m->num_row; i++)
        rs[i + 1] += rs[i];

    int64_t *fill = jm_calloc_array(m->num_row, sizeof(int64_t));
    if (fill == nullptr) {
        free(rs); free(ri); free(rv);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            int64_t i = m->a_index[k];
            int64_t p = rs[i] + fill[i]++;
            ri[p] = j;
            rv[p] = m->a_value[k];
        }
    }
    free(fill);

    free(m->ar_start);
    free(m->ar_index);
    free(m->ar_value);
    m->ar_start = rs;
    m->ar_index = ri;
    m->ar_value = rv;
    m->rowwise_valid = true;
    return JAOS_OK;
}
