/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr int PIVOT_SEARCH_LIMIT = 4;

constexpr double DROP_REL = 1e-14;

constexpr double TINY = 1e-300;

void jm_svec_free(jm_svec *v)
{
    free(v->idx);
    free(v->val);
    memset(v, 0, sizeof *v);
}

static bool grow_pair(int64_t **idx, double **val, int64_t *cap, int64_t need)
{
    int64_t cap_idx = *cap;
    int64_t cap_val = *cap;
    if (!jm_grow((void **)idx, &cap_idx, need, sizeof **idx))
        return false;
    if (!jm_grow((void **)val, &cap_val, need, sizeof **val))
        return false;
    *cap = cap_idx < cap_val ? cap_idx : cap_val;

    assert(*cap >= need);
    return true;
}

bool jm_svec_push(jm_svec *v, int64_t i, double x)
{
    if (v->n == v->cap && !grow_pair(&v->idx, &v->val, &v->cap, v->n + 1))
        return false;
    v->idx[v->n] = i;
    v->val[v->n] = x;
    v->n++;
    return true;
}

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

typedef struct {
    int64_t *idx;
    int64_t n, cap;
} pat;

typedef struct {
    int64_t dim;

    jm_svec *col;
    pat *row;
    int64_t *col_cnt;
    int64_t *row_cnt;
    bool *col_done;
    bool *row_done;

    int64_t *bhead;
    int64_t *bnext;
    int64_t *bprev;
    bool *in_bucket;

    int64_t *piv_row;
    double *piv_mult;
    int64_t piv_n;

    double *mult_of;
    bool *mult_set;
    bool *hit;

    int64_t *seen;
    double *rowval;

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
    free(e->mult_of);
    free(e->mult_set);
    free(e->hit);
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
    jm_svec_free(&lu->ft);
    free(lu->ft_source);
    free(lu->slot_at); free(lu->pos_of);
    free(lu->perm_row); free(lu->perm_col);
    free(lu->inv_col);
    free(lu->tmp);
    free(lu->spike);
    free(lu->mark);
    free(lu->dfs_node);
    free(lu->dfs_next);
    free(lu->pattern);
    free(lu->lrow_start);
    free(lu->lrow_index);
    memset(lu, 0, sizeof *lu);
}

static void svec_release(jm_svec *v, int64_t **idx, double **val)
{
    *idx = v->idx;
    *val = v->val;
    memset(v, 0, sizeof *v);
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

    lu->drop = mat_max > 0.0 ? mat_max * DROP_REL : TINY;
    e.drop = lu->drop;

    jm_svec lacc = {0}, uacc = {0};
    int64_t *us_start = jm_alloc_array(dim + 1, sizeof(int64_t));

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

    lu->mark     = jm_calloc_array(dim, sizeof(int64_t));
    lu->stamp    = 0;
    lu->dfs_node = jm_alloc_array(dim, sizeof(int64_t));
    lu->dfs_next = jm_alloc_array(dim, sizeof(int64_t));
    lu->pattern  = jm_alloc_array(dim, sizeof(int64_t));

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
    e.mult_of   = jm_calloc_array(dim, sizeof(double));
    e.mult_set  = jm_calloc_array(dim, sizeof(bool));
    e.hit       = jm_calloc_array(dim, sizeof(bool));
    e.piv_row   = jm_alloc_array(dim, sizeof(int64_t));
    e.piv_mult  = jm_alloc_array(dim, sizeof(double));
    e.seen      = jm_calloc_array(dim, sizeof(int64_t));
    e.rowval    = jm_alloc_array(dim, sizeof(double));

    if (!us_start || !inv_row || !lu->l_start || !lu->u_diag || !lu->urow ||
        !lu->ucol || !lu->slot_at || !lu->pos_of || !lu->perm_row ||
        !lu->perm_col || !lu->inv_col || !lu->tmp || !lu->spike ||
        !lu->mark || !lu->dfs_node || !lu->dfs_next || !lu->pattern ||
        !e.col || !e.row || !e.col_cnt || !e.row_cnt || !e.col_done ||
        !e.row_done || !e.bhead || !e.bnext || !e.bprev || !e.in_bucket ||
        !e.mult_of || !e.mult_set || !e.hit || !e.piv_row || !e.piv_mult ||
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
#ifndef NDEBUG

        for (int64_t i = 0; i < dim; i++)
            assert(!e.mult_set[i]);
#endif
        int64_t pi, pj;
        double pv;
        if (!find_pivot(&e, pivot_tol, &pi, &pj, &pv))
            break;

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

        for (int64_t k = 0; k < e.piv_n; k++) {
            e.mult_of[e.piv_row[k]] = e.piv_mult[k];
            e.mult_set[e.piv_row[k]] = true;
        }

        us_start[step] = uacc.n;
        for (int64_t rk = 0; rk < e.row[pi].n; rk++) {
            int64_t j = e.row[pi].idx[rk];
            jm_svec *cv = &e.col[j];
            double urow = e.rowval[rk];

            if (!jm_svec_push(&uacc, j, urow)) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }

            if (e.piv_n == 0) {
                int64_t keep = 0;
                for (int64_t k = 0; k < cv->n; k++) {
                    if (e.row_done[cv->idx[k]])
                        continue;
                    cv->idx[keep] = cv->idx[k];
                    cv->val[keep] = cv->val[k];
                    keep++;
                }
                cv->n = keep;
                bucket_move(&e, j, keep);
                continue;
            }

            int64_t found = 0, keep = 0;
            for (int64_t k = 0; k < cv->n; k++) {

                assert(keep <= k);
                int64_t i = cv->idx[k];
                if (e.row_done[i])
                    continue;
                double v = cv->val[k];
                if (e.mult_set[i]) {
                    v -= e.mult_of[i] * urow;
                    e.hit[i] = true;
                    found++;
                }
                if (fabs(v) <= e.drop) {
                    e.row_cnt[i]--;
                    continue;
                }
                cv->idx[keep] = i;
                cv->val[keep] = v;
                keep++;
            }

            if (found < e.piv_n && keep + e.piv_n - found > cv->cap &&
                !grow_pair(&cv->idx, &cv->val, &cv->cap,
                           keep + e.piv_n - found)) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }

            for (int64_t k = 0; k < e.piv_n; k++) {
                int64_t i = e.piv_row[k];
                if (e.hit[i]) {
                    e.hit[i] = false;
                    continue;
                }
                double v = -(e.piv_mult[k] * urow);
                if (!pat_push(&e.row[i], j)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                e.row_cnt[i]++;
                if (fabs(v) <= e.drop) {
                    e.row_cnt[i]--;
                    continue;
                }
                cv->idx[keep] = i;
                cv->val[keep] = v;
                keep++;
            }

            jm_work_add(w, e.piv_n * JM_WORK_ELIMINATED);

            cv->n = keep;
            bucket_move(&e, j, keep);
        }
        us_start[step + 1] = uacc.n;

        for (int64_t k = 0; k < e.piv_n; k++)
            e.mult_set[e.piv_row[k]] = false;

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

    for (int64_t s = 0; s < dim; s++) {
        lu->slot_at[s] = s;
        lu->pos_of[s] = s;
    }

    if (lu->rank == dim) {

        for (int64_t k = 0; k < lacc.n; k++) {
            assert(inv_row[lacc.idx[k]] >= 0);
            lacc.idx[k] = inv_row[lacc.idx[k]];
        }
        for (int64_t s = 0; s < dim; s++) {
            for (int64_t k = us_start[s]; k < us_start[s + 1]; k++) {
                int64_t c = lu->inv_col[uacc.idx[k]];
                assert(c >= 0);
                double v = uacc.val[k];
                if (!jm_svec_push(&lu->urow[s], c, v) ||
                    !jm_svec_push(&lu->ucol[c], s, v)) {
                    st = JAOS_ERR_OUT_OF_MEMORY;
                    goto done;
                }
            }
        }
    }

    svec_release(&lacc, &lu->l_index, &lu->l_value);

    if (lu->rank == dim && dim > 0) {
        const int64_t lnnz = lu->l_start[dim];
        lu->lrow_start = jm_calloc_array(dim + 1, sizeof(int64_t));
        lu->lrow_index = jm_alloc_array(lnnz > 0 ? lnnz : 1,
                                        sizeof(int64_t));
        if (lu->lrow_start == nullptr || lu->lrow_index == nullptr) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        for (int64_t p = 0; p < lnnz; p++)
            lu->lrow_start[lu->l_index[p] + 1]++;
        for (int64_t t = 0; t < dim; t++)
            lu->lrow_start[t + 1] += lu->lrow_start[t];
        for (int64_t s = 0; s < dim; s++)
            for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
                lu->lrow_index[lu->lrow_start[lu->l_index[p]]++] = s;

        for (int64_t t = dim; t > 0; t--)
            lu->lrow_start[t] = lu->lrow_start[t - 1];
        lu->lrow_start[0] = 0;
    }

done:
    free(us_start);
    free(inv_row);
    jm_svec_free(&lacc);
    jm_svec_free(&uacc);
    elim_free(&e);
    if (st != JAOS_OK)
        jm_lu_free(lu);
    return st;
}

static void ftran_prefix(const jm_lu *lu, const double *b, double *y,
                         jm_work *w)
{
    const int64_t n = lu->dim;

    for (int64_t s = 0; s < n; s++)
        y[s] = b[lu->perm_row[s]];

    for (int64_t s = 0; s < n; s++) {
        double ys = y[s];
        if (ys == 0.0)
            continue;
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            y[lu->l_index[p]] -= lu->l_value[p] * ys;
        jm_work_add(w, (lu->l_start[s + 1] - lu->l_start[s]) * JM_WORK_NONZERO);
    }

    for (int64_t k = 0; k < lu->ft.n; k++)
        y[lu->ft.idx[k]] -= lu->ft.val[k] * y[lu->ft_source[k]];
    jm_work_add(w, lu->ft.n * JM_WORK_NONZERO);
}

void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w)
{
    jm_lu_ftran_sparse(lu, x, w, nullptr, nullptr);
}

void jm_lu_ftran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat)
{
    const int64_t n = lu->dim;
    double *y = lu->tmp;

    if (npat != nullptr)
        *npat = 0;
    if (lu->rank != n)
        return;

    ftran_prefix(lu, x, y, w);

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

    if (pat == nullptr) {
        for (int64_t s = 0; s < n; s++)
            x[lu->perm_col[s]] = y[s];
        return;
    }
    int64_t k = 0;
    for (int64_t s = 0; s < n; s++) {
        const double v = y[s];
        const int64_t row = lu->perm_col[s];
        x[row] = v;
        if (v != 0.0)
            pat[k++] = row;
    }
    *npat = k;
}

static int64_t btran_u_pattern(jm_lu *lu, const double *y, jm_work *w)
{
    const int64_t n = lu->dim;
    int64_t top = n;
    int64_t edges = 0;

    lu->stamp++;

    assert(lu->stamp > 0);
    for (int64_t root = 0; root < n; root++) {
        if (y[root] == 0.0 || lu->mark[root] == lu->stamp)
            continue;

        lu->mark[root] = lu->stamp;
        lu->dfs_node[0] = root;
        lu->dfs_next[0] = 0;
        int64_t sp = 1;

        while (sp > 0) {
            const int64_t t = lu->dfs_node[sp - 1];
            const jm_svec *row = &lu->urow[t];
            int64_t i = lu->dfs_next[sp - 1];
            bool descended = false;

            while (i < row->n) {
                const int64_t c = row->idx[i];
                i++;
                edges++;
                if (lu->mark[c] != lu->stamp) {
                    lu->mark[c] = lu->stamp;
                    lu->dfs_next[sp - 1] = i;
                    lu->dfs_node[sp] = c;
                    lu->dfs_next[sp] = 0;
                    sp++;
                    descended = true;
                    break;
                }
            }
            if (!descended) {
                lu->dfs_next[sp - 1] = i;
                sp--;
                lu->pattern[--top] = t;
            }
        }
    }

    jm_work_add(w, edges * JM_WORK_NONZERO);

    assert(top >= 0);
    return top;
}

static int64_t btran_l_pattern(jm_lu *lu, const double *y, jm_work *w)
{
    const int64_t n = lu->dim;
    int64_t top = n;
    int64_t edges = 0;

    lu->stamp++;
    assert(lu->stamp > 0);
    for (int64_t root = 0; root < n; root++) {
        if (y[root] == 0.0 || lu->mark[root] == lu->stamp)
            continue;

        lu->mark[root] = lu->stamp;
        lu->dfs_node[0] = root;
        lu->dfs_next[0] = 0;
        int64_t sp = 1;

        while (sp > 0) {
            const int64_t t = lu->dfs_node[sp - 1];
            const int64_t re = lu->lrow_start[t + 1];
            int64_t p = lu->lrow_start[t] + lu->dfs_next[sp - 1];
            bool descended = false;

            while (p < re) {
                const int64_t c = lu->lrow_index[p];
                p++;
                edges++;
                if (lu->mark[c] != lu->stamp) {
                    lu->mark[c] = lu->stamp;
                    lu->dfs_next[sp - 1] = p - lu->lrow_start[t];
                    lu->dfs_node[sp] = c;
                    lu->dfs_next[sp] = 0;
                    sp++;
                    descended = true;
                    break;
                }
            }
            if (!descended) {
                lu->dfs_next[sp - 1] = p - lu->lrow_start[t];
                sp--;
                lu->pattern[--top] = t;
            }
        }
    }

    jm_work_add(w, edges * JM_WORK_NONZERO);
    assert(top >= 0);
    return top;
}

void jm_lu_btran(jm_lu *lu, double *x, jm_work *w)
{
    jm_lu_btran_sparse(lu, x, w, nullptr, nullptr);
}

void jm_lu_btran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat)
{
    const int64_t n = lu->dim;
    double *y = lu->tmp;

    if (npat != nullptr)
        *npat = 0;
    if (lu->rank != n)
        return;

    for (int64_t s = 0; s < n; s++)
        y[s] = x[lu->perm_col[s]];

    const int64_t first = btran_u_pattern(lu, y, w);
    for (int64_t k = first; k < n; k++) {
        int64_t s = lu->pattern[k];
        const jm_svec *col = &lu->ucol[s];
        double sum = y[s];
        for (int64_t p = 0; p < col->n; p++)
            sum -= col->val[p] * y[col->idx[p]];
        y[s] = sum / lu->u_diag[s];
        jm_work_add(w, col->n * JM_WORK_NONZERO);
    }

    for (int64_t k = lu->ft.n - 1; k >= 0; k--)
        y[lu->ft_source[k]] -= lu->ft.val[k] * y[lu->ft.idx[k]];
    jm_work_add(w, lu->ft.n * JM_WORK_NONZERO);

    const int64_t lfirst = btran_l_pattern(lu, y, w);
    for (int64_t k = lfirst; k < n; k++) {
        const int64_t s = lu->pattern[k];
        double sum = y[s];
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            sum -= lu->l_value[p] * y[lu->l_index[p]];
        y[s] = sum;
        jm_work_add(w, (lu->l_start[s + 1] - lu->l_start[s]) * JM_WORK_NONZERO);
    }

    if (pat == nullptr) {
        for (int64_t s = 0; s < n; s++)
            x[lu->perm_row[s]] = y[s];
        return;
    }
    int64_t k = 0;
    for (int64_t s = 0; s < n; s++) {
        const double v = y[s];
        const int64_t row = lu->perm_row[s];
        x[row] = v;
        if (v != 0.0)
            pat[k++] = row;
    }
    *npat = k;
}

static bool ft_push(jm_lu *lu, int64_t target, int64_t source, double factor)
{
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

    jm_work_add(w, JM_WORK_UPDATE);

    ftran_prefix(lu, new_col, sp, w);

    double mx = 0.0;
    for (int64_t s = 0; s < n; s++) {
        double a = fabs(sp[s]);
        if (a > mx)
            mx = a;
    }
    const double drop = lu->drop;

    const int64_t p = lu->pos_of[s_out];

    for (int64_t s = 0; s < n; s++)
        row[s] = 0.0;
    for (int64_t k = 0; k < lu->urow[s_out].n; k++)
        row[lu->urow[s_out].idx[k]] = lu->urow[s_out].val[k];
    row[s_out] = sp[s_out];

    for (int64_t k = 0; k < lu->urow[s_out].n; k++)
        jm_svec_erase(&lu->ucol[lu->urow[s_out].idx[k]], s_out);
    for (int64_t k = 0; k < lu->ucol[s_out].n; k++)
        jm_svec_erase(&lu->urow[lu->ucol[s_out].idx[k]], s_out);
    lu->urow[s_out].n = 0;
    lu->ucol[s_out].n = 0;

    for (int64_t s = 0; s < n; s++) {
        if (s == s_out || fabs(sp[s]) <= drop)
            continue;
        if (!jm_svec_push(&lu->ucol[s_out], s, sp[s]) ||
            !jm_svec_push(&lu->urow[s], s_out, sp[s])) {
            lu->rank = -1;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }

    for (int64_t k = p; k < n - 1; k++) {
        lu->slot_at[k] = lu->slot_at[k + 1];
        lu->pos_of[lu->slot_at[k]] = k;
    }
    lu->slot_at[n - 1] = s_out;
    lu->pos_of[s_out] = n - 1;
#ifndef NDEBUG

    for (int64_t k = 0; k < lu->ucol[s_out].n; k++)
        assert(lu->pos_of[lu->ucol[s_out].idx[k]] < n - 1);
#endif

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
        jm_work_add(w, r->n * JM_WORK_ELIMINATED);

        if (!ft_push(lu, s_out, s, factor)) {
            lu->rank = -1;
            return JAOS_ERR_OUT_OF_MEMORY;
        }
    }

    double newdiag = row[s_out];
    row[s_out] = 0.0;
    if (fabs(newdiag) <= TINY || fabs(newdiag) < min_pivot_ratio * mx) {

        lu->rank = -1;
        return JAOS_ERR_NUMERICAL;
    }

    lu->u_diag[s_out] = newdiag;
    lu->n_updates++;
    return JAOS_OK;
}
