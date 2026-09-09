/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include "jaos_sys.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double  PDLP_TOL                = 1e-4;
constexpr int64_t PDLP_MAX_ITER           = 200000;
constexpr int64_t PDLP_CHECK_EVERY        = 64;
constexpr double  PDLP_RESTART_SUFFICIENT = 0.2;
constexpr double  PDLP_RESTART_NECESSARY  = 0.8;
constexpr double  PDLP_RESTART_ARTIFICIAL = 0.36;
constexpr int64_t PDLP_STEP_TRIES         = 64;
constexpr int64_t PDLP_RUIZ_ROUNDS        = 10;
constexpr double  PDLP_INFEAS_TOL         = 1e-8;
constexpr int64_t PDLP_INFEAS_FROM        = 4096;

enum { HAS_LO = JM_BX_LO, HAS_UP = JM_BX_UP, FIXED = JM_BX_FIXED };

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    double *av, *av0, *rs, *cs, *sl;
    double *lo, *up, *cost;
    uint8_t *kind;
    double norm_c, norm_bound;

    double *z, *y, *ez, *g;
    double *zn, *yn, *ezn;
    double *zs, *ys;
    double ws;
    double *za, *ya, *eza, *ga;
    double *zr, *yr;
    double *lam;
    double *zc, *yc, *dz, *dy, *dg, *de;
    bool snapped;

    double eta, omega;
    double kkt_restart, kkt_prev;
    int64_t epoch;

    jm_work work;
    double started;
    int64_t iters, restarts;
    bool handoff;
} px;

static void px_free(px *s)
{
    free(s->av);  free(s->av0); free(s->rs);   free(s->cs);   free(s->sl);
    free(s->lo);  free(s->up);  free(s->cost); free(s->kind);
    free(s->z);   free(s->y);   free(s->ez);   free(s->g);
    free(s->zn);  free(s->yn);  free(s->ezn);
    free(s->zs);  free(s->ys);
    free(s->za);  free(s->ya);  free(s->eza);  free(s->ga);
    free(s->zr);  free(s->yr);  free(s->lam);
    free(s->zc);  free(s->yc);  free(s->dz);
    free(s->dy);  free(s->dg);  free(s->de);
    memset(s, 0, sizeof *s);
}

static void scale_pass(px *s, double *rowagg, double *colagg, bool by_max)
{
    const jaos_model *m = s->m;
    const int64_t nr = s->nrow, nv = s->nvar;
    memset(rowagg, 0, (size_t)nr * sizeof(double));
    memset(colagg, 0, (size_t)nv * sizeof(double));
    for (int64_t j = 0; j < s->ncol; j++) {
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double a = fabs(s->av0[k] * s->rs[i] * s->cs[j]);
            if (by_max) {
                if (a > rowagg[i]) rowagg[i] = a;
                if (a > colagg[j]) colagg[j] = a;
            } else {
                rowagg[i] += a;
                colagg[j] += a;
            }
        }
    }
    for (int64_t i = 0; i < nr; i++) {
        const double a = fabs(s->rs[i] * s->cs[s->ncol + i]);
        if (by_max) {
            if (a > rowagg[i]) rowagg[i] = a;
            colagg[s->ncol + i] = a;
        } else {
            rowagg[i] += a;
            colagg[s->ncol + i] += a;
        }
    }
    for (int64_t i = 0; i < nr; i++)
        if (rowagg[i] > 0.0)
            s->rs[i] /= sqrt(rowagg[i]);
    for (int64_t j = 0; j < nv; j++)
        if (colagg[j] > 0.0)
            s->cs[j] /= sqrt(colagg[j]);
    jm_work_add(&s->work, (m->num_nz + 2 * nr + 2 * nv) * JM_WORK_NONZERO);
}

static void precondition(px *s)
{
    const jaos_model *m = s->m;
    const int64_t nr = s->nrow, nv = s->nvar;
    for (int64_t i = 0; i < nr; i++)
        s->rs[i] = 1.0;
    for (int64_t j = 0; j < nv; j++)
        s->cs[j] = 1.0;
    for (int64_t round = 0; round < PDLP_RUIZ_ROUNDS; round++)
        scale_pass(s, s->ez, s->g, true);
    scale_pass(s, s->ez, s->g, false);
    memset(s->ez, 0, (size_t)nr * sizeof(double));
    memset(s->g, 0, (size_t)nv * sizeof(double));

    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = s->av0[k] * s->rs[m->a_index[k]] * s->cs[j];
    for (int64_t i = 0; i < nr; i++)
        s->sl[i] = s->rs[i] * s->cs[s->ncol + i];
    for (int64_t j = 0; j < nv; j++) {
        s->lo[j] /= s->cs[j];
        s->up[j] /= s->cs[j];
        s->cost[j] *= s->cs[j];
    }
    jm_work_add(&s->work, (m->num_nz + nr + 3 * nv) * JM_WORK_NONZERO);
}

static jaos_status px_init(px *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }

    const int64_t nv = s->nvar, nr = s->nrow;
    s->av   = jm_alloc_array(m->num_nz > 0 ? m->num_nz : 1, sizeof(double));
    s->av0  = jm_alloc_array(m->num_nz > 0 ? m->num_nz : 1, sizeof(double));
    s->rs   = jm_alloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->cs   = jm_alloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->sl   = jm_alloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->lo   = jm_alloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->up   = jm_alloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->cost = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->kind = jm_calloc_array(nv > 0 ? nv : 1, sizeof(uint8_t));
    s->z    = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->y    = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->ez   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->g    = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->zn   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->yn   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->ezn  = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->zs   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->ys   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->za   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->ya   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->eza  = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->ga   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->zr   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->yr   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->lam  = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->zc   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->yc   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->dz   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->dy   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    s->dg   = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    s->de   = jm_calloc_array(nr > 0 ? nr : 1, sizeof(double));
    if (!s->av || !s->av0 || !s->rs || !s->cs || !s->sl ||
        !s->lo || !s->up || !s->cost || !s->kind || !s->z ||
        !s->y || !s->ez || !s->g || !s->zn || !s->yn || !s->ezn || !s->zs ||
        !s->ys || !s->za || !s->ya || !s->eza || !s->ga || !s->zr || !s->yr ||
        !s->lam || !s->zc || !s->yc || !s->dz || !s->dy || !s->dg ||
        !s->de) {
        px_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    const double *rho = m->row_scale, *gamma = m->col_scale;
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];

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
    memcpy(s->av0, s->av, (size_t)m->num_nz * sizeof(double));
    precondition(s);

    double c2 = 0.0, b2 = 0.0;
    for (int64_t j = 0; j < nv; j++) {
        uint8_t k = 0;
        if (isfinite(s->lo[j]))
            k |= HAS_LO;
        if (isfinite(s->up[j]))
            k |= HAS_UP;
        if (k == (HAS_LO | HAS_UP) && s->lo[j] == s->up[j])
            k = FIXED;
        s->kind[j] = k;
        double bound = 0.0;
        if (k & HAS_LO) bound = fabs(s->lo[j]);
        if (k & HAS_UP) { if (fabs(s->up[j]) > bound) bound = fabs(s->up[j]); }
        if (k == FIXED) bound = fabs(s->lo[j]);
        b2 += bound * bound;
        c2 += s->cost[j] * s->cost[j];
    }
    s->norm_c = sqrt(c2);
    s->norm_bound = sqrt(b2);
    jm_work_add(&s->work, (m->num_nz + 2 * nv) * JM_WORK_NONZERO);
    return JAOS_OK;
}

static void mul_e(px *s, const double *z, double *out)
{
    const jaos_model *m = s->m;
    for (int64_t i = 0; i < s->nrow; i++)
        out[i] = -s->sl[i] * z[s->ncol + i];
    for (int64_t j = 0; j < s->ncol; j++) {
        const double zj = z[j];
        if (zj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            out[m->a_index[k]] += s->av[k] * zj;
    }
    jm_work_add(&s->work, (m->num_nz + s->nrow) * JM_WORK_NONZERO);
}

static void mul_et(px *s, const double *y, double *out)
{
    const jaos_model *m = s->m;
    for (int64_t j = 0; j < s->ncol; j++) {
        double t = 0.0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            t += s->av[k] * y[m->a_index[k]];
        out[j] = t;
    }
    for (int64_t i = 0; i < s->nrow; i++)
        out[s->ncol + i] = -s->sl[i] * y[i];
    jm_work_add(&s->work, (m->num_nz + s->nrow) * JM_WORK_NONZERO);
}

static double clamp(const px *s, int64_t j, double v)
{
    const uint8_t k = s->kind[j];
    if (k == FIXED)
        return s->lo[j];
    if ((k & HAS_LO) && v < s->lo[j])
        return s->lo[j];
    if ((k & HAS_UP) && v > s->up[j])
        return s->up[j];
    return v;
}

typedef struct {
    double pres, dres, gap, pobj, dobj;
} kkt;

static double dual_slack(const px *s, int64_t j, double r)
{
    const uint8_t k = s->kind[j];
    if (k == FIXED)
        return r;
    if ((k & HAS_LO) && r > 0.0)
        return r;
    if ((k & HAS_UP) && r < 0.0)
        return r;
    return 0.0;
}

static void residuals(px *s, const double *z, const double *ez,
                      const double *g, double *lam, kkt *out)
{
    double p2 = 0.0, d2 = 0.0, pobj = 0.0, dobj = 0.0;
    for (int64_t i = 0; i < s->nrow; i++)
        p2 += ez[i] * ez[i];
    for (int64_t j = 0; j < s->nvar; j++) {
        const double r = s->cost[j] - g[j];
        const double l = dual_slack(s, j, r);
        const double d = r - l;
        d2 += d * d;
        pobj += s->cost[j] * z[j];
        if (l > 0.0)
            dobj += s->lo[j] * l;
        else if (l < 0.0)
            dobj += s->up[j] * l;
        if (lam != nullptr)
            lam[j] = l;
    }
    out->pres = sqrt(p2);
    out->dres = sqrt(d2);
    out->pobj = pobj;
    out->dobj = dobj;
    out->gap = fabs(pobj - dobj);
    jm_work_add(&s->work, (2 * s->nvar + s->nrow) * JM_WORK_NONZERO);
}

static double weighted(const px *s, const kkt *k)
{
    return sqrt(s->omega * k->pres * k->pres + k->dres * k->dres / s->omega +
                k->gap * k->gap);
}

static bool converged(const px *s, const kkt *k)
{
    return k->pres <= PDLP_TOL * (1.0 + s->norm_bound) &&
           k->dres <= PDLP_TOL * (1.0 + s->norm_c) &&
           k->gap <= PDLP_TOL * (1.0 + fabs(k->pobj) + fabs(k->dobj));
}

static double bound_value(const px *s, int64_t j, double l)
{
    if (l > 0.0)
        return s->lo[j] * l;
    if (l < 0.0)
        return s->up[j] * l;
    return 0.0;
}

static bool dual_ray(px *s, double *worth)
{
    mul_et(s, s->dy, s->dg);
    double res2 = 0.0, q = 0.0;
    for (int64_t j = 0; j < s->nvar; j++) {
        const double r = -s->dg[j];
        const double l = dual_slack(s, j, r);
        const double d = r - l;
        res2 += d * d;
        q += bound_value(s, j, l);
    }
    jm_work_add(&s->work, 2 * s->nvar * JM_WORK_NONZERO);
    *worth = q;
    return isfinite(q) && q > 0.0 && sqrt(res2) <= PDLP_INFEAS_TOL * q;
}

static bool primal_ray(px *s, double *worth)
{
    mul_e(s, s->dz, s->de);
    double res2 = 0.0, drop = 0.0;
    for (int64_t i = 0; i < s->nrow; i++)
        res2 += s->de[i] * s->de[i];
    for (int64_t j = 0; j < s->nvar; j++) {
        const uint8_t k = s->kind[j];
        const double d = s->dz[j];
        double out = 0.0;
        if (k == FIXED)
            out = fabs(d);
        else {
            if ((k & HAS_LO) && d < 0.0)
                out = -d;
            if ((k & HAS_UP) && d > 0.0)
                out = d;
        }
        res2 += out * out;
        drop += s->cost[j] * d;
    }
    jm_work_add(&s->work, 2 * s->nvar * JM_WORK_NONZERO);
    *worth = drop;
    return isfinite(drop) && drop < 0.0 &&
           sqrt(res2) <= PDLP_INFEAS_TOL * -drop;
}

static bool finite_point(const px *s, const double *z, const double *y)
{
    for (int64_t j = 0; j < s->nvar; j++)
        if (!isfinite(z[j]))
            return false;
    for (int64_t i = 0; i < s->nrow; i++)
        if (!isfinite(y[i]))
            return false;
    return true;
}

static void take_average(px *s)
{
    const double inv = 1.0 / s->ws;
    for (int64_t j = 0; j < s->nvar; j++)
        s->za[j] = clamp(s, j, s->zs[j] * inv);
    for (int64_t i = 0; i < s->nrow; i++)
        s->ya[i] = s->ys[i] * inv;
    jm_work_add(&s->work, (s->nvar + s->nrow) * JM_WORK_NONZERO);
    mul_e(s, s->za, s->eza);
    mul_et(s, s->ya, s->ga);
}

static void restart_at(px *s, bool average, double kkt_now)
{
    if (average) {
        memcpy(s->z, s->za, (size_t)s->nvar * sizeof(double));
        memcpy(s->y, s->ya, (size_t)s->nrow * sizeof(double));
        memcpy(s->ez, s->eza, (size_t)s->nrow * sizeof(double));
        memcpy(s->g, s->ga, (size_t)s->nvar * sizeof(double));
    }
    double dz2 = 0.0, dy2 = 0.0;
    for (int64_t j = 0; j < s->nvar; j++) {
        const double d = s->z[j] - s->zr[j];
        dz2 += d * d;
    }
    for (int64_t i = 0; i < s->nrow; i++) {
        const double d = s->y[i] - s->yr[i];
        dy2 += d * d;
    }
    if (dz2 > 0.0 && dy2 > 0.0) {
        const double w = sqrt(sqrt(dy2 / dz2) * s->omega);
        if (isfinite(w) && w > 0.0)
            s->omega = w;
    }
    memcpy(s->zr, s->z, (size_t)s->nvar * sizeof(double));
    memcpy(s->yr, s->y, (size_t)s->nrow * sizeof(double));
    memset(s->zs, 0, (size_t)s->nvar * sizeof(double));
    memset(s->ys, 0, (size_t)s->nrow * sizeof(double));
    s->ws = 0.0;
    s->epoch = 0;
    s->kkt_restart = kkt_now;
    s->kkt_prev = HUGE_VAL;
    s->restarts++;
    jm_work_add(&s->work, (3 * s->nvar + 3 * s->nrow) * JM_WORK_NONZERO);
}

static jaos_status px_run(px *s, jaos_solve_status *out)
{
    jaos_model *m = s->m;
    const int64_t nv = s->nvar, nr = s->nrow;

    for (int64_t j = 0; j < nv; j++)
        s->z[j] = clamp(s, j, 0.0);
    mul_e(s, s->z, s->ez);
    memset(s->g, 0, (size_t)nv * sizeof(double));
    s->omega = (s->norm_c > 0.0 && s->norm_bound > 0.0)
                   ? s->norm_c / s->norm_bound : 1.0;
    double emax = 0.0;
    for (int64_t k = 0; k < m->num_nz; k++)
        if (fabs(s->av[k]) > emax)
            emax = fabs(s->av[k]);
    for (int64_t i = 0; i < nr; i++)
        if (fabs(s->sl[i]) > emax)
            emax = fabs(s->sl[i]);
    s->eta = emax > 0.0 ? 1.0 / emax : 1.0;
    memcpy(s->zr, s->z, (size_t)nv * sizeof(double));
    memcpy(s->yr, s->y, (size_t)nr * sizeof(double));
    {
        kkt k0;
        residuals(s, s->z, s->ez, s->g, nullptr, &k0);
        s->kkt_restart = weighted(s, &k0);
    }
    s->kkt_prev = HUGE_VAL;
    jm_work_add(&s->work, (m->num_nz + nv) * JM_WORK_NONZERO);

    for (;;) {
        if (s->iters % PDLP_CHECK_EVERY == 0) {
            kkt kc, ka;
            residuals(s, s->z, s->ez, s->g, nullptr, &kc);
            double wc = weighted(s, &kc);
            bool use_avg = false;
            double wa = HUGE_VAL;
            if (s->ws > 0.0) {
                take_average(s);
                residuals(s, s->za, s->eza, s->ga, nullptr, &ka);
                wa = weighted(s, &ka);
                use_avg = wa < wc;
            }
            const kkt *kb = use_avg ? &ka : &kc;
            const double wb = use_avg ? wa : wc;

            jm_log(m, JAOS_LOG_PROGRESS,
                   "pdlp %lld: primal %.3e dual %.3e gap %.3e objective "
                   "%.10g, %lld restarts, step %.3e weight %.3e, %lld work "
                   "units",
                   (long long)s->iters, kb->pres, kb->dres, kb->gap, kb->pobj,
                   (long long)s->restarts, s->eta, s->omega,
                   (long long)s->work.units);

            if (!isfinite(wb) || !finite_point(s, s->z, s->y)) {
                jm_set_err(m, "the first-order method diverged after %lld "
                              "iterations: the model may be infeasible or "
                              "unbounded, which it does not certify; the "
                              "dual simplex does",
                           (long long)s->iters);
                s->handoff = true;
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
            if (converged(s, kb)) {
                if (use_avg)
                    restart_at(s, true, wb);
                *out = JAOS_SOLVE_OPTIMAL;
                return JAOS_OK;
            }
            const bool sufficient = wb <= PDLP_RESTART_SUFFICIENT * s->kkt_restart;
            const bool necessary = wb <= PDLP_RESTART_NECESSARY * s->kkt_restart &&
                                   wb > s->kkt_prev;
            const bool artificial =
                s->iters > 0 &&
                (double)s->epoch >= PDLP_RESTART_ARTIFICIAL * (double)s->iters;
            if (s->epoch > 0 && (sufficient || necessary || artificial))
                restart_at(s, use_avg, wb);
            else
                s->kkt_prev = wb;

            if (s->snapped && s->iters >= PDLP_INFEAS_FROM) {
                for (int64_t j = 0; j < nv; j++)
                    s->dz[j] = s->z[j] - s->zc[j];
                for (int64_t i = 0; i < nr; i++)
                    s->dy[i] = s->y[i] - s->yc[i];
                double worth = 0.0;
                const char *side = nullptr;
                if (dual_ray(s, &worth))
                    side = "dual";
                else if (primal_ray(s, &worth))
                    side = "primal";
                if (side != nullptr) {
                    jm_set_err(m, "the difference of the first-order method's "
                                  "iterates walks a %s ray worth %.3e after "
                                  "%lld iterations, so the model is "
                                  "infeasible or unbounded, which the ray "
                                  "does not tell apart; the dual simplex "
                                  "does", side, worth, (long long)s->iters);
                    s->handoff = true;
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
            }
            if (s->iters + PDLP_CHECK_EVERY >= PDLP_INFEAS_FROM) {
                memcpy(s->zc, s->z, (size_t)nv * sizeof(double));
                memcpy(s->yc, s->y, (size_t)nr * sizeof(double));
                s->snapped = true;
                jm_work_add(&s->work, (nv + nr) * JM_WORK_NONZERO);
            }
        }

        if (m->cfg.work_limit > 0 && s->work.units >= m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % PDLP_CHECK_EVERY == 0 && m->cfg.time_limit > 0.0 &&
            jm_monotonic_seconds() - s->started >= m->cfg.time_limit) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (m->cfg.progress_cb != nullptr && s->iters % PDLP_CHECK_EVERY == 0) {
            const jaos_progress pr = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->kkt_restart,
            };
            if (m->cfg.progress_cb(&pr, m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters >= PDLP_MAX_ITER) {
            jm_set_err(m, "the first-order method did not converge in %lld "
                          "iterations: the model may be infeasible or "
                          "unbounded, which it does not certify; the dual "
                          "simplex does",
                       (long long)s->iters);
            s->handoff = true;
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        const double k1 = (double)(s->iters + 1);
        const double shrink = 1.0 - 1.0 / sqrt(sqrt(k1));
        const double grow = 1.0 + 1.0 / sqrt(k1);
        double used = s->eta;
        for (int64_t attempt = 0; attempt < PDLP_STEP_TRIES; attempt++) {
            const double tau = s->eta / s->omega, sig = s->eta * s->omega;
            for (int64_t j = 0; j < nv; j++)
                s->zn[j] = clamp(s, j, s->z[j] - tau * (s->cost[j] - s->g[j]));
            mul_e(s, s->zn, s->ezn);
            double dz2 = 0.0, dy2 = 0.0, inter = 0.0;
            for (int64_t j = 0; j < nv; j++) {
                const double d = s->zn[j] - s->z[j];
                dz2 += d * d;
            }
            for (int64_t i = 0; i < nr; i++) {
                s->yn[i] = s->y[i] - sig * (2.0 * s->ezn[i] - s->ez[i]);
                const double dy = s->yn[i] - s->y[i];
                dy2 += dy * dy;
                inter += (s->ezn[i] - s->ez[i]) * dy;
            }
            jm_work_add(&s->work, (3 * nv + 3 * nr) * JM_WORK_NONZERO);
            const double denom = 2.0 * fabs(inter);
            const double limit = denom > 0.0
                ? (s->omega * dz2 + dy2 / s->omega) / denom : HUGE_VAL;
            const double next = fmin(shrink * limit, grow * s->eta);
            used = s->eta;
            if (s->eta <= limit || attempt + 1 == PDLP_STEP_TRIES) {
                double *t;
                t = s->z;  s->z = s->zn;   s->zn = t;
                t = s->y;  s->y = s->yn;   s->yn = t;
                t = s->ez; s->ez = s->ezn; s->ezn = t;
                s->eta = isfinite(next) && next > 0.0 ? next : s->eta;
                break;
            }
            s->eta = isfinite(next) && next > 0.0 ? next : s->eta * 0.5;
        }
        mul_et(s, s->y, s->g);
        for (int64_t j = 0; j < nv; j++)
            s->zs[j] += used * s->z[j];
        for (int64_t i = 0; i < nr; i++)
            s->ys[i] += used * s->y[i];
        s->ws += used;
        s->epoch++;
        s->iters++;
        jm_work_add(&s->work, (nv + nr) * JM_WORK_NONZERO);
    }
}

static double published(double x)
{
    return x == 0.0 ? 0.0 : x;
}

static jaos_status px_publish(px *s, jaos_solve_status status, jm_presolve *p)
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
        for (int64_t j = 0; j < m->num_col; j++) {
            m->sol_col[j] = published(gamma[j] * s->cs[j] * s->z[j]);
            m->sol_redcost[j] = published(
                sigma * (s->cost[j] - s->g[j]) / (s->cs[j] * gamma[j]));
        }
        for (int64_t i = 0; i < m->num_row; i++) {
            m->sol_row[i] =
                published(s->cs[m->num_col + i] * s->z[m->num_col + i] / rho[i]);
            m->sol_dual[i] = published(sigma * s->rs[i] * s->y[i] * rho[i]);
        }
        for (int64_t j = 0; j < s->nvar; j++) {
            const uint8_t k = s->kind[j];
            jaos_basis_status b = JAOS_BASIS_BASIC;
            if (k == FIXED)
                b = JAOS_BASIS_AT_LOWER;
            else if (k == 0)
                b = JAOS_BASIS_FREE;
            else {
                const double r = s->cost[j] - s->g[j];
                const bool at_lo = (k & HAS_LO) && s->z[j] - s->lo[j] < fabs(r);
                const bool at_up = (k & HAS_UP) && s->up[j] - s->z[j] < fabs(r);
                if (at_lo && (!at_up || s->z[j] - s->lo[j] <= s->up[j] - s->z[j]))
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

static jaos_status px_crossover(px *s)
{
    const int64_t nv = s->nvar;
    double *w = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    double *v = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    double *zl = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    double *zu = jm_calloc_array(nv > 0 ? nv : 1, sizeof(double));
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (w == nullptr || v == nullptr || zl == nullptr || zu == nullptr)
        goto done;
    for (int64_t j = 0; j < nv; j++) {
        const uint8_t k = s->kind[j];
        const double r = s->cost[j] - s->g[j];
        const double l = dual_slack(s, j, r);
        if (k & HAS_LO) {
            w[j] = s->cs[j] * (s->z[j] - s->lo[j]);
            zl[j] = (l > 0.0 ? l : 0.0) / s->cs[j];
        }
        if (k & HAS_UP) {
            v[j] = s->cs[j] * (s->up[j] - s->z[j]);
            zu[j] = (l < 0.0 ? -l : 0.0) / s->cs[j];
        }
    }
    jm_work_add(&s->work, nv * JM_WORK_NONZERO);
    st = jm_crash_basis(s->m, s->kind, w, v, zl, zu, s->av0, &s->work);
done:
    free(w);
    free(v);
    free(zl);
    free(zu);
    return st;
}

jaos_status jm_pdlp(jaos_model *m, jaos_model *target, jm_presolve *p,
                    jm_work *work, bool *crossover, bool *handoff,
                    int64_t *iters)
{
    *crossover = false;
    *handoff = false;
    *iters = 0;
    px s;
    jaos_status st = px_init(&s, target);
    if (st != JAOS_OK)
        return st;
    s.work = *work;
    s.started = jm_monotonic_seconds();

    jm_log(m, JAOS_LOG_SUMMARY,
           "pdlp: %lld rows, %lld columns, %lld nonzeros, tolerance %.3g",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, PDLP_TOL);

    jaos_solve_status outcome = JAOS_SOLVE_NUMERICAL_ERROR;
    st = px_run(&s, &outcome);
    *iters = s.iters;
    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL &&
        !m->cfg.barrier_no_crossover) {
        st = px_crossover(&s);
        *work = s.work;
        *crossover = st == JAOS_OK;
        jm_log(m, JAOS_LOG_SUMMARY,
               "pdlp converged after %lld iterations and %lld restarts, %lld "
               "work units; crossing over to the simplex from its basis guess",
               (long long)s.iters, (long long)s.restarts,
               (long long)s.work.units);
        px_free(&s);
        return st;
    }
    if (st == JAOS_OK && outcome == JAOS_SOLVE_NUMERICAL_ERROR && s.handoff) {
        jm_log(m, JAOS_LOG_SUMMARY,
               "pdlp stopped after %lld iterations, %lld work units: %s; "
               "the dual simplex takes over from the slack basis",
               (long long)s.iters, (long long)s.work.units, target->err);
        target->err[0] = '\0';
        *work = s.work;
        *handoff = true;
        px_free(&s);
        return JAOS_OK;
    }
    if (st == JAOS_OK)
        st = px_publish(&s, outcome, p);

    if (target != m && target->err[0] != '\0' &&
        (st != JAOS_OK || outcome == JAOS_SOLVE_NUMERICAL_ERROR))
        memcpy(m->err, target->err, sizeof m->err);

    if (st != JAOS_OK)
        m->solve_iters = s.iters;
    m->solve_primal_iters = 0;
    m->solve_phase1_iters = 0;

    if (st == JAOS_OK)
        jm_log(m, JAOS_LOG_SUMMARY,
               "%s after %lld pdlp iterations and %lld restarts, %lld work "
               "units",
               jaos_solve_status_str(outcome), (long long)s.iters,
               (long long)s.restarts, (long long)s.work.units);
    else
        jm_log(m, JAOS_LOG_SUMMARY,
               "abandoned after %lld pdlp iterations, %lld work units: %s",
               (long long)s.iters, (long long)s.work.units,
               jaos_status_str(st));

    px_free(&s);
    return st;
}
