/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include "jaos_sys.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double  BARRIER_TOL      = 1e-8;
constexpr double  BARRIER_TOL_QP   = 1e-10;
constexpr int64_t BARRIER_LEG2_ITERS = 50;
constexpr double  BARRIER_STEP     = 0.99995;
constexpr double  BARRIER_REG      = 1e-9;
constexpr double  BARRIER_FREE_REG = 1e-8;
constexpr double  BARRIER_DELTA    = 1e-10;
constexpr double  BARRIER_AUG_FLOOR = 1e-30;
constexpr double  BARRIER_START_MIN = 1e-6;
constexpr int64_t BARRIER_MAX_ITER = 200;
constexpr double  BARRIER_DIVERGE  = 1e6;
constexpr double  BARRIER_DIVERGE_QP = 1e10;
constexpr double  BARRIER_DENSE_FACTOR = 10.0;
constexpr int64_t BARRIER_DENSE_MIN    = 30;
constexpr int64_t BARRIER_DENSE_MAX    = 100;
constexpr int64_t BARRIER_STALL_ITERS = 5;
constexpr double  BARRIER_STALL_DROP  = 0.9;
constexpr double  BARRIER_REG_RETRY   = 1e-8;
constexpr double  BARRIER_REG_GROWTH  = 100.0;
constexpr double  BARRIER_REG_MAX     = 1e-2;
constexpr double  BARRIER_STALL_SIGMA  = 0.5;
constexpr double  BARRIER_STALL_DELTA  = 1e-3;
constexpr double  BARRIER_NEAR_TOL     = 1e-6;
constexpr double  QP_PUSH_TOL    = 1e-9;
constexpr double  QP_PUSH_REG    = 1e-6;
constexpr double  QP_PUSH_DENSE_THETA = 1e-30;
constexpr double  QP_PUSH_DELTA  = 1e-8;
constexpr int64_t QP_PUSH_REFINE = 8;
constexpr double  QP_PUSH_NEAR   = 1e-7;
constexpr double  QP_PUSH_USER_TOL = 1e-7;
constexpr int64_t QP_PUSH_ROUNDS = 40;
constexpr int64_t QP_PUSH_FREEINGS = 3;

enum { HAS_LO = JM_BX_LO, HAS_UP = JM_BX_UP, FIXED = JM_BX_FIXED };
enum { PUSH_FREE = 0, PUSH_LOWER = 1, PUSH_UPPER = 2 };

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    double *av, *arv;
    double *lo, *up, *cost, *quad;
    int64_t *qs, *qi;
    double  *qv, *qz;
    int64_t *qt_start, *qt_index;
    double  *qt_value;
    int64_t qnz;
    uint8_t *kind;
    bool quadratic;
    double norm_b, norm_c, norm_bound;
    double fixed_obj;

    double *z, *w, *v, *y, *zl, *zu;

    double *rp, *rw, *rv, *rd;
    double *rl, *ru, *rt, *theta;
    double *dz, *dw, *dv, *dy, *dzl, *dzu;
    double *dwa, *dva, *dzla, *dzua;
    double *rhs, *tmp, *b;

    int64_t *n_start, *n_index;
    double  *n_value;
    double  *acc;
    int64_t *mark;
    jm_chol chol;

    bool augmented;
    int64_t naug, nlive;
    int64_t *aug_start, *aug_index;
    double  *aug_value, *aug_rhs;
    int64_t *aug_of;
    int8_t  *aug_sign;
    double  *inv;
    jm_chol aug;

    jm_work work;
    double started;
    int64_t iters;
    int64_t bounded;
    bool handoff;
    double best_worst, reg_floor;
    int64_t stalled;
    bool equal_steps, near, pushed, delta_locked;
    double tol_stop;
    int64_t iter_cap;
    double delta;

    bool *dense;
    int64_t *dense_idx;
    int64_t ndense;
    double *wmat, *smat, *tvec, *tvec2;

    const int8_t *pin;
    bool *dec;
} bx;

static void bx_free(bx *s)
{
    free(s->av);    free(s->arv);
    free(s->lo);    free(s->up);    free(s->cost);  free(s->kind);
    free(s->quad);
    free(s->z);     free(s->w);     free(s->v);     free(s->y);
    free(s->zl);    free(s->zu);
    free(s->rp);    free(s->rw);    free(s->rv);    free(s->rd);
    free(s->rl);    free(s->ru);    free(s->rt);    free(s->theta);
    free(s->dz);    free(s->dw);    free(s->dv);    free(s->dy);
    free(s->dzl);   free(s->dzu);
    free(s->dwa);   free(s->dva);   free(s->dzla);  free(s->dzua);
    free(s->rhs);   free(s->tmp);   free(s->b);
    free(s->n_start); free(s->n_index); free(s->n_value);
    free(s->acc);   free(s->mark);
    free(s->dense); free(s->dense_idx);
    free(s->wmat);  free(s->smat);  free(s->tvec);  free(s->tvec2);
    free(s->qs); free(s->qi); free(s->qv); free(s->qz);
    free(s->qt_start); free(s->qt_index); free(s->qt_value);
    free(s->aug_start); free(s->aug_index); free(s->aug_value);
    free(s->aug_rhs);   free(s->aug_of);    free(s->aug_sign);
    free(s->inv);
    jm_chol_free(&s->chol);
    jm_chol_free(&s->aug);
    memset(s, 0, sizeof *s);
}

static jaos_status bx_init(bx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    s->best_worst = HUGE_VAL;
    s->delta = BARRIER_DELTA;
    s->tol_stop = BARRIER_TOL;
    s->iter_cap = BARRIER_MAX_ITER;
    jm_chol_init(&s->chol);
    jm_chol_init(&s->aug);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }
    {
        jaos_status st = jm_model_ensure_rowwise(m);
        if (st != JAOS_OK)
            return st;
    }

    const int64_t nv = s->nvar, nr = s->nrow;
    s->av    = jm_alloc_array(m->num_nz, sizeof(double));
    s->arv   = jm_alloc_array(m->num_nz, sizeof(double));
    s->lo    = jm_alloc_array(nv, sizeof(double));
    s->up    = jm_alloc_array(nv, sizeof(double));
    s->cost  = jm_calloc_array(nv, sizeof(double));
    s->quad  = jm_calloc_array(nv, sizeof(double));
    s->kind  = jm_calloc_array(nv, sizeof(uint8_t));
    s->z     = jm_calloc_array(nv, sizeof(double));
    s->w     = jm_calloc_array(nv, sizeof(double));
    s->v     = jm_calloc_array(nv, sizeof(double));
    s->y     = jm_calloc_array(nr, sizeof(double));
    s->zl    = jm_calloc_array(nv, sizeof(double));
    s->zu    = jm_calloc_array(nv, sizeof(double));
    s->rp    = jm_calloc_array(nr, sizeof(double));
    s->rw    = jm_calloc_array(nv, sizeof(double));
    s->rv    = jm_calloc_array(nv, sizeof(double));
    s->rd    = jm_calloc_array(nv, sizeof(double));
    s->rl    = jm_calloc_array(nv, sizeof(double));
    s->ru    = jm_calloc_array(nv, sizeof(double));
    s->rt    = jm_calloc_array(nv, sizeof(double));
    s->theta = jm_calloc_array(nv, sizeof(double));
    s->dz    = jm_calloc_array(nv, sizeof(double));
    s->dw    = jm_calloc_array(nv, sizeof(double));
    s->dv    = jm_calloc_array(nv, sizeof(double));
    s->dy    = jm_calloc_array(nr, sizeof(double));
    s->dzl   = jm_calloc_array(nv, sizeof(double));
    s->dzu   = jm_calloc_array(nv, sizeof(double));
    s->dwa   = jm_calloc_array(nv, sizeof(double));
    s->dva   = jm_calloc_array(nv, sizeof(double));
    s->dzla  = jm_calloc_array(nv, sizeof(double));
    s->dzua  = jm_calloc_array(nv, sizeof(double));
    s->rhs   = jm_calloc_array(nr, sizeof(double));
    s->tmp   = jm_calloc_array(nv, sizeof(double));
    s->b     = jm_calloc_array(nr, sizeof(double));
    s->acc   = jm_calloc_array(nr, sizeof(double));
    s->mark  = jm_alloc_array(nr, sizeof(int64_t));
    if (!s->av || !s->arv || !s->lo || !s->up || !s->cost || !s->quad ||
        !s->kind ||
        !s->z || !s->w || !s->v || !s->y || !s->zl || !s->zu || !s->rp ||
        !s->rw || !s->rv || !s->rd || !s->rl || !s->ru || !s->rt ||
        !s->theta || !s->dz || !s->dw || !s->dv || !s->dy || !s->dzl ||
        !s->dzu || !s->dwa || !s->dva || !s->dzla || !s->dzua || !s->rhs ||
        !s->tmp || !s->b || !s->acc || !s->mark) {
        bx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    const double *rho = m->row_scale, *gamma = m->col_scale;
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];
    for (int64_t i = 0; i < nr; i++)
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            s->arv[p] = rho[i] * m->ar_value[p] * gamma[m->ar_index[p]];

    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    for (int64_t j = 0; j < s->ncol; j++) {
        s->lo[j] = m->col_lower[j] / gamma[j];
        s->up[j] = m->col_upper[j] / gamma[j];
        s->cost[j] = sigma * m->col_cost[j] * gamma[j];
        if (m->col_quad != nullptr && m->col_quad[j] != 0.0) {
            s->quad[j] = sigma * m->col_quad[j] * gamma[j] * gamma[j];
            s->quadratic = true;
        }
    }
    if (m->q_nz > 0) {

        s->qs = jm_alloc_array(s->ncol + 1, sizeof *s->qs);
        s->qi = jm_alloc_array(m->q_nz, sizeof *s->qi);
        s->qv = jm_alloc_array(m->q_nz, sizeof *s->qv);
        s->qz = jm_calloc_array(nv, sizeof *s->qz);
        if (!s->qs || !s->qi || !s->qv || !s->qz) {
            bx_free(s);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t j = 0; j <= s->ncol; j++)
            s->qs[j] = m->q_start[j];
        for (int64_t j = 0; j < s->ncol; j++)
            for (int64_t p = m->q_start[j]; p < m->q_start[j + 1]; p++) {
                s->qi[p] = m->q_index[p];
                s->qv[p] = sigma * m->q_value[p] * gamma[m->q_index[p]] *
                           gamma[j];
            }
        s->qnz = m->q_nz;
        s->quadratic = true;

        s->qt_start = jm_calloc_array(s->ncol + 1, sizeof *s->qt_start);
        s->qt_index = jm_alloc_array(m->q_nz, sizeof *s->qt_index);
        s->qt_value = jm_alloc_array(m->q_nz, sizeof *s->qt_value);
        int64_t *fill = jm_calloc_array(s->ncol > 0 ? s->ncol : 1,
                                        sizeof *fill);
        if (!s->qt_start || !s->qt_index || !s->qt_value || !fill) {
            free(fill);
            bx_free(s);
            return JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t p = 0; p < m->q_nz; p++)
            s->qt_start[s->qi[p] + 1]++;
        for (int64_t j = 0; j < s->ncol; j++)
            s->qt_start[j + 1] += s->qt_start[j];
        for (int64_t j = 0; j < s->ncol; j++)
            for (int64_t p = s->qs[j]; p < s->qs[j + 1]; p++) {
                const int64_t i = s->qi[p];
                const int64_t at = s->qt_start[i] + fill[i]++;
                s->qt_index[at] = j;
                s->qt_value[at] = s->qv[p];
            }
        free(fill);
    }
    for (int64_t i = 0; i < nr; i++) {
        s->lo[s->ncol + i] = m->row_lower[i] * rho[i];
        s->up[s->ncol + i] = m->row_upper[i] * rho[i];
    }

    s->norm_c = 0.0;
    s->norm_bound = 0.0;
    s->bounded = 0;
    for (int64_t j = 0; j < nv; j++) {
        uint8_t k = 0;
        if (isfinite(s->lo[j]))
            k |= HAS_LO;
        if (isfinite(s->up[j]))
            k |= HAS_UP;
        if (k == (HAS_LO | HAS_UP) && s->lo[j] == s->up[j])
            k = FIXED;
        s->kind[j] = k;
        if (k & HAS_LO) { s->bounded++; if (fabs(s->lo[j]) > s->norm_bound) s->norm_bound = fabs(s->lo[j]); }
        if (k & HAS_UP) { s->bounded++; if (fabs(s->up[j]) > s->norm_bound) s->norm_bound = fabs(s->up[j]); }
        if (fabs(s->cost[j]) > s->norm_c)
            s->norm_c = fabs(s->cost[j]);
        if (k == FIXED) {
            s->z[j] = s->lo[j];
            s->fixed_obj += s->cost[j] * s->lo[j];
        }
    }
    for (int64_t i = 0; i < nr; i++)
        s->mark[i] = -1;

    s->dense = jm_calloc_array(nv, sizeof(bool));
    if (s->dense == nullptr) {
        bx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    s->augmented = m->cfg.barrier_augmented || m->q_nz > 0;
    if (!s->augmented) {
        const double avg = s->ncol > 0 ? (double)m->num_nz / (double)s->ncol
                                       : 0.0;
        double thr = BARRIER_DENSE_FACTOR * avg;
        if (thr < (double)BARRIER_DENSE_MIN)
            thr = (double)BARRIER_DENSE_MIN;
        int64_t k = 0;
        for (int64_t j = 0; j < s->ncol; j++)
            if (s->kind[j] != FIXED &&
                (double)(m->a_start[j + 1] - m->a_start[j]) > thr)
                k++;
        if (k > 0 && k <= BARRIER_DENSE_MAX && nr > 0) {
            s->dense_idx = jm_alloc_array(k, sizeof(int64_t));
            s->wmat = jm_alloc_array(k * nr, sizeof(double));
            s->smat = jm_alloc_array(k * k, sizeof(double));
            s->tvec = jm_alloc_array(k, sizeof(double));
            s->tvec2 = jm_alloc_array(k, sizeof(double));
            if (!s->dense_idx || !s->wmat || !s->smat || !s->tvec || !s->tvec2) {
                bx_free(s);
                return JAOS_ERR_OUT_OF_MEMORY;
            }
            for (int64_t j = 0; j < s->ncol; j++)
                if (s->kind[j] != FIXED &&
                    (double)(m->a_start[j + 1] - m->a_start[j]) > thr) {
                    s->dense[j] = true;
                    s->dense_idx[s->ndense++] = j;
                }
        }
        jm_work_add(&s->work, s->ncol * JM_WORK_NONZERO);
    }
    return JAOS_OK;
}

/* The quadratic gradient Q z, with Q held as its diagonal in `quad` and
 * its strict lower triangle in qs/qi/qv.  Every off-diagonal entry is
 * stored once and counts twice, once on each side. */
static void mul_q(bx *s, const double *z, double *out)
{
    for (int64_t j = 0; j < s->nvar; j++)
        out[j] = s->quad[j] * z[j];
    if (s->qnz == 0)
        return;
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t p = s->qs[j]; p < s->qs[j + 1]; p++) {
            const int64_t i = s->qi[p];
            out[i] += s->qv[p] * z[j];
            out[j] += s->qv[p] * z[i];
        }
    jm_work_add(&s->work, 2 * s->qnz * JM_WORK_NONZERO);
}

static const double *grad_q(bx *s, const double *z)
{
    if (s->qnz == 0)
        return nullptr;
    mul_q(s, z, s->qz);
    return s->qz;
}

static void mul_e(bx *s, const double *z, double *out)
{
    const jaos_model *m = s->m;
    for (int64_t i = 0; i < s->nrow; i++)
        out[i] = -z[s->ncol + i];
    for (int64_t j = 0; j < s->ncol; j++) {
        const double zj = z[j];
        if (zj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            out[m->a_index[k]] += s->av[k] * zj;
    }
    jm_work_add(&s->work, (m->num_nz + s->nrow) * JM_WORK_NONZERO);
}

static void mul_et(bx *s, const double *y, double *out)
{
    const jaos_model *m = s->m;
    for (int64_t j = 0; j < s->ncol; j++) {
        double t = 0.0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            t += s->av[k] * y[m->a_index[k]];
        out[j] = t;
    }
    for (int64_t i = 0; i < s->nrow; i++)
        out[s->ncol + i] = -y[i];
    jm_work_add(&s->work, (m->num_nz + s->nrow) * JM_WORK_NONZERO);
}

static jaos_status build_normal_pattern(bx *s)
{
    const jaos_model *m = s->m;
    const int64_t nr = s->nrow;
    s->n_start = jm_alloc_array(nr + 1, sizeof(int64_t));
    if (s->n_start == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;

    int64_t nnz = 0;
    for (int pass = 0; pass < 2; pass++) {
        nnz = 0;
        for (int64_t i = 0; i < nr; i++) {
            if (pass == 1)
                s->n_start[i] = nnz;
            s->mark[i] = i;
            if (pass == 1)
                s->n_index[nnz] = i;
            nnz++;
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
                const int64_t j = m->ar_index[p];
                if (s->kind[j] == FIXED || s->dense[j])
                    continue;
                for (int64_t q = m->a_start[j]; q < m->a_start[j + 1]; q++) {
                    const int64_t k = m->a_index[q];
                    if (s->mark[k] == i)
                        continue;
                    s->mark[k] = i;
                    if (pass == 1)
                        s->n_index[nnz] = k;
                    nnz++;
                }
            }
        }
        if (pass == 0) {
            s->n_index = jm_alloc_array(nnz, sizeof(int64_t));
            s->n_value = jm_alloc_array(nnz, sizeof(double));
            if (s->n_index == nullptr || s->n_value == nullptr)
                return JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t i = 0; i < nr; i++)
            s->mark[i] = -1;
    }
    s->n_start[nr] = nnz;
    jm_work_add(&s->work, 2 * nnz * JM_WORK_NONZERO);
    return jm_chol_symbolic(&s->chol, nr, s->n_start, s->n_index, &s->work);
}

/* The augmented system the Newton step really solves:
 *
 *     [ -inv   E^T ] [ dz ]   [ rt ]
 *     [   E    dI  ] [ dy ] = [ r  ]
 *
 * where inv is the diagonal of Theta^{-1}.  Eliminating dz gives the
 * normal equations `form_normal` builds, so the two paths answer the
 * same question; this one keeps its shape when inv stops being diagonal,
 * which is what a full Q costs.  A fixed column has dz = 0 and is left
 * out, so the live variables come first and the rows after them.
 */
static jaos_status build_aug_pattern(bx *s)
{
    const jaos_model *m = s->m;
    const int64_t nr = s->nrow, nv = s->nvar;

    s->aug_of = jm_alloc_array(nv > 0 ? nv : 1, sizeof *s->aug_of);
    if (s->aug_of == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    int64_t live = 0;
    for (int64_t j = 0; j < nv; j++)
        s->aug_of[j] = s->kind[j] == FIXED ? -1 : live++;
    s->nlive = live;
    s->naug = live + nr;

    s->aug_start = jm_alloc_array(s->naug + 1, sizeof *s->aug_start);
    s->aug_sign = jm_alloc_array(s->naug > 0 ? s->naug : 1,
                                 sizeof *s->aug_sign);
    s->aug_rhs = jm_alloc_array(s->naug > 0 ? s->naug : 1,
                                sizeof *s->aug_rhs);
    s->inv = jm_alloc_array(nv > 0 ? nv : 1, sizeof *s->inv);
    if (s->aug_start == nullptr || s->aug_sign == nullptr ||
        s->aug_rhs == nullptr || s->inv == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t k = 0; k < live; k++)
        s->aug_sign[k] = -1;
    for (int64_t i = 0; i < nr; i++)
        s->aug_sign[live + i] = 1;

    int64_t nnz = 0;
    for (int pass = 0; pass < 2; pass++) {
        nnz = 0;
        for (int64_t j = 0; j < nv; j++) {
            const int64_t c = s->aug_of[j];
            if (c < 0)
                continue;
            if (pass == 1) {
                s->aug_start[c] = nnz;
                s->aug_index[nnz] = c;
            }
            nnz++;
            if (j < s->ncol) {

                for (int64_t p = s->qs != nullptr ? s->qs[j] : 0;
                     s->qs != nullptr && p < s->qs[j + 1]; p++) {
                    const int64_t r = s->aug_of[s->qi[p]];
                    if (r < 0)
                        continue;
                    if (pass == 1)
                        s->aug_index[nnz] = r;
                    nnz++;
                }
                for (int64_t p = s->qt_start != nullptr ? s->qt_start[j] : 0;
                     s->qt_start != nullptr && p < s->qt_start[j + 1]; p++) {
                    const int64_t r = s->aug_of[s->qt_index[p]];
                    if (r < 0)
                        continue;
                    if (pass == 1)
                        s->aug_index[nnz] = r;
                    nnz++;
                }
                for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
                    if (pass == 1)
                        s->aug_index[nnz] = live + m->a_index[p];
                    nnz++;
                }
            } else {
                if (pass == 1)
                    s->aug_index[nnz] = live + (j - s->ncol);
                nnz++;
            }
        }

        for (int64_t i = 0; i < nr; i++) {
            if (pass == 1) {
                s->aug_start[live + i] = nnz;
                s->aug_index[nnz] = live + i;
            }
            nnz++;
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
                const int64_t c = s->aug_of[m->ar_index[p]];
                if (c < 0)
                    continue;
                if (pass == 1)
                    s->aug_index[nnz] = c;
                nnz++;
            }
            if (s->aug_of[s->ncol + i] >= 0) {
                if (pass == 1)
                    s->aug_index[nnz] = s->aug_of[s->ncol + i];
                nnz++;
            }
        }
        if (pass == 0) {
            s->aug_index = jm_alloc_array(nnz > 0 ? nnz : 1,
                                          sizeof *s->aug_index);
            s->aug_value = jm_alloc_array(nnz > 0 ? nnz : 1,
                                          sizeof *s->aug_value);
            if (s->aug_index == nullptr || s->aug_value == nullptr)
                return JAOS_ERR_OUT_OF_MEMORY;
        }
    }
    s->aug_start[s->naug] = nnz;
    jm_work_add(&s->work, 2 * nnz * JM_WORK_NONZERO);
    return jm_chol_symbolic(&s->aug, s->naug, s->aug_start, s->aug_index,
                            &s->work);
}

/* `with_q` is false only for the starting point, which solves a plain
 * least-squares system: the normal path builds it with theta at 1,
 * which leaves Q's diagonal out, so Q's off-diagonal entries have to
 * stay out too. Half of Q makes the block indefinite, the LDL
 * replaces the pivots it cannot sign, and the dual iterate leaves
 * the model on the first step. */
static jaos_status form_aug(bx *s, bool with_q)
{
    const jaos_model *m = s->m;
    const int64_t nv = s->nvar, live = s->nlive;
    const int8_t *pin = s->pin;
    int64_t p = 0;
    for (int64_t j = 0; j < nv; j++) {
        if (s->aug_of[j] < 0)
            continue;
        const bool out = pin != nullptr && pin[j] != PUSH_FREE;
        s->aug_value[p++] = out ? -1.0 : -s->inv[j];
        if (j < s->ncol) {
            if (s->qs != nullptr)
                for (int64_t q = s->qs[j]; q < s->qs[j + 1]; q++)
                    if (s->aug_of[s->qi[q]] >= 0)
                        s->aug_value[p++] =
                            with_q && !out &&
                            (pin == nullptr || pin[s->qi[q]] == PUSH_FREE)
                                ? -s->qv[q] : 0.0;
            if (s->qt_start != nullptr)
                for (int64_t q = s->qt_start[j]; q < s->qt_start[j + 1]; q++)
                    if (s->aug_of[s->qt_index[q]] >= 0)
                        s->aug_value[p++] =
                            with_q && !out &&
                            (pin == nullptr ||
                             pin[s->qt_index[q]] == PUSH_FREE)
                                ? -s->qt_value[q] : 0.0;
            for (int64_t q = m->a_start[j]; q < m->a_start[j + 1]; q++)
                s->aug_value[p++] = out ? 0.0 : s->av[q];
        } else {
            s->aug_value[p++] = out ? 0.0 : -1.0;
        }
    }
    for (int64_t i = 0; i < s->nrow; i++) {
        const bool dec = s->dec != nullptr && s->dec[i];
        s->aug_value[p++] = dec ? 1.0
                          : pin != nullptr ? QP_PUSH_DELTA : s->delta;
        for (int64_t q = m->ar_start[i]; q < m->ar_start[i + 1]; q++)
            if (s->aug_of[m->ar_index[q]] >= 0)
                s->aug_value[p++] =
                    pin != nullptr && pin[m->ar_index[q]] != PUSH_FREE
                        ? 0.0 : s->arv[q];
        if (s->aug_of[s->ncol + i] >= 0)
            s->aug_value[p++] =
                pin != nullptr && pin[s->ncol + i] != PUSH_FREE ? 0.0 : -1.0;
    }
    (void)live;
    jm_work_add(&s->work, p * JM_WORK_NONZERO);
    const jaos_status st = jm_ldlt_numeric(&s->aug, s->aug_value, s->aug_sign,
                                           BARRIER_AUG_FLOOR, &s->work);
    if (st == JAOS_OK && s->aug.replaced > 0)
        jm_log(s->m, JAOS_LOG_DETAIL,
               "  augmented: %lld pivots replaced at the floor",
               (long long)s->aug.replaced);
    return st;
}

static void newton_aug(bx *s, const double *r, const double *rt, double *dy,
                       double *dz)
{
    const int64_t live = s->nlive;
    for (int64_t j = 0; j < s->nvar; j++)
        if (s->aug_of[j] >= 0)
            s->aug_rhs[s->aug_of[j]] = rt[j];
    for (int64_t i = 0; i < s->nrow; i++)
        s->aug_rhs[live + i] = r[i];
    jm_ldlt_solve(&s->aug, s->aug_rhs, &s->work);
    for (int64_t j = 0; j < s->nvar; j++)
        dz[j] = s->aug_of[j] < 0 ? 0.0 : s->aug_rhs[s->aug_of[j]];
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = s->aug_rhs[live + i];
    jm_work_add(&s->work, 2 * s->naug * JM_WORK_NONZERO);
}

static jaos_status dense_update(bx *s)
{
    const jaos_model *m = s->m;
    const int64_t nr = s->nrow, k = s->ndense;
    int64_t dnz = 0;
    for (int64_t q = 0; q < k; q++) {
        const int64_t j = s->dense_idx[q];
        double *wq = s->wmat + q * nr;
        memset(wq, 0, (size_t)nr * sizeof(double));
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            wq[m->a_index[p]] = s->av[p];
        dnz += m->a_start[j + 1] - m->a_start[j];
        jm_chol_solve(&s->chol, wq, &s->work);
    }
    for (int64_t q = 0; q < k; q++) {
        const int64_t j = s->dense_idx[q];
        for (int64_t r = 0; r <= q; r++) {
            const double *wr = s->wmat + r * nr;
            double t = 0.0;
            for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
                t += s->av[p] * wr[m->a_index[p]];
            if (r == q)
                t += 1.0 / s->theta[j];
            s->smat[q * k + r] = t;
        }
    }
    for (int64_t q = 0; q < k; q++) {
        for (int64_t r = 0; r <= q; r++) {
            double t = s->smat[q * k + r];
            for (int64_t p = 0; p < r; p++)
                t -= s->smat[q * k + p] * s->smat[r * k + p];
            if (r == q) {
                if (!(t > 0.0) || !isfinite(t))
                    return JAOS_ERR_NUMERICAL;
                s->smat[q * k + q] = sqrt(t);
            } else {
                s->smat[q * k + r] = t / s->smat[r * k + r];
            }
        }
    }
    jm_work_add(&s->work,
                (k * nr + k * dnz + (k * k * k) / 6 + k * k) * JM_WORK_NONZERO);
    return JAOS_OK;
}

static void solve_normal(bx *s, double *x)
{
    jm_chol_solve(&s->chol, x, &s->work);
    const int64_t k = s->ndense;
    if (k == 0)
        return;
    const jaos_model *m = s->m;
    const int64_t nr = s->nrow;
    int64_t dnz = 0;
    for (int64_t q = 0; q < k; q++) {
        const int64_t j = s->dense_idx[q];
        double t = 0.0;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            t += s->av[p] * x[m->a_index[p]];
        s->tvec[q] = t;
        dnz += m->a_start[j + 1] - m->a_start[j];
    }
    for (int64_t q = 0; q < k; q++) {
        double t = s->tvec[q];
        for (int64_t p = 0; p < q; p++)
            t -= s->smat[q * k + p] * s->tvec2[p];
        s->tvec2[q] = t / s->smat[q * k + q];
    }
    for (int64_t q = k - 1; q >= 0; q--) {
        double t = s->tvec2[q];
        for (int64_t p = q + 1; p < k; p++)
            t -= s->smat[p * k + q] * s->tvec[p];
        s->tvec[q] = t / s->smat[q * k + q];
    }
    for (int64_t q = 0; q < k; q++) {
        const double *wq = s->wmat + q * nr;
        const double zq = s->tvec[q];
        if (zq == 0.0)
            continue;
        for (int64_t i = 0; i < nr; i++)
            x[i] -= wq[i] * zq;
    }
    jm_work_add(&s->work, (k * nr + dnz + k * k) * JM_WORK_NONZERO);
}

static jaos_status drop_dense(bx *s)
{
    jm_log(s->m, JAOS_LOG_DETAIL,
           "the %lld dense columns' correction is not positive definite; "
           "they rejoin the normal matrix", (long long)s->ndense);
    memset(s->dense, 0, (size_t)s->nvar * sizeof(bool));
    s->ndense = 0;
    free(s->n_start);  s->n_start = nullptr;
    free(s->n_index);  s->n_index = nullptr;
    free(s->n_value);  s->n_value = nullptr;
    jm_chol_free(&s->chol);
    jm_chol_init(&s->chol);
    return build_normal_pattern(s);
}

static jaos_status form_normal(bx *s)
{
    const jaos_model *m = s->m;
    int64_t terms = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            const int64_t j = m->ar_index[p];
            if (s->kind[j] == FIXED || s->dense[j])
                continue;
            const double t = s->arv[p] * s->theta[j];
            for (int64_t q = m->a_start[j]; q < m->a_start[j + 1]; q++)
                s->acc[m->a_index[q]] += t * s->av[q];
            terms += m->a_start[j + 1] - m->a_start[j];
        }
        if (s->kind[s->ncol + i] != FIXED)
            s->acc[i] += s->theta[s->ncol + i];
        s->acc[i] += s->pin != nullptr ? QP_PUSH_DELTA : s->delta;
        if (s->dec != nullptr && s->dec[i])
            s->acc[i] = 1.0;
        for (int64_t q = s->n_start[i]; q < s->n_start[i + 1]; q++) {
            const int64_t k = s->n_index[q];
            s->n_value[q] = s->acc[k];
            s->acc[k] = 0.0;
        }
    }
    jm_work_add(&s->work, (terms + s->n_start[s->nrow]) * JM_WORK_NONZERO);
    jaos_status st = jm_chol_numeric(&s->chol, s->n_value, &s->work);
    if (st != JAOS_OK || s->ndense == 0)
        return st;
    if (dense_update(s) == JAOS_OK)
        return JAOS_OK;
    st = drop_dense(s);
    if (st != JAOS_OK)
        return st;
    return form_normal(s);
}

static void newton(bx *s, const double *r, const double *rt, double *dy,
                   double *dz)
{
    if (s->augmented) {
        newton_aug(s, r, rt, dy, dz);
        return;
    }
    for (int64_t j = 0; j < s->nvar; j++)
        s->tmp[j] = s->kind[j] == FIXED ? 0.0 : s->theta[j] * rt[j];
    mul_e(s, s->tmp, s->rhs);
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = r[i] + s->rhs[i];
    solve_normal(s, dy);
    mul_et(s, dy, dz);
    for (int64_t j = 0; j < s->nvar; j++)
        dz[j] = s->kind[j] == FIXED ? 0.0 : s->theta[j] * (dz[j] - rt[j]);
    jm_work_add(&s->work, 2 * s->nvar * JM_WORK_NONZERO);
}

/* A fixed column has no residual and no complementarity, so it drops out of
 * the Newton system, but it still stands in the objective: `fixed_obj`
 * carries its linear part and its share of `1/2 z'Qz` is added here.  It
 * goes to both objectives with the same sign, because their difference is
 * the complementarity of the columns that are not fixed and a fixed column
 * has none; adding it to one side only would put a term in the gap that
 * nothing drives to zero.
 */
static void residuals(bx *s, double *pobj, double *dobj, double *mu)
{
    mul_e(s, s->z, s->rp);
    for (int64_t i = 0; i < s->nrow; i++)
        s->rp[i] = -s->rp[i];
    mul_et(s, s->y, s->rd);
    const double *qz = grad_q(s, s->z);
    double po = s->fixed_obj, dob = 0.0, prod = 0.0;
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED) {
            s->rd[j] = 0.0;
            s->rw[j] = s->rv[j] = 0.0;
            const double gqf = qz != nullptr ? qz[j] : s->quad[j] * s->z[j];
            if (gqf != 0.0) {
                const double half = 0.5 * gqf * s->z[j];
                po += half;
                dob += half;
            }
            continue;
        }
        po += s->cost[j] * s->z[j];
        const double gq = qz != nullptr ? qz[j] : s->quad[j] * s->z[j];
        if (gq != 0.0) {
            const double half = 0.5 * gq * s->z[j];
            po += half;
            dob -= half;
        }
        s->rd[j] = s->cost[j] + gq - s->rd[j] - s->zl[j] + s->zu[j];
        s->rw[j] = (k & HAS_LO) ? s->lo[j] - s->z[j] + s->w[j] : 0.0;
        s->rv[j] = (k & HAS_UP) ? s->up[j] - s->z[j] - s->v[j] : 0.0;
        if (k & HAS_LO) { dob += s->lo[j] * s->zl[j]; prod += s->w[j] * s->zl[j]; }
        if (k & HAS_UP) { dob -= s->up[j] * s->zu[j]; prod += s->v[j] * s->zu[j]; }
    }
    for (int64_t i = 0; i < s->nrow; i++)
        dob += s->b[i] * s->y[i];
    *pobj = po;
    *dobj = dob + s->fixed_obj;
    *mu = s->bounded > 0 ? prod / (double)s->bounded : 0.0;
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
}

static double inf_norm(const double *x, int64_t n)
{
    double t = 0.0;
    for (int64_t i = 0; i < n; i++)
        if (fabs(x[i]) > t)
            t = fabs(x[i]);
    return t;
}

static void complementarity(bx *s, double target, bool corrector)
{
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED) {
            s->rl[j] = s->ru[j] = 0.0;
            continue;
        }
        s->rl[j] = (k & HAS_LO) ? target - s->w[j] * s->zl[j] -
                                      (corrector ? s->dwa[j] * s->dzla[j] : 0.0)
                                : 0.0;
        s->ru[j] = (k & HAS_UP) ? target - s->v[j] * s->zu[j] -
                                      (corrector ? s->dva[j] * s->dzua[j] : 0.0)
                                : 0.0;
    }
}

static void direction(bx *s, double *dw, double *dv, double *dzl, double *dzu)
{
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED) {
            s->rt[j] = 0.0;
            continue;
        }
        double t = s->rd[j];
        if (k & HAS_LO)
            t -= (s->rl[j] + s->zl[j] * s->rw[j]) / s->w[j];
        if (k & HAS_UP)
            t += (s->ru[j] - s->zu[j] * s->rv[j]) / s->v[j];
        s->rt[j] = t;
    }
    newton(s, s->rp, s->rt, s->dy, s->dz);
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        dw[j] = dv[j] = dzl[j] = dzu[j] = 0.0;
        if (k == FIXED)
            continue;
        if (k & HAS_LO) {
            dw[j] = s->dz[j] - s->rw[j];
            dzl[j] = (s->rl[j] - s->zl[j] * dw[j]) / s->w[j];
        }
        if (k & HAS_UP) {
            dv[j] = s->rv[j] - s->dz[j];
            dzu[j] = (s->ru[j] - s->zu[j] * dv[j]) / s->v[j];
        }
    }
    jm_work_add(&s->work, 2 * s->nvar * JM_WORK_NONZERO);
}

static void step_lengths(bx *s, const double *dw, const double *dv,
                         const double *dzl, const double *dzu, double *ap,
                         double *ad)
{
    double p = 1.0, d = 1.0;
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED)
            continue;
        if (k & HAS_LO) {
            if (dw[j] < 0.0 && -s->w[j] / dw[j] < p)
                p = -s->w[j] / dw[j];
            if (dzl[j] < 0.0 && -s->zl[j] / dzl[j] < d)
                d = -s->zl[j] / dzl[j];
        }
        if (k & HAS_UP) {
            if (dv[j] < 0.0 && -s->v[j] / dv[j] < p)
                p = -s->v[j] / dv[j];
            if (dzu[j] < 0.0 && -s->zu[j] / dzu[j] < d)
                d = -s->zu[j] / dzu[j];
        }
    }
    *ap = p;
    *ad = d;
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
}

static jaos_status starting_point(bx *s)
{
    const int64_t nv = s->nvar;
    for (int64_t j = 0; j < nv; j++) {
        s->theta[j] = s->kind[j] == FIXED ? 0.0 : 1.0;
        if (s->augmented)
            s->inv[j] = 1.0;
    }
    jaos_status st = s->augmented ? form_aug(s, false)
                                  : form_normal(s);
    if (st != JAOS_OK)
        return st;

    for (int64_t j = 0; j < nv; j++)
        s->tmp[j] = s->kind[j] == FIXED ? s->z[j] : 0.0;
    mul_e(s, s->tmp, s->b);
    for (int64_t i = 0; i < s->nrow; i++)
        s->b[i] = -s->b[i];
    s->norm_b = inf_norm(s->b, s->nrow);
    if (s->norm_bound > s->norm_b)
        s->norm_b = s->norm_bound;

    memset(s->rt, 0, (size_t)nv * sizeof(double));
    for (int64_t i = 0; i < s->nrow; i++)
        s->rp[i] = s->b[i];
    newton(s, s->rp, s->rt, s->dy, s->dz);
    for (int64_t j = 0; j < nv; j++)
        if (s->kind[j] != FIXED)
            s->z[j] = s->dz[j];

    memset(s->rp, 0, (size_t)s->nrow * sizeof(double));
    newton(s, s->rp, s->cost, s->y, s->dz);

    double min_p = HUGE_VAL, min_d = HUGE_VAL;
    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED)
            continue;
        const double d = -s->dz[j];
        if (k & HAS_LO) {
            s->w[j] = s->z[j] - s->lo[j];
            s->zl[j] = (k & HAS_UP) ? (d > 0.0 ? d : 0.0) : d;
            if (s->w[j] < min_p) min_p = s->w[j];
            if (s->zl[j] < min_d) min_d = s->zl[j];
        }
        if (k & HAS_UP) {
            s->v[j] = s->up[j] - s->z[j];
            s->zu[j] = (k & HAS_LO) ? (d < 0.0 ? -d : 0.0) : -d;
            if (s->v[j] < min_p) min_p = s->v[j];
            if (s->zu[j] < min_d) min_d = s->zu[j];
        }
    }
    if (s->bounded == 0)
        return JAOS_OK;

    double shift_p = min_p < 0.0 ? -1.5 * min_p : 0.0;
    double shift_d = min_d < 0.0 ? -1.5 * min_d : 0.0;
    double prod = 0.0, sum_p = 0.0, sum_d = 0.0;
    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED)
            continue;
        if (k & HAS_LO) {
            s->w[j] += shift_p;  s->zl[j] += shift_d;
            prod += s->w[j] * s->zl[j];
            sum_p += s->w[j];    sum_d += s->zl[j];
        }
        if (k & HAS_UP) {
            s->v[j] += shift_p;  s->zu[j] += shift_d;
            prod += s->v[j] * s->zu[j];
            sum_p += s->v[j];    sum_d += s->zu[j];
        }
    }
    double add_p = sum_d > 0.0 ? 0.5 * prod / sum_d : 0.0;
    double add_d = sum_p > 0.0 ? 0.5 * prod / sum_p : 0.0;
    if (prod <= 0.0 || add_p < BARRIER_START_MIN || add_d < BARRIER_START_MIN)
        add_p = add_d = 1.0;
    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED)
            continue;
        if (k & HAS_LO) {
            s->w[j] += add_p;
            s->zl[j] += add_d;
            if (!(s->w[j] > 0.0))  s->w[j] = 1.0;
            if (!(s->zl[j] > 0.0)) s->zl[j] = 1.0;
        }
        if (k & HAS_UP) {
            s->v[j] += add_p;
            s->zu[j] += add_d;
            if (!(s->v[j] > 0.0))  s->v[j] = 1.0;
            if (!(s->zu[j] > 0.0)) s->zu[j] = 1.0;
        }
        if (k == HAS_LO)
            s->z[j] = s->lo[j] + s->w[j];
        else if (k == HAS_UP)
            s->z[j] = s->up[j] - s->v[j];
    }
    jm_work_add(&s->work, 3 * nv * JM_WORK_NONZERO);
    return JAOS_OK;
}

static double published(double x)
{
    return x == 0.0 ? 0.0 : x;
}

static jaos_status bx_publish(bx *s, jaos_solve_status status, jm_presolve *p)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    (void)p;

    m->solve_status = status;
    m->solve_iters = s->iters;
    jaos_status st = jm_model_ensure_solution_arrays(m);
    if (st != JAOS_OK)
        return st;

    memset(m->sol_farkas, 0, (size_t)m->num_row * sizeof(double));
    memset(m->sol_ray, 0, (size_t)m->num_col * sizeof(double));
    memset(m->sol_col_status, 0, (size_t)m->num_col * sizeof *m->sol_col_status);
    memset(m->sol_row_status, 0, (size_t)m->num_row * sizeof *m->sol_row_status);
    m->sol_basis_ok = false;
    m->solve_work = s->work.units;
    m->solve_time = jm_monotonic_seconds() - s->started;

    if (status != JAOS_SOLVE_OPTIMAL) {
        m->objective = 0.0;
        memset(m->sol_col, 0, (size_t)m->num_col * sizeof(double));
        memset(m->sol_row, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_dual, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_redcost, 0, (size_t)m->num_col * sizeof(double));
    } else {
        const double *rho = m->row_scale, *gamma = m->col_scale;
        mul_et(s, s->y, s->tmp);
        const double *qz = grad_q(s, s->z);
        for (int64_t j = 0; j < m->num_col; j++) {
            const double gq = qz != nullptr ? qz[j] : s->quad[j] * s->z[j];
            m->sol_col[j] = published(gamma[j] * s->z[j]);
            m->sol_redcost[j] = published(
                sigma * (s->cost[j] + gq - s->tmp[j]) / gamma[j]);
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            m->sol_row[i] = published(s->z[m->num_col + i] / rho[i]);
            m->sol_dual[i] = published(sigma * s->y[i] * rho[i]);
        }
        for (int64_t j = 0; j < s->nvar; j++) {
            const uint8_t k = s->kind[j];
            jaos_basis_status b = JAOS_BASIS_BASIC;
            if (k == FIXED)
                b = JAOS_BASIS_AT_LOWER;
            else if (k == 0)
                b = JAOS_BASIS_FREE;
            else {
                const bool at_lo = (k & HAS_LO) && s->w[j] < s->zl[j];
                const bool at_up = (k & HAS_UP) && s->v[j] < s->zu[j];
                if (at_lo && (!at_up || s->w[j] <= s->v[j]))
                    b = JAOS_BASIS_AT_LOWER;
                else if (at_up)
                    b = JAOS_BASIS_AT_UPPER;
            }
            if (j < m->num_col)
                m->sol_col_status[j] = b;
            else
                m->sol_row_status[j - m->num_col] = b;
        }
        jm_model_publish_objective(m);
        m->sol_basis_ok = jm_model_basis_count_ok(m);
        if (m->sol_basis_ok)
            (void)jm_model_remember_basis(m);
    }
#if !defined(JAOS_NO_PRESOLVE)
    if (p->outcome == JM_PRESOLVE_REDUCED) {
        jaos_status pst = jm_postsolve_expand(p);
        if (pst != JAOS_OK)
            return pst;
    }
#endif
    return JAOS_OK;
}

static jaos_status bx_run(bx *s, jaos_solve_status *out, bool resume)
{
    jaos_model *m = s->m;
    jaos_status st = JAOS_OK;
    if (!resume) {
        s->augmented = m->cfg.barrier_augmented || m->q_nz > 0;
        st = s->augmented ? build_aug_pattern(s) : build_normal_pattern(s);
        if (st != JAOS_OK)
            return st;
        st = starting_point(s);
        if (st != JAOS_OK)
            return st;
    }

    for (;;) {
        double pobj, dobj, mu;
        residuals(s, &pobj, &dobj, &mu);
        const double pres = inf_norm(s->rp, s->nrow);
        double pinf = pres, t;
        if ((t = inf_norm(s->rw, s->nvar)) > pinf) pinf = t;
        if ((t = inf_norm(s->rv, s->nvar)) > pinf) pinf = t;
        const double dinf = inf_norm(s->rd, s->nvar);
        const double rel_p = pinf / (1.0 + s->norm_b);
        const double rel_d = dinf / (1.0 + s->norm_c);
        const double gap = fabs(pobj - dobj) / (1.0 + fabs(pobj));
        double worst_now = rel_p > rel_d ? rel_p : rel_d;
        if (gap > worst_now)
            worst_now = gap;
        const bool progressed = worst_now < BARRIER_STALL_DROP * s->best_worst;

        jm_log(m, JAOS_LOG_PROGRESS,
               "barrier %lld: primal %.3e dual %.3e gap %.3e mu %.3e "
               "objective %.10g, %lld work units",
               (long long)s->iters, rel_p, rel_d, gap, mu, pobj,
               (long long)s->work.units);

        if (!isfinite(pobj) || !isfinite(dobj) || !isfinite(mu) ||
            !isfinite(pinf) || !isfinite(dinf)) {
            jm_set_err(m, "the barrier diverged after %lld iterations: the "
                          "model may be infeasible or unbounded, which the "
                          "barrier does not certify; the dual simplex does",
                       (long long)s->iters);
            s->handoff = true;
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }
        if (rel_p <= s->tol_stop && rel_d <= s->tol_stop && gap <= s->tol_stop) {
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }
        {
            const double pn = inf_norm(s->z, s->nvar);
            double dn = inf_norm(s->y, s->nrow);
            if ((t = inf_norm(s->zl, s->nvar)) > dn) dn = t;
            if ((t = inf_norm(s->zu, s->nvar)) > dn) dn = t;
            const double pgrow = pn / (1.0 + s->norm_b + s->norm_bound);
            const double dgrow = dn / (1.0 + s->norm_c);
            jm_work_add(&s->work, (3 * s->nvar + s->nrow) * JM_WORK_NONZERO);
            jm_log(m, JAOS_LOG_DETAIL, "  iterate %.3e/%.3e of the data",
                   pgrow, dgrow);
            const double far = s->quadratic ? BARRIER_DIVERGE_QP
                                            : BARRIER_DIVERGE;
            if ((pgrow > far || dgrow > far) && !progressed) {
                jm_set_err(m, "the barrier's iterate grew past %.3g times "
                              "the data after %lld iterations (primal %.3e, "
                              "dual %.3e): the model may be infeasible or "
                              "unbounded, which the barrier does not "
                              "certify; the dual simplex does",
                           BARRIER_DIVERGE, (long long)s->iters, pgrow, dgrow);
                s->handoff = true;
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }
        if (m->cfg.work_limit > 0 && s->work.units >= m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (m->cfg.time_limit > 0.0 &&
            jm_monotonic_seconds() - s->started >= m->cfg.time_limit) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (m->cfg.progress_cb != nullptr) {
            const jaos_progress pr = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = pres,
            };
            if (m->cfg.progress_cb(&pr, m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters >= s->iter_cap) {
            jm_set_err(m, "the barrier did not converge in %lld iterations "
                          "(primal %.3e, dual %.3e, gap %.3e): the model may "
                          "be infeasible or unbounded, which the barrier does "
                          "not certify; the dual simplex does",
                       (long long)s->iters, rel_p, rel_d, gap);
            s->near = worst_now <= BARRIER_NEAR_TOL;
            s->handoff = true;
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        double worst = rel_p > rel_d ? rel_p : rel_d;
        if (gap > worst)
            worst = gap;
        if (worst < BARRIER_STALL_DROP * s->best_worst) {
            s->best_worst = worst;
            s->stalled = 0;
        } else if (s->quadratic && ++s->stalled >= BARRIER_STALL_ITERS &&
                   !s->equal_steps) {
            s->equal_steps = true;
            jm_log(m, JAOS_LOG_SUMMARY,
                   "barrier %lld: no progress over %lld iterations; one step "
                   "length for both sides from here",
                   (long long)s->iters, (long long)BARRIER_STALL_ITERS);
        }
        if (s->equal_steps && s->delta == BARRIER_DELTA && !s->delta_locked &&
            rel_p > BARRIER_TOL && rel_p >= rel_d) {
            s->delta = BARRIER_DELTA * BARRIER_STALL_DELTA;
            jm_log(m, JAOS_LOG_DETAIL,
                   "  the rows hold the walk; their regularisation drops to "
                   "%.1e", s->delta);
        }
        for (;;) {
            double reg = BARRIER_REG * (worst < 1.0 ? worst : 1.0);
            if (reg < s->reg_floor)
                reg = s->reg_floor;
            for (int64_t j = 0; j < s->nvar; j++) {
                const uint8_t k = s->kind[j];
                if (k == FIXED) {
                    s->theta[j] = 0.0;
                    continue;
                }
                double inv = (k == 0 && reg < BARRIER_FREE_REG
                                  ? BARRIER_FREE_REG : reg) + s->quad[j];
                if (k & HAS_LO) inv += s->zl[j] / s->w[j];
                if (k & HAS_UP) inv += s->zu[j] / s->v[j];
                if (s->augmented)
                    s->inv[j] = inv;
                s->theta[j] = 1.0 / inv;
            }
            st = s->augmented ? form_aug(s, true) : form_normal(s);
            if (st != JAOS_OK)
                return st;
            const int64_t lost = s->augmented ? s->aug.replaced
                                              : s->chol.replaced;
            if (lost > 0 && s->delta < BARRIER_DELTA) {
                s->delta = BARRIER_DELTA;
                s->delta_locked = true;
                jm_log(m, JAOS_LOG_DETAIL,
                       "  %lld pivots replaced with the rows' regularisation "
                       "dropped; it goes back to %.1e for good",
                       (long long)lost, s->delta);
                continue;
            }
            if (!s->augmented || s->aug.replaced == 0 ||
                s->reg_floor >= BARRIER_REG_MAX)
                break;
            s->reg_floor = s->reg_floor == 0.0
                               ? BARRIER_REG_RETRY
                               : s->reg_floor * BARRIER_REG_GROWTH;
            if (s->reg_floor > BARRIER_REG_MAX)
                s->reg_floor = BARRIER_REG_MAX;
            jm_log(m, JAOS_LOG_DETAIL,
                   "  %lld pivots replaced; refactoring with a "
                   "regularisation of %.1e",
                   (long long)s->aug.replaced, s->reg_floor);
        }

        complementarity(s, 0.0, false);
        direction(s, s->dwa, s->dva, s->dzla, s->dzua);
        double ap, ad;
        step_lengths(s, s->dwa, s->dva, s->dzla, s->dzua, &ap, &ad);
        double prod = 0.0;
        for (int64_t j = 0; j < s->nvar; j++) {
            const uint8_t k = s->kind[j];
            if (k == FIXED)
                continue;
            if (k & HAS_LO)
                prod += (s->w[j] + ap * s->dwa[j]) * (s->zl[j] + ad * s->dzla[j]);
            if (k & HAS_UP)
                prod += (s->v[j] + ap * s->dva[j]) * (s->zu[j] + ad * s->dzua[j]);
        }
        const double mu_aff = s->bounded > 0 ? prod / (double)s->bounded : 0.0;
        double sig = mu > 0.0 ? mu_aff / mu : 0.0;
        sig = sig * sig * sig;
        if (sig > 1.0) sig = 1.0;
        if (sig < 0.0) sig = 0.0;
        if (s->equal_steps && sig < BARRIER_STALL_SIGMA)
            sig = BARRIER_STALL_SIGMA;

        complementarity(s, sig * mu, true);
        direction(s, s->dw, s->dv, s->dzl, s->dzu);
        step_lengths(s, s->dw, s->dv, s->dzl, s->dzu, &ap, &ad);
        ap *= BARRIER_STEP;
        ad *= BARRIER_STEP;
        if (ap > 1.0) ap = 1.0;
        if (ad > 1.0) ad = 1.0;
        if (s->equal_steps)
            ap = ad = ap < ad ? ap : ad;

        jm_log(m, JAOS_LOG_DETAIL, "  step %.3e/%.3e, sigma %.3e", ap, ad, sig);

        for (int64_t j = 0; j < s->nvar; j++) {
            const uint8_t k = s->kind[j];
            if (k == FIXED)
                continue;
            s->z[j] += ap * s->dz[j];
            if (k & HAS_LO) { s->w[j] += ap * s->dw[j]; s->zl[j] += ad * s->dzl[j]; }
            if (k & HAS_UP) { s->v[j] += ap * s->dv[j]; s->zu[j] += ad * s->dzu[j]; }
        }
        for (int64_t i = 0; i < s->nrow; i++)
            s->y[i] += ad * s->dy[i];
        jm_work_add(&s->work, (2 * s->nvar + s->nrow) * JM_WORK_NONZERO);
        s->iters++;
    }
}

typedef struct {
    double score;
    int64_t index;
} ranked;

static int by_score(const void *a, const void *b)
{
    const ranked *x = a, *y = b;
    if (x->score != y->score)
        return x->score > y->score ? -1 : 1;
    return x->index < y->index ? -1 : x->index > y->index;
}

jaos_status jm_crash_basis(jaos_model *m, const uint8_t *kind,
                           const double *w, const double *v,
                           const double *zl, const double *zu,
                           const double *av, jm_work *work)
{
    const int64_t nv = m->num_col + m->num_row, nr = m->num_row;
    const int64_t ncol = m->num_col;
    ranked *order = jm_alloc_array(nv > 0 ? nv : 1, sizeof *order);
    jaos_basis_status *want = jm_alloc_array(nv > 0 ? nv : 1, sizeof *want);
    int64_t *basis = jm_alloc_array(nr > 0 ? nr : 1, sizeof *basis);
    int64_t *bs = jm_alloc_array(nr + 1, sizeof *bs);
    int64_t *bi = jm_alloc_array(m->num_nz + nr + 1, sizeof *bi);
    double *bv = jm_alloc_array(m->num_nz + nr + 1, sizeof *bv);
    bool *covered = jm_calloc_array(nr > 0 ? nr : 1, sizeof *covered);
    bool *used = jm_calloc_array(nr > 0 ? nr : 1, sizeof *used);
    jm_lu lu;
    jm_lu_init(&lu);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (!order || !want || !basis || !bs || !bi || !bv || !covered || !used)
        goto done;

    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = kind[j];
        double sc;
        if (k == FIXED)
            sc = -1.0;
        else if (k == 0)
            sc = 2.0;
        else {
            double pd = HUGE_VAL, dd = 0.0;
            if (k & HAS_LO) { pd = w[j]; dd = zl[j]; }
            if (k & HAS_UP) {
                if (v[j] < pd) pd = v[j];
                if (zu[j] > dd) dd = zu[j];
            }
            sc = pd + dd > 0.0 ? pd / (pd + dd) : 0.5;
        }
        order[j].score = sc;
        order[j].index = j;
    }
    qsort(order, (size_t)nv, sizeof *order, by_score);
    {
        int64_t bits = 0;
        for (int64_t t = nv; t > 1; t >>= 1)
            bits++;
        jm_work_add(work, nv * (bits + 2) * JM_WORK_NONZERO);
    }

    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = kind[j];
        if (k == 0)
            want[j] = JAOS_BASIS_FREE;
        else if (k == HAS_UP)
            want[j] = JAOS_BASIS_AT_UPPER;
        else if (k == (HAS_LO | HAS_UP) && v[j] < w[j])
            want[j] = JAOS_BASIS_AT_UPPER;
        else
            want[j] = JAOS_BASIS_AT_LOWER;
    }
    for (int64_t q = 0; q < nr && q < nv; q++) {
        basis[q] = order[q].index;
        want[basis[q]] = JAOS_BASIS_BASIC;
    }

    for (int attempt = 0; attempt < 4 && nr > 0; attempt++) {
        int64_t p = 0;
        for (int64_t q = 0; q < nr; q++) {
            bs[q] = p;
            const int64_t var = basis[q];
            if (var < ncol) {
                for (int64_t k = m->a_start[var]; k < m->a_start[var + 1]; k++) {
                    bi[p] = m->a_index[k];
                    bv[p] = av[k];
                    p++;
                }
            } else {
                bi[p] = var - ncol;
                bv[p] = -1.0;
                p++;
            }
        }
        bs[nr] = p;
        st = jm_lu_factor(&lu, nr, bs, bi, bv, LU_PIVOT_TOL, work);
        if (st != JAOS_OK)
            goto done;
        if (lu.rank == nr)
            break;
        memset(covered, 0, (size_t)nr * sizeof *covered);
        memset(used, 0, (size_t)nr * sizeof *used);
        for (int64_t k = 0; k < lu.rank; k++) {
            covered[lu.perm_row[k]] = true;
            used[lu.perm_col[k]] = true;
        }
        int64_t i = 0;
        for (int64_t q = 0; q < nr; q++) {
            if (used[q])
                continue;
            while (i < nr && (covered[i] || want[ncol + i] == JAOS_BASIS_BASIC))
                i++;
            if (i >= nr)
                break;
            const int64_t leaving = basis[q];
            want[leaving] = kind[leaving] == 0 ? JAOS_BASIS_FREE
                            : (kind[leaving] == HAS_UP ? JAOS_BASIS_AT_UPPER
                                                       : JAOS_BASIS_AT_LOWER);
            basis[q] = ncol + i;
            want[ncol + i] = JAOS_BASIS_BASIC;
            i++;
        }
        jm_work_add(work, 2 * nr * JM_WORK_NONZERO);
    }

    st = jm_model_ensure_solution_arrays(m);
    if (st != JAOS_OK)
        goto done;
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col_status[j] = want[j];
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row_status[i] = want[m->num_col + i];
    st = jm_model_remember_basis(m);
    int64_t structural = 0;
    for (int64_t q = 0; q < nr; q++)
        structural += basis[q] < ncol;
    jm_log(m, JAOS_LOG_DETAIL,
           "crossover: %lld of %lld basics are structural, the guess "
           "factored at rank %lld",
           (long long)structural, (long long)nr, (long long)lu.rank);

done:
    jm_lu_free(&lu);
    free(order);  free(want);  free(basis);
    free(bs);     free(bi);    free(bv);
    free(covered); free(used);
    return st;
}

static jaos_status crash_basis(bx *s)
{
    return jm_crash_basis(s->m, s->kind, s->w, s->v, s->zl, s->zu, s->av,
                          &s->work);
}

static jaos_status qp_push(bx *s)
{
    const jaos_model *m = s->m;
    const int64_t nv = s->nvar, nr = s->nrow;
    int8_t *pin = jm_alloc_array(nv > 0 ? nv : 1, sizeof *pin);
    double *zn = jm_alloc_array(nv > 0 ? nv : 1, sizeof *zn);
    double *yn = jm_alloc_array(nr > 0 ? nr : 1, sizeof *yn);
    double *d = jm_calloc_array(nv > 0 ? nv : 1, sizeof *d);
    bool *dec = jm_calloc_array(nr > 0 ? nr : 1, sizeof *dec);
    double *rt2 = jm_alloc_array(nv > 0 ? nv : 1, sizeof *rt2);
    double *rp2 = jm_alloc_array(nr > 0 ? nr : 1, sizeof *rp2);
    double *dz2 = jm_alloc_array(nv > 0 ? nv : 1, sizeof *dz2);
    double *dy2 = jm_alloc_array(nr > 0 ? nr : 1, sizeof *dy2);
    double *row_tol = jm_alloc_array(nr > 0 ? nr : 1, sizeof *row_tol);
    double *col_tol = jm_alloc_array(nv > 0 ? nv : 1, sizeof *col_tol);
    if (pin == nullptr || zn == nullptr || yn == nullptr || d == nullptr ||
        dec == nullptr || rt2 == nullptr || rp2 == nullptr ||
        dz2 == nullptr || dy2 == nullptr || row_tol == nullptr ||
        col_tol == nullptr) {
        free(pin);
        free(zn);
        free(yn);
        free(d);
        free(dec);
        free(rt2);
        free(rp2);
        free(dz2);
        free(dy2);
        free(row_tol);
        free(col_tol);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    s->pin = pin;
    s->dec = dec;
    jaos_status st = JAOS_OK;
    memcpy(zn, s->z, (size_t)nv * sizeof *zn);
    memcpy(yn, s->y, (size_t)nr * sizeof *yn);
    int64_t pinned = 0;
    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = s->kind[j];
        pin[j] = PUSH_FREE;
        if (k == FIXED || k == 0)
            continue;
        const double near = QP_PUSH_NEAR * (1.0 + s->norm_b);
        const bool at_lo = (k & HAS_LO) &&
                           (s->w[j] < s->zl[j] || s->w[j] <= near);
        const bool at_up = (k & HAS_UP) &&
                           (s->v[j] < s->zu[j] || s->v[j] <= near);
        if (at_lo && (!at_up || s->w[j] <= s->v[j]))
            pin[j] = PUSH_LOWER;
        else if (at_up)
            pin[j] = PUSH_UPPER;
        if (pin[j] != PUSH_FREE)
            pinned++;
    }
    const double tol_p = QP_PUSH_TOL * (1.0 + s->norm_b);
    const double tol_d = QP_PUSH_TOL * (1.0 + s->norm_c);
    const double near_p = QP_PUSH_NEAR * (1.0 + s->norm_b);
    for (int64_t i = 0; i < nr; i++)
        row_tol[i] = 1.0;
    for (int64_t j = 0; j < s->ncol; j++) {
        const double xj = fabs(m->col_scale[j] * s->z[j]);
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            row_tol[m->a_index[p]] += fabs(m->a_value[p]) * xj;
    }
    for (int64_t i = 0; i < nr; i++) {
        const double user = QP_PUSH_USER_TOL * m->row_scale[i] * row_tol[i];
        row_tol[i] = user < tol_p ? user : tol_p;
    }
    jm_work_add(&s->work, (m->num_nz + nr) * JM_WORK_NONZERO);
    for (int64_t j = 0; j < nv; j++) {
        const double user = j < s->ncol ? QP_PUSH_USER_TOL / m->col_scale[j]
                                        : QP_PUSH_USER_TOL * m->row_scale[j - s->ncol];
        col_tol[j] = user < tol_p ? user : tol_p;
    }
    int64_t round = 0, moved = -1, wrong = 0, freed = 0, releases = 0;
    bool stuck = false;
    double worst_sign = 0.0;
    double *zt = s->dw;
    bool settled = false, fresh = true;
    const double *gamma = m->col_scale, *rho = m->row_scale;
    double alpha = 1.0;
    for (round = 1; round <= QP_PUSH_ROUNDS; round++) {
        for (int64_t j = 0; j < nv; j++) {
            const uint8_t k = s->kind[j];
            zt[j] = pin[j] == PUSH_LOWER ? s->lo[j]
                  : pin[j] == PUSH_UPPER ? s->up[j] : zn[j];
            if (k == FIXED || pin[j] != PUSH_FREE) {
                s->theta[j] = s->dense[j] ? QP_PUSH_DENSE_THETA : 0.0;
                continue;
            }
            const double inv = s->quad[j] + QP_PUSH_REG;
            if (s->augmented)
                s->inv[j] = inv;
            s->theta[j] = 1.0 / inv;
        }
        for (int64_t i = 0; i < nr; i++) {
            bool any = s->kind[s->ncol + i] != FIXED &&
                       pin[s->ncol + i] == PUSH_FREE;
            for (int64_t p = m->ar_start[i]; !any && p < m->ar_start[i + 1];
                 p++) {
                const int64_t j = m->ar_index[p];
                any = s->kind[j] != FIXED && pin[j] == PUSH_FREE;
            }
            dec[i] = !any;
        }
        jm_work_add(&s->work, (nv + nr + m->num_nz) * JM_WORK_NONZERO);
        mul_e(s, zt, s->rp);
        for (int64_t i = 0; i < nr; i++)
            s->rp[i] = -s->rp[i];
        mul_et(s, yn, s->tmp);
        const double *qz = grad_q(s, zt);
        for (int64_t j = 0; j < nv; j++) {
            const double gq = qz != nullptr ? qz[j] : s->quad[j] * zt[j];
            s->rt[j] = (s->kind[j] == FIXED || pin[j] != PUSH_FREE)
                           ? 0.0
                           : s->cost[j] + gq - s->tmp[j];
        }
        if (fresh) {
            st = s->augmented ? form_aug(s, true) : form_normal(s);
            if (st != JAOS_OK)
                break;
            fresh = false;
        }
        newton(s, s->rp, s->rt, s->dy, s->dz);
        for (int64_t pass = 0; pass < QP_PUSH_REFINE; pass++) {
            mul_e(s, s->dz, rp2);
            double rres = 0.0;
            for (int64_t i = 0; i < nr; i++) {
                rp2[i] = s->rp[i] - rp2[i];
                if (fabs(rp2[i]) / row_tol[i] > rres)
                    rres = fabs(rp2[i]) / row_tol[i];
            }
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "  push round %lld refinement %lld: rows off by %.3e of "
                   "their tolerance",
                   (long long)round, (long long)pass, rres);
            if (rres <= 0.01)
                break;
            mul_et(s, s->dy, s->tmp);
            const double *qd = grad_q(s, s->dz);
            for (int64_t j = 0; j < nv; j++) {
                if (s->kind[j] == FIXED || pin[j] != PUSH_FREE) {
                    rt2[j] = 0.0;
                    continue;
                }
                const double qdz = (qd != nullptr ? qd[j] : s->quad[j] * s->dz[j]) +
                                   QP_PUSH_REG * s->dz[j];
                rt2[j] = s->rt[j] + qdz - s->tmp[j];
            }
            newton(s, rp2, rt2, dy2, dz2);
            for (int64_t j = 0; j < nv; j++)
                s->dz[j] += dz2[j];
            for (int64_t i = 0; i < nr; i++)
                s->dy[i] += dy2[i];
            jm_work_add(&s->work, (2 * nv + nr) * JM_WORK_NONZERO);
        }

        alpha = 1.0;
        for (int64_t j = 0; j < nv; j++) {
            const uint8_t k = s->kind[j];
            if (k == FIXED || pin[j] != PUSH_FREE)
                continue;
            const double dj = s->dz[j];
            if ((k & HAS_LO) && dj < 0.0 && zn[j] + dj < s->lo[j] - col_tol[j]) {
                const double a = (s->lo[j] - zn[j]) / dj;
                if (a < alpha)
                    alpha = a;
            }
            if ((k & HAS_UP) && dj > 0.0 && zn[j] + dj > s->up[j] + col_tol[j]) {
                const double a = (s->up[j] - zn[j]) / dj;
                if (a < alpha)
                    alpha = a;
            }
        }
        if (!(alpha > 0.0))
            alpha = 0.0;
        moved = 0;
        for (int64_t j = 0; j < nv; j++) {
            const uint8_t k = s->kind[j];
            if (k == FIXED)
                continue;
            if (pin[j] != PUSH_FREE) {
                zn[j] = alpha == 1.0 ? zt[j] : zn[j] + alpha * (zt[j] - zn[j]);
                continue;
            }
            const double dj = s->dz[j];
            zn[j] += alpha * dj;
            if (alpha == 1.0)
                continue;
            if ((k & HAS_LO) && dj < 0.0 && zn[j] <= s->lo[j] + near_p) {
                pin[j] = PUSH_LOWER;
                zn[j] = s->lo[j];
                moved++;
            } else if ((k & HAS_UP) && dj > 0.0 &&
                       zn[j] >= s->up[j] - near_p) {
                pin[j] = PUSH_UPPER;
                zn[j] = s->up[j];
                moved++;
            }
        }
        for (int64_t i = 0; i < nr; i++)
            yn[i] += s->dy[i];
        pinned += moved;
        jm_work_add(&s->work, (3 * nv + nr) * JM_WORK_NONZERO);
        if (alpha < 1.0) {
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "  push round %lld: step %.3e, %lld more pinned, %lld "
                   "pinned in all",
                   (long long)round, alpha, (long long)moved,
                   (long long)pinned);
            if (moved == 0) {
                stuck = true;
                break;
            }
            fresh = true;
            continue;
        }

        mul_e(s, zn, s->rp);
        wrong = 0;
        double pres = 0.0;
        for (int64_t i = 0; i < nr; i++) {
            if (!(fabs(s->rp[i]) <= row_tol[i]))
                wrong++;
            if (!(fabs(s->rp[i]) <= pres))
                pres = fabs(s->rp[i]);
        }
        for (int64_t j = 0; j < nv; j++)
            if (!isfinite(zn[j]))
                wrong++;
        for (int64_t i = 0; i < nr; i++)
            if (!isfinite(yn[i]))
                wrong++;
        if (wrong > 0) {
            int64_t released = 0;
            if (isfinite(pres)) {
                for (int64_t i = 0; i < nr; i++) {
                    if (!(fabs(s->rp[i]) > row_tol[i]))
                        continue;
                    int64_t best = -1;
                    double slack = HUGE_VAL;
                    for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1];
                         p++) {
                        const int64_t j = m->ar_index[p];
                        if (s->kind[j] == FIXED || pin[j] == PUSH_FREE)
                            continue;
                        const double sl = pin[j] == PUSH_LOWER ? s->zl[j]
                                                               : s->zu[j];
                        if (sl < slack) {
                            slack = sl;
                            best = j;
                        }
                    }
                    const int64_t sj = s->ncol + i;
                    if (s->kind[sj] != FIXED && pin[sj] != PUSH_FREE) {
                        const double sl = pin[sj] == PUSH_LOWER ? s->zl[sj]
                                                                : s->zu[sj];
                        if (sl < slack) {
                            slack = sl;
                            best = sj;
                        }
                    }
                    if (best >= 0) {
                        pin[best] = PUSH_FREE;
                        pinned--;
                        released++;
                    }
                }
                jm_work_add(&s->work, (nr + m->num_nz) * JM_WORK_NONZERO);
            }
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "  push round %lld: full step, %lld rows or values off, "
                   "the rows by up to %.3e against %.3e, %lld pins released",
                   (long long)round, (long long)wrong, pres, tol_p,
                   (long long)released);
            if (released == 0) {
                stuck = true;
                break;
            }
            releases += released;
            fresh = true;
            continue;
        }
        mul_et(s, yn, s->tmp);
        qz = grad_q(s, zn);
        wrong = 0;
        worst_sign = 0.0;
        int64_t loose = 0;
        for (int64_t j = 0; j < nv; j++) {
            const uint8_t k = s->kind[j];
            const double gq = qz != nullptr ? qz[j] : s->quad[j] * zn[j];
            d[j] = k == FIXED ? 0.0 : s->cost[j] + gq - s->tmp[j];
            if (k == FIXED)
                continue;
            const double thr_user = j < s->ncol
                                        ? QP_PUSH_USER_TOL * gamma[j]
                                        : QP_PUSH_USER_TOL / rho[j - s->ncol];
            const double thr = thr_user < tol_d ? thr_user : tol_d;
            if (pin[j] == PUSH_FREE) {
                if (fabs(d[j]) > thr)
                    loose++;
            } else if (pin[j] == PUSH_LOWER && d[j] < -thr) {
                wrong++;
                if (-d[j] > worst_sign)
                    worst_sign = -d[j];
            } else if (pin[j] == PUSH_UPPER && d[j] > thr) {
                wrong++;
                if (d[j] > worst_sign)
                    worst_sign = d[j];
            }
        }
        jm_work_add(&s->work, 2 * nv * JM_WORK_NONZERO);
        jm_log(s->m, JAOS_LOG_DETAIL,
               "  push round %lld: full step, %lld pinned, %lld with the "
               "wrong sign, %lld free with a reduced cost",
               (long long)round, (long long)pinned, (long long)wrong,
               (long long)loose);
        if (wrong == 0 && loose == 0) {
            settled = true;
            break;
        }
        if (wrong == 0)
            continue;
        if (freed >= QP_PUSH_FREEINGS)
            break;
        for (int64_t j = 0; j < nv; j++) {
            if (s->kind[j] == FIXED || pin[j] == PUSH_FREE)
                continue;
            const double thr_user = j < s->ncol
                                        ? QP_PUSH_USER_TOL * gamma[j]
                                        : QP_PUSH_USER_TOL / rho[j - s->ncol];
            const double thr = thr_user < tol_d ? thr_user : tol_d;
            if ((pin[j] == PUSH_LOWER && d[j] < -thr) ||
                (pin[j] == PUSH_UPPER && d[j] > thr)) {
                pin[j] = PUSH_FREE;
                pinned--;
            }
        }
        freed++;
        fresh = true;
    }
    if (st == JAOS_OK && settled) {
        s->pushed = true;
        memcpy(s->z, zn, (size_t)nv * sizeof *zn);
        memcpy(s->y, yn, (size_t)nr * sizeof *yn);
        for (int64_t j = 0; j < nv; j++) {
            const uint8_t k = s->kind[j];
            if (k == FIXED)
                continue;
            s->w[j] = (k & HAS_LO) ? s->z[j] - s->lo[j] : 0.0;
            s->v[j] = (k & HAS_UP) ? s->up[j] - s->z[j] : 0.0;
            s->zl[j] = pin[j] == PUSH_LOWER && d[j] > 0.0 ? d[j] : 0.0;
            s->zu[j] = pin[j] == PUSH_UPPER && d[j] < 0.0 ? -d[j] : 0.0;
        }
        jm_log(s->m, JAOS_LOG_SUMMARY,
               "push: %lld of %lld variables pinned on a bound, settled in "
               "%lld round%s, %lld freed",
               (long long)pinned, (long long)nv, (long long)round,
               round == 1 ? "" : "s", (long long)freed);
    } else if (st == JAOS_OK && stuck) {
        jm_log(s->m, JAOS_LOG_SUMMARY,
               "push: the step of round %lld left %lld rows unsatisfied; "
               "the barrier's point stands",
               (long long)round, (long long)wrong);
    } else if (st == JAOS_OK && releases > 0 && !settled && round > QP_PUSH_ROUNDS) {
        jm_log(s->m, JAOS_LOG_SUMMARY,
               "push: the rows stayed unsatisfied after %lld rounds and %lld "
               "pins released; the barrier's point stands",
               (long long)QP_PUSH_ROUNDS, (long long)releases);
    } else if (st == JAOS_OK && wrong > 0) {
        jm_log(s->m, JAOS_LOG_SUMMARY,
               "push: %lld pinned variables have a reduced cost of the wrong "
               "sign after %lld freeings, the worst %.3e against %.3e; the "
               "barrier's point stands",
               (long long)wrong, (long long)freed, worst_sign, tol_d);
    } else if (st == JAOS_OK) {
        jm_log(s->m, JAOS_LOG_SUMMARY,
               "push: the active set did not settle in %lld rounds, %lld "
               "pinned; the barrier's point stands",
               (long long)QP_PUSH_ROUNDS, (long long)pinned);
    } else {
        jm_log(s->m, JAOS_LOG_SUMMARY,
               "push: the factorisation failed in round %lld (%s); the "
               "barrier's point stands",
               (long long)round, jaos_status_str(st));
        st = JAOS_OK;
    }
    s->pin = nullptr;
    s->dec = nullptr;
    free(pin);
    free(zn);
    free(yn);
    free(d);
    free(dec);
    free(rt2);
    free(rp2);
    free(dz2);
    free(dy2);
    free(row_tol);
    free(col_tol);
    return st;
}

jaos_status jm_barrier(jaos_model *m, jaos_model *target, jm_presolve *p,
                       jm_work *work, bool *crossover, bool *handoff,
                       int64_t *iters)
{
    *crossover = false;
    *handoff = false;
    *iters = 0;
    bx s;
    jaos_status st = bx_init(&s, target);
    if (st != JAOS_OK)
        return st;
    s.work = *work;
    s.started = jm_monotonic_seconds();

    jm_log(m, JAOS_LOG_SUMMARY,
           "barrier: %lld rows, %lld columns, %lld nonzeros, tolerance %.3g, "
           "%lld dense columns left out of the normal matrix",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, BARRIER_TOL, (long long)s.ndense);

    jaos_solve_status outcome = JAOS_SOLVE_NUMERICAL_ERROR;
    st = bx_run(&s, &outcome, false);
    *iters = s.iters;
    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL &&
        !m->cfg.barrier_no_crossover && !s.quadratic) {
        st = crash_basis(&s);
        *work = s.work;
        *crossover = st == JAOS_OK;
        jm_log(m, JAOS_LOG_SUMMARY,
               "barrier converged after %lld iterations, %lld work units; "
               "crossing over to the simplex from its basis guess",
               (long long)s.iters, (long long)s.work.units);
        bx_free(&s);
        return st;
    }
    if (st == JAOS_OK && outcome == JAOS_SOLVE_NUMERICAL_ERROR && s.handoff &&
        s.quadratic && s.near && !m->cfg.barrier_no_crossover) {
        st = qp_push(&s);
        if (st != JAOS_OK) {
            bx_free(&s);
            return st;
        }
        if (s.pushed) {
            jm_log(m, JAOS_LOG_SUMMARY,
                   "barrier stopped after %lld iterations within %.1e of "
                   "converged, and the push settled from there",
                   (long long)s.iters, BARRIER_NEAR_TOL);
            outcome = JAOS_SOLVE_OPTIMAL;
            s.handoff = false;
            target->err[0] = '\0';
            m->err[0] = '\0';
        }
    }
    if (st == JAOS_OK && outcome == JAOS_SOLVE_NUMERICAL_ERROR && s.handoff) {
        if (s.quadratic)
            jm_log(m, JAOS_LOG_SUMMARY,
                   "barrier stopped after %lld iterations, %lld work units: "
                   "%s; the dual simplex decides feasibility over the same "
                   "rows and bounds, which the quadratic term does not change",
                   (long long)s.iters, (long long)s.work.units, target->err);
        else
            jm_log(m, JAOS_LOG_SUMMARY,
                   "barrier stopped after %lld iterations, %lld work units: "
                   "%s; the dual simplex takes over from the slack basis",
                   (long long)s.iters, (long long)s.work.units, target->err);
        if (!s.quadratic)
            target->err[0] = '\0';
        *work = s.work;
        *handoff = true;
        bx_free(&s);
        return JAOS_OK;
    }
    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL && s.quadratic &&
        !s.pushed && !m->cfg.barrier_no_crossover)
        st = qp_push(&s);
    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL && s.quadratic &&
        !s.pushed && !m->cfg.barrier_no_crossover) {
        const size_t nvb = (size_t)s.nvar * sizeof(double);
        const size_t nrb = (size_t)s.nrow * sizeof(double);
        double *kz = jm_alloc_array(s.nvar, sizeof(double));
        double *kw = jm_alloc_array(s.nvar, sizeof(double));
        double *kv = jm_alloc_array(s.nvar, sizeof(double));
        double *kl = jm_alloc_array(s.nvar, sizeof(double));
        double *ku = jm_alloc_array(s.nvar, sizeof(double));
        double *ky = jm_alloc_array(s.nrow > 0 ? s.nrow : 1, sizeof(double));
        if (kz && kw && kv && kl && ku && ky) {
            memcpy(kz, s.z, nvb);  memcpy(kw, s.w, nvb);  memcpy(kv, s.v, nvb);
            memcpy(kl, s.zl, nvb); memcpy(ku, s.zu, nvb); memcpy(ky, s.y, nrb);
            const int64_t first_iters = s.iters;
            s.tol_stop = BARRIER_TOL_QP;
            s.iter_cap = s.iters + BARRIER_LEG2_ITERS;
            s.handoff = false;
            jaos_solve_status again = JAOS_SOLVE_NUMERICAL_ERROR;
            st = bx_run(&s, &again, true);
            if (st == JAOS_OK && again == JAOS_SOLVE_OPTIMAL) {
                st = qp_push(&s);
                jm_log(m, JAOS_LOG_SUMMARY,
                       "the push did not settle at %.1e, so the walk went on "
                       "to %.1e in %lld more iterations and the push %s",
                       BARRIER_TOL, BARRIER_TOL_QP,
                       (long long)(s.iters - first_iters),
                       s.pushed ? "settled" : "did not settle there either");
            } else if (st == JAOS_OK) {
                memcpy(s.z, kz, nvb);  memcpy(s.w, kw, nvb);  memcpy(s.v, kv, nvb);
                memcpy(s.zl, kl, nvb); memcpy(s.zu, ku, nvb); memcpy(s.y, ky, nrb);
                s.handoff = false;
                target->err[0] = '\0';
                jm_log(m, JAOS_LOG_SUMMARY,
                       "the push did not settle at %.1e, the walk could not "
                       "reach %.1e in %lld more iterations, and the point at "
                       "%.1e stands",
                       BARRIER_TOL, BARRIER_TOL_QP,
                       (long long)(s.iters - first_iters), BARRIER_TOL);
            }
        }
        free(kz); free(kw); free(kv); free(kl); free(ku); free(ky);
        *iters = s.iters;
    }
    if (st == JAOS_OK)
        st = bx_publish(&s, outcome, p);

    if (target != m && target->err[0] != '\0' &&
        (st != JAOS_OK || outcome == JAOS_SOLVE_NUMERICAL_ERROR))
        memcpy(m->err, target->err, sizeof m->err);

    if (st != JAOS_OK)
        m->solve_iters = s.iters;
    m->solve_primal_iters = 0;
    m->solve_phase1_iters = 0;

    if (st == JAOS_OK)
        jm_log(m, JAOS_LOG_SUMMARY,
               "%s after %lld barrier iterations, %lld work units; %lld "
               "nonzeros in the Cholesky factor, %lld pivots replaced in the "
               "last factorisation",
               jaos_solve_status_str(outcome), (long long)s.iters,
               (long long)s.work.units, (long long)s.chol.nnz,
               (long long)s.chol.replaced);
    else
        jm_log(m, JAOS_LOG_SUMMARY,
               "abandoned after %lld barrier iterations, %lld work units: %s",
               (long long)s.iters, (long long)s.work.units,
               jaos_status_str(st));

    bx_free(&s);
    return st;
}
