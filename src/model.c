/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <float.h>
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
    free(m->sol_farkas);
    free(m->sol_ray);
    free(m->start_col_status);
    free(m->start_row_status);
    jm_model_drop_exact(m);
    jm_model_take_names(m, nullptr, nullptr, nullptr);
    free(m->model_name);
    free(m->col_integer);
    free(m->col_semi);
    free(m->row_ind_col);
    free(m->row_ind_val);
    free(m->sos_type);
    free(m->sos_start);
    free(m->sos_col);
    free(m->sos_weight);
    free(m->mip_start);
    free(m->mip_inc_x);
    free(m->mip_pool_x);
    free(m->mip_pool_obj);

    const jm_config cfg = m->cfg;
    memset(m, 0, sizeof *m);
    m->cfg = cfg;
}

static void free_names(char **names, int64_t n)
{
    if (names == nullptr)
        return;
    for (int64_t k = 0; k < n; k++)
        free(names[k]);
    free(names);
}

static char *dup_name(const char *name)
{
    const size_t len = strlen(name) + 1;
    char *copy = malloc(len);
    if (copy != nullptr)
        memcpy(copy, name, len);
    return copy;
}

static void name_map_is_stale(jaos_model *m)
{
    jm_nmap_free(&m->col_map);
    jm_nmap_free(&m->row_map);
    m->name_map_valid = false;
}

void jm_model_take_names(jaos_model *m, char **col, char **row, char *obj)
{
    free_names(m->col_name, m->num_col);
    free_names(m->row_name, m->num_row);
    free(m->obj_name);
    m->col_name = col;
    m->row_name = row;
    m->obj_name = obj;
    name_map_is_stale(m);
}

bool jm_name_ok(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return false;
    int64_t len = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (*p <= ' ' || *p == 0x7f)
            return false;
        if (++len > JAOS_NAME_MAX)
            return false;
    }
    return true;
}

const char *jm_col_name(const jaos_model *m, int64_t j, char *buf)
{
    if (m->col_name != nullptr && m->col_name[j] != nullptr)
        return m->col_name[j];
    snprintf(buf, JM_NAME_BUF, "C%lld", (long long)j + 1);
    return buf;
}

const char *jm_row_name(const jaos_model *m, int64_t i, char *buf)
{
    if (m->row_name != nullptr && m->row_name[i] != nullptr)
        return m->row_name[i];
    snprintf(buf, JM_NAME_BUF, "R%lld", (long long)i + 1);
    return buf;
}

const char *jm_obj_name(const jaos_model *m)
{
    return m->obj_name != nullptr ? m->obj_name : "COST";
}

static jaos_status name_out(jaos_model *m, const char *name, char *buf,
                            int64_t cap)
{
    const size_t len = strlen(name);
    if (buf == nullptr || cap <= 0 || (size_t)cap <= len) {
        jm_set_err(m, "the name '%s' needs %zu bytes and the buffer holds "
                   "%lld", name, len + 1, (long long)cap);
        return JAOS_ERR_INVALID_INPUT;
    }
    memcpy(buf, name, len + 1);
    return JAOS_OK;
}

jaos_status jaos_col_name(const jaos_model *m, int64_t j, char *buf,
                          int64_t cap)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    char tmp[JM_NAME_BUF];
    return name_out((jaos_model *)m, jm_col_name(m, j, tmp), buf, cap);
}

jaos_status jaos_row_name(const jaos_model *m, int64_t i, char *buf,
                          int64_t cap)
{
    if (m == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    char tmp[JM_NAME_BUF];
    return name_out((jaos_model *)m, jm_row_name(m, i, tmp), buf, cap);
}

jaos_status jaos_objective_name(const jaos_model *m, char *buf, int64_t cap)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    return name_out((jaos_model *)m, jm_obj_name(m), buf, cap);
}

static jaos_status set_name(jaos_model *m, char ***names, int64_t n,
                            int64_t k, const char *name)
{
    if (name != nullptr && name[0] != '\0' && !jm_name_ok(name)) {
        jm_set_err(m, "a name is 1 to %d bytes with no whitespace or "
                   "control character", JAOS_NAME_MAX);
        return JAOS_ERR_INVALID_INPUT;
    }
    char *copy = nullptr;
    if (name != nullptr && name[0] != '\0') {
        copy = dup_name(name);
        if (copy == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (*names == nullptr) {
        if (copy == nullptr)
            return JAOS_OK;
        *names = jm_calloc_array(n, sizeof(char *));
        if (*names == nullptr) {
            free(copy);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }
    free((*names)[k]);
    (*names)[k] = copy;
    name_map_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_set_col_name(jaos_model *m, int64_t j, const char *name)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    return set_name(m, &m->col_name, m->num_col, j, name);
}

jaos_status jaos_set_row_name(jaos_model *m, int64_t i, const char *name)
{
    if (m == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    return set_name(m, &m->row_name, m->num_row, i, name);
}

jaos_status jaos_set_objective_name(jaos_model *m, const char *name)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (name != nullptr && name[0] != '\0' && !jm_name_ok(name)) {
        jm_set_err(m, "a name is 1 to %d bytes with no whitespace or "
                   "control character", JAOS_NAME_MAX);
        return JAOS_ERR_INVALID_INPUT;
    }
    char *copy = nullptr;
    if (name != nullptr && name[0] != '\0') {
        copy = dup_name(name);
        if (copy == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    free(m->obj_name);
    m->obj_name = copy;
    return JAOS_OK;
}

jaos_status jaos_model_name(const jaos_model *m, char *buf, int64_t cap)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    return name_out((jaos_model *)m,
                    m->model_name != nullptr ? m->model_name : "JAOS",
                    buf, cap);
}

jaos_status jaos_set_model_name(jaos_model *m, const char *name)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (name != nullptr && name[0] != '\0' && !jm_name_ok(name)) {
        jm_set_err(m, "a name is 1 to %d bytes with no whitespace or "
                   "control character", JAOS_NAME_MAX);
        return JAOS_ERR_INVALID_INPUT;
    }
    char *copy = nullptr;
    if (name != nullptr && name[0] != '\0') {
        copy = dup_name(name);
        if (copy == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    free(m->model_name);
    m->model_name = copy;
    return JAOS_OK;
}

static bool build_name_map(jm_nmap *map, char **names, int64_t n)
{
    if (names == nullptr)
        return true;
    for (int64_t k = 0; k < n; k++) {
        int64_t dummy;
        if (names[k] == nullptr || jm_nmap_get(map, names[k], &dummy))
            continue;
        if (!jm_nmap_insert(map, names[k], k))
            return false;
    }
    return true;
}

static bool positional(const char *name, char prefix, char **names,
                       int64_t n, int64_t *out)
{
    if (name[0] != prefix || name[1] < '1' || name[1] > '9')
        return false;
    int64_t k = 0;
    for (const char *p = name + 1; *p; p++) {
        if (*p < '0' || *p > '9' || k > n)
            return false;
        k = k * 10 + (*p - '0');
    }
    if (k < 1 || k > n || (names != nullptr && names[k - 1] != nullptr))
        return false;
    *out = k - 1;
    return true;
}

static jaos_status find_by_name(jaos_model *m, bool is_col, const char *name,
                                int64_t *out)
{
    if (m == nullptr || name == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!m->name_map_valid) {
        name_map_is_stale(m);
        if (!build_name_map(&m->col_map, m->col_name, m->num_col) ||
            !build_name_map(&m->row_map, m->row_name, m->num_row)) {
            name_map_is_stale(m);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        m->name_map_valid = true;
    }
    if (jm_nmap_get(is_col ? &m->col_map : &m->row_map, name, out))
        return JAOS_OK;
    if (is_col ? positional(name, 'C', m->col_name, m->num_col, out)
               : positional(name, 'R', m->row_name, m->num_row, out))
        return JAOS_OK;
    jm_set_err(m, "no %s is named '%s'", is_col ? "column" : "row", name);
    return JAOS_ERR_INVALID_INPUT;
}

jaos_status jaos_col_index(jaos_model *m, const char *name, int64_t *col)
{
    return find_by_name(m, true, name, col);
}

jaos_status jaos_row_index(jaos_model *m, const char *name, int64_t *row)
{
    return find_by_name(m, false, name, row);
}

static jaos_status names_grow(char ***names, int64_t old_n, int64_t add)
{
    if (*names == nullptr)
        return JAOS_OK;
    char **p = realloc(*names, (size_t)(old_n + add) * sizeof *p);
    if (p == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = old_n; k < old_n + add; k++)
        p[k] = nullptr;
    *names = p;
    return JAOS_OK;
}

static void names_compact(char **names, const bool *keep, int64_t old_n)
{
    if (names == nullptr)
        return;
    int64_t at = 0;
    for (int64_t k = 0; k < old_n; k++) {
        if (keep[k])
            names[at++] = names[k];
        else
            free(names[k]);
    }
    for (; at < old_n; at++)
        names[at] = nullptr;
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

static void model_matrix_is_stale(jaos_model *m);

static void model_answer_is_stale(jaos_model *m)
{
    free(m->sol_col);        m->sol_col = nullptr;
    free(m->sol_row);        m->sol_row = nullptr;
    free(m->sol_dual);       m->sol_dual = nullptr;
    free(m->sol_redcost);    m->sol_redcost = nullptr;
    free(m->sol_col_status); m->sol_col_status = nullptr;
    free(m->sol_row_status); m->sol_row_status = nullptr;
    m->sol_basis_ok = false;
    free(m->sol_farkas);     m->sol_farkas = nullptr;
    m->farkas_ok = false;
    free(m->sol_ray);        m->sol_ray = nullptr;
    m->ray_ok = false;
    jm_model_drop_exact(m);
    free(m->mip_inc_x);      m->mip_inc_x = nullptr;
    free(m->mip_pool_x);     m->mip_pool_x = nullptr;
    free(m->mip_pool_obj);   m->mip_pool_obj = nullptr;
    m->mip_pool_n = 0;
    m->mip_has_incumbent = false;
    m->mip_nodes = m->mip_solves = 0;
    m->mip_rcfix_n = m->mip_prop_n = 0;
    m->mip_bound = 0.0;
    m->solve_status = JAOS_SOLVE_NOT_RUN;
    m->objective = 0.0;
    m->solve_work = 0;
    m->solve_iters = 0;
    m->solve_primal_iters = 0;
    m->solve_phase1_iters = 0;
    m->solve_time = 0.0;
}

static bool bound_pair_ok(double lower, double upper)
{
    return !isnan(lower) && !isnan(upper);
}

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

jaos_status jaos_objective_sense(const jaos_model *m, jaos_obj_sense *sense)
{
    if (m == nullptr || sense == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *sense = m->sense;
    return JAOS_OK;
}

jaos_status jaos_objective_offset(const jaos_model *m, double *offset)
{
    if (m == nullptr || offset == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *offset = m->obj_offset;
    return JAOS_OK;
}

jaos_status jaos_set_objective_sense(jaos_model *m, jaos_obj_sense sense)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (sense != JAOS_MINIMIZE && sense != JAOS_MAXIMIZE) {
        jm_set_err(m, "objective sense %d is neither JAOS_MINIMIZE nor "
                      "JAOS_MAXIMIZE", (int)sense);
        return JAOS_ERR_INVALID_INPUT;
    }
    m->sense = sense;
    model_answer_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_set_objective_offset(jaos_model *m, double offset)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!isfinite(offset)) {
        jm_set_err(m, "objective constant must be finite");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->obj_offset = offset;
    model_answer_is_stale(m);
    return JAOS_OK;
}

static void copy_slice(int64_t lo, int64_t hi,
                       const int64_t *src_index, const double *src_value,
                       int64_t *index, double *value)
{
    const size_t n = (size_t)(hi - lo);
    if (n == 0)
        return;
    if (index)
        memcpy(index, &src_index[lo], n * sizeof *index);
    if (value)
        memcpy(value, &src_value[lo], n * sizeof *value);
}

jaos_status jaos_col_entries(const jaos_model *m, int64_t j, int64_t *count,
                             int64_t *index, double *value)
{
    if (m == nullptr || count == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    const int64_t lo = m->a_start[j], hi = m->a_start[j + 1];
    *count = hi - lo;
    copy_slice(lo, hi, m->a_index, m->a_value, index, value);
    return JAOS_OK;
}

jaos_status jaos_row_entries(jaos_model *m, int64_t i, int64_t *count,
                             int64_t *index, double *value)
{
    if (m == nullptr || count == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    const jaos_status st = jm_model_ensure_rowwise(m);
    if (st != JAOS_OK)
        return st;
    const int64_t lo = m->ar_start[i], hi = m->ar_start[i + 1];
    *count = hi - lo;
    copy_slice(lo, hi, m->ar_index, m->ar_value, index, value);
    return JAOS_OK;
}

jaos_status jaos_coefficient(const jaos_model *m, int64_t i, int64_t j,
                             double *value)
{
    if (m == nullptr || value == nullptr || i < 0 || i >= m->num_row ||
        j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;

    int64_t lo = m->a_start[j], hi = m->a_start[j + 1];
    *value = 0.0;
    while (lo < hi) {
        const int64_t mid = lo + (hi - lo) / 2;
        if (m->a_index[mid] < i) {
            lo = mid + 1;
        } else if (m->a_index[mid] > i) {
            hi = mid;
        } else {
            *value = m->a_value[mid];
            break;
        }
    }
    return JAOS_OK;
}

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

    int64_t at = m->a_start[col];
    const int64_t end = m->a_start[col + 1];
    while (at < end && m->a_index[at] < row)
        at++;
    const bool present = at < end && m->a_index[at] == row;

    if (!present && value == 0.0)
        return JAOS_OK;

    if (present && value != 0.0) {
        m->a_value[at] = value;
    } else if (present) {

        memmove(&m->a_index[at], &m->a_index[at + 1],
                (size_t)(m->num_nz - at - 1) * sizeof *m->a_index);
        memmove(&m->a_value[at], &m->a_value[at + 1],
                (size_t)(m->num_nz - at - 1) * sizeof *m->a_value);
        for (int64_t j = col + 1; j <= m->num_col; j++)
            m->a_start[j]--;
        m->num_nz--;
    } else {

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

    model_matrix_is_stale(m);
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

    jm_model_drop_exact(m);

    if (jm_model_has_integer(m))
        return jm_branch_and_bound(m);
    return jm_dual_simplex(m);
}

jaos_status jaos_set_col_integer(jaos_model *m, int64_t j, bool is_integer)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    if (m->col_integer == nullptr) {
        if (!is_integer)
            return JAOS_OK;
        m->col_integer = jm_calloc_array(m->num_col, sizeof(bool));
        if (m->col_integer == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (m->col_integer[j] != is_integer) {
        m->col_integer[j] = is_integer;
        model_answer_is_stale(m);
    }
    return JAOS_OK;
}

jaos_status jaos_col_integer(const jaos_model *m, int64_t j, bool *is_integer)
{
    if (m == nullptr || is_integer == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    *is_integer = m->col_integer != nullptr && m->col_integer[j];
    return JAOS_OK;
}

jaos_status jaos_set_col_semicontinuous(jaos_model *m, int64_t j, bool is_semi)
{
    if (m == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    if (m->col_semi == nullptr) {
        if (!is_semi)
            return JAOS_OK;
        m->col_semi = jm_calloc_array(m->num_col, sizeof(bool));
        if (m->col_semi == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (is_semi && m->col_integer == nullptr) {
        m->col_integer = jm_calloc_array(m->num_col, sizeof(bool));
        if (m->col_integer == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (m->col_semi[j] != is_semi) {
        m->col_semi[j] = is_semi;
        model_answer_is_stale(m);
    }
    return JAOS_OK;
}

jaos_status jaos_col_semicontinuous(const jaos_model *m, int64_t j,
                                    bool *is_semi)
{
    if (m == nullptr || is_semi == nullptr || j < 0 || j >= m->num_col)
        return JAOS_ERR_INVALID_INPUT;
    *is_semi = m->col_semi != nullptr && m->col_semi[j];
    return JAOS_OK;
}

jaos_status jaos_add_sos(jaos_model *m, int type, int64_t n,
                         const int64_t *cols, const double *weights)
{
    if (m == nullptr || (type != 1 && type != 2) || n < 1 ||
        cols == nullptr || weights == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t k = 0; k < n; k++) {
        if (cols[k] < 0 || cols[k] >= m->num_col) {
            jm_set_err(m, "SOS member %lld is not a column of the model",
                       (long long)cols[k]);
            return JAOS_ERR_INVALID_INPUT;
        }
        if (!isfinite(weights[k])) {
            jm_set_err(m, "SOS weights must be finite");
            return JAOS_ERR_INVALID_INPUT;
        }
        for (int64_t t = 0; t < k; t++)
            if (cols[t] == cols[k] || weights[t] == weights[k]) {
                jm_set_err(m, "SOS members must be distinct columns with "
                              "distinct weights");
                return JAOS_ERR_INVALID_INPUT;
            }
    }
    const int64_t base = m->num_sos > 0 ? m->sos_start[m->num_sos] : 0;
    int *ty = realloc(m->sos_type, (size_t)(m->num_sos + 1) * sizeof *ty);
    if (ty == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    m->sos_type = ty;
    int64_t *ss = realloc(m->sos_start, (size_t)(m->num_sos + 2) * sizeof *ss);
    if (ss == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    m->sos_start = ss;
    int64_t *sc = realloc(m->sos_col, (size_t)(base + n) * sizeof *sc);
    if (sc == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    m->sos_col = sc;
    double *sw = realloc(m->sos_weight, (size_t)(base + n) * sizeof *sw);
    if (sw == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    m->sos_weight = sw;
    if (m->col_integer == nullptr) {
        m->col_integer = jm_calloc_array(m->num_col, sizeof(bool));
        if (m->col_integer == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (m->num_sos == 0)
        m->sos_start[0] = 0;
    for (int64_t k = 0; k < n; k++) {
        int64_t p = base + k;
        while (p > base && m->sos_weight[p - 1] > weights[k]) {
            m->sos_col[p] = m->sos_col[p - 1];
            m->sos_weight[p] = m->sos_weight[p - 1];
            p--;
        }
        m->sos_col[p] = cols[k];
        m->sos_weight[p] = weights[k];
    }
    m->sos_type[m->num_sos] = type;
    m->sos_start[m->num_sos + 1] = base + n;
    m->num_sos++;
    model_answer_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_set_row_indicator(jaos_model *m, int64_t i, int64_t col,
                                   int value)
{
    if (m == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    if (col < 0) {
        if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0) {
            m->row_ind_col[i] = -1;
            model_answer_is_stale(m);
        }
        return JAOS_OK;
    }
    if (col >= m->num_col) {
        jm_set_err(m, "indicator column %lld is not a column of the model",
                   (long long)col);
        return JAOS_ERR_INVALID_INPUT;
    }
    if (value != 0 && value != 1) {
        jm_set_err(m, "an indicator fires at 0 or at 1");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (m->col_integer == nullptr || !m->col_integer[col]) {
        jm_set_err(m, "the indicator column must be marked integer first");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (m->row_ind_col == nullptr) {
        m->row_ind_col = jm_alloc_array(m->num_row, sizeof(int64_t));
        m->row_ind_val = jm_calloc_array(m->num_row, sizeof(int));
        if (m->row_ind_col == nullptr || m->row_ind_val == nullptr) {
            free(m->row_ind_col); m->row_ind_col = nullptr;
            free(m->row_ind_val); m->row_ind_val = nullptr;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t r = 0; r < m->num_row; r++)
            m->row_ind_col[r] = -1;
    }
    m->row_ind_col[i] = col;
    m->row_ind_val[i] = value;
    model_answer_is_stale(m);
    return JAOS_OK;
}

jaos_status jaos_row_indicator(const jaos_model *m, int64_t i, int64_t *col,
                               int *value)
{
    if (m == nullptr || i < 0 || i >= m->num_row)
        return JAOS_ERR_INVALID_INPUT;
    const bool has = m->row_ind_col != nullptr && m->row_ind_col[i] >= 0;
    if (col != nullptr)
        *col = has ? m->row_ind_col[i] : -1;
    if (value != nullptr)
        *value = has ? m->row_ind_val[i] : 0;
    return JAOS_OK;
}

int64_t jaos_num_sos(const jaos_model *m)
{
    return m == nullptr ? 0 : m->num_sos;
}

jaos_status jaos_sos(const jaos_model *m, int64_t k, int *type, int64_t *n,
                     int64_t *cols, double *weights)
{
    if (m == nullptr || k < 0 || k >= m->num_sos)
        return JAOS_ERR_INVALID_INPUT;
    const int64_t b = m->sos_start[k], e = m->sos_start[k + 1];
    if (type != nullptr)
        *type = m->sos_type[k];
    if (n != nullptr)
        *n = e - b;
    if (cols != nullptr)
        memcpy(cols, m->sos_col + b, (size_t)(e - b) * sizeof *cols);
    if (weights != nullptr)
        memcpy(weights, m->sos_weight + b, (size_t)(e - b) * sizeof *weights);
    return JAOS_OK;
}

jaos_status jaos_set_mip_gap(jaos_model *m, double gap)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!(gap >= 0.0) || !isfinite(gap)) {
        jm_set_err(m, "the MIP gap must be finite and non-negative");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_gap = gap;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive(jaos_model *m, bool on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_dive = on;
    return JAOS_OK;
}

jaos_status jaos_set_mip_cut_rounds(jaos_model *m, int64_t rounds)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_cut_rounds_set = rounds >= 0;
    m->cfg.mip_cut_rounds = rounds >= 0 ? rounds : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_cut_depth(jaos_model *m, int64_t depth)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_cut_depth_set = depth >= 0;
    m->cfg.mip_cut_depth = depth >= 0 ? depth : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_heuristics(jaos_model *m, bool on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_no_heuristics = !on;
    return JAOS_OK;
}

jaos_status jaos_set_mip_node_limit(jaos_model *m, int64_t nodes)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (nodes < 0) {
        jm_set_err(m, "the node limit must be 0 or more");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_node_limit = nodes;
    return JAOS_OK;
}

jaos_status jaos_set_mip_branching(jaos_model *m, jaos_branching rule)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (rule != JAOS_BRANCH_PSEUDOCOST && rule != JAOS_BRANCH_MOST_FRACTIONAL) {
        jm_set_err(m, "the branching rule must be pseudocost or most-fractional");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_branching = (int)rule;
    return JAOS_OK;
}

jaos_status jaos_set_algorithm(jaos_model *m, jaos_algorithm alg)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (alg != JAOS_ALGORITHM_DUAL && alg != JAOS_ALGORITHM_PRIMAL) {
        jm_set_err(m, "the algorithm must be dual or primal");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.force_primal = alg == JAOS_ALGORITHM_PRIMAL;
    return JAOS_OK;
}

jaos_algorithm jaos_algorithm_of(const jaos_model *m)
{
    if (m == nullptr || !m->cfg.force_primal)
        return JAOS_ALGORITHM_DUAL;
    return JAOS_ALGORITHM_PRIMAL;
}

jaos_status jaos_set_mip_reliability(jaos_model *m, int64_t branches)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_reliability_set = branches >= 0;
    m->cfg.mip_reliability = branches >= 0 ? branches : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_probe_cap(jaos_model *m, double multiple)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(multiple) || multiple == INFINITY) {
        jm_set_err(m, "the probe cap must be a finite multiple of the node's "
                      "work, 0 for none, or negative for the default");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_probe_cap_set = multiple >= 0.0;
    m->cfg.mip_probe_cap = multiple >= 0.0 ? multiple : 0.0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive_child(jaos_model *m, jaos_dive_child rule)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (rule != JAOS_DIVE_NEARER && rule != JAOS_DIVE_UP &&
        rule != JAOS_DIVE_DOWN && rule != JAOS_DIVE_PSEUDOCOST) {
        jm_set_err(m, "the dive's child rule must be nearer, up, down or "
                      "pseudocost");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_dive_child = (int)rule;
    return JAOS_OK;
}

jaos_status jaos_set_mip_cut_drop(jaos_model *m, bool on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_no_cut_drop = !on;
    return JAOS_OK;
}

jaos_status jaos_set_mip_node_cut_cap(jaos_model *m, int64_t cap)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_node_cut_cap_set = cap >= 0;
    m->cfg.mip_node_cut_cap = cap >= 0 ? cap : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_cover_rounds(jaos_model *m, int64_t rounds)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_cover_rounds_set = rounds >= 0;
    m->cfg.mip_cover_rounds = rounds >= 0 ? rounds : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_cut_stall(jaos_model *m, double fraction)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(fraction) || isinf(fraction)) {
        jm_set_err(m, "the cut stall must be a finite fraction of the bound, "
                      "0 for none, or negative for the default");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_cut_stall_set = fraction >= 0.0;
    m->cfg.mip_cut_stall = fraction >= 0.0 ? fraction : 0.0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_node_cut_stall(jaos_model *m, double fraction)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(fraction) || isinf(fraction)) {
        jm_set_err(m, "the node cut stall must be a finite fraction of the "
                      "bound, 0 for none, or negative for the default");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_node_cut_stall_set = fraction >= 0.0;
    m->cfg.mip_node_cut_stall = fraction >= 0.0 ? fraction : 0.0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_root_cut_drop(jaos_model *m, int on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_root_cut_drop_set = on >= 0;
    m->cfg.mip_root_cut_drop = on > 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_cover_lift(jaos_model *m, int on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_cover_lift_set = on >= 0;
    m->cfg.mip_cover_lift = on > 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_mir_rounds(jaos_model *m, int64_t rounds)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_mir_rounds_set = rounds >= 0;
    m->cfg.mip_mir_rounds = rounds >= 0 ? rounds : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive_backtrack(jaos_model *m, int64_t times)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_dive_backtrack_set = times >= 0;
    m->cfg.mip_dive_backtrack = times >= 0 ? times : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive_gap(jaos_model *m, double fraction)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(fraction) || isinf(fraction)) {
        jm_set_err(m, "the dive gap must be a finite fraction of the bound, "
                      "0 for none, or negative for the default");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_dive_gap_set = fraction >= 0.0;
    m->cfg.mip_dive_gap = fraction >= 0.0 ? fraction : 0.0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_mir_aggregate(jaos_model *m, int64_t rows)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_mir_aggregate_set = rows >= 0;
    m->cfg.mip_mir_aggregate = rows >= 0 ? rows : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive_heuristic(jaos_model *m, int64_t solves)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_dive_heuristic_set = solves >= 0;
    m->cfg.mip_dive_heuristic = solves >= 0 ? solves : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive_heuristic_depth(jaos_model *m, int64_t depth)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_dive_heuristic_depth_set = depth >= 0;
    m->cfg.mip_dive_heuristic_depth = depth >= 0 ? depth : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_rins(jaos_model *m, int64_t solves)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_rins_set = solves >= 0;
    m->cfg.mip_rins = solves >= 0 ? solves : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_dive_degrade(jaos_model *m, double frac)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(frac) || isinf(frac)) {
        jm_set_err(m, "the dive's degradation bound must be a finite fraction "
                      "of the parent's bound, 0 for none, or negative for the "
                      "default");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_dive_degrade_set = frac >= 0.0;
    m->cfg.mip_dive_degrade = frac >= 0.0 ? frac : 0.0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_feaspump(jaos_model *m, int64_t rounds)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_feaspump_set = rounds >= 0;
    m->cfg.mip_feaspump = rounds >= 0 ? rounds : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_pump_general(jaos_model *m, int on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_pump_general_set = on >= 0;
    m->cfg.mip_pump_general = on > 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_pump_obj(jaos_model *m, double decay)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(decay) || decay >= 1.0) {
        jm_set_err(m, "the objective pump's decay must be a fraction below "
                      "1, 0 for the plain pump, or negative for the default");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_pump_obj_set = decay >= 0.0;
    m->cfg.mip_pump_obj = decay >= 0.0 ? decay : 0.0;
    return JAOS_OK;
}

jaos_status jaos_presolve_result(const jaos_model *m,
                                 jaos_presolve_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    const jm_presolve_counts *c = &m->presolve_counts;
    *out = (jaos_presolve_report){
        .num_row = m->presolve_num_row,
        .num_col = m->presolve_num_col,
        .num_nz = m->presolve_num_nz,
        .rounds = c->rounds,
        .fixed_col = c->fixed_col,
        .empty_row = c->empty_row,
        .empty_col = c->empty_col,
        .singleton_row = c->singleton_row,
        .singleton_col = c->singleton_col,
        .free_col_singleton = c->free_col_singleton,
        .forcing_row = c->forcing_row,
        .redundant_row = c->redundant_row,
        .implied_free_col = c->implied_free_col,
        .tightened_bound = c->tightened_bound,
        .duplicate_row = c->duplicate_row,
        .duplicate_col = c->duplicate_col,
        .dominated_col = c->dominated_col,
    };
    return JAOS_OK;
}

jaos_status jaos_model_statistics(const jaos_model *m, jaos_model_stats *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    jaos_model_stats st = {0};
    st.num_row = m->num_row;
    st.num_col = m->num_col;
    st.num_nz = m->num_nz;

    for (int64_t i = 0; i < m->num_row; i++) {
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        const bool flo = lo > -INFINITY, fhi = hi < INFINITY;
        if (flo && fhi)
            st.equality_row += lo == hi, st.ranged_row += lo != hi;
        else if (flo || fhi)
            st.one_sided_row++;
        else
            st.free_row++;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        const double lo = m->col_lower[j], hi = m->col_upper[j];
        const bool flo = lo > -INFINITY, fhi = hi < INFINITY;
        if (flo && fhi)
            st.fixed_col += lo == hi, st.ranged_col += lo != hi;
        else if (flo || fhi)
            st.one_sided_col++;
        else
            st.free_col++;
        if (m->col_integer != nullptr && m->col_integer[j]) {
            st.integer_col++;

            if (flo && fhi && ceil(lo) == 0.0 && floor(hi) == 1.0)
                st.binary_col++;
        }
        if (m->col_cost[j] != 0.0) {
            const double a = fabs(m->col_cost[j]);
            st.obj_nz++;
            if (st.obj_min_abs == 0.0 || a < st.obj_min_abs)
                st.obj_min_abs = a;
            if (a > st.obj_max_abs)
                st.obj_max_abs = a;
        }
        const int64_t s0 = m->a_start[j], s1 = m->a_start[j + 1];
        if (s1 == s0)
            st.empty_col++;
        for (int64_t k = s0; k < s1; k++) {
            const double a = fabs(m->a_value[k]);
            if (st.min_abs == 0.0 || a < st.min_abs)
                st.min_abs = a;
            if (a > st.max_abs)
                st.max_abs = a;
        }
    }

    if (m->num_row > 0 && m->num_nz >= 0) {
        bool *touched = calloc((size_t)m->num_row, sizeof *touched);
        if (touched == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
        for (int64_t k = 0; k < m->num_nz; k++)
            touched[m->a_index[k]] = true;
        for (int64_t i = 0; i < m->num_row; i++)
            st.empty_row += !touched[i];
        free(touched);
    }
    *out = st;
    return JAOS_OK;
}

jaos_status jaos_set_mip_start(jaos_model *m, const double *col_value)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    free(m->mip_start);
    m->mip_start = nullptr;
    if (col_value == nullptr)
        return JAOS_OK;
    for (int64_t j = 0; j < m->num_col; j++)
        if (isnan(col_value[j]) || isinf(col_value[j])) {
            jm_set_err(m, "the starting point's value for column %lld is not "
                          "finite", (long long)j);
            return JAOS_ERR_INVALID_INPUT;
        }
    m->mip_start = malloc((size_t)(m->num_col > 0 ? m->num_col : 1)
                          * sizeof *m->mip_start);
    if (m->mip_start == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    memcpy(m->mip_start, col_value, (size_t)m->num_col * sizeof *m->mip_start);
    return JAOS_OK;
}

jaos_status jaos_set_mip_cutoff(jaos_model *m, double cutoff)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (isnan(cutoff)) {
        jm_set_err(m, "a cutoff must be a finite objective, or an infinity "
                      "to remove it");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_cutoff_set = !isinf(cutoff);
    m->cfg.mip_cutoff = isinf(cutoff) ? 0.0 : cutoff;
    return JAOS_OK;
}

jaos_status jaos_set_mip_pump_always(jaos_model *m, int on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_pump_always_set = on >= 0;
    m->cfg.mip_pump_always = on > 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_rcfix(jaos_model *m, int on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_rcfix_set = on >= 0;
    m->cfg.mip_rcfix = on > 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_propagate(jaos_model *m, int64_t rounds)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_propagate_set = rounds >= 0;
    m->cfg.mip_propagate = rounds >= 0 ? rounds : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_propagate_depth(jaos_model *m, int64_t depth)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_propagate_depth_set = depth >= 0;
    m->cfg.mip_propagate_depth = depth >= 0 ? depth : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_node_mir(jaos_model *m, int on)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_node_mir_set = on >= 0;
    m->cfg.mip_node_mir = on > 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_probe_depth(jaos_model *m, int64_t depth)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.mip_probe_depth_set = depth >= 0;
    m->cfg.mip_probe_depth = depth >= 0 ? depth : 0;
    return JAOS_OK;
}

jaos_status jaos_set_mip_pool_size(jaos_model *m, int64_t size)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (size == 0) {
        jm_set_err(m, "the solution pool must hold at least one point; a "
                      "negative size restores 1");
        return JAOS_ERR_INVALID_INPUT;
    }
    m->cfg.mip_pool_size = size > 0 ? size : 0;
    return JAOS_OK;
}

jaos_status jaos_set_incumbent_callback(jaos_model *m, jaos_incumbent_fn cb,
                                        void *user)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    m->cfg.incumbent_cb = cb;
    m->cfg.incumbent_user = user;
    return JAOS_OK;
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
double  jaos_solve_time(const jaos_model *m) { return m ? m->solve_time : 0.0; }

jaos_status jaos_solution(const jaos_model *m, double *col_value,
    double *row_activity, double *row_dual, double *col_dual)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;

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

    if (!m->sol_basis_ok || m->sol_col_status == nullptr ||
        m->sol_row_status == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (col_status)
        memcpy(col_status, m->sol_col_status,
               (size_t)m->num_col * sizeof *col_status);
    if (row_status)
        memcpy(row_status, m->sol_row_status,
               (size_t)m->num_row * sizeof *row_status);
    return JAOS_OK;
}

jaos_status jaos_certificate(const jaos_model *m, double *row_ray)
{
    if (m == nullptr || row_ray == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (m->solve_status != JAOS_SOLVE_INFEASIBLE || !m->farkas_ok ||
        m->sol_farkas == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    memcpy(row_ray, m->sol_farkas, (size_t)m->num_row * sizeof *row_ray);
    return JAOS_OK;
}

jaos_status jaos_unbounded_ray(const jaos_model *m, double *col_ray)
{
    if (m == nullptr || col_ray == nullptr)
        return JAOS_ERR_INVALID_INPUT;

    if (m->solve_status != JAOS_SOLVE_UNBOUNDED || !m->ray_ok ||
        m->sol_ray == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    memcpy(col_ray, m->sol_ray, (size_t)m->num_col * sizeof *col_ray);
    return JAOS_OK;
}

#define JM_BASIS_PAIRED(m) \
    (((m)->start_col_status == nullptr) == ((m)->start_row_status == nullptr))

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

bool jm_model_basis_count_ok(const jaos_model *m)
{
    if (m->sol_col_status == nullptr || m->sol_row_status == nullptr)
        return false;
    int64_t basic = 0;
    for (int64_t j = 0; j < m->num_col; j++)
        basic += m->sol_col_status[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < m->num_row; i++)
        basic += m->sol_row_status[i] == JAOS_BASIS_BASIC;
    return basic == m->num_row;
}

jaos_status jm_model_remember_basis(jaos_model *m)
{
    assert(JM_BASIS_PAIRED(m));
    if (m->sol_col_status == nullptr || m->sol_row_status == nullptr)
        return JAOS_OK;
    return store_basis(m, m->sol_col_status, m->sol_row_status);
}

void jm_obj_add(double *sum, double *comp, double t)
{

    assert(sum != comp);
    const double a = *sum, u = a + t;
    *comp += (fabs(a) >= fabs(t)) ? ((a - u) + t) : ((t - u) + a);
    *sum = u;
}

double jm_two_product_residue(double a, double b, double p)
{
    constexpr double SPLIT = 134217729.0;
    constexpr double BIG   = 0x1p996;
    if (!isfinite(p) || fabs(a) > BIG || fabs(b) > BIG)
        return 0.0;
    const double ca = SPLIT * a, ah = ca - (ca - a), al = a - ah;
    const double cb = SPLIT * b, bh = cb - (cb - b), bl = b - bh;
    const double e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    return isfinite(e) ? e : 0.0;
}

void jm_model_publish_objective(jaos_model *m)
{

    assert(m->solve_status == JAOS_SOLVE_OPTIMAL);
    assert(m->num_col == 0 || m->sol_col != nullptr);
    assert(m->num_row == 0 || m->sol_row != nullptr);
    assert(m->num_row == 0 || m->sol_dual != nullptr);
    assert(m->num_col == 0 || m->sol_redcost != nullptr);
    assert(m->num_col == 0 || m->sol_col_status != nullptr);
    assert(m->num_row == 0 || m->sol_row_status != nullptr);

    double sum = m->obj_offset, comp = 0.0;
    if (m->sol_col != nullptr) {
        for (int64_t j = 0; j < m->num_col; j++) {
            const double c = m->col_cost[j], x = m->sol_col[j];
            const double t = c * x;
            jm_obj_add(&sum, &comp, t);
            const double e = jm_two_product_residue(c, x, t);
            if (e != 0.0)
                jm_obj_add(&sum, &comp, e);
        }
    }

    m->objective = (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

static bool status_in_range(jaos_basis_status s)
{
    return s == JAOS_BASIS_BASIC || s == JAOS_BASIS_AT_LOWER ||
           s == JAOS_BASIS_AT_UPPER || s == JAOS_BASIS_FREE;
}

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

    if (num_col > 0 && (!all_finite(col_cost, num_col) ||
                        any_nan(col_lower, num_col) || any_nan(col_upper, num_col)))
        return JAOS_ERR_INVALID_INPUT;
    if (num_row > 0 && (any_nan(row_lower, num_row) || any_nan(row_upper, num_row)))
        return JAOS_ERR_INVALID_INPUT;

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

    int64_t kept = 0;
    for (int64_t k = 0; k < num_nz; k++)
        if (a_value[k] != 0.0)
            kept++;

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

    return JAOS_OK;
}

static void model_matrix_is_stale(jaos_model *m)
{
    m->rowwise_valid = false;
    m->scale_valid = false;
    m->scale_clamped = false;
    model_answer_is_stale(m);
}

static void basis_survives_or_goes(jaos_model *m)
{
    assert(JM_BASIS_PAIRED(m));
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

static jaos_basis_status arriving_status(double lower, double upper)
{
    if (isfinite(lower))
        return JAOS_BASIS_AT_LOWER;
    if (isfinite(upper))
        return JAOS_BASIS_AT_UPPER;
    return JAOS_BASIS_FREE;
}

static void basis_extend(jaos_basis_status **arr, int64_t old_n, int64_t add,
                         const jaos_basis_status *fill, jaos_model *m)
{
    assert(JM_BASIS_PAIRED(m));
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

    jaos_basis_status *arriving = nullptr;
    if (m->start_col_status != nullptr) {
        arriving = jm_alloc_array(num_new, sizeof(jaos_basis_status));
        if (arriving == nullptr) {
            jaos_clear_basis(m);
        } else {
            for (int64_t j = 0; j < num_new; j++) {
                arriving[j] = arriving_status(col_lower[j], col_upper[j]);
                if (arriving[j] == JAOS_BASIS_FREE) {

                    free(arriving);
                    arriving = nullptr;
                    jaos_clear_basis(m);
                    break;
                }
            }
        }
    }

    if (names_grow(&m->col_name, m->num_col, num_new) != JAOS_OK) {
        free(arriving);
        free(cost); free(cl); free(cu); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    if (m->col_integer != nullptr) {
        bool *p = realloc(m->col_integer, (size_t)ncol * sizeof *p);
        if (p == nullptr) {
            free(arriving);
            free(cost); free(cl); free(cu); free(as); free(ai); free(av);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t j = m->num_col; j < ncol; j++)
            p[j] = false;
        m->col_integer = p;
    }
    if (m->col_semi != nullptr) {
        bool *p = realloc(m->col_semi, (size_t)ncol * sizeof *p);
        if (p == nullptr) {
            free(arriving);
            free(cost); free(cl); free(cu); free(as); free(ai); free(av);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t j = m->num_col; j < ncol; j++)
            p[j] = false;
        m->col_semi = p;
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
    name_map_is_stale(m);
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

    if (names_grow(&m->row_name, m->num_row, num_new) != JAOS_OK) {
        free(rl); free(ru); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    free(m->row_lower); free(m->row_upper);
    free(m->a_start);   free(m->a_index); free(m->a_value);
    m->row_lower = rl;  m->row_upper = ru;
    m->a_start = as;    m->a_index = ai;  m->a_value = av;

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
    if (m->row_ind_col != nullptr) {
        int64_t *ic = realloc(m->row_ind_col, (size_t)nrow * sizeof *ic);
        int *iv = ic ? realloc(m->row_ind_val, (size_t)nrow * sizeof *iv)
                     : nullptr;
        if (ic != nullptr)
            m->row_ind_col = ic;
        if (iv != nullptr)
            m->row_ind_val = iv;
        if (ic == nullptr || iv == nullptr) {
            free(m->row_ind_col); m->row_ind_col = nullptr;
            free(m->row_ind_val); m->row_ind_val = nullptr;
        } else {
            for (int64_t i = m->num_row; i < nrow; i++) {
                m->row_ind_col[i] = -1;
                m->row_ind_val[i] = 0;
            }
        }
    }
    m->num_row = nrow;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    name_map_is_stale(m);
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
    int64_t *newidx = nullptr;
    if (m->num_sos > 0 || m->row_ind_col != nullptr) {
        newidx = malloc((size_t)m->num_col * sizeof *newidx);
        if (newidx == nullptr) {
            free(keep);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }

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
    names_compact(m->col_name, keep, m->num_col);
    if (m->col_integer != nullptr) {
        int64_t at = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            if (keep[j])
                m->col_integer[at++] = m->col_integer[j];
    }
    if (m->col_semi != nullptr) {
        int64_t at = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            if (keep[j])
                m->col_semi[at++] = m->col_semi[j];
    }
    if (m->num_sos > 0 && newidx != nullptr) {
        int64_t at = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            newidx[j] = keep[j] ? at++ : -1;
        int64_t nsets = 0, pos = 0;
        for (int64_t k = 0; k < m->num_sos; k++) {
            const int64_t b = m->sos_start[k], e = m->sos_start[k + 1];
            const int64_t start = pos;
            for (int64_t t = b; t < e; t++) {
                const int64_t nj = newidx[m->sos_col[t]];
                if (nj < 0)
                    continue;
                m->sos_col[pos] = nj;
                m->sos_weight[pos] = m->sos_weight[t];
                pos++;
            }
            if (pos > start) {
                m->sos_type[nsets] = m->sos_type[k];
                m->sos_start[nsets] = start;
                nsets++;
            }
        }
        m->sos_start[nsets] = pos;
        m->num_sos = nsets;
    }
    if (m->row_ind_col != nullptr && newidx != nullptr) {
        int64_t at = 0;
        for (int64_t j = 0; j < m->num_col; j++)
            newidx[j] = keep[j] ? at++ : -1;
        for (int64_t i = 0; i < m->num_row; i++)
            if (m->row_ind_col[i] >= 0)
                m->row_ind_col[i] = newidx[m->row_ind_col[i]];
    }
    free(newidx);
    free(keep);

    free(m->col_cost);  free(m->col_lower); free(m->col_upper);
    free(m->a_start);   free(m->a_index);   free(m->a_value);
    m->col_cost = cost; m->col_lower = cl;  m->col_upper = cu;
    m->a_start = as;    m->a_index = ai;    m->a_value = av;
    m->num_col = ncol;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    name_map_is_stale(m);
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
    names_compact(m->row_name, keep, m->num_row);
    if (m->row_ind_col != nullptr) {
        int64_t at = 0;
        for (int64_t i = 0; i < m->num_row; i++)
            if (keep[i]) {
                m->row_ind_col[at] = m->row_ind_col[i];
                m->row_ind_val[at] = m->row_ind_val[i];
                at++;
            }
    }
    free(keep);

    free(m->row_lower); free(m->row_upper);
    free(m->a_start);   free(m->a_index); free(m->a_value);
    m->row_lower = rl;  m->row_upper = ru;
    m->a_start = as;    m->a_index = ai;  m->a_value = av;
    m->num_row = nrow;
    m->num_nz  = nnz;

    model_matrix_is_stale(m);
    name_map_is_stale(m);
    basis_survives_or_goes(m);
    return JAOS_OK;
}

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

jaos_status jaos_model_copy(const jaos_model *src, jaos_model **out)
{
    if (src == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *out = nullptr;
    jaos_model *m = nullptr;
    jaos_status st = jaos_model_new(&m);
    if (st != JAOS_OK)
        return st;
    st = jaos_load_lp(m, src->num_col, src->num_row, src->sense,
                      src->obj_offset, src->col_cost, src->col_lower,
                      src->col_upper, src->row_lower, src->row_upper,
                      src->num_nz, src->a_start, src->a_index, src->a_value);
    if (st != JAOS_OK)
        goto fail;

    char **cn = nullptr, **rn = nullptr, *on = nullptr;
    if (src->col_name != nullptr) {
        cn = jm_calloc_array(src->num_col, sizeof(char *));
        if (cn == nullptr)
            goto oom;
        for (int64_t j = 0; j < src->num_col; j++)
            if (src->col_name[j] != nullptr &&
                (cn[j] = jm_name_copy(src->col_name[j])) == nullptr)
                goto oom_names;
    }
    if (src->row_name != nullptr) {
        rn = jm_calloc_array(src->num_row, sizeof(char *));
        if (rn == nullptr)
            goto oom_names;
        for (int64_t i = 0; i < src->num_row; i++)
            if (src->row_name[i] != nullptr &&
                (rn[i] = jm_name_copy(src->row_name[i])) == nullptr)
                goto oom_names;
    }
    if (src->obj_name != nullptr &&
        (on = jm_name_copy(src->obj_name)) == nullptr)
        goto oom_names;
    jm_model_take_names(m, cn, rn, on);
    if (src->model_name != nullptr &&
        (m->model_name = jm_name_copy(src->model_name)) == nullptr)
        goto oom;
    if (src->col_integer != nullptr) {
        m->col_integer = malloc((size_t)(src->num_col > 0 ? src->num_col : 1)
                                * sizeof *m->col_integer);
        if (m->col_integer == nullptr)
            goto oom;
        memcpy(m->col_integer, src->col_integer,
               (size_t)src->num_col * sizeof *m->col_integer);
    }
    if (src->col_semi != nullptr) {
        m->col_semi = malloc((size_t)(src->num_col > 0 ? src->num_col : 1)
                             * sizeof *m->col_semi);
        if (m->col_semi == nullptr)
            goto oom;
        memcpy(m->col_semi, src->col_semi,
               (size_t)src->num_col * sizeof *m->col_semi);
    }
    if (src->row_ind_col != nullptr) {
        const size_t nr = (size_t)(src->num_row > 0 ? src->num_row : 1);
        m->row_ind_col = malloc(nr * sizeof *m->row_ind_col);
        m->row_ind_val = malloc(nr * sizeof *m->row_ind_val);
        if (m->row_ind_col == nullptr || m->row_ind_val == nullptr)
            goto oom;
        memcpy(m->row_ind_col, src->row_ind_col,
               (size_t)src->num_row * sizeof *m->row_ind_col);
        memcpy(m->row_ind_val, src->row_ind_val,
               (size_t)src->num_row * sizeof *m->row_ind_val);
    }
    if (src->num_sos > 0) {
        const int64_t nz = src->sos_start[src->num_sos];
        m->sos_type = malloc((size_t)src->num_sos * sizeof *m->sos_type);
        m->sos_start = malloc((size_t)(src->num_sos + 1) * sizeof *m->sos_start);
        m->sos_col = malloc((size_t)(nz > 0 ? nz : 1) * sizeof *m->sos_col);
        m->sos_weight = malloc((size_t)(nz > 0 ? nz : 1) * sizeof *m->sos_weight);
        if (!m->sos_type || !m->sos_start || !m->sos_col || !m->sos_weight)
            goto oom;
        memcpy(m->sos_type, src->sos_type, (size_t)src->num_sos * sizeof *m->sos_type);
        memcpy(m->sos_start, src->sos_start,
               (size_t)(src->num_sos + 1) * sizeof *m->sos_start);
        memcpy(m->sos_col, src->sos_col, (size_t)nz * sizeof *m->sos_col);
        memcpy(m->sos_weight, src->sos_weight, (size_t)nz * sizeof *m->sos_weight);
        m->num_sos = src->num_sos;
    }

    m->cfg = src->cfg;

    if (src->mip_start != nullptr) {
        st = jaos_set_mip_start(m, src->mip_start);
        if (st != JAOS_OK)
            goto fail;
    }

    if (src->start_col_status != nullptr && src->start_row_status != nullptr) {
        st = jaos_set_basis(m, src->start_col_status, src->start_row_status);
        if (st != JAOS_OK)
            goto fail;
    }
    *out = m;
    return JAOS_OK;

oom_names:
    free_names(cn, src->num_col);
    free_names(rn, src->num_row);
    free(on);
oom:
    st = JAOS_ERR_OUT_OF_MEMORY;
fail:
    jaos_model_free(m);
    return st;
}
