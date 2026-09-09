/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include "jaos_sys.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double  BARRIER_TOL      = 1e-8;
constexpr double  BARRIER_STEP     = 0.99995;
constexpr double  BARRIER_REG      = 1e-9;
constexpr double  BARRIER_FREE_REG = 1e-8;
constexpr double  BARRIER_DELTA    = 1e-10;
constexpr int64_t BARRIER_MAX_ITER = 200;
constexpr double  BARRIER_DIVERGE  = 1e6;

enum { HAS_LO = 1, HAS_UP = 2, FIXED = 4 };

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    double *av, *arv;
    double *lo, *up, *cost;
    uint8_t *kind;
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

    jm_work work;
    double started;
    int64_t iters;
    int64_t bounded;
    bool handoff;
} bx;

static void bx_free(bx *s)
{
    free(s->av);    free(s->arv);
    free(s->lo);    free(s->up);    free(s->cost);  free(s->kind);
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
    jm_chol_free(&s->chol);
    memset(s, 0, sizeof *s);
}

static jaos_status bx_init(bx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    jm_chol_init(&s->chol);
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
    if (!s->av || !s->arv || !s->lo || !s->up || !s->cost || !s->kind ||
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
    return JAOS_OK;
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
                if (s->kind[j] == FIXED)
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

static jaos_status form_normal(bx *s)
{
    const jaos_model *m = s->m;
    int64_t terms = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            const int64_t j = m->ar_index[p];
            if (s->kind[j] == FIXED)
                continue;
            const double t = s->arv[p] * s->theta[j];
            for (int64_t q = m->a_start[j]; q < m->a_start[j + 1]; q++)
                s->acc[m->a_index[q]] += t * s->av[q];
            terms += m->a_start[j + 1] - m->a_start[j];
        }
        if (s->kind[s->ncol + i] != FIXED)
            s->acc[i] += s->theta[s->ncol + i];
        s->acc[i] += BARRIER_DELTA;
        for (int64_t q = s->n_start[i]; q < s->n_start[i + 1]; q++) {
            const int64_t k = s->n_index[q];
            s->n_value[q] = s->acc[k];
            s->acc[k] = 0.0;
        }
    }
    jm_work_add(&s->work, (terms + s->n_start[s->nrow]) * JM_WORK_NONZERO);
    return jm_chol_numeric(&s->chol, s->n_value, &s->work);
}

static void newton(bx *s, const double *r, const double *rt, double *dy,
                   double *dz)
{
    for (int64_t j = 0; j < s->nvar; j++)
        s->tmp[j] = s->kind[j] == FIXED ? 0.0 : s->theta[j] * rt[j];
    mul_e(s, s->tmp, s->rhs);
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = r[i] + s->rhs[i];
    jm_chol_solve(&s->chol, dy, &s->work);
    mul_et(s, dy, dz);
    for (int64_t j = 0; j < s->nvar; j++)
        dz[j] = s->kind[j] == FIXED ? 0.0 : s->theta[j] * (dz[j] - rt[j]);
    jm_work_add(&s->work, 2 * s->nvar * JM_WORK_NONZERO);
}

static void residuals(bx *s, double *pobj, double *dobj, double *mu)
{
    mul_e(s, s->z, s->rp);
    for (int64_t i = 0; i < s->nrow; i++)
        s->rp[i] = -s->rp[i];
    mul_et(s, s->y, s->rd);
    double po = s->fixed_obj, dob = 0.0, prod = 0.0;
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        if (k == FIXED) {
            s->rd[j] = 0.0;
            s->rw[j] = s->rv[j] = 0.0;
            continue;
        }
        po += s->cost[j] * s->z[j];
        s->rd[j] = s->cost[j] - s->rd[j] - s->zl[j] + s->zu[j];
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
    for (int64_t j = 0; j < nv; j++)
        s->theta[j] = s->kind[j] == FIXED ? 0.0 : 1.0;
    jaos_status st = form_normal(s);
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
    if (prod <= 0.0)
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
        for (int64_t j = 0; j < m->num_col; j++) {
            m->sol_col[j] = published(gamma[j] * s->z[j]);
            m->sol_redcost[j] =
                published(sigma * (s->cost[j] - s->tmp[j]) / gamma[j]);
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

static jaos_status bx_run(bx *s, jaos_solve_status *out)
{
    jaos_model *m = s->m;
    jaos_status st = build_normal_pattern(s);
    if (st != JAOS_OK)
        return st;
    st = starting_point(s);
    if (st != JAOS_OK)
        return st;

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
        if (rel_p <= BARRIER_TOL && rel_d <= BARRIER_TOL && gap <= BARRIER_TOL) {
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
            if (pgrow > BARRIER_DIVERGE || dgrow > BARRIER_DIVERGE) {
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
        if (s->iters >= BARRIER_MAX_ITER) {
            jm_set_err(m, "the barrier did not converge in %lld iterations "
                          "(primal %.3e, dual %.3e, gap %.3e): the model may "
                          "be infeasible or unbounded, which the barrier does "
                          "not certify; the dual simplex does",
                       (long long)s->iters, rel_p, rel_d, gap);
            s->handoff = true;
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        double worst = rel_p > rel_d ? rel_p : rel_d;
        if (gap > worst)
            worst = gap;
        const double reg = BARRIER_REG * (worst < 1.0 ? worst : 1.0);
        for (int64_t j = 0; j < s->nvar; j++) {
            const uint8_t k = s->kind[j];
            if (k == FIXED) {
                s->theta[j] = 0.0;
                continue;
            }
            double inv = k == 0 ? BARRIER_FREE_REG : reg;
            if (k & HAS_LO) inv += s->zl[j] / s->w[j];
            if (k & HAS_UP) inv += s->zu[j] / s->v[j];
            s->theta[j] = 1.0 / inv;
        }
        st = form_normal(s);
        if (st != JAOS_OK)
            return st;

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

        complementarity(s, sig * mu, true);
        direction(s, s->dw, s->dv, s->dzl, s->dzu);
        step_lengths(s, s->dw, s->dv, s->dzl, s->dzu, &ap, &ad);
        ap *= BARRIER_STEP;
        ad *= BARRIER_STEP;
        if (ap > 1.0) ap = 1.0;
        if (ad > 1.0) ad = 1.0;

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

static jaos_status crash_basis(bx *s)
{
    jaos_model *m = s->m;
    const int64_t nv = s->nvar, nr = s->nrow;
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
        const uint8_t k = s->kind[j];
        double sc;
        if (k == FIXED)
            sc = -1.0;
        else if (k == 0)
            sc = 2.0;
        else {
            double pd = HUGE_VAL, dd = 0.0;
            if (k & HAS_LO) { pd = s->w[j]; dd = s->zl[j]; }
            if (k & HAS_UP) {
                if (s->v[j] < pd) pd = s->v[j];
                if (s->zu[j] > dd) dd = s->zu[j];
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
        jm_work_add(&s->work, nv * (bits + 2) * JM_WORK_NONZERO);
    }

    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = s->kind[j];
        if (k == 0)
            want[j] = JAOS_BASIS_FREE;
        else if (k == HAS_UP)
            want[j] = JAOS_BASIS_AT_UPPER;
        else if (k == (HAS_LO | HAS_UP) && s->v[j] < s->w[j])
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
            const int64_t v = basis[q];
            if (v < s->ncol) {
                for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                    bi[p] = m->a_index[k];
                    bv[p] = s->av[k];
                    p++;
                }
            } else {
                bi[p] = v - s->ncol;
                bv[p] = -1.0;
                p++;
            }
        }
        bs[nr] = p;
        st = jm_lu_factor(&lu, nr, bs, bi, bv, LU_PIVOT_TOL, &s->work);
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
            while (i < nr && (covered[i] || want[s->ncol + i] == JAOS_BASIS_BASIC))
                i++;
            if (i >= nr)
                break;
            const int64_t leaving = basis[q];
            want[leaving] = s->kind[leaving] == 0 ? JAOS_BASIS_FREE
                            : (s->kind[leaving] == HAS_UP ? JAOS_BASIS_AT_UPPER
                                                          : JAOS_BASIS_AT_LOWER);
            basis[q] = s->ncol + i;
            want[s->ncol + i] = JAOS_BASIS_BASIC;
            i++;
        }
        jm_work_add(&s->work, 2 * nr * JM_WORK_NONZERO);
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
        structural += basis[q] < s->ncol;
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
           "barrier: %lld rows, %lld columns, %lld nonzeros, tolerance %.3g",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, BARRIER_TOL);

    jaos_solve_status outcome = JAOS_SOLVE_NUMERICAL_ERROR;
    st = bx_run(&s, &outcome);
    *iters = s.iters;
    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL &&
        !m->cfg.barrier_no_crossover) {
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
    if (st == JAOS_OK && outcome == JAOS_SOLVE_NUMERICAL_ERROR && s.handoff) {
        jm_log(m, JAOS_LOG_SUMMARY,
               "barrier stopped after %lld iterations, %lld work units: %s; "
               "the dual simplex takes over from the slack basis",
               (long long)s.iters, (long long)s.work.units, target->err);
        target->err[0] = '\0';
        *work = s.work;
        *handoff = true;
        bx_free(&s);
        return JAOS_OK;
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
