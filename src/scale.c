/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

constexpr int    CR_MAX_ITER  = 30;
constexpr double CR_TOL       = 1e-8;
constexpr int    GEO_MAX_PASS = 20;
constexpr double GEO_TOL      = 1e-3;

constexpr double EXP_LIMIT = 512.0;

static double pow2_of(double exponent, bool *clamped)
{
    if (!isfinite(exponent)) {
        *clamped = true;
        return 1.0;
    }
    double r = round(exponent);
    if (r > EXP_LIMIT) {
        r = EXP_LIMIT;
        *clamped = true;
    } else if (r < -EXP_LIMIT) {
        r = -EXP_LIMIT;
        *clamped = true;
    }
    return ldexp(1.0, (int)r);
}

double jm_scaled_abs(const jaos_model *m, int64_t j, int64_t k)
{
    double v = fabs(m->a_value[k]);
    if (m->scale_valid)
        v *= m->row_scale[m->a_index[k]] * m->col_scale[j];
    return v;
}

static void cr_matvec(const jaos_model *m, const double *nr, const double *nc,
                      const double *pr, const double *pc,
                      double *qr, double *qc)
{
    for (int64_t i = 0; i < m->num_row; i++)
        qr[i] = nr[i] * pr[i];
    for (int64_t j = 0; j < m->num_col; j++) {
        double acc = nc[j] * pc[j];
        double pcj = pc[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            int64_t i = m->a_index[k];
            qr[i] += pcj;
            acc += pr[i];
        }
        qc[j] = acc;
    }
}

static jaos_status scale_curtis_reid(jaos_model *m)
{
    const int64_t nrow = m->num_row, ncol = m->num_col;
    jaos_status st = JAOS_OK;

    double *nr = jm_calloc_array(nrow, sizeof(double));
    double *nc = jm_calloc_array(ncol, sizeof(double));
    double *sr = jm_calloc_array(nrow, sizeof(double));
    double *sc = jm_calloc_array(ncol, sizeof(double));
    double *r  = jm_calloc_array(nrow, sizeof(double));
    double *c  = jm_calloc_array(ncol, sizeof(double));
    double *zr = jm_calloc_array(nrow, sizeof(double));
    double *zc = jm_calloc_array(ncol, sizeof(double));
    double *pr = jm_calloc_array(nrow, sizeof(double));
    double *pc = jm_calloc_array(ncol, sizeof(double));
    double *qr = jm_calloc_array(nrow, sizeof(double));
    double *qc = jm_calloc_array(ncol, sizeof(double));
    if (!nr || !nc || !sr || !sc || !r || !c || !zr || !zc || !pr || !pc ||
        !qr || !qc) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }

    for (int64_t j = 0; j < ncol; j++) {
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            int64_t i = m->a_index[k];
            double l = log2(fabs(m->a_value[k]));
            nr[i] += 1.0;
            nc[j] += 1.0;
            sr[i] += l;
            sc[j] += l;
        }
    }
    for (int64_t i = 0; i < nrow; i++)
        if (nr[i] == 0.0)
            nr[i] = 1.0;
    for (int64_t j = 0; j < ncol; j++)
        if (nc[j] == 0.0)
            nc[j] = 1.0;

    double rz = 0.0;
    for (int64_t i = 0; i < nrow; i++) {
        zr[i] = sr[i] / nr[i];
        pr[i] = zr[i];
        rz += sr[i] * zr[i];
    }
    for (int64_t j = 0; j < ncol; j++) {
        zc[j] = sc[j] / nc[j];
        pc[j] = zc[j];
        rz += sc[j] * zc[j];
    }
    const double rz0 = rz;

    for (int it = 0; it < CR_MAX_ITER && rz > CR_TOL * CR_TOL * rz0 &&
                     rz > 0.0; it++) {
        cr_matvec(m, nr, nc, pr, pc, qr, qc);

        double pq = 0.0;
        for (int64_t i = 0; i < nrow; i++)
            pq += pr[i] * qr[i];
        for (int64_t j = 0; j < ncol; j++)
            pq += pc[j] * qc[j];
        if (!(pq > 0.0))
            break;

        double alpha = rz / pq;
        for (int64_t i = 0; i < nrow; i++) {
            r[i] += alpha * pr[i];
            sr[i] -= alpha * qr[i];
        }
        for (int64_t j = 0; j < ncol; j++) {
            c[j] += alpha * pc[j];
            sc[j] -= alpha * qc[j];
        }

        double rz_new = 0.0;
        for (int64_t i = 0; i < nrow; i++) {
            zr[i] = sr[i] / nr[i];
            rz_new += sr[i] * zr[i];
        }
        for (int64_t j = 0; j < ncol; j++) {
            zc[j] = sc[j] / nc[j];
            rz_new += sc[j] * zc[j];
        }
        double beta = rz_new / rz;
        for (int64_t i = 0; i < nrow; i++)
            pr[i] = zr[i] + beta * pr[i];
        for (int64_t j = 0; j < ncol; j++)
            pc[j] = zc[j] + beta * pc[j];
        rz = rz_new;
    }

    for (int64_t i = 0; i < nrow; i++)
        m->row_scale[i] = pow2_of(-r[i], &m->scale_clamped);
    for (int64_t j = 0; j < ncol; j++)
        m->col_scale[j] = pow2_of(-c[j], &m->scale_clamped);

done:
    free(nr); free(nc); free(sr); free(sc); free(r); free(c);
    free(zr); free(zc); free(pr); free(pc); free(qr); free(qc);
    return st;
}

static jaos_status scale_geometric(jaos_model *m)
{
    const int64_t nrow = m->num_row, ncol = m->num_col;

    double *la = jm_alloc_array(m->num_nz, sizeof(double));
    double *lr = jm_calloc_array(nrow, sizeof(double));
    double *lc = jm_calloc_array(ncol, sizeof(double));
    double *rmin = jm_alloc_array(nrow, sizeof(double));
    double *rmax = jm_alloc_array(nrow, sizeof(double));
    if (!la || !lr || !lc || !rmin || !rmax) {
        free(la); free(lr); free(lc); free(rmin); free(rmax);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t k = 0; k < m->num_nz; k++)
        la[k] = log2(fabs(m->a_value[k]));

    double prev_spread = HUGE_VAL;
    for (int pass = 0; pass < GEO_MAX_PASS; pass++) {
        for (int64_t i = 0; i < nrow; i++) {
            rmin[i] = HUGE_VAL;
            rmax[i] = -HUGE_VAL;
        }
        for (int64_t j = 0; j < ncol; j++)
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                int64_t i = m->a_index[k];
                double v = la[k] + lr[i] + lc[j];
                if (v < rmin[i]) rmin[i] = v;
                if (v > rmax[i]) rmax[i] = v;
            }

        double spread = 0.0;
        for (int64_t i = 0; i < nrow; i++) {
            if (rmax[i] == -HUGE_VAL)
                continue;
            if (rmax[i] - rmin[i] > spread)
                spread = rmax[i] - rmin[i];
            lr[i] -= 0.5 * (rmin[i] + rmax[i]);
        }

        for (int64_t j = 0; j < ncol; j++) {
            double cmin = HUGE_VAL, cmax = -HUGE_VAL;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                double v = la[k] + lr[m->a_index[k]] + lc[j];
                if (v < cmin) cmin = v;
                if (v > cmax) cmax = v;
            }
            if (cmax != -HUGE_VAL)
                lc[j] -= 0.5 * (cmin + cmax);
        }

        if (spread <= 0.0 || prev_spread - spread < GEO_TOL)
            break;
        prev_spread = spread;
    }

    for (int64_t i = 0; i < nrow; i++)
        m->row_scale[i] = pow2_of(lr[i], &m->scale_clamped);
    for (int64_t j = 0; j < ncol; j++)
        m->col_scale[j] = pow2_of(lc[j], &m->scale_clamped);

    free(la); free(lr); free(lc); free(rmin); free(rmax);
    return JAOS_OK;
}

static void identity_fill(jaos_model *m)
{
    for (int64_t i = 0; i < m->num_row; i++)
        m->row_scale[i] = 1.0;
    for (int64_t j = 0; j < m->num_col; j++)
        m->col_scale[j] = 1.0;
    m->scale_clamped = false;
}

jaos_status jm_model_scale(jaos_model *m, jm_scale_mode mode)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (mode != JM_SCALE_NONE && mode != JM_SCALE_CURTIS_REID &&
        mode != JM_SCALE_GEOMETRIC)
        return JAOS_ERR_INVALID_INPUT;

    double *rs = jm_alloc_array(m->num_row, sizeof(double));
    double *cs = jm_alloc_array(m->num_col, sizeof(double));
    if (rs == nullptr || cs == nullptr) {
        free(rs);
        free(cs);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    free(m->row_scale);
    free(m->col_scale);
    m->row_scale = rs;
    m->col_scale = cs;
    m->scale_valid = true;
    m->scale_clamped = false;

    jaos_status st = JAOS_OK;
    if (mode == JM_SCALE_CURTIS_REID)
        st = scale_curtis_reid(m);
    else if (mode == JM_SCALE_GEOMETRIC)
        st = scale_geometric(m);
    else
        identity_fill(m);

    if (st != JAOS_OK) {

        identity_fill(m);
        jm_set_err(m, "out of memory while scaling");
    }

#ifndef NDEBUG
    for (int64_t i = 0; i < m->num_row; i++) {
        int e;
        assert(m->row_scale[i] > 0.0 && frexp(m->row_scale[i], &e) == 0.5);
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        int e;
        assert(m->col_scale[j] > 0.0 && frexp(m->col_scale[j], &e) == 0.5);
    }
#endif

    return st;
}
