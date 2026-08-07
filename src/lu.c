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
/* Density monitoring helpers (M2 component 1)                            */
/* --------------------------------------------------------------------- */

/* Updates the running density stats for the hyper-sparse gate. Called
 * at the end of each FTRAN or BTRAN after the result density is known. */
static void update_density_stats(jm_lu *lu, bool is_ftran, int64_t nnz_result)
{
    int64_t *calls = is_ftran ? &lu->ftran_calls : &lu->btran_calls;
    int64_t *dense = is_ftran ? &lu->ftran_dense : &lu->btran_dense;
    double  *ema   = is_ftran ? &lu->ftran_density_ema : &lu->btran_density_ema;
    bool    *hs    = is_ftran ? &lu->ftran_hyper_sparse : &lu->btran_hyper_sparse;

    double dens = (double)nnz_result / (double)lu->dim;
    if (*calls == 0) {
        *ema = dens;
    } else {
        /* EMA with alpha = 0.1, smoothing over ~10 calls. */
        *ema = 0.9 * *ema + 0.1 * dens;
    }
    (*calls)++;
    if (dens > lu->density_threshold)
        (*dense)++;
    *hs = *ema < lu->density_threshold;
}

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

    /* Mirrors jm_lu.drop for the duration of the elimination. */
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

/* Rewrites the pivot row's pattern down to one entry per column that
 * genuinely still carries a live value, caching each value found.
 *
 * The pattern is append-only, so it accumulates two kinds of lie: an exact
 * cancellation leaves a column behind that no longer has an entry here,
 * and later fill-in appends that same column a second time. Anything that
 * counts once per pattern entry would then decrement a column's live count
 * twice for one real entry, drifting it negative until a bucket index goes
 * out of range. Caching the values also spares the U loop a second search
 * for what this pass already found.
 *
 * `step` supplies a stamp that is unique per pivot, so duplicates are
 * detected without clearing anything between steps. */
static void compact_pivot_row(elim *e, int64_t pi, int64_t step)
{
    const int64_t stamp = step + 1;
    int64_t keep = 0;

    for (int64_t k = 0; k < e->row[pi].n; k++) {
        int64_t j = e->row[pi].idx[k];
        if (e->col_done[j] || e->seen[j] == stamp)
            continue;
        e->seen[j] = stamp;

        const jm_svec *cv = &e->col[j];
        double aij = 0.0;
        for (int64_t q = 0; q < cv->n; q++)
            if (cv->idx[q] == pi) {
                aij = cv->val[q];
                break;
            }
        if (aij == 0.0)
            continue;

        e->row[pi].idx[keep] = j;
        e->rowval[keep] = aij;
        keep++;
    }
    e->row[pi].n = keep;
}

/* --------------------------------------------------------------------- */
/* Lifecycle                                                             */
/* --------------------------------------------------------------------- */

void jm_lu_init(jm_lu *lu)
{
    memset(lu, 0, sizeof *lu);
    lu->density_threshold = 0.10;
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
    jm_svec_free(&lu->ft);
    free(lu->ft_source);
    free(lu->slot_at); free(lu->pos_of);
    free(lu->perm_row); free(lu->perm_col);
    free(lu->inv_col);
    free(lu->tmp);
    free(lu->spike);
    free(lu->reach_mark);
    free(lu->reach_work);
    memset(lu, 0, sizeof *lu);
}

/* Hands a jm_svec's storage over to a pair of raw arrays and empties the
 * vector, so accumulating into a jm_svec costs no copy at the end. */
static void svec_release(jm_svec *v, int64_t **idx, double **val)
{
    *idx = v->idx;
    *val = v->val;
    memset(v, 0, sizeof *v);
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

    /* Reset density monitoring stats for the new factorization. */
    lu->ftran_calls = lu->btran_calls = 0;
    lu->ftran_dense = lu->btran_dense = 0;
    lu->ftran_density_ema = lu->btran_density_ema = 0.0;
    lu->ftran_hyper_sparse = lu->btran_hyper_sparse = false;
    lu->density_threshold = 0.10;

    jaos_status st = JAOS_OK;
    elim e = {0};
    e.dim = dim;
    /* Relative, so a uniformly small matrix is factored rather than
     * declared structurally empty. Kept on the factorization so updates
     * measure against the same yardstick. */
    lu->drop = mat_max > 0.0 ? mat_max * DROP_REL : TINY;
    e.drop = lu->drop;

    /* L and U are accumulated into ordinary sparse vectors and their
     * storage handed to the factorization at the end, so no growth code
     * is written twice. U additionally needs its row boundaries while it
     * is being built — row s spans [us_start[s], us_start[s+1]) — before
     * it is expanded into both orientations. */
    jm_svec lacc = {0}, uacc = {0};
    int64_t *us_start = jm_alloc_array(dim + 1, sizeof(int64_t));

    /* Row -> slot, needed only to renumber L and U at the end. It is
     * scratch, not state: no caller looks up a slot by row. */
    int64_t *inv_row = jm_alloc_array(dim, sizeof(int64_t));

    lu->l_start  = jm_alloc_array(dim + 1, sizeof(int64_t));
    lu->u_diag   = jm_alloc_array(dim, sizeof(double));
    lu->urow     = jm_calloc_array(dim, sizeof(jm_svec));
    lu->ucol     = jm_calloc_array(dim, sizeof(jm_svec));
    lu->slot_at  = jm_alloc_array(dim, sizeof(int64_t));
    lu->pos_of   = jm_alloc_array(dim, sizeof(int64_t));
    lu->perm_row = jm_alloc_array(dim, sizeof(int64_t));
    lu->perm_col = jm_alloc_array(dim, sizeof(int64_t));
    lu->inv_col  = jm_alloc_array(dim, sizeof(int64_t));
    lu->tmp      = jm_alloc_array(dim, sizeof(double));
    lu->spike    = jm_alloc_array(dim, sizeof(double));
    lu->reach_mark = jm_calloc_array(dim, sizeof(int64_t));
    lu->reach_work = jm_alloc_array(2 * dim, sizeof(int64_t));
    lu->reach_stamp = 0;

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

    if (!us_start || !inv_row || !lu->l_start || !lu->u_diag || !lu->urow ||
        !lu->ucol || !lu->slot_at || !lu->pos_of || !lu->perm_row ||
        !lu->perm_col || !lu->inv_col || !lu->tmp || !lu->spike ||
        !lu->reach_mark || !lu->reach_work ||
        !e.col || !e.row || !e.col_cnt || !e.row_cnt || !e.col_done ||
        !e.row_done || !e.bhead || !e.bnext || !e.bprev || !e.in_bucket ||
        !e.work || !e.work_set || !e.touched || !e.piv_row || !e.piv_mult ||
        !e.seen || !e.rowval) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }

    for (int64_t i = 0; i < dim; i++) {
        inv_row[i] = -1;
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
        inv_row[pi] = step;
        lu->inv_col[pj] = step;
        lu->u_diag[step] = pv;

        e.piv_n = 0;
        lu->l_start[step] = lacc.n;
        {
            const jm_svec *pcol = &e.col[pj];
            for (int64_t k = 0; k < pcol->n; k++) {
                int64_t i = pcol->idx[k];
                if (i == pi || e.row_done[i])
                    continue;
                double mult = pcol->val[k] / pv;
                if (mult == 0.0)
                    continue;
                if (!jm_svec_push(&lacc, i, mult)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                e.piv_row[e.piv_n] = i;
                e.piv_mult[e.piv_n] = mult;
                e.piv_n++;
            }
        }
        lu->l_start[step + 1] = lacc.n;

        e.col_done[pj] = true;
        bucket_remove(&e, pj);

        compact_pivot_row(&e, pi, step);

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

        us_start[step] = uacc.n;
        for (int64_t rk = 0; rk < e.row[pi].n; rk++) {
            int64_t j = e.row[pi].idx[rk];
            jm_svec *cv = &e.col[j];
            double urow = e.rowval[rk];   /* found during compaction */

            if (!jm_svec_push(&uacc, j, urow)) {
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
        us_start[step + 1] = uacc.n;
        lu->rank = step + 1;
    }

    for (int64_t step = lu->rank; step < dim; step++) {
        lu->l_start[step] = lacc.n;
        lu->l_start[step + 1] = lacc.n;
        us_start[step] = uacc.n;
        us_start[step + 1] = uacc.n;
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
        for (int64_t k = 0; k < lacc.n; k++)
            lacc.idx[k] = inv_row[lacc.idx[k]];
        for (int64_t s = 0; s < dim; s++) {
            for (int64_t k = us_start[s]; k < us_start[s + 1]; k++) {
                int64_t c = lu->inv_col[uacc.idx[k]];
                double v = uacc.val[k];
                if (!jm_svec_push(&lu->urow[s], c, v) ||
                    !jm_svec_push(&lu->ucol[c], s, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
            }
        }
    }

    /* L keeps the accumulator's storage; U's staging copy has served its
     * purpose now that both orientations exist. */
    svec_release(&lacc, &lu->l_index, &lu->l_value);

done:
    free(us_start);
    free(inv_row);
    jm_svec_free(&lacc);   /* no-op after a successful release */
    jm_svec_free(&uacc);
    elim_free(&e);
    if (st != JAOS_OK)
        jm_lu_free(lu);
    return st;
}

/* --------------------------------------------------------------------- */
/* Triangular solves                                                     */
/* --------------------------------------------------------------------- */

/* DFS-based reach analysis for the column dependency graph of L.
 *
 * Starting from every column s where b[perm_row[s]] != 0, traverses the
 * implicit graph where column s has an edge to column i > s whenever
 * L[i][s] != 0. Returns the reachable set in postorder (reverse topological
 * order) in reach[0..nreach).
 *
 * A timestamp mark array avoids clearing between calls: mark[s] == stamp
 * means "already visited in this call". */
static int64_t reach_l(const jm_lu *lu, const double *b,
                        int64_t *mark, int64_t stamp,
                        int64_t *reach, int64_t *stack)
{
    const int64_t n = lu->dim;
    int64_t nreach = 0;

    for (int64_t s = 0; s < n; s++) {
        if (b[lu->perm_row[s]] == 0.0)
            continue;
        if (mark[s] == stamp)
            continue;

        /* Iterative DFS: push root, expand first unvisited child, backtrack
         * when none remain. Postorder is collected on backtrack. */
        int64_t sp = 0;
        stack[sp++] = s;
        mark[s] = stamp;

        while (sp > 0) {
            int64_t cur = stack[sp - 1];
            bool found = false;
            for (int64_t p = lu->l_start[cur]; p < lu->l_start[cur + 1]; p++) {
                int64_t i = lu->l_index[p];
                if (i > cur && mark[i] != stamp) {
                    mark[i] = stamp;
                    stack[sp++] = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                sp--;
                reach[nreach++] = cur;
            }
        }
    }
    return nreach;
}

/* Forward substitution L^{-1} using only the reachable columns.
 *
 * y has already been initialized to b[perm_row[:]]. This function scatters
 * from the reachable columns in reverse postorder (topological order), so
 * the result is numerically identical to processing all columns. */
static void solve_from_reach_l(const jm_lu *lu, double *y,
                                const int64_t *reach, int64_t nreach)
{
    for (int64_t k = nreach - 1; k >= 0; k--) {
        int64_t s = reach[k];
        double ys = y[s];
        if (ys == 0.0)
            continue;
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            y[lu->l_index[p]] -= lu->l_value[p] * ys;
    }
}

/* y = L^-1 P b, then the accumulated row transformations. Shared by FTRAN
 * and by the update, which needs exactly this prefix to form the spike.
 *
 * Uses the Gilbert-Peierls hyper-sparse path when the running density EMA
 * indicates the result is likely to be sparse. */
static void ftran_prefix(const jm_lu *lu, const double *b, double *y,
                          jm_work *w)
{
    const int64_t n = lu->dim;

    for (int64_t s = 0; s < n; s++)
        y[s] = b[lu->perm_row[s]];

    if (lu->ftran_hyper_sparse) {
        jm_lu *lw = (jm_lu *)lu;
        int64_t stamp = lw->reach_stamp + 1;
        if (stamp == 0) stamp = 1;
        int64_t *reach = lw->reach_work;
        int64_t *stack = lw->reach_work + n;
        int64_t nreach = reach_l(lu, b, lw->reach_mark, stamp,
                                  reach, stack);
        solve_from_reach_l(lu, y, reach, nreach);
        lw->reach_stamp = stamp;

        int64_t work = 0;
        for (int64_t k = 0; k < nreach; k++)
            work += lu->l_start[reach[k] + 1] - lu->l_start[reach[k]];
        jm_work_add(w, work * JM_WORK_NONZERO);
    } else {
        /* L by columns, so each step scatters. */
        for (int64_t s = 0; s < n; s++) {
            double ys = y[s];
            if (ys == 0.0)
                continue;
            for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
                y[lu->l_index[p]] -= lu->l_value[p] * ys;
            jm_work_add(w, (lu->l_start[s + 1] - lu->l_start[s]) * JM_WORK_NONZERO);
        }
    }

    /* E = E_t ... E_1, applied in creation order. */
    for (int64_t k = 0; k < lu->ft.n; k++)
        y[lu->ft.idx[k]] -= lu->ft.val[k] * y[lu->ft_source[k]];
    jm_work_add(w, lu->ft.n * JM_WORK_NONZERO);
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

    /* Count nonzeros for density monitoring. */
    int64_t ftran_nnz = 0;
    for (int64_t s = 0; s < n; s++)
        if (y[s] != 0.0) ftran_nnz++;
    update_density_stats(lu, true, ftran_nnz);

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
    for (int64_t k = lu->ft.n - 1; k >= 0; k--)
        y[lu->ft_source[k]] -= lu->ft.val[k] * y[lu->ft.idx[k]];
    jm_work_add(w, lu->ft.n * JM_WORK_NONZERO);

    /* L' u = v, backward: L by columns is L' by rows, a dot product. L is
     * unit triangular, so there is no division. */
    for (int64_t s = n - 1; s >= 0; s--) {
        double sum = y[s];
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            sum -= lu->l_value[p] * y[lu->l_index[p]];
        y[s] = sum;
        jm_work_add(w, (lu->l_start[s + 1] - lu->l_start[s]) * JM_WORK_NONZERO);
    }

    /* Count nonzeros for density monitoring. */
    int64_t btran_nnz = 0;
    for (int64_t s = 0; s < n; s++)
        if (y[s] != 0.0) btran_nnz++;
    update_density_stats(lu, false, btran_nnz);

    for (int64_t s = 0; s < n; s++)
        x[lu->perm_row[s]] = y[s];
}

/* --------------------------------------------------------------------- */
/* Density report (M2 component 1)                                        */
/* --------------------------------------------------------------------- */

void jm_lu_density_report(const jm_lu *lu, jm_lu_density_info *ftran,
                          jm_lu_density_info *btran)
{
    ftran->calls            = lu->ftran_calls;
    ftran->dense_calls      = lu->ftran_dense;
    ftran->running_density  = lu->ftran_density_ema;
    ftran->hyper_sparse     = lu->ftran_hyper_sparse;
    ftran->density_threshold = lu->density_threshold;

    btran->calls            = lu->btran_calls;
    btran->dense_calls      = lu->btran_dense;
    btran->running_density  = lu->btran_density_ema;
    btran->hyper_sparse     = lu->btran_hyper_sparse;
    btran->density_threshold = lu->density_threshold;
}

/* --------------------------------------------------------------------- */
/* Forrest-Tomlin update                                                 */
/* --------------------------------------------------------------------- */

/* Appends one row transformation. The three arrays grow together. */
static bool ft_push(jm_lu *lu, int64_t target, int64_t source, double factor)
{
    /* (target, factor) is an ordinary index/value pair, so jm_svec carries
     * it; source is one more parallel array, grown the same way pat_push
     * grows its own. Two mechanisms with one capacity each beats one
     * mechanism with three capacities to reconcile. */
    if (!jm_svec_push(&lu->ft, target, factor))
        return false;
    if (!JM_GROW(lu->ft_source, lu->ft_source_cap, lu->ft.n))
        return false;
    lu->ft_source[lu->ft.n - 1] = source;
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

    /* The spike's largest magnitude is what the new pivot is judged
     * against; the structural threshold stays the factorization's. */
    double mx = 0.0;
    for (int64_t s = 0; s < n; s++) {
        double a = fabs(sp[s]);
        if (a > mx)
            mx = a;
    }
    const double drop = lu->drop;

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
        /* Same elimination axpy as the factorization performs, so it costs
         * the same: the unit is defined by what the operation does, not by
         * which routine happens to be running it (D16). */
        jm_work_add(w, r->n * JM_WORK_ELIMINATED);

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
