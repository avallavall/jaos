/* Sparse LU factorization of a basis, with Markowitz threshold pivoting
 * and Forrest-Tomlin updates.
 *
 * Sparse Gaussian elimination serves two masters that pull apart.
 * Stability wants the largest available pivot; sparsity wants the pivot
 * creating the least fill-in, and fill-in is what turns a sparse
 * factorization dense. Markowitz [6] settles it by minimising the expected
 * fill (r_i - 1)(c_j - 1) among candidates within a factor of the largest
 * magnitude in their column — stability as a constraint, sparsity as the
 * objective [4][20].
 *
 * The elimination keeps the active submatrix in both orientations at once:
 * columns carry values because the rank-1 update rewrites them, rows carry
 * pattern only because that is what makes the Markowitz cost knowable
 * without scanning. Candidate columns come from per-count buckets, so a
 * singleton column — cost zero, the pivot any sparse factorization wants
 * first — is found rather than searched for.
 *
 * Storage during elimination is one growable array per column, and the
 * finished U keeps both orientations. Both are correctness-first choices:
 * a single arena with compaction is where this ends up, but that change
 * belongs to M2 with a measurement behind it (D17).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Candidate columns inspected before the pivot search settles for the best
 * it has seen. Four is the classic compromise. */
constexpr int PIVOT_SEARCH_LIMIT = 4;

/* A value below this fraction of the matrix's largest magnitude is treated
 * as structurally absent. Relative, because an absolute floor would call a
 * uniformly small basis singular — which is a different thing entirely. */
constexpr double DROP_REL = 1e-14;

/* Absolute floor used where no scale is available to compare against. */
constexpr double TINY = 1e-300;

/* --------------------------------------------------------------------- */
/* Sparse vectors                                                        */
/* --------------------------------------------------------------------- */

void jm_svec_free(jm_svec *v)
{
    free(v->idx);
    free(v->val);
    memset(v, 0, sizeof *v);
}

/* Grows a parallel (index, value) pair of arrays. Everything in JAOS that
 * grows an array goes through jm_grow, so the overflow check alloc.c
 * promises is not something each call site gets to skip. The two-step
 * dance matters: jm_grow leaves the pointer untouched when it fails, so a
 * failure on the second array still leaves the first one freeable. */
static bool grow_pair(int64_t **idx, double **val, int64_t *cap, int64_t need)
{
    int64_t cap_idx = *cap;
    int64_t cap_val = *cap;
    if (!jm_grow((void **)idx, &cap_idx, need, sizeof **idx))
        return false;
    if (!jm_grow((void **)val, &cap_val, need, sizeof **val))
        return false;
    *cap = cap_idx < cap_val ? cap_idx : cap_val;
    return true;
}

bool jm_svec_push(jm_svec *v, int64_t i, double x)
{
    if (!grow_pair(&v->idx, &v->val, &v->cap, v->n + 1))
        return false;
    v->idx[v->n] = i;
    v->val[v->n] = x;
    v->n++;
    return true;
}

/* Removes index i by swapping the last entry into its place. Order is not
 * preserved, but it stays a deterministic function of the call history,
 * which is what D8 asks for.
 *
 * Known cost: this is a linear scan, and jm_lu_update calls it once per
 * entry of the outgoing slot's row and column, so detaching a slot holding
 * f nonzeros is O(f^2) — on the simplex's hottest path, and f grows with
 * every update because nothing compacts U between refactorizations. A
 * position map removes the inner scan. It is not done here because D17
 * says the change needs a measurement behind it, and there is nothing to
 * measure until the simplex exists; PLAN.md carries it as M2 work. */
void jm_svec_erase(jm_svec *v, int64_t i)
{
    for (int64_t k = 0; k < v->n; k++) {
        if (v->idx[k] == i) {
            v->idx[k] = v->idx[v->n - 1];
            v->val[k] = v->val[v->n - 1];
            v->n--;
            return;
        }
    }
}

/* --------------------------------------------------------------------- */
/* Elimination workspace                                                 */
/* --------------------------------------------------------------------- */

typedef struct {
    int64_t *idx;
    int64_t n, cap;
} pat;   /* active row: pattern only */

typedef struct {
    int64_t dim;

    jm_svec *col;
    pat *row;
    int64_t *col_cnt;
    int64_t *row_cnt;
    bool *col_done;
    bool *row_done;

    int64_t *bhead;     /* [dim + 1] buckets of active columns by count */
    int64_t *bnext;
    int64_t *bprev;
    bool *in_bucket;

    double *work;
    bool *work_set;
    int64_t *touched;

    int64_t *piv_row;   /* live rows of the pivot column ... */
    double *piv_mult;   /* ... and their multipliers */
    int64_t piv_n;

    /* Compacting the pivot row: `seen` stamps a column as already taken
     * this step, `rowval` caches the value found for it. */
    int64_t *seen;
    double *rowval;

    /* Structural drop threshold, relative to the largest magnitude in the
     * matrix. An absolute floor would declare a uniformly tiny basis
     * singular, which is a different thing from being singular. */
    double drop;
} elim;

static bool pat_push(pat *p, int64_t j)
{
    if (!JM_GROW(p->idx, p->cap, p->n + 1))
        return false;
    p->idx[p->n] = j;
    p->n++;
    return true;
}

static void elim_free(elim *e)
{
    if (e->col)
        for (int64_t j = 0; j < e->dim; j++)
            jm_svec_free(&e->col[j]);
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
    free(e->seen);
    free(e->rowval);
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

static void bucket_move(elim *e, int64_t j, int64_t new_cnt)
{
    bucket_remove(e, j);
    e->col_cnt[j] = new_cnt;
    if (!e->col_done[j])
        bucket_insert(e, j);
}

static double col_max_abs(const elim *e, int64_t j)
{
    double mx = 0.0;
    const jm_svec *v = &e->col[j];
    for (int64_t k = 0; k < v->n; k++) {
        if (e->row_done[v->idx[k]])
            continue;
        double a = fabs(v->val[k]);
        if (a > mx)
            mx = a;
    }
    return mx;
}

static bool find_pivot(const elim *e, double tol, int64_t *pi, int64_t *pj,
                       double *pval)
{
    int64_t best_i = -1, best_j = -1, best_cost = -1;
    double best_val = 0.0;
    int examined = 0;

    /* Counts start at zero: a column can legitimately reach zero live
     * entries and must still be visited, or a nonsingular matrix comes
     * back rank deficient. */
    for (int64_t cnt = 0; cnt <= e->dim; cnt++) {
        for (int64_t j = e->bhead[cnt]; j >= 0; j = e->bnext[j]) {
            double mx = col_max_abs(e, j);
            if (mx <= e->drop)
                continue;

            const jm_svec *v = &e->col[j];
            for (int64_t k = 0; k < v->n; k++) {
                int64_t i = v->idx[k];
                if (e->row_done[i])
                    continue;
                double a = fabs(v->val[k]);
                if (a < tol * mx)
                    continue;
                int64_t live = e->row_cnt[i] < 1 ? 1 : e->row_cnt[i];
                int64_t cost = (live - 1) * (cnt < 1 ? 0 : cnt - 1);
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
                goto found;
            if (examined >= PIVOT_SEARCH_LIMIT && best_cost >= 0)
                goto found;
        }
        if (best_cost >= 0 && cnt >= 1 &&
            best_cost <= (cnt - 1) * (cnt - 1))
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
/* Lifecycle                                                             */
/* --------------------------------------------------------------------- */

void jm_lu_init(jm_lu *lu)
{
    memset(lu, 0, sizeof *lu);
}

void jm_lu_free(jm_lu *lu)
{
    if (lu->urow)
        for (int64_t s = 0; s < lu->dim; s++)
            jm_svec_free(&lu->urow[s]);
    if (lu->ucol)
        for (int64_t s = 0; s < lu->dim; s++)
            jm_svec_free(&lu->ucol[s]);
    free(lu->urow);
    free(lu->ucol);
    free(lu->l_start); free(lu->l_index); free(lu->l_value);
    free(lu->u_diag);
    free(lu->ft_target); free(lu->ft_source); free(lu->ft_factor);
    free(lu->slot_at); free(lu->pos_of);
    free(lu->perm_row); free(lu->perm_col);
    free(lu->inv_row); free(lu->inv_col);
    free(lu->tmp);
    free(lu->spike);
    memset(lu, 0, sizeof *lu);
}

static bool push_pair(int64_t **idx, double **val, int64_t *n, int64_t *cap,
                      int64_t i, double x)
{
    if (!grow_pair(idx, val, cap, *n + 1))
        return false;
    (*idx)[*n] = i;
    (*val)[*n] = x;
    (*n)++;
    return true;
}

/* --------------------------------------------------------------------- */
/* Factorization                                                         */
/* --------------------------------------------------------------------- */

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

    /* Validate the structure before destroying what the caller already
     * has, so INVALID_INPUT keeps its meaning: nothing happened. */
    double mat_max = 0.0;
    for (int64_t j = 0; j < dim; j++) {
        if (start[j] > start[j + 1])
            return JAOS_ERR_INVALID_INPUT;
        for (int64_t k = start[j]; k < start[j + 1]; k++) {
            if (index[k] < 0 || index[k] >= dim)
                return JAOS_ERR_INVALID_INPUT;
            double a = fabs(value[k]);
            if (a > mat_max)
                mat_max = a;
        }
    }
    if (dim > 0 && start[0] != 0)
        return JAOS_ERR_INVALID_INPUT;

    jm_work_add(w, JM_WORK_FACTOR);

    jm_lu_free(lu);
    lu->dim = dim;

    jaos_status st = JAOS_OK;
    elim e = {0};
    e.dim = dim;
    /* Relative, so a uniformly small matrix is factored rather than
     * declared structurally empty. */
    e.drop = mat_max * DROP_REL;

    /* U is collected compactly first — row s spans [us_start[s],
     * us_start[s+1]) — then expanded into both orientations. */
    int64_t *us_start = jm_alloc_array(dim + 1, sizeof(int64_t));
    int64_t *us_index = nullptr;
    double *us_value = nullptr;
    int64_t l_cap = 0, u_cap = 0, l_n = 0, u_n = 0;

    lu->l_start  = jm_alloc_array(dim + 1, sizeof(int64_t));
    lu->u_diag   = jm_alloc_array(dim, sizeof(double));
    lu->urow     = jm_calloc_array(dim, sizeof(jm_svec));
    lu->ucol     = jm_calloc_array(dim, sizeof(jm_svec));
    lu->slot_at  = jm_alloc_array(dim, sizeof(int64_t));
    lu->pos_of   = jm_alloc_array(dim, sizeof(int64_t));
    lu->perm_row = jm_alloc_array(dim, sizeof(int64_t));
    lu->perm_col = jm_alloc_array(dim, sizeof(int64_t));
    lu->inv_row  = jm_alloc_array(dim, sizeof(int64_t));
    lu->inv_col  = jm_alloc_array(dim, sizeof(int64_t));
    lu->tmp      = jm_alloc_array(dim, sizeof(double));
    lu->spike    = jm_alloc_array(dim, sizeof(double));

    e.col       = jm_calloc_array(dim, sizeof(jm_svec));
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
    e.seen      = jm_calloc_array(dim, sizeof(int64_t));
    e.rowval    = jm_alloc_array(dim, sizeof(double));

    if (!us_start || !lu->l_start || !lu->u_diag || !lu->urow || !lu->ucol ||
        !lu->slot_at || !lu->pos_of || !lu->perm_row || !lu->perm_col ||
        !lu->inv_row || !lu->inv_col || !lu->tmp || !lu->spike ||
        !e.col || !e.row || !e.col_cnt || !e.row_cnt || !e.col_done ||
        !e.row_done || !e.bhead || !e.bnext || !e.bprev || !e.in_bucket ||
        !e.work || !e.work_set || !e.touched || !e.piv_row || !e.piv_mult ||
        !e.seen || !e.rowval) {
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
            if (fabs(v) <= e.drop)
                continue;
            int64_t i = index[k];
            if (!jm_svec_push(&e.col[j], i, v) || !pat_push(&e.row[i], j)) {
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

        e.piv_n = 0;
        lu->l_start[step] = l_n;
        {
            const jm_svec *pcol = &e.col[pj];
            for (int64_t k = 0; k < pcol->n; k++) {
                int64_t i = pcol->idx[k];
                if (i == pi || e.row_done[i])
                    continue;
                double mult = pcol->val[k] / pv;
                if (mult == 0.0)
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

        e.col_done[pj] = true;
        bucket_remove(&e, pj);

        /* Compact the pivot row's pattern before anything counts through
         * it. The pattern is append-only: an exact cancellation leaves a
         * stale column behind, and later fill-in appends that column a
         * second time. Counting once per pattern entry then decrements
         * col_cnt twice for one real entry, drifting it negative until a
         * bucket index goes out of range. Keep one entry per column that
         * genuinely still carries a live value in this row, caching that
         * value so the U loop below need not search for it again. */
        {
            const int64_t stamp = step + 1;
            int64_t keep = 0;
            for (int64_t k = 0; k < e.row[pi].n; k++) {
                int64_t j = e.row[pi].idx[k];
                if (e.col_done[j] || e.seen[j] == stamp)
                    continue;
                e.seen[j] = stamp;

                const jm_svec *cv = &e.col[j];
                double aij = 0.0;
                for (int64_t q = 0; q < cv->n; q++)
                    if (cv->idx[q] == pi) {
                        aij = cv->val[q];
                        break;
                    }
                if (aij == 0.0)
                    continue;

                e.row[pi].idx[keep] = j;
                e.rowval[keep] = aij;
                keep++;
            }
            e.row[pi].n = keep;
        }

        e.row_done[pi] = true;
        for (int64_t k = 0; k < e.col[pj].n; k++) {
            int64_t i = e.col[pj].idx[k];
            if (!e.row_done[i])
                e.row_cnt[i]--;
        }
        for (int64_t k = 0; k < e.row[pi].n; k++) {
            int64_t j = e.row[pi].idx[k];
            bucket_move(&e, j, e.col_cnt[j] - 1);
        }

        us_start[step] = u_n;
        for (int64_t rk = 0; rk < e.row[pi].n; rk++) {
            int64_t j = e.row[pi].idx[rk];
            jm_svec *cv = &e.col[j];
            double urow = e.rowval[rk];   /* found during compaction */

            if (!push_pair(&us_index, &us_value, &u_n, &u_cap, j, urow)) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }

            int64_t nt = 0;
            for (int64_t k = 0; k < cv->n; k++) {
                int64_t i = cv->idx[k];
                if (e.row_done[i])
                    continue;
                e.work[i] = cv->val[k];
                e.work_set[i] = true;
                e.touched[nt++] = i;
            }

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

            cv->n = 0;
            int64_t live = 0;
            for (int64_t k = 0; k < nt; k++) {
                int64_t i = e.touched[k];
                double v = e.work[i];
                e.work[i] = 0.0;
                e.work_set[i] = false;
                if (fabs(v) <= e.drop) {
                    e.row_cnt[i]--;          /* exact cancellation */
                    continue;
                }
                if (!jm_svec_push(cv, i, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                live++;
            }
            bucket_move(&e, j, live);
        }
        us_start[step + 1] = u_n;
        lu->rank = step + 1;
    }

    for (int64_t step = lu->rank; step < dim; step++) {
        lu->l_start[step] = l_n;
        lu->l_start[step + 1] = l_n;
        us_start[step] = u_n;
        us_start[step + 1] = u_n;
        lu->u_diag[step] = 0.0;
        lu->perm_row[step] = -1;
        lu->perm_col[step] = -1;
    }

    /* Slots start out in factorization order; updates move them. */
    for (int64_t s = 0; s < dim; s++) {
        lu->slot_at[s] = s;
        lu->pos_of[s] = s;
    }

    if (lu->rank == dim) {
        /* Renumber into slot space. Every row an eta touches is pivoted
         * after its own step, and likewise for U's columns, so the map is
         * total on what was stored. */
        for (int64_t k = 0; k < l_n; k++)
            lu->l_index[k] = lu->inv_row[lu->l_index[k]];
        for (int64_t s = 0; s < dim; s++) {
            for (int64_t k = us_start[s]; k < us_start[s + 1]; k++) {
                int64_t c = lu->inv_col[us_index[k]];
                double v = us_value[k];
                if (!jm_svec_push(&lu->urow[s], c, v) ||
                    !jm_svec_push(&lu->ucol[c], s, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
            }
        }
    }

done:
    free(us_start);
    free(us_index);
    free(us_value);
    elim_free(&e);
    if (st != JAOS_OK)
        jm_lu_free(lu);
    return st;
}

/* --------------------------------------------------------------------- */
/* Triangular solves                                                     */
/* --------------------------------------------------------------------- */

/* y = L^-1 P b, then the accumulated row transformations. Shared by FTRAN
 * and by the update, which needs exactly this prefix to form the spike. */
static void ftran_prefix(const jm_lu *lu, const double *b, double *y,
                         jm_work *w)  /* reads only; y is caller-owned */
{
    const int64_t n = lu->dim;

    for (int64_t s = 0; s < n; s++)
        y[s] = b[lu->perm_row[s]];

    /* L by columns, so each step scatters. */
    for (int64_t s = 0; s < n; s++) {
        double ys = y[s];
        if (ys == 0.0)
            continue;
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            y[lu->l_index[p]] -= lu->l_value[p] * ys;
        jm_work_add(w, (lu->l_start[s + 1] - lu->l_start[s]) * JM_WORK_NONZERO);
    }

    /* E = E_t ... E_1, applied in creation order. */
    for (int64_t k = 0; k < lu->ft_n; k++)
        y[lu->ft_target[k]] -= lu->ft_factor[k] * y[lu->ft_source[k]];
    jm_work_add(w, lu->ft_n * JM_WORK_NONZERO);
}

void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w)
{
    const int64_t n = lu->dim;
    double *y = lu->tmp;

    if (lu->rank != n)
        return;   /* singular, or wrecked by a failed update */

    ftran_prefix(lu, x, y, w);

    /* U z = y, backward in position order. U by column, so each step
     * scatters what it has just resolved. */
    for (int64_t k = n - 1; k >= 0; k--) {
        int64_t s = lu->slot_at[k];
        double z = y[s] / lu->u_diag[s];
        y[s] = z;
        if (z == 0.0)
            continue;
        const jm_svec *col = &lu->ucol[s];
        for (int64_t p = 0; p < col->n; p++)
            y[col->idx[p]] -= col->val[p] * z;
        jm_work_add(w, col->n * JM_WORK_NONZERO);
    }

    for (int64_t s = 0; s < n; s++)
        x[lu->perm_col[s]] = y[s];
}

void jm_lu_btran(jm_lu *lu, double *x, jm_work *w)
{
    const int64_t n = lu->dim;
    double *y = lu->tmp;

    if (lu->rank != n)
        return;   /* singular, or wrecked by a failed update */

    /* B' = Q U' E^-T L' P, so this starts from the column permutation. */
    for (int64_t s = 0; s < n; s++)
        y[s] = x[lu->perm_col[s]];

    /* U' v = y, forward in position order. U by column is U' by row, so
     * each step is a dot product over already-resolved slots. */
    for (int64_t k = 0; k < n; k++) {
        int64_t s = lu->slot_at[k];
        const jm_svec *col = &lu->ucol[s];
        double sum = y[s];
        for (int64_t p = 0; p < col->n; p++)
            sum -= col->val[p] * y[col->idx[p]];
        y[s] = sum / lu->u_diag[s];
        jm_work_add(w, col->n * JM_WORK_NONZERO);
    }

    /* E^T = E_1^T ... E_t^T: reverse order, and each transposed swaps the
     * roles of target and source. Getting this backwards produces
     * plausible residuals that are quietly wrong. */
    for (int64_t k = lu->ft_n - 1; k >= 0; k--)
        y[lu->ft_source[k]] -= lu->ft_factor[k] * y[lu->ft_target[k]];
    jm_work_add(w, lu->ft_n * JM_WORK_NONZERO);

    /* L' u = v, backward: L by columns is L' by rows, a dot product. L is
     * unit triangular, so there is no division. */
    for (int64_t s = n - 1; s >= 0; s--) {
        double sum = y[s];
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            sum -= lu->l_value[p] * y[lu->l_index[p]];
        y[s] = sum;
        jm_work_add(w, (lu->l_start[s + 1] - lu->l_start[s]) * JM_WORK_NONZERO);
    }

    for (int64_t s = 0; s < n; s++)
        x[lu->perm_row[s]] = y[s];
}

/* --------------------------------------------------------------------- */
/* Forrest-Tomlin update                                                 */
/* --------------------------------------------------------------------- */

/* Appends one row transformation. The three arrays grow together. */
static bool ft_push(jm_lu *lu, int64_t target, int64_t source, double factor)
{
    int64_t cap_src = lu->ft_cap;
    if (!grow_pair(&lu->ft_target, &lu->ft_factor, &lu->ft_cap, lu->ft_n + 1) ||
        !jm_grow((void **)&lu->ft_source, &cap_src, lu->ft_n + 1,
                 sizeof *lu->ft_source))
        return false;
    if (cap_src < lu->ft_cap)
        lu->ft_cap = cap_src;
    lu->ft_target[lu->ft_n] = target;
    lu->ft_source[lu->ft_n] = source;
    lu->ft_factor[lu->ft_n] = factor;
    lu->ft_n++;
    return true;
}

jaos_status jm_lu_update(jm_lu *lu, int64_t col_out, const double *new_col,
                         double min_pivot_ratio, jm_work *w)
{
    if (lu == nullptr || new_col == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (lu->rank != lu->dim)
        return JAOS_ERR_INVALID_INPUT;
    if (col_out < 0 || col_out >= lu->dim)
        return JAOS_ERR_INVALID_INPUT;
    if (!(min_pivot_ratio > 0.0 && min_pivot_ratio <= 1.0))
        return JAOS_ERR_INVALID_INPUT;

    const int64_t n = lu->dim;
    const int64_t s_out = lu->inv_col[col_out];
    double *sp = lu->spike;
    double *row = lu->tmp;

    /* Three O(dim) passes follow whatever the elimination costs; charge
     * the floor so a work budget describes the run it actually buys. */
    jm_work_add(w, JM_WORK_UPDATE);

    /* The spike is the entering column seen through everything left of U. */
    ftran_prefix(lu, new_col, sp, w);

    double mx = 0.0;
    for (int64_t s = 0; s < n; s++) {
        double a = fabs(sp[s]);
        if (a > mx)
            mx = a;
    }
    /* Structural threshold for this update, scaled to the spike itself. */
    const double drop = mx > 0.0 ? mx * DROP_REL : TINY;

    const int64_t p = lu->pos_of[s_out];

    /* Take a dense copy of the outgoing slot's row, with the spike's own
     * diagonal entry standing in for the column being replaced. */
    for (int64_t s = 0; s < n; s++)
        row[s] = 0.0;
    for (int64_t k = 0; k < lu->urow[s_out].n; k++)
        row[lu->urow[s_out].idx[k]] = lu->urow[s_out].val[k];
    row[s_out] = sp[s_out];

    /* Detach the slot from both orientations. */
    for (int64_t k = 0; k < lu->urow[s_out].n; k++)
        jm_svec_erase(&lu->ucol[lu->urow[s_out].idx[k]], s_out);
    for (int64_t k = 0; k < lu->ucol[s_out].n; k++)
        jm_svec_erase(&lu->urow[lu->ucol[s_out].idx[k]], s_out);
    lu->urow[s_out].n = 0;
    lu->ucol[s_out].n = 0;

    /* Install the entering column. Once s_out moves to the end, every
     * off-diagonal entry of the spike sits above the diagonal, so the
     * column needs no elimination at all — only the row does. */
    for (int64_t s = 0; s < n; s++) {
        if (s == s_out || fabs(sp[s]) <= drop)
            continue;
        if (!jm_svec_push(&lu->ucol[s_out], s, sp[s]) ||
            !jm_svec_push(&lu->urow[s], s_out, sp[s])) {
            lu->rank = -1;   /* half-installed column: unusable */
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }

    /* Cyclic permutation: s_out to the end, everything after p shifts down
     * by one. This is why positions are indirect — O(dim), not O(nnz). */
    for (int64_t k = p; k < n - 1; k++) {
        lu->slot_at[k] = lu->slot_at[k + 1];
        lu->pos_of[lu->slot_at[k]] = k;
    }
    lu->slot_at[n - 1] = s_out;
    lu->pos_of[s_out] = n - 1;

    /* Eliminate the spike row, which now sits below the diagonal in the
     * last position. Each step may create entries further right, which
     * later steps then handle. */
    for (int64_t k = p; k < n - 1; k++) {
        int64_t s = lu->slot_at[k];
        if (fabs(row[s]) <= drop) {
            row[s] = 0.0;
            continue;
        }
        double factor = row[s] / lu->u_diag[s];
        row[s] = 0.0;
        const jm_svec *r = &lu->urow[s];
        for (int64_t q = 0; q < r->n; q++)
            row[r->idx[q]] -= factor * r->val[q];
        jm_work_add(w, r->n * JM_WORK_NONZERO);

        if (!ft_push(lu, s_out, s, factor)) {
            lu->rank = -1;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }

    double newdiag = row[s_out];
    row[s_out] = 0.0;
    if (fabs(newdiag) <= TINY || fabs(newdiag) < min_pivot_ratio * mx) {
        /* U has already been rewritten; there is no old factorization to
         * fall back to. Mark it unusable so a stale solve cannot happen. */
        lu->rank = -1;
        return JAOS_ERR_NUMERICAL;
    }

    lu->u_diag[s_out] = newdiag;
    lu->n_updates++;
    return JAOS_OK;
}
