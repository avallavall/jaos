/* Sparse LU factorization of a basis, with Markowitz threshold pivoting.
 *
 * Gaussian elimination on a sparse matrix faces two demands that pull
 * against each other. Numerical stability wants the largest available
 * pivot; sparsity wants the pivot whose elimination creates the fewest new
 * nonzeros, because fill-in is what turns a sparse factorization dense.
 * Markowitz [6] resolves it by minimising the expected fill
 * (r_i - 1)(c_j - 1) among candidates within a factor of the largest
 * magnitude in their column — stability as a constraint, sparsity as the
 * objective [4][20].
 *
 * The elimination keeps the active submatrix in both orientations at once:
 * columns carry values, rows carry pattern only. Columns are what the
 * update rewrites; rows are what makes the Markowitz cost knowable without
 * scanning. Candidate columns come from per-count buckets, so a singleton
 * column — cost zero, the pivot every sparse factorization wants first —
 * is found immediately rather than searched for.
 *
 * Storage during elimination is one growable array per column. That is a
 * deliberate correctness-first choice: a single arena with compaction is
 * where this ends up, but that change belongs to M2 with a measurement
 * behind it (D17), not to a guess now.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* How many candidate columns the pivot search inspects before settling for
 * the best it has seen. Four is the classic compromise: enough to find a
 * good pivot, few enough that the search does not dominate. */
#define PIVOT_SEARCH_LIMIT 4

/* Below this a value is treated as structurally absent. */
#define DROP_TOL 1e-14

/* --------------------------------------------------------------------- */
/* Elimination workspace                                                 */
/* --------------------------------------------------------------------- */

typedef struct {
    int64_t *idx;
    double  *val;
    int64_t n, cap;
} vec;   /* active column: indices and values */

typedef struct {
    int64_t *idx;
    int64_t n, cap;
} pat;   /* active row: pattern only */

typedef struct {
    int64_t dim;

    vec *col;
    pat *row;
    int64_t *col_cnt;   /* live entries per column */
    int64_t *row_cnt;
    bool *col_done;
    bool *row_done;

    /* Doubly linked buckets of active columns by live count. */
    int64_t *bhead;     /* [dim + 1] */
    int64_t *bnext;     /* [dim] */
    int64_t *bprev;     /* [dim] */
    bool *in_bucket;

    /* Rank-1 update workspace: scatter values, remember what was touched
     * so the gather stays linear in the work actually done. */
    double *work;
    bool *work_set;
    int64_t *touched;

    /* Live rows of the current pivot column, with their multipliers. */
    int64_t *piv_row;
    double *piv_mult;
    int64_t piv_n;
} elim;

static bool vec_push(vec *v, int64_t i, double x)
{
    if (v->n == v->cap) {
        int64_t nc = v->cap < 8 ? 8 : v->cap * 2;
        int64_t *ni = realloc(v->idx, (size_t)nc * sizeof *ni);
        if (ni == nullptr)
            return false;
        v->idx = ni;
        double *nv = realloc(v->val, (size_t)nc * sizeof *nv);
        if (nv == nullptr)
            return false;
        v->val = nv;
        v->cap = nc;
    }
    v->idx[v->n] = i;
    v->val[v->n] = x;
    v->n++;
    return true;
}

static bool pat_push(pat *p, int64_t j)
{
    if (p->n == p->cap) {
        int64_t nc = p->cap < 8 ? 8 : p->cap * 2;
        int64_t *ni = realloc(p->idx, (size_t)nc * sizeof *ni);
        if (ni == nullptr)
            return false;
        p->idx = ni;
        p->cap = nc;
    }
    p->idx[p->n] = j;
    p->n++;
    return true;
}

static void elim_free(elim *e)
{
    if (e->col)
        for (int64_t j = 0; j < e->dim; j++) {
            free(e->col[j].idx);
            free(e->col[j].val);
        }
    if (e->row)
        for (int64_t i = 0; i < e->dim; i++)
            free(e->row[i].idx);
    free(e->col);
    free(e->row);
    free(e->col_cnt);
    free(e->row_cnt);
    free(e->col_done);
    free(e->row_done);
    free(e->bhead);
    free(e->bnext);
    free(e->bprev);
    free(e->in_bucket);
    free(e->work);
    free(e->work_set);
    free(e->touched);
    free(e->piv_row);
    free(e->piv_mult);
    memset(e, 0, sizeof *e);
}

static void bucket_remove(elim *e, int64_t j)
{
    if (!e->in_bucket[j])
        return;
    int64_t p = e->bprev[j], n = e->bnext[j];
    if (p >= 0)
        e->bnext[p] = n;
    else
        e->bhead[e->col_cnt[j]] = n;
    if (n >= 0)
        e->bprev[n] = p;
    e->in_bucket[j] = false;
}

static void bucket_insert(elim *e, int64_t j)
{
    int64_t c = e->col_cnt[j];
    e->bnext[j] = e->bhead[c];
    e->bprev[j] = -1;
    if (e->bhead[c] >= 0)
        e->bprev[e->bhead[c]] = j;
    e->bhead[c] = j;
    e->in_bucket[j] = true;
}

/* col_cnt[j] changed: move it to the right bucket. */
static void bucket_move(elim *e, int64_t j, int64_t new_cnt)
{
    bucket_remove(e, j);
    e->col_cnt[j] = new_cnt;
    if (!e->col_done[j])
        bucket_insert(e, j);
}

/* --------------------------------------------------------------------- */
/* Pivot search                                                          */
/* --------------------------------------------------------------------- */

static double col_max_abs(const elim *e, int64_t j)
{
    double mx = 0.0;
    const vec *v = &e->col[j];
    for (int64_t k = 0; k < v->n; k++) {
        if (e->row_done[v->idx[k]])
            continue;
        double a = fabs(v->val[k]);
        if (a > mx)
            mx = a;
    }
    return mx;
}

/* Returns false when no acceptable pivot exists, meaning the remaining
 * submatrix is structurally or numerically singular. */
static bool find_pivot(const elim *e, double tol, int64_t *pi, int64_t *pj,
                       double *pval)
{
    int64_t best_i = -1, best_j = -1, best_cost = -1;
    double best_val = 0.0;
    int examined = 0;

    for (int64_t cnt = 1; cnt <= e->dim; cnt++) {
        for (int64_t j = e->bhead[cnt]; j >= 0; j = e->bnext[j]) {
            double mx = col_max_abs(e, j);
            if (mx <= DROP_TOL)
                continue; /* numerically empty */

            const vec *v = &e->col[j];
            for (int64_t k = 0; k < v->n; k++) {
                int64_t i = v->idx[k];
                if (e->row_done[i])
                    continue;
                double a = fabs(v->val[k]);
                if (a < tol * mx)
                    continue;
                int64_t cost = (e->row_cnt[i] - 1) * (cnt - 1);
                /* Ties go to the larger pivot: same fill, better stability. */
                if (best_cost < 0 || cost < best_cost ||
                    (cost == best_cost && a > fabs(best_val))) {
                    best_cost = cost;
                    best_i = i;
                    best_j = j;
                    best_val = v->val[k];
                }
            }
            examined++;
            if (best_cost == 0)
                goto found;   /* nothing can beat zero fill */
            if (examined >= PIVOT_SEARCH_LIMIT && best_cost >= 0)
                goto found;
        }
        /* No column in a later bucket can produce a cost below (cnt-1)^2. */
        if (best_cost >= 0 && best_cost <= (cnt - 1) * (cnt - 1))
            goto found;
    }

found:
    if (best_cost < 0)
        return false;
    *pi = best_i;
    *pj = best_j;
    *pval = best_val;
    return true;
}

/* --------------------------------------------------------------------- */
/* Factorization                                                         */
/* --------------------------------------------------------------------- */

void jm_lu_init(jm_lu *lu)
{
    memset(lu, 0, sizeof *lu);
}

void jm_lu_free(jm_lu *lu)
{
    free(lu->l_start); free(lu->l_index); free(lu->l_value);
    free(lu->u_start); free(lu->u_index); free(lu->u_value);
    free(lu->u_diag);
    free(lu->perm_row); free(lu->perm_col);
    free(lu->inv_row); free(lu->inv_col);
    free(lu->tmp);
    memset(lu, 0, sizeof *lu);
}

/* Appends to a growable (index, value) pair array. */
static bool push_pair(int64_t **idx, double **val, int64_t *n, int64_t *cap,
                      int64_t i, double x)
{
    if (*n == *cap) {
        int64_t nc = *cap < 64 ? 64 : *cap * 2;
        int64_t *ni = realloc(*idx, (size_t)nc * sizeof *ni);
        if (ni == nullptr)
            return false;
        *idx = ni;
        double *nv = realloc(*val, (size_t)nc * sizeof *nv);
        if (nv == nullptr)
            return false;
        *val = nv;
        *cap = nc;
    }
    (*idx)[*n] = i;
    (*val)[*n] = x;
    (*n)++;
    return true;
}

jaos_status jm_lu_factor(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w)
{
    if (lu == nullptr || dim < 0)
        return JAOS_ERR_INVALID_INPUT;
    if (dim > 0 && (start == nullptr || index == nullptr || value == nullptr))
        return JAOS_ERR_INVALID_INPUT;
    if (!(pivot_tol > 0.0 && pivot_tol <= 1.0))
        return JAOS_ERR_INVALID_INPUT;

    jm_work_add(w, JM_WORK_FACTOR);

    jm_lu_free(lu);
    lu->dim = dim;

    jaos_status st = JAOS_OK;
    elim e = {0};
    e.dim = dim;
    int64_t l_cap = 0, u_cap = 0, l_n = 0, u_n = 0;

    lu->l_start  = jm_alloc_array(dim + 1, sizeof(int64_t));
    lu->u_start  = jm_alloc_array(dim + 1, sizeof(int64_t));
    lu->u_diag   = jm_alloc_array(dim, sizeof(double));
    lu->perm_row = jm_alloc_array(dim, sizeof(int64_t));
    lu->perm_col = jm_alloc_array(dim, sizeof(int64_t));
    lu->inv_row  = jm_alloc_array(dim, sizeof(int64_t));
    lu->inv_col  = jm_alloc_array(dim, sizeof(int64_t));
    lu->tmp      = jm_alloc_array(dim, sizeof(double));

    e.col       = jm_calloc_array(dim, sizeof(vec));
    e.row       = jm_calloc_array(dim, sizeof(pat));
    e.col_cnt   = jm_calloc_array(dim, sizeof(int64_t));
    e.row_cnt   = jm_calloc_array(dim, sizeof(int64_t));
    e.col_done  = jm_calloc_array(dim, sizeof(bool));
    e.row_done  = jm_calloc_array(dim, sizeof(bool));
    e.bhead     = jm_alloc_array(dim + 1, sizeof(int64_t));
    e.bnext     = jm_alloc_array(dim, sizeof(int64_t));
    e.bprev     = jm_alloc_array(dim, sizeof(int64_t));
    e.in_bucket = jm_calloc_array(dim, sizeof(bool));
    e.work      = jm_calloc_array(dim, sizeof(double));
    e.work_set  = jm_calloc_array(dim, sizeof(bool));
    e.touched   = jm_alloc_array(dim, sizeof(int64_t));
    e.piv_row   = jm_alloc_array(dim, sizeof(int64_t));
    e.piv_mult  = jm_alloc_array(dim, sizeof(double));

    if (!lu->l_start || !lu->u_start || !lu->u_diag || !lu->perm_row ||
        !lu->perm_col || !lu->inv_row || !lu->inv_col || !lu->tmp ||
        !e.col || !e.row || !e.col_cnt || !e.row_cnt || !e.col_done ||
        !e.row_done || !e.bhead || !e.bnext || !e.bprev || !e.in_bucket ||
        !e.work || !e.work_set || !e.touched || !e.piv_row || !e.piv_mult) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }

    for (int64_t i = 0; i < dim; i++) {
        lu->inv_row[i] = -1;
        lu->inv_col[i] = -1;
    }
    for (int64_t c = 0; c <= dim; c++)
        e.bhead[c] = -1;

    for (int64_t j = 0; j < dim; j++) {
        for (int64_t k = start[j]; k < start[j + 1]; k++) {
            double v = value[k];
            if (fabs(v) <= DROP_TOL)
                continue;
            int64_t i = index[k];
            if (i < 0 || i >= dim) {
                st = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            if (!vec_push(&e.col[j], i, v) || !pat_push(&e.row[i], j)) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }
            e.col_cnt[j]++;
            e.row_cnt[i]++;
        }
    }
    for (int64_t j = 0; j < dim; j++)
        bucket_insert(&e, j);

    for (int64_t step = 0; step < dim; step++) {
        int64_t pi, pj;
        double pv;
        if (!find_pivot(&e, pivot_tol, &pi, &pj, &pv))
            break;  /* singular; the rank is what we have */

        lu->perm_row[step] = pi;
        lu->perm_col[step] = pj;
        lu->inv_row[pi] = step;
        lu->inv_col[pj] = step;
        lu->u_diag[step] = pv;

        /* Multipliers for the live rows of the pivot column. */
        e.piv_n = 0;
        lu->l_start[step] = l_n;
        {
            const vec *pcol = &e.col[pj];
            for (int64_t k = 0; k < pcol->n; k++) {
                int64_t i = pcol->idx[k];
                if (i == pi || e.row_done[i])
                    continue;
                double mult = pcol->val[k] / pv;
                if (fabs(mult) <= DROP_TOL)
                    continue;
                if (!push_pair(&lu->l_index, &lu->l_value, &l_n, &l_cap,
                               i, mult)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                e.piv_row[e.piv_n] = i;
                e.piv_mult[e.piv_n] = mult;
                e.piv_n++;
            }
        }
        lu->l_start[step + 1] = l_n;

        /* Retire pivot row and column, so counts describe what is left. */
        e.row_done[pi] = true;
        e.col_done[pj] = true;
        bucket_remove(&e, pj);
        for (int64_t k = 0; k < e.col[pj].n; k++) {
            int64_t i = e.col[pj].idx[k];
            if (!e.row_done[i])
                e.row_cnt[i]--;
        }
        for (int64_t k = 0; k < e.row[pi].n; k++) {
            int64_t j = e.row[pi].idx[k];
            if (!e.col_done[j])
                bucket_move(&e, j, e.col_cnt[j] - 1);
        }

        /* The pivot row becomes a row of U and drives the rank-1 update of
         * every column it touches. */
        lu->u_start[step] = u_n;
        for (int64_t rk = 0; rk < e.row[pi].n; rk++) {
            int64_t j = e.row[pi].idx[rk];
            if (e.col_done[j])
                continue;

            vec *cv = &e.col[j];
            double urow = 0.0;
            bool found = false;
            for (int64_t k = 0; k < cv->n; k++) {
                if (cv->idx[k] == pi) {
                    urow = cv->val[k];
                    found = true;
                    break;
                }
            }
            if (!found || fabs(urow) <= DROP_TOL)
                continue;  /* cancelled earlier; nothing to eliminate */

            if (!push_pair(&lu->u_index, &lu->u_value, &u_n, &u_cap, j, urow)) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }

            /* Scatter the live part of column j. */
            int64_t nt = 0;
            for (int64_t k = 0; k < cv->n; k++) {
                int64_t i = cv->idx[k];
                if (e.row_done[i])
                    continue;
                e.work[i] = cv->val[k];
                e.work_set[i] = true;
                e.touched[nt++] = i;
            }

            /* column_j -= urow * multipliers. */
            for (int64_t k = 0; k < e.piv_n; k++) {
                int64_t i = e.piv_row[k];
                double delta = e.piv_mult[k] * urow;
                if (e.work_set[i]) {
                    e.work[i] -= delta;
                } else {
                    e.work[i] = -delta;      /* fill-in */
                    e.work_set[i] = true;
                    e.touched[nt++] = i;
                    if (!pat_push(&e.row[i], j)) {
                        st = JAOS_ERR_OUT_OF_MEMORY;
                        goto done;
                    }
                    e.row_cnt[i]++;
                }
                jm_work_add(w, JM_WORK_ELIMINATED);
            }

            /* Gather, dropping whatever the update annihilated. */
            cv->n = 0;
            int64_t live = 0;
            for (int64_t k = 0; k < nt; k++) {
                int64_t i = e.touched[k];
                double v = e.work[i];
                e.work[i] = 0.0;
                e.work_set[i] = false;
                if (fabs(v) <= DROP_TOL) {
                    e.row_cnt[i]--;          /* exact cancellation */
                    continue;
                }
                if (!vec_push(cv, i, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                live++;
            }
            bucket_move(&e, j, live);
        }
        lu->u_start[step + 1] = u_n;
        lu->rank = step + 1;
    }

    /* Keep the tail well formed when the matrix turned out singular. */
    for (int64_t step = lu->rank; step < dim; step++) {
        lu->l_start[step] = l_n;
        lu->l_start[step + 1] = l_n;
        lu->u_start[step] = u_n;
        lu->u_start[step + 1] = u_n;
        lu->u_diag[step] = 0.0;
        lu->perm_row[step] = -1;
        lu->perm_col[step] = -1;
    }

    /* Renumber into pivot space: every row an eta touches is pivoted after
     * its own step, and likewise for U's columns, so the map is total on
     * what was stored. Only meaningful for a full-rank factorization. */
    if (lu->rank == dim) {
        for (int64_t k = 0; k < l_n; k++)
            lu->l_index[k] = lu->inv_row[lu->l_index[k]];
        for (int64_t k = 0; k < u_n; k++)
            lu->u_index[k] = lu->inv_col[lu->u_index[k]];
    }

done:
    elim_free(&e);
    if (st != JAOS_OK)
        jm_lu_free(lu);
    return st;
}

/* --------------------------------------------------------------------- */
/* Triangular solves                                                     */
/* --------------------------------------------------------------------- */

void jm_lu_ftran(const jm_lu *lu, double *x, jm_work *w)
{
    const int64_t n = lu->dim;
    double *y = lu->tmp;

    for (int64_t k = 0; k < n; k++)
        y[k] = x[lu->perm_row[k]];

    /* L y = Pb, forward: L by columns, so each step scatters. */
    for (int64_t k = 0; k < n; k++) {
        double yk = y[k];
        if (yk == 0.0)
            continue;
        for (int64_t p = lu->l_start[k]; p < lu->l_start[k + 1]; p++)
            y[lu->l_index[p]] -= lu->l_value[p] * yk;
        jm_work_add(w, (lu->l_start[k + 1] - lu->l_start[k]) * JM_WORK_NONZERO);
    }

    /* U z = y, backward: U by rows, so each step is a dot product. */
    for (int64_t k = n - 1; k >= 0; k--) {
        double s = y[k];
        for (int64_t p = lu->u_start[k]; p < lu->u_start[k + 1]; p++)
            s -= lu->u_value[p] * y[lu->u_index[p]];
        y[k] = s / lu->u_diag[k];
        jm_work_add(w, (lu->u_start[k + 1] - lu->u_start[k]) * JM_WORK_NONZERO);
    }

    for (int64_t k = 0; k < n; k++)
        x[lu->perm_col[k]] = y[k];
}

void jm_lu_btran(const jm_lu *lu, double *x, jm_work *w)
{
    const int64_t n = lu->dim;
    double *y = lu->tmp;

    /* B' = Q U' L' P, so this starts from the column permutation. */
    for (int64_t k = 0; k < n; k++)
        y[k] = x[lu->perm_col[k]];

    /* U' v = y, forward: U stored by rows is U' by columns, a scatter. */
    for (int64_t k = 0; k < n; k++) {
        double vk = y[k] / lu->u_diag[k];
        y[k] = vk;
        if (vk == 0.0)
            continue;
        for (int64_t p = lu->u_start[k]; p < lu->u_start[k + 1]; p++)
            y[lu->u_index[p]] -= lu->u_value[p] * vk;
        jm_work_add(w, (lu->u_start[k + 1] - lu->u_start[k]) * JM_WORK_NONZERO);
    }

    /* L' u = v, backward: L stored by columns is L' by rows, a dot
     * product. L is unit triangular, so there is no division. */
    for (int64_t k = n - 1; k >= 0; k--) {
        double s = y[k];
        for (int64_t p = lu->l_start[k]; p < lu->l_start[k + 1]; p++)
            s -= lu->l_value[p] * y[lu->l_index[p]];
        y[k] = s;
        jm_work_add(w, (lu->l_start[k + 1] - lu->l_start[k]) * JM_WORK_NONZERO);
    }

    for (int64_t k = 0; k < n; k++)
        x[lu->perm_row[k]] = y[k];
}
