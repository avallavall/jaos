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
    free(m->start_col_status);
    free(m->start_row_status);
    /* The problem goes; the configuration stays. Saved as one object rather
     * than field by field, because field by field is a list and a list can be
     * forgotten — the primal tolerance was missing from it once, and the log
     * callback was missing from it from the day logging landed until D78
     * noticed. jaos_model_free calls this too and then frees the struct, so
     * putting the configuration back there is harmless. */
    const jm_config cfg = m->cfg;
    memset(m, 0, sizeof *m);
    m->cfg = cfg;
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
    m->cfg.work_limit = units;
    return JAOS_OK;
}

jaos_status jaos_set_time_limit(jaos_model *m, double seconds)
{
    if (m == nullptr || isnan(seconds))
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.time_limit = seconds;
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
    return set_tolerance(m, tol, m ? &m->cfg.primal_tol : nullptr, "primal");
}

jaos_status jaos_set_dual_tolerance(jaos_model *m, double tol)
{
    return set_tolerance(m, tol, m ? &m->cfg.dual_tol : nullptr, "dual");
}

/* Every modification goes through this.
 *
 * The answer on the model was computed for the problem as it stood, and the
 * moment any of it moves that answer describes a different problem. Leaving
 * it queryable would let a caller change a bound and read back the previous
 * optimum with nothing to say it was stale — a wrong answer delivered with
 * full confidence, which is the failure this project is built against.
 *
 * The solution arrays are freed rather than kept: `jaos_solution` reports
 * JAOS_ERR_INVALID_INPUT when there is no optimum to copy, so a stale read
 * becomes an error at the call rather than a number.
 *
 * What is *not* invalidated, and deliberately: the scaling and the row-wise
 * mirror. Both are derived from the matrix alone — scale.c reads no bound
 * and no cost — so changing a bound or a cost leaves them exactly correct.
 * A modification that touches the matrix must invalidate them, and there are
 * five: jaos_set_coefficient and the four that move a dimension. All five go
 * through model_matrix_is_stale, which is this plus both derived copies, so
 * the two lists cannot drift apart.
 *
 * Nor is the starting basis, and that one is not an omission but the point.
 * The answer stops being true when a bound moves; the basis that produced it
 * does not stop being a basis, and it is very probably close to the new
 * problem's. Discarding it here would throw away the one thing that makes
 * re-solving cheaper than solving, and it is why start_*_status is stored
 * apart from sol_*_status in the first place (see jaos_internal.h). */
static void model_answer_is_stale(jaos_model *m)
{
    free(m->sol_col);        m->sol_col = nullptr;
    free(m->sol_row);        m->sol_row = nullptr;
    free(m->sol_dual);       m->sol_dual = nullptr;
    free(m->sol_redcost);    m->sol_redcost = nullptr;
    free(m->sol_col_status); m->sol_col_status = nullptr;
    free(m->sol_row_status); m->sol_row_status = nullptr;
    m->solve_status = JAOS_SOLVE_NOT_RUN;
    m->objective = 0.0;
    m->solve_work = 0;
    m->solve_iters = 0;
}

/* Bounds may be infinite but never NaN, and `lower > upper` is a model with
 * no feasible point rather than a call to refuse — exactly the rule
 * jaos_load_lp applies, because a modification that accepted less than a
 * load would make the same model buildable one way and not the other. */
static bool bound_pair_ok(double lower, double upper)
{
    return !isnan(lower) && !isnan(upper);
}

/* Reading the problem back. Out of range is refused rather than answered with
 * a default, for the reason jaos_objective refuses when there is no optimum: a
 * number handed back for a column that does not exist cannot be told apart
 * from one that does. */
jaos_status jaos_col_cost(const jaos_model *m, int64_t j, double *cost)
{
    if (m == nullptr || cost == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    *cost = m->col_cost[j];
    return JAOS_OK;
}

jaos_status jaos_col_bounds(const jaos_model *m, int64_t j,
                            double *lower, double *upper)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    if (lower)
        *lower = m->col_lower[j];
    if (upper)
        *upper = m->col_upper[j];
    return JAOS_OK;
}

jaos_status jaos_row_bounds(const jaos_model *m, int64_t i,
                            double *lower, double *upper)
{
    if (m == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    if (lower)
        *lower = m->row_lower[i];
    if (upper)
        *upper = m->row_upper[i];
    return JAOS_OK;
}

jaos_status jaos_set_col_cost(jaos_model *m, int64_t j, double cost)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    if (!isfinite(cost)) {
        jm_set_err(m, "cost for column %lld must be finite", (long long)j);
        return JAOS_ERR_INVALID_INPUT;
    }
    m->col_cost[j] = cost;
    model_answer_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_set_col_bounds(jaos_model *m, int64_t j,
                                double lower, double upper)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    if (!bound_pair_ok(lower, upper)) {
        jm_set_err(m, "bounds for column %lld must not be NaN", (long long)j);
        return JAOS_ERR_INVALID_INPUT;
    }
    m->col_lower[j] = lower;
    m->col_upper[j] = upper;
    model_answer_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_set_row_bounds(jaos_model *m, int64_t i,
                                double lower, double upper)
{
    if (m == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    if (!bound_pair_ok(lower, upper)) {
        jm_set_err(m, "bounds for row %lld must not be NaN", (long long)i);
        return JAOS_ERR_INVALID_INPUT;
    }
    m->row_lower[i] = lower;
    m->row_upper[i] = upper;
    model_answer_is_stale(m);
    return JAOS_OK;
}

/* Changing one entry of the matrix.
 *
 * The stored copy is the authority the checker judges against, and it holds
 * an invariant the readers and the solver both rely on: within a column,
 * entries ascend by row index, with no duplicates and no explicit zeros. So
 * this is three operations wearing one name — replace where the entry
 * exists, delete when the new value is zero, insert in sorted position where
 * it does not. Writing a zero into the array instead of removing it would
 * leave a model that loads differently from the one it came from.
 *
 * Unlike a bound or a cost, this invalidates both derived copies: the
 * row-wise mirror holds the values, and the scaling was computed from the
 * matrix that just changed. */
jaos_status jaos_set_coefficient(jaos_model *m, int64_t row, int64_t col,
                                 double value)
{
    if (m == nullptr || row < 0 || row >= m->num_row ||
        col < 0 || col >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    if (!isfinite(value)) {
        jm_set_err(m, "coefficient (%lld, %lld) must be finite",
                   (long long)row, (long long)col);
        return JAOS_ERR_INVALID_INPUT;
    }

    /* Where it is, or where it would go: the column is sorted, so the first
     * entry at or past `row` is both answers at once. */
    int64_t at = m->a_start[col];
    const int64_t end = m->a_start[col + 1];
    while (at < end && m->a_index[at] < row)
        at++;
    const bool present = at < end && m->a_index[at] == row;

    if (!present && value == 0.0)
        return JAOS_OK;      /* a structural zero asked to stay one */

    if (present && value != 0.0) {
        m->a_value[at] = value;
    } else if (present) {
        /* Delete: close the gap and pull every later column start down. */
        memmove(&m->a_index[at], &m->a_index[at + 1],
                (size_t)(m->num_nz - at - 1) * sizeof *m->a_index);
        memmove(&m->a_value[at], &m->a_value[at + 1],
                (size_t)(m->num_nz - at - 1) * sizeof *m->a_value);
        for (int64_t j = col + 1; j <= m->num_col; j++)
            m->a_start[j]--;
        m->num_nz--;
    } else {
        /* Insert. Both arrays are grown before either is written, so a
         * failure here leaves the model exactly as it was rather than
         * half-enlarged. */
        int64_t *ni = realloc(m->a_index,
                              (size_t)(m->num_nz + 1) * sizeof *m->a_index);
        if (ni == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
        m->a_index = ni;
        double *nv = realloc(m->a_value,
                             (size_t)(m->num_nz + 1) * sizeof *m->a_value);
        if (nv == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
        m->a_value = nv;

        memmove(&m->a_index[at + 1], &m->a_index[at],
                (size_t)(m->num_nz - at) * sizeof *m->a_index);
        memmove(&m->a_value[at + 1], &m->a_value[at],
                (size_t)(m->num_nz - at) * sizeof *m->a_value);
        m->a_index[at] = row;
        m->a_value[at] = value;
        for (int64_t j = col + 1; j <= m->num_col; j++)
            m->a_start[j]++;
        m->num_nz++;
    }

    m->rowwise_valid = false;
    m->scale_valid = false;
    m->scale_clamped = false;
    model_answer_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_set_log_callback(jaos_model *m, jaos_log_fn cb, void *user)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.log_cb = cb;
    m->cfg.log_user = user;
    return JAOS_OK;
}

jaos_status jaos_set_progress_callback(jaos_model *m, jaos_progress_fn cb,
                                       void *user)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.progress_cb = cb;
    m->cfg.progress_user = user;
    return JAOS_OK;
}

jaos_status jaos_set_log_level(jaos_model *m, jaos_log_level level)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (level < JAOS_LOG_OFF || level > JAOS_LOG_DETAIL) {
        jm_set_err(m, "log level %d is not one of the defined levels",
                   (int)level);
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.log_level = level;
    return JAOS_OK;
}

/* One line out. Not called unless jm_logging_at said the level is wanted, so
 * the formatting cost lands only on a caller who asked for the line.
 *
 * The buffer is a local: a log line is diagnostic, it is not solver state,
 * and nothing in the library may hold a pointer to it afterwards. */
void jm_log(const jaos_model *m, jaos_log_level level, const char *fmt, ...)
{
    if (!jm_logging_at(m, level))
        return;
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    m->cfg.log_cb(m->cfg.log_user, level, line);
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

/* Puts a basis where the next solve will find it. Allocates both arrays or
 * neither, so `start_col_status != nullptr` is the whole test of whether a
 * starting basis exists — two flags for one fact is an invariant the types
 * would not be enforcing. */
static jaos_status store_basis(jaos_model *m, const jaos_basis_status *col,
                               const jaos_basis_status *row)
{
    if (m->start_col_status == nullptr || m->start_row_status == nullptr) {
        free(m->start_col_status);
        free(m->start_row_status);
        m->start_col_status =
            jm_alloc_array(m->num_col, sizeof *m->start_col_status);
        m->start_row_status =
            jm_alloc_array(m->num_row, sizeof *m->start_row_status);
        if (m->start_col_status == nullptr || m->start_row_status == nullptr) {
            free(m->start_col_status); m->start_col_status = nullptr;
            free(m->start_row_status); m->start_row_status = nullptr;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }
    if (m->num_col > 0)
        memcpy(m->start_col_status, col,
               (size_t)m->num_col * sizeof *m->start_col_status);
    if (m->num_row > 0)
        memcpy(m->start_row_status, row,
               (size_t)m->num_row * sizeof *m->start_row_status);
    return JAOS_OK;
}

jaos_status jm_model_remember_basis(jaos_model *m)
{
    if (m->sol_col_status == nullptr || m->sol_row_status == nullptr)
        return JAOS_OK;   /* nothing to remember; not a failure */
    return store_basis(m, m->sol_col_status, m->sol_row_status);
}

static bool status_in_range(jaos_basis_status s)
{
    return s == JAOS_BASIS_BASIC || s == JAOS_BASIS_AT_LOWER ||
           s == JAOS_BASIS_AT_UPPER || s == JAOS_BASIS_FREE;
}

/* What this checks, and what it deliberately leaves to the solve.
 *
 * Checked here: that the values are statuses at all, and that exactly num_row
 * of them are basic. Those are structural — they say whether the thing handed
 * over is a basis, and no later event can make a wrong count right.
 *
 * Not checked here: whether a nonbasic's status names a bound the variable
 * actually has, or whether the columns are linearly independent. Both depend
 * on numbers that move underneath a stored basis — jaos_set_col_bounds can
 * retire the very bound a status points at, and jaos_set_coefficient can turn
 * a nonsingular basis singular — so the solve has to cope with both anyway
 * (see build_initial_basis and repair_singular_basis). Refusing them here
 * would make a basis handed in behave differently from the identical one a
 * previous solve left behind, which is one rule too many for one object. */
jaos_status jaos_set_basis(jaos_model *m, const jaos_basis_status *col_status,
                           const jaos_basis_status *row_status)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if ((col_status == nullptr && m->num_col > 0) ||
        (row_status == nullptr && m->num_row > 0)) {
        jm_set_err(m, "a basis needs both halves: %lld column statuses and "
                      "%lld row statuses",
                   (long long)m->num_col, (long long)m->num_row);
        return JAOS_ERR_INVALID_INPUT;
    }

    int64_t basic = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!status_in_range(col_status[j])) {
            jm_set_err(m, "column %lld has no such basis status: %d",
                       (long long)j, (int)col_status[j]);
            return JAOS_ERR_INVALID_INPUT;
        }
        basic += col_status[j] == JAOS_BASIS_BASIC;
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        if (!status_in_range(row_status[i])) {
            jm_set_err(m, "row %lld has no such basis status: %d",
                       (long long)i, (int)row_status[i]);
            return JAOS_ERR_INVALID_INPUT;
        }
        basic += row_status[i] == JAOS_BASIS_BASIC;
    }
    if (basic != m->num_row) {
        jm_set_err(m, "a model with %lld rows needs %lld basic variables, "
                      "not %lld", (long long)m->num_row,
                   (long long)m->num_row, (long long)basic);
        return JAOS_ERR_INVALID_INPUT;
    }

    return store_basis(m, col_status, row_status);
}

void jaos_clear_basis(jaos_model *m)
{
    if (m == nullptr)
        return;
    free(m->start_col_status); m->start_col_status = nullptr;
    free(m->start_row_status); m->start_row_status = nullptr;
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

    /* Configuration is not problem data, and model_release_arrays is now
     * where that is enforced — there is nothing to list here any more. */
    model_release_arrays(m);
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
/* Adding and deleting rows and columns                                  */
/* --------------------------------------------------------------------- */

/* Both derived copies are computed from the matrix, so any of the four below
 * discards both — and neither is resized here, because jm_model_ensure_rowwise
 * and the scaling each allocate fresh from the model's current dimensions and
 * free what was there. */
static void model_matrix_is_stale(jaos_model *m)
{
    m->rowwise_valid = false;
    m->scale_valid = false;
    m->scale_clamped = false;
    model_answer_is_stale(m);
}

/* One rule for what a dimension change does to a stored basis, rather than a
 * case for each of the four operations: a model with n rows needs n basic
 * variables. That is exactly what jaos_set_basis enforces on a basis handed
 * in, and it is structural — no later event makes a wrong count right.
 *
 * The adds are built to keep it: a new row arrives basic, which is where the
 * slack basis puts it, and a new column arrives nonbasic. The deletes usually
 * break it, and are meant to. A basis that no longer counts is not a worse
 * starting point, it is not a basis, so it goes here rather than being left
 * for build_warm_basis to reject — the model should never hold one it already
 * knows is unusable. */
static void basis_survives_or_goes(jaos_model *m)
{
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return;
    int64_t basic = 0;
    for (int64_t j = 0; j < m->num_col; j++)
        basic += m->start_col_status[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < m->num_row; i++)
        basic += m->start_row_status[i] == JAOS_BASIS_BASIC;
    if (basic != m->num_row)
        jaos_clear_basis(m);
}

/* A set of indices to delete, checked as a set: in range, and each named
 * once. A repeated index is refused rather than absorbed because a caller who
 * has named one twice has lost track of what they are deleting, and the
 * second deletion they think they are asking for is of something else.
 *
 * Returns the keep mask, which the caller owns, or null on a bad set. */
static bool *deletion_mask(jaos_model *m, int64_t n_del, const int64_t *idx,
                           int64_t n, const char *what)
{
    bool *keep = jm_alloc_array(n, sizeof(bool));
    if (keep == nullptr)
        return nullptr;
    for (int64_t k = 0; k < n; k++)
        keep[k] = true;

    for (int64_t k = 0; k < n_del; k++) {
        const int64_t v = idx[k];
        if (v < 0 || v >= n) {
            jm_set_err(m, "%s %lld is not a %s of this model",
                       what, (long long)v, what);
            free(keep);
            return nullptr;
        }
        if (!keep[v]) {
            jm_set_err(m, "%s %lld is named twice in one deletion",
                       what, (long long)v);
            free(keep);
            return nullptr;
        }
        keep[v] = false;
    }
    return keep;
}

/* Where a nonbasic column rests when it arrives. A variable with no finite
 * bound has nowhere to rest, and the answer is not to invent one — see
 * jaos_add_cols. */
static jaos_basis_status arriving_status(double lower, double upper)
{
    if (isfinite(lower))
        return JAOS_BASIS_AT_LOWER;
    if (isfinite(upper))
        return JAOS_BASIS_AT_UPPER;
    return JAOS_BASIS_FREE;
}

/* Growing the stored basis. Failing to allocate here loses the basis and not
 * the operation: a starting basis is an optimisation, dropping one is always
 * correct, and refusing an otherwise complete modification because the
 * *hint* could not be extended would be trading a working call for a faster
 * one that never happens. */
static void basis_extend(jaos_basis_status **arr, int64_t old_n, int64_t add,
                         const jaos_basis_status *fill, jaos_model *m)
{
    if (*arr == nullptr)
        return;
    jaos_basis_status *grown =
        realloc(*arr, (size_t)(old_n + add) * sizeof **arr);
    if (grown == nullptr) {
        jaos_clear_basis(m);
        return;
    }
    for (int64_t k = 0; k < add; k++)
        grown[old_n + k] = fill[k];
    *arr = grown;
}

jaos_status jaos_add_cols(jaos_model *m, int64_t num_new,
    const double *col_cost, const double *col_lower, const double *col_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value)
{
    if (m == nullptr || num_new < 0 || num_nz < 0)
        return JAOS_ERR_INVALID_INPUT;
    if (num_new == 0)
        return num_nz == 0 ? JAOS_OK : JAOS_ERR_INVALID_INPUT;
    if (col_cost == nullptr || col_lower == nullptr || col_upper == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (!all_finite(col_cost, num_new) ||
        any_nan(col_lower, num_new) || any_nan(col_upper, num_new)) {
        jm_set_err(m, "costs must be finite and bounds must not be NaN");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (a_start == nullptr) {
        if (num_nz != 0)
            return JAOS_ERR_INVALID_INPUT;
    } else {
        if (a_start[0] != 0 || a_start[num_new] != num_nz)
            return JAOS_ERR_INVALID_INPUT;
        for (int64_t j = 0; j < num_new; j++)
            if (a_start[j] > a_start[j + 1])
                return JAOS_ERR_INVALID_INPUT;
    }
    if (num_nz > 0 && (a_index == nullptr || a_value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t k = 0; k < num_nz; k++) {
        if (a_index[k] < 0 || a_index[k] >= m->num_row) {
            jm_set_err(m, "row index %lld is not a row of this model",
                       (long long)a_index[k]);
            return JAOS_ERR_INVALID_INPUT;
        }
        if (!isfinite(a_value[k]))
            return JAOS_ERR_INVALID_INPUT;
    }

    int64_t kept = 0;
    for (int64_t k = 0; k < num_nz; k++)
        if (a_value[k] != 0.0)
            kept++;

    const int64_t ncol = m->num_col + num_new;
    const int64_t nnz  = m->num_nz + kept;

    /* Built whole before the model is touched, so a failure anywhere leaves
     * it exactly as it was — jaos_load_lp's rule, and for the same reason. */
    double  *cost = jm_alloc_array(ncol, sizeof(double));
    double  *cl   = jm_alloc_array(ncol, sizeof(double));
    double  *cu   = jm_alloc_array(ncol, sizeof(double));
    int64_t *as   = jm_alloc_array(ncol + 1, sizeof(int64_t));
    int64_t *ai   = jm_alloc_array(nnz, sizeof(int64_t));
    double  *av   = jm_alloc_array(nnz, sizeof(double));
    if (!cost || !cl || !cu || !as || !ai || !av) {
        free(cost); free(cl); free(cu); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    memcpy(cost, m->col_cost,  (size_t)m->num_col * sizeof(double));
    memcpy(cl,   m->col_lower, (size_t)m->num_col * sizeof(double));
    memcpy(cu,   m->col_upper, (size_t)m->num_col * sizeof(double));
    memcpy(cost + m->num_col, col_cost,  (size_t)num_new * sizeof(double));
    memcpy(cl   + m->num_col, col_lower, (size_t)num_new * sizeof(double));
    memcpy(cu   + m->num_col, col_upper, (size_t)num_new * sizeof(double));

    /* The existing columns are untouched: appending changes no index below
     * num_col, so the whole prefix of every array copies straight over. */
    memcpy(as, m->a_start, (size_t)(m->num_col + 1) * sizeof(int64_t));
    memcpy(ai, m->a_index, (size_t)m->num_nz * sizeof(int64_t));
    memcpy(av, m->a_value, (size_t)m->num_nz * sizeof(double));

    jaos_status err = JAOS_OK;
    int64_t pos = m->num_nz;
    for (int64_t j = 0; j < num_new; j++) {
        const int64_t lo = a_start ? a_start[j] : 0;
        const int64_t hi = a_start ? a_start[j + 1] : 0;
        const int64_t begin = pos;
        for (int64_t k = lo; k < hi; k++) {
            if (a_value[k] != 0.0) {
                ai[pos] = a_index[k];
                av[pos] = a_value[k];
                pos++;
            }
        }
        sort_column(ai + begin, av + begin, pos - begin);
        for (int64_t k = begin + 1; k < pos; k++) {
            if (ai[k] == ai[k - 1]) {
                jm_set_err(m, "new column %lld names row %lld twice",
                           (long long)j, (long long)ai[k]);
                err = JAOS_ERR_INVALID_INPUT;
                break;
            }
        }
        if (err != JAOS_OK)
            break;
        as[m->num_col + j + 1] = pos;
    }
    if (err != JAOS_OK) {
        free(cost); free(cl); free(cu); free(as); free(ai); free(av);
        return err;
    }

    /* Where each arriving column rests, decided before the swap so the basis
     * can be dropped whole if any of them has nowhere to. */
    jaos_basis_status *arriving = nullptr;
    if (m->start_col_status != nullptr) {
        arriving = jm_alloc_array(num_new, sizeof(jaos_basis_status));
        if (arriving == nullptr) {
            jaos_clear_basis(m);
        } else {
            for (int64_t j = 0; j < num_new; j++) {
                arriving[j] = arriving_status(col_lower[j], col_upper[j]);
                if (arriving[j] == JAOS_BASIS_FREE) {
                    /* A nonbasic with no bounds is pinned at zero and this
                     * solver cannot always price it back off (see
                     * build_warm_basis). Refuse to create one. */
                    free(arriving);
                    arriving = nullptr;
                    jaos_clear_basis(m);
                    break;
                }
            }
        }
    }

    free(m->col_cost);  free(m->col_lower); free(m->col_upper);
    free(m->a_start);   free(m->a_index);   free(m->a_value);
    m->col_cost = cost; m->col_lower = cl;  m->col_upper = cu;
    m->a_start = as;    m->a_index = ai;    m->a_value = av;

    if (arriving != nullptr) {
        basis_extend(&m->start_col_status, m->num_col, num_new, arriving, m);
        free(arriving);
    }
    m->num_col = ncol;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    basis_survives_or_goes(m);
    return JAOS_OK;
}

jaos_status jaos_add_rows(jaos_model *m, int64_t num_new,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *ar_start, const int64_t *ar_index,
    const double *ar_value)
{
    if (m == nullptr || num_new < 0 || num_nz < 0)
        return JAOS_ERR_INVALID_INPUT;
    if (num_new == 0)
        return num_nz == 0 ? JAOS_OK : JAOS_ERR_INVALID_INPUT;
    if (row_lower == nullptr || row_upper == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (any_nan(row_lower, num_new) || any_nan(row_upper, num_new)) {
        jm_set_err(m, "row bounds must not be NaN");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (ar_start == nullptr) {
        if (num_nz != 0)
            return JAOS_ERR_INVALID_INPUT;
    } else {
        if (ar_start[0] != 0 || ar_start[num_new] != num_nz)
            return JAOS_ERR_INVALID_INPUT;
        for (int64_t i = 0; i < num_new; i++)
            if (ar_start[i] > ar_start[i + 1])
                return JAOS_ERR_INVALID_INPUT;
    }
    if (num_nz > 0 && (ar_index == nullptr || ar_value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t k = 0; k < num_nz; k++) {
        if (ar_index[k] < 0 || ar_index[k] >= m->num_col) {
            jm_set_err(m, "column index %lld is not a column of this model",
                       (long long)ar_index[k]);
            return JAOS_ERR_INVALID_INPUT;
        }
        if (!isfinite(ar_value[k]))
            return JAOS_ERR_INVALID_INPUT;
    }

    /* The new rows arrive across and the matrix is stored down, so this is a
     * transpose of the addition rather than an append: every column that any
     * new row touches gains entries in the middle of the array. Counting per
     * column first turns that into one rebuild instead of one insertion per
     * entry, which is what jaos_set_coefficient would have cost. */
    int64_t *added = jm_calloc_array(m->num_col, sizeof(int64_t));
    if (added == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    int64_t kept = 0;
    for (int64_t k = 0; k < num_nz; k++) {
        if (ar_value[k] != 0.0) {
            added[ar_index[k]]++;
            kept++;
        }
    }

    const int64_t nrow = m->num_row + num_new;
    const int64_t nnz  = m->num_nz + kept;

    double  *rl = jm_alloc_array(nrow, sizeof(double));
    double  *ru = jm_alloc_array(nrow, sizeof(double));
    int64_t *as = jm_alloc_array(m->num_col + 1, sizeof(int64_t));
    int64_t *ai = jm_alloc_array(nnz, sizeof(int64_t));
    double  *av = jm_alloc_array(nnz, sizeof(double));
    int64_t *cursor = jm_alloc_array(m->num_col, sizeof(int64_t));
    if (!rl || !ru || !as || !ai || !av || !cursor) {
        free(added); free(rl); free(ru); free(as); free(ai); free(av);
        free(cursor);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    memcpy(rl, m->row_lower, (size_t)m->num_row * sizeof(double));
    memcpy(ru, m->row_upper, (size_t)m->num_row * sizeof(double));
    memcpy(rl + m->num_row, row_lower, (size_t)num_new * sizeof(double));
    memcpy(ru + m->num_row, row_upper, (size_t)num_new * sizeof(double));

    /* Old entries first in every column, then room for the new ones. Every
     * new row index is at least num_row and every old one is below it, so
     * appending inside the column keeps it ascending with no sort at all. */
    as[0] = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        const int64_t old_len = m->a_start[j + 1] - m->a_start[j];
        memcpy(ai + as[j], m->a_index + m->a_start[j],
               (size_t)old_len * sizeof(int64_t));
        memcpy(av + as[j], m->a_value + m->a_start[j],
               (size_t)old_len * sizeof(double));
        cursor[j] = as[j] + old_len;
        as[j + 1] = cursor[j] + added[j];
    }
    free(added);

    jaos_status err = JAOS_OK;
    for (int64_t i = 0; i < num_new && err == JAOS_OK; i++) {
        const int64_t lo = ar_start ? ar_start[i] : 0;
        const int64_t hi = ar_start ? ar_start[i + 1] : 0;
        for (int64_t k = lo; k < hi; k++) {
            if (ar_value[k] == 0.0)
                continue;
            const int64_t j = ar_index[k];
            /* Rows are walked in order, so a repeat inside one row is the
             * only way two equal indices can land next to each other. */
            if (cursor[j] > m->a_start[j + 1] - m->a_start[j] + as[j] &&
                ai[cursor[j] - 1] == m->num_row + i) {
                jm_set_err(m, "new row %lld names column %lld twice",
                           (long long)i, (long long)j);
                err = JAOS_ERR_INVALID_INPUT;
                break;
            }
            ai[cursor[j]] = m->num_row + i;
            av[cursor[j]] = ar_value[k];
            cursor[j]++;
        }
    }
    free(cursor);
    if (err != JAOS_OK) {
        free(rl); free(ru); free(as); free(ai); free(av);
        return err;
    }

    free(m->row_lower); free(m->row_upper);
    free(m->a_start);   free(m->a_index); free(m->a_value);
    m->row_lower = rl;  m->row_upper = ru;
    m->a_start = as;    m->a_index = ai;  m->a_value = av;

    /* A new row's activity arrives basic, which is both where a slack basis
     * puts it and what keeps the basic count equal to the row count. */
    if (m->start_row_status != nullptr) {
        jaos_basis_status *arriving =
            jm_alloc_array(num_new, sizeof(jaos_basis_status));
        if (arriving == nullptr) {
            jaos_clear_basis(m);
        } else {
            for (int64_t i = 0; i < num_new; i++)
                arriving[i] = JAOS_BASIS_BASIC;
            basis_extend(&m->start_row_status, m->num_row, num_new,
                         arriving, m);
            free(arriving);
        }
    }
    m->num_row = nrow;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    basis_survives_or_goes(m);
    return JAOS_OK;
}

jaos_status jaos_delete_cols(jaos_model *m, int64_t num_del,
                             const int64_t *cols)
{
    if (m == nullptr || num_del < 0)
        return JAOS_ERR_INVALID_INPUT;
    if (num_del == 0)
        return JAOS_OK;
    if (cols == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    bool *keep = deletion_mask(m, num_del, cols, m->num_col, "column");
    if (keep == nullptr)
        return m->err[0] ? JAOS_ERR_INVALID_INPUT : JAOS_ERR_OUT_OF_MEMORY;

    const int64_t ncol = m->num_col - num_del;
    int64_t nnz = 0;
    for (int64_t j = 0; j < m->num_col; j++)
        if (keep[j])
            nnz += m->a_start[j + 1] - m->a_start[j];

    double  *cost = jm_alloc_array(ncol, sizeof(double));
    double  *cl   = jm_alloc_array(ncol, sizeof(double));
    double  *cu   = jm_alloc_array(ncol, sizeof(double));
    int64_t *as   = jm_alloc_array(ncol + 1, sizeof(int64_t));
    int64_t *ai   = jm_alloc_array(nnz, sizeof(int64_t));
    double  *av   = jm_alloc_array(nnz, sizeof(double));
    if (!cost || !cl || !cu || !as || !ai || !av) {
        free(keep);
        free(cost); free(cl); free(cu); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    /* What survives keeps its relative order, so one forward pass renumbers
     * it: the columns a caller did not name come out in the order they went
     * in, densely, and nothing they hold has to be rewritten. */
    int64_t out = 0, pos = 0;
    as[0] = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!keep[j])
            continue;
        cost[out] = m->col_cost[j];
        cl[out]   = m->col_lower[j];
        cu[out]   = m->col_upper[j];
        const int64_t len = m->a_start[j + 1] - m->a_start[j];
        memcpy(ai + pos, m->a_index + m->a_start[j],
               (size_t)len * sizeof(int64_t));
        memcpy(av + pos, m->a_value + m->a_start[j],
               (size_t)len * sizeof(double));
        pos += len;
        out++;
        as[out] = pos;
    }

    if (m->start_col_status != nullptr) {
        int64_t at = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            if (keep[j])
                m->start_col_status[at++] = m->start_col_status[j];
    }
    free(keep);

    free(m->col_cost);  free(m->col_lower); free(m->col_upper);
    free(m->a_start);   free(m->a_index);   free(m->a_value);
    m->col_cost = cost; m->col_lower = cl;  m->col_upper = cu;
    m->a_start = as;    m->a_index = ai;    m->a_value = av;
    m->num_col = ncol;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    basis_survives_or_goes(m);
    return JAOS_OK;
}

jaos_status jaos_delete_rows(jaos_model *m, int64_t num_del,
                             const int64_t *rows)
{
    if (m == nullptr || num_del < 0)
        return JAOS_ERR_INVALID_INPUT;
    if (num_del == 0)
        return JAOS_OK;
    if (rows == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    bool *keep = deletion_mask(m, num_del, rows, m->num_row, "row");
    if (keep == nullptr)
        return m->err[0] ? JAOS_ERR_INVALID_INPUT : JAOS_ERR_OUT_OF_MEMORY;

    const int64_t nrow = m->num_row - num_del;

    /* Deleting a row renumbers every row after it, and the matrix stores row
     * indices, so every surviving entry has to be rewritten. The map is built
     * once; doing it per entry is where the arithmetic would go wrong. */
    int64_t *renum = jm_alloc_array(m->num_row, sizeof(int64_t));
    if (renum == nullptr) {
        free(keep);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    int64_t next = 0;
    for (int64_t i = 0; i < m->num_row; i++)
        renum[i] = keep[i] ? next++ : -1;

    int64_t nnz = 0;
    for (int64_t k = 0; k < m->num_nz; k++)
        if (keep[m->a_index[k]])
            nnz++;

    double  *rl = jm_alloc_array(nrow, sizeof(double));
    double  *ru = jm_alloc_array(nrow, sizeof(double));
    int64_t *as = jm_alloc_array(m->num_col + 1, sizeof(int64_t));
    int64_t *ai = jm_alloc_array(nnz, sizeof(int64_t));
    double  *av = jm_alloc_array(nnz, sizeof(double));
    if (!rl || !ru || !as || !ai || !av) {
        free(keep); free(renum);
        free(rl); free(ru); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    int64_t out = 0;
    for (int64_t i = 0; i < m->num_row; i++)
        if (keep[i]) {
            rl[out] = m->row_lower[i];
            ru[out] = m->row_upper[i];
            out++;
        }

    /* Columns keep their order and their identity; only what they point at
     * moves. A column may end up empty, which is a column with no
     * coefficients and not an error. */
    int64_t pos = 0;
    as[0] = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (!keep[i])
                continue;
            ai[pos] = renum[i];
            av[pos] = m->a_value[k];
            pos++;
        }
        as[j + 1] = pos;
    }
    free(renum);

    if (m->start_row_status != nullptr) {
        int64_t at = 0;
        for (int64_t i = 0; i < m->num_row; i++)
            if (keep[i])
                m->start_row_status[at++] = m->start_row_status[i];
    }
    free(keep);

    free(m->row_lower); free(m->row_upper);
    free(m->a_start);   free(m->a_index); free(m->a_value);
    m->row_lower = rl;  m->row_upper = ru;
    m->a_start = as;    m->a_index = ai;  m->a_value = av;
    m->num_row = nrow;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    basis_survives_or_goes(m);
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
