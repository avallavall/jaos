/* Model lifecycle: create, load, query, free — and the CSR mirror.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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
    free(m->sol_farkas);
    free(m->sol_ray);
    free(m->start_col_status);
    free(m->start_row_status);
    jm_model_take_names(m, nullptr, nullptr, nullptr);
    /* The problem goes; the configuration stays. Saved as one object rather
     * than field by field (D78). */
    const jm_config cfg = m->cfg;
    memset(m, 0, sizeof *m);
    m->cfg = cfg;
}

/* --------------------------------------------------------------------- */
/* Names                                                                 */
/* --------------------------------------------------------------------- */

/* A name array's strings, then the array. `n` entries, any of them null. */
static void free_names(char **names, int64_t n)
{
    if (names == nullptr)
        return;
    for (int64_t k = 0; k < n; k++)
        free(names[k]);
    free(names);
}

/* An owned copy of a name. strdup is C23 but not every libc exposes it
 * under -std=c23 alone, and this is two lines. */
static char *dup_name(const char *name)
{
    const size_t len = strlen(name) + 1;
    char *copy = malloc(len);
    if (copy != nullptr)
        memcpy(copy, name, len);
    return copy;
}

/* The lookup is rebuilt on the next jaos_col_index or jaos_row_index. */
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

/* Copies `name` into the caller's buffer, or refuses with the size it
 * would have needed. */
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

/* Stores one name into slot k of an array of n, allocating the array on
 * the first name it ever holds. NULL or "" clears the slot. */
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
            return JAOS_OK;            /* clearing a name nothing holds */
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

/* Fills one map from one name array: stored names only, the first holder
 * of a repeated name winning, which is the "lower index" the header
 * promises. */
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

/* A positional name, `<prefix><k>` with k in 1..n and no leading zero,
 * naming slot k-1 -- but only when that slot has no name of its own,
 * because then it is called something else. */
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

/* Names through a dimension change. Growing appends unnamed slots; a
 * deletion keeps the survivors' names in their new positions and frees the
 * rest. Both leave the array nullptr when it was, since no name arrived. */
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

/* The two tolerances the caller owns. Rejected rather than clamped when they
 * are not usable numbers. Zero restores the default. */
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

/* Declared before its first caller: `jaos_set_coefficient` is the first of
 * the five matrix modifications and the definition sits with the other four.
 * It set the three flags inline until D219, which is how the list in the
 * comment below could drift from the list that enforces it. */
static void model_matrix_is_stale(jaos_model *m);

/* Every modification goes through this (D66).
 *
 * The solution arrays are freed rather than kept: `jaos_solution` reports
 * JAOS_ERR_INVALID_INPUT when there is no optimum to copy, so a stale read
 * becomes an error at the call rather than a number.
 *
 * What is not invalidated: the scaling and the row-wise mirror. Both are
 * derived from the matrix alone — scale.c reads no bound and no cost — so
 * changing a bound or a cost leaves them exactly correct. A modification
 * that touches the matrix must invalidate them, and there are five:
 * jaos_set_coefficient and the four that move a dimension. All five go
 * through model_matrix_is_stale, which is this plus both derived copies, so
 * the two lists cannot drift apart (D219).
 *
 * Nor is the starting basis. The answer stops being true when a bound moves;
 * the basis that produced it does not stop being a basis (D68). That is why
 * start_*_status is stored apart from sol_*_status (see jaos_internal.h). */
static void model_answer_is_stale(jaos_model *m)
{
    free(m->sol_col);        m->sol_col = nullptr;
    free(m->sol_row);        m->sol_row = nullptr;
    free(m->sol_dual);       m->sol_dual = nullptr;
    free(m->sol_redcost);    m->sol_redcost = nullptr;
    free(m->sol_col_status); m->sol_col_status = nullptr;
    free(m->sol_row_status); m->sol_row_status = nullptr;
    free(m->sol_farkas);     m->sol_farkas = nullptr;
    m->farkas_ok = false;
    free(m->sol_ray);        m->sol_ray = nullptr;
    m->ray_ok = false;
    m->solve_status = JAOS_SOLVE_NOT_RUN;
    m->objective = 0.0;
    m->solve_work = 0;
    m->solve_iters = 0;
    m->solve_primal_iters = 0;
    m->solve_phase1_iters = 0;
    m->solve_time = 0.0;
}

/* Bounds may be infinite but never NaN, and `lower > upper` is a model with
 * no feasible point rather than a call to refuse — exactly the rule
 * jaos_load_lp applies. */
static bool bound_pair_ok(double lower, double upper)
{
    return !isnan(lower) && !isnan(upper);
}

/* Reading the problem back. Out of range is refused rather than answered with
 * a default. */
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

/* The objective's sense and constant, read back and changed (D283). Every
 * consumer reads m->sense at the moment it needs the sign -- the simplex,
 * presolve, the checker, ranging and the verifier each compute their sigma
 * from it -- so nothing derived from the sense is cached and the setter has
 * nothing to invalidate beyond the answer. The same holds for the constant:
 * jaos_objective adds m->obj_offset when it is asked. */
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

/* Reading the matrix back (D283). A column is a slice of the stored copy. A
 * row is a slice of the row-wise mirror, which is built here if a matrix
 * change discarded it -- the same build the next solve would do, so the
 * cost is moved and not added. The copies are guarded on their length
 * because memcpy of zero bytes from a null source is undefined, and an
 * empty column or row is exactly the case with nothing to copy. */
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
    /* The column is sorted by row with no duplicates, so a binary search
     * either lands on the entry or proves its absence. */
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

/* Changing one entry of the matrix (D67).
 *
 * The stored copy holds an invariant the readers and the solver both rely
 * on: within a column, entries ascend by row index, with no duplicates and
 * no explicit zeros. So this is three operations — replace where the entry
 * exists, delete when the new value is zero, insert in sorted position where
 * it does not.
 *
 * Unlike a bound or a cost, this invalidates both derived copies. */
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

/* One line out. The buffer is a local: nothing in the library may hold a
 * pointer to it afterwards. */
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
double  jaos_solve_time(const jaos_model *m) { return m ? m->solve_time : 0.0; }

jaos_status jaos_solution(const jaos_model *m, double *col_value,
    double *row_activity, double *row_dual, double *col_dual)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* Same rule as jaos_objective: a solve that found no optimum has no
     * solution to hand out. */
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

jaos_status jaos_certificate(const jaos_model *m, double *row_ray)
{
    if (m == nullptr || row_ray == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    /* The flag is the availability: the simplex's own refusal on this
     * model's rows set it, or the postsolve did after lifting a reduced
     * solve's or a presolve site's ray back into them (D254, D256). An
     * inverted box, which has no ray, leaves it false. */
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
    /* Same availability discipline as jaos_certificate: a ray proof on
     * this model's own columns, or one lifted into them by the postsolve,
     * set the flag (D255, D256). */
    if (m->solve_status != JAOS_SOLVE_UNBOUNDED || !m->ray_ok ||
        m->sol_ray == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    memcpy(col_ray, m->sol_ray, (size_t)m->num_col * sizeof *col_ray);
    return JAOS_OK;
}

/* Both start arrays are allocated together and freed together, so
 * `start_col_status != nullptr` is the whole test of whether a starting basis
 * exists. Nothing enforced it until D219: `basis_extend` grows one array at a
 * time, and the pair survived a failure there only because its error path
 * calls `jaos_clear_basis`, which clears both. This is that invariant, at
 * every function that reads either array as a claim about the other. */
#define JM_BASIS_PAIRED(m) \
    (((m)->start_col_status == nullptr) == ((m)->start_row_status == nullptr))

/* Puts a basis where the next solve will find it. Allocates both arrays or
 * neither, so `start_col_status != nullptr` is the whole test of whether a
 * starting basis exists. */
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
    assert(JM_BASIS_PAIRED(m));
    if (m->sol_col_status == nullptr || m->sol_row_status == nullptr)
        return JAOS_OK;   /* nothing to remember; not a failure */
    return store_basis(m, m->sol_col_status, m->sol_row_status);
}

/* One Neumaier step into a running sum and its compensation. Declared in
 * jaos_internal.h rather than kept here, because `settled_objective` in the
 * simplex needs the same step over the same kind of sum (D175). */
void jm_obj_add(double *sum, double *comp, double t)
{
    /* Aliased, the compensation is silently wrong: the correction would be
     * added into the total it is correcting. Not `restrict` (D75, D76). */
    assert(sum != comp);
    const double a = *sum, u = a + t;
    *comp += (fabs(a) >= fabs(t)) ? ((a - u) + t) : ((t - u) + a);
    *sum = u;
}

/* What `a * b` lost when it rounded to a double, by Dekker's split (D172).
 *
 * Dekker's split rather than `fma`. The split is preferred because it needs
 * no claim about libm at all (D169, D34).
 *
 * What protects the split is `-fno-associative-math`, not
 * `-ffp-contract=off`. What would delete `ca - (ca - a)` is reassociation,
 * and only `-ffast-math` and `-Ofast` enable it; the Makefile uses neither.
 *
 * The precondition is `FLT_EVAL_METHOD == 0`, asserted in `jaos_internal.h`
 * beside the declaration (D175). At 2, `ca - (ca - a)` evaluated at 80 bits
 * returns exactly `a`, so `al` is zero and the residue is wrong.
 *
 * Two ends where the residue is not available, and both return zero. A
 * product at the top of the range: the guard on the factors is not enough on
 * its own, because Veltkamp's split rounds the high part and `|ah|` can exceed
 * `|a|`, so `ah * bh` overflows while `p` itself is finite. Testing the result
 * covers every overflow route. And a subnormal product, where the true residue
 * is below 2^-1074 and cannot be represented at all, so zero is the best
 * available. */
double jm_two_product_residue(double a, double b, double p)
{
    constexpr double SPLIT = 134217729.0;   /* 2^27 + 1 */
    constexpr double BIG   = 0x1p996;       /* written exactly, not decimal */
    if (!isfinite(p) || fabs(a) > BIG || fabs(b) > BIG)
        return 0.0;
    const double ca = SPLIT * a, ah = ca - (ca - a), al = a - ah;
    const double cb = SPLIT * b, bh = cb - (cb - b), bl = b - bh;
    const double e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    return isfinite(e) ? e : 0.0;
}

/* The objective of the solution the model is holding, computed from that
 * solution and from nothing else (D169). The sum is compensated (D165, D168);
 * `long double` is not available here (D34).
 *
 * Requires the six solution arrays and an OPTIMAL solve; callers reach it
 * only there. */
void jm_model_publish_objective(jaos_model *m)
{
    /* The precondition (D169). Without it a caller reaching here before the
     * solution arrays exist gets the bare `obj_offset` published beside an
     * OPTIMAL status. The null test stays, because a model with no columns
     * has nothing to allocate.
     *
     * The sentence above says "the six solution arrays and an OPTIMAL
     * solve"; until D219 one array was checked and the status was not.
     * All three callers set it before they reach here: `publish` at the top
     * and on the OPTIMAL branch only, `jm_postsolve_expand` after its own
     * non-OPTIMAL return, `jm_postsolve_solved` two statements earlier. */
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
    /* An infinite or NaN partial sum carries no residue a correction could
     * hold, and `inf + (inf - inf)` is a NaN. The guard is D165's. */
    m->objective = (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

static bool status_in_range(jaos_basis_status s)
{
    return s == JAOS_BASIS_BASIC || s == JAOS_BASIS_AT_LOWER ||
           s == JAOS_BASIS_AT_UPPER || s == JAOS_BASIS_FREE;
}

/* What this checks, and what it leaves to the solve.
 *
 * Checked here: that the values are statuses at all, and that exactly num_row
 * of them are basic.
 *
 * Not checked here: whether a nonbasic's status names a bound the variable
 * actually has, or whether the columns are linearly independent. Both depend
 * on numbers that move underneath a stored basis, so the solve has to cope
 * with both anyway (see build_initial_basis and repair_singular_basis). */
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

    /* Configuration is not problem data, and model_release_arrays is where
     * that is enforced. */
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

/* One rule for what a dimension change does to a stored basis: a model with
 * n rows needs n basic variables. That is exactly what jaos_set_basis
 * enforces on a basis handed in.
 *
 * The adds are built to keep it: a new row arrives basic, and a new column
 * arrives nonbasic. The deletes usually break it, and are meant to. A basis
 * that no longer counts goes here rather than being left for
 * build_warm_basis to reject. */
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

/* A set of indices to delete, checked as a set: in range, and each named
 * once. A repeated index is refused rather than absorbed.
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
 * bound has nowhere to rest — see jaos_add_cols. */
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
 * correct. */
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

    /* The names of the columns already here stay; the new ones arrive
     * unnamed. Grown before anything is swapped, so a failure leaves the
     * model as it was. */
    if (names_grow(&m->col_name, m->num_col, num_new) != JAOS_OK) {
        free(arriving);
        free(cost); free(cl); free(cu); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
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

    /* The new rows arrive across and the matrix is stored down, so this is a
     * transpose of the addition rather than an append. Counting per column
     * first turns that into one rebuild instead of one insertion per entry. */
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

    if (names_grow(&m->row_name, m->num_row, num_new) != JAOS_OK) {
        free(rl); free(ru); free(as); free(ai); free(av);
        return JAOS_ERR_OUT_OF_MEMORY;
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
     * in. */
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

    /* Deleting a row renumbers every row after it, and the matrix stores row
     * indices, so every surviving entry has to be rewritten. The map is built
     * once. */
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
    names_compact(m->row_name, keep, m->num_row);
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
