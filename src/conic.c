/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"
#include "jaos_sys.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double  CONIC_TOL        = 1e-10;
constexpr double  CONIC_TOL_STALL  = 1e-8;
constexpr double  CONIC_TOL_ROUGH  = 1e-6;
constexpr double  CONIC_TOL_INFEAS = 1e-8;
constexpr double  CONIC_TOL_INFEAS_STALL = 1e-5;
constexpr int64_t CONIC_MAX_ITER   = 200;
constexpr double  CONIC_STEP       = 0.99;
constexpr double  CONIC_REG        = 1e-7;
constexpr double  CONIC_PIVOT      = 1e-13;
constexpr int64_t CONIC_REFINE     = 10;
constexpr int64_t CONIC_RUIZ       = 10;
constexpr double  CONIC_SCALE_MIN  = 1e-4;
constexpr double  CONIC_SCALE_MAX  = 1e4;
constexpr double  CONIC_STALL_STEP = 1e-10;
constexpr int64_t CONIC_STALL_ITERS = 3;
constexpr int64_t CONIC_NEWTON_WIDE = 64;

typedef enum { CK_ZERO, CK_NONNEG, CK_SOC } ck_kind;

typedef struct {
    ck_kind kind;
    int64_t start, dim;
    double eta;
    int64_t aux;
} ck_cone;

typedef struct {
    const jaos_model *log;
    int64_t n, m, ncone, degree;
    ck_cone *cone;
    int64_t *cone_of;

    int64_t *ps, *pi;
    double *pv;
    int64_t *as, *ai;
    double *av;
    int64_t *rs, *ri;
    double *rv;
    double *q, *b;

    double *dcol, *erow, cscale;

    double *x, *s, *z, tau, kappa;
    double *w, *lambda;

    int64_t kn;
    int64_t *ks, *ki;
    double *kv;
    int8_t *ksign;
    int64_t *kdiag;
    int64_t *hpos;
    int64_t hn;
    jm_chol ldl;

    double *rx, *rz, *dx1, *dz1, *dx2, *dz2, *dxa, *dza, *dsa, *ds;
    double *t1, *t2, *t3, *kr, *ksol, *kres;
    double *px, *atz;
    double *xi, *pxi, *g, *wg, *tm, *tm2, *rhx, *rhz, *e;

    jm_work work;
} ck;

static void ck_free(ck *c)
{
    free(c->cone); free(c->cone_of);
    free(c->ps); free(c->pi); free(c->pv);
    free(c->as); free(c->ai); free(c->av);
    free(c->rs); free(c->ri); free(c->rv);
    free(c->q); free(c->b);
    free(c->dcol); free(c->erow);
    free(c->x); free(c->s); free(c->z);
    free(c->w); free(c->lambda);
    free(c->ks); free(c->ki); free(c->kv); free(c->ksign); free(c->kdiag);
    free(c->hpos);
    jm_chol_free(&c->ldl);
    free(c->rx); free(c->rz); free(c->dx1); free(c->dz1); free(c->dx2);
    free(c->dz2); free(c->dxa); free(c->dza); free(c->dsa); free(c->ds);
    free(c->t1); free(c->t2); free(c->t3); free(c->kr); free(c->ksol);
    free(c->kres); free(c->px); free(c->atz);
    free(c->xi); free(c->pxi); free(c->g); free(c->wg); free(c->tm);
    free(c->tm2); free(c->rhx); free(c->rhz); free(c->e);
    memset(c, 0, sizeof *c);
}

static double inf_norm(const double *v, int64_t n)
{
    double r = 0.0;
    for (int64_t i = 0; i < n; i++)
        if (fabs(v[i]) > r)
            r = fabs(v[i]);
    return r;
}

static double dot(const double *a, const double *b, int64_t n)
{
    double r = 0.0;
    for (int64_t i = 0; i < n; i++)
        r += a[i] * b[i];
    return r;
}

static void mul_p(ck *c, const double *x, double *out)
{
    for (int64_t j = 0; j < c->n; j++)
        out[j] = 0.0;
    for (int64_t j = 0; j < c->n; j++)
        for (int64_t p = c->ps[j]; p < c->ps[j + 1]; p++)
            out[c->pi[p]] += c->pv[p] * x[j];
    jm_work_add(&c->work, (c->n + c->ps[c->n]) * JM_WORK_NONZERO);
}

static void mul_a(ck *c, const double *x, double *out)
{
    for (int64_t i = 0; i < c->m; i++)
        out[i] = 0.0;
    for (int64_t j = 0; j < c->n; j++) {
        const double xj = x[j];
        if (xj == 0.0)
            continue;
        for (int64_t p = c->as[j]; p < c->as[j + 1]; p++)
            out[c->ai[p]] += c->av[p] * xj;
    }
    jm_work_add(&c->work, (c->m + c->as[c->n]) * JM_WORK_NONZERO);
}

static void mul_at(ck *c, const double *z, double *out)
{
    for (int64_t j = 0; j < c->n; j++) {
        double t = 0.0;
        for (int64_t p = c->as[j]; p < c->as[j + 1]; p++)
            t += c->av[p] * z[c->ai[p]];
        out[j] = t;
    }
    jm_work_add(&c->work, (c->n + c->as[c->n]) * JM_WORK_NONZERO);
}

static double dot2(const double *a, const double *b, int64_t n)
{
    double s = 0.0, c = 0.0;
    for (int64_t i = 0; i < n; i++) {
        const double p = a[i] * b[i];
        jm_obj_add(&s, &c, p);
        c += jm_two_product_residue(a[i], b[i], p);
    }
    return s + c;
}

static double lorentz(const double *a, const double *b, int64_t d)
{
    double s = 0.0, c = 0.0;
    for (int64_t i = 0; i < d; i++) {
        const double q = a[i] * b[i];
        const double e = jm_two_product_residue(a[i], b[i], q);
        jm_obj_add(&s, &c, i == 0 ? q : -q);
        c += i == 0 ? e : -e;
    }
    return s + c;
}

static double soc_det(const double *v, int64_t d)
{
    return lorentz(v, v, d);
}

static void cone_identity(ck *c, double *v)
{
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        for (int64_t t = 0; t < cn->dim; t++)
            v[cn->start + t] = 0.0;
        if (cn->kind == CK_NONNEG)
            for (int64_t t = 0; t < cn->dim; t++)
                v[cn->start + t] = 1.0;
        else if (cn->kind == CK_SOC)
            v[cn->start] = 1.0;
    }
}

static void nt_scaling(ck *c)
{
    for (int64_t k = 0; k < c->ncone; k++) {
        ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start, d = cn->dim;
        if (cn->kind == CK_ZERO) {
            for (int64_t t = 0; t < d; t++) {
                c->w[o + t] = 0.0;
                c->lambda[o + t] = 0.0;
            }
        } else if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < d; t++) {
                const double si = c->s[o + t], zi = c->z[o + t];
                c->w[o + t] = sqrt(si / zi);
                c->lambda[o + t] = sqrt(si * zi);
            }
        } else {
            const double *s = c->s + o, *z = c->z + o;
            const double sd = soc_det(s, d), zd = soc_det(z, d);
            const double ss = sqrt(sd), zs = sqrt(zd);
            const double sz = dot2(s, z, d) / (ss * zs);
            const double gamma = sqrt(0.5 * (1.0 + sz));
            double *wb = c->w + o;
            wb[0] = (s[0] / ss + z[0] / zs) / (2.0 * gamma);
            for (int64_t t = 1; t < d; t++)
                wb[t] = (s[t] / ss - z[t] / zs) / (2.0 * gamma);
            cn->eta = sqrt(sqrt(sd / zd));
            double *lam = c->lambda + o;
            const double w1z1 = dot2(wb + 1, z + 1, d - 1);
            lam[0] = cn->eta * dot2(wb, z, d);
            const double f = z[0] + w1z1 / (1.0 + wb[0]);
            for (int64_t t = 1; t < d; t++)
                lam[t] = cn->eta * (z[t] + f * wb[t]);
        }
    }
    jm_work_add(&c->work, 4 * c->m * JM_WORK_NONZERO);
}

static void apply_w(const ck *c, const double *v, double *out, bool inverse)
{
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start, d = cn->dim;
        if (cn->kind == CK_ZERO) {
            for (int64_t t = 0; t < d; t++)
                out[o + t] = 0.0;
        } else if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < d; t++)
                out[o + t] = inverse ? v[o + t] / c->w[o + t]
                                     : v[o + t] * c->w[o + t];
        } else {
            const double *wb = c->w + o, *vv = v + o;
            const double sg = inverse ? -1.0 : 1.0;
            const double sc = inverse ? 1.0 / cn->eta : cn->eta;
            const double w1v1 = dot2(wb + 1, vv + 1, d - 1);
            const double v0 = vv[0];
            out[o] = sc * (inverse ? lorentz(wb, vv, d) : dot2(wb, vv, d));
            const double f = sg * v0 + w1v1 / (1.0 + wb[0]);
            for (int64_t t = 1; t < d; t++)
                out[o + t] = sc * (vv[t] + f * wb[t]);
        }
    }
}

static void jordan_div(const ck *c, const double *lam, const double *d,
                       double *out)
{
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start, dim = cn->dim;
        if (cn->kind == CK_ZERO) {
            for (int64_t t = 0; t < dim; t++)
                out[o + t] = 0.0;
        } else if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < dim; t++)
                out[o + t] = d[o + t] / lam[o + t];
        } else {
            const double *l = lam + o, *dd = d + o;
            const double x0 = lorentz(l, dd, dim) / soc_det(l, dim);
            out[o] = x0;
            for (int64_t t = 1; t < dim; t++)
                out[o + t] = (dd[t] - l[t] * x0) / l[0];
        }
    }
}

static void jordan_prod(const ck *c, const double *u, const double *v,
                        double *out)
{
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start, dim = cn->dim;
        if (cn->kind == CK_ZERO) {
            for (int64_t t = 0; t < dim; t++)
                out[o + t] = 0.0;
        } else if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < dim; t++)
                out[o + t] = u[o + t] * v[o + t];
        } else {
            const double u0 = u[o], v0 = v[o];
            out[o] = dot2(u + o, v + o, dim);
            for (int64_t t = 1; t < dim; t++)
                out[o + t] = u0 * v[o + t] + v0 * u[o + t];
        }
    }
}

static double soc_step(const double *v, const double *dv, int64_t d)
{
    const double a = lorentz(dv, dv, d), b = 2.0 * lorentz(v, dv, d);
    const double cc = lorentz(v, v, d);
    if (!(cc > 0.0))
        return 0.0;
    double best = HUGE_VAL;
    if (a == 0.0) {
        if (b < 0.0)
            best = -cc / b;
    } else {
        const double disc = b * b - 4.0 * a * cc;
        if (disc >= 0.0) {
            const double sq = sqrt(disc);
            const double t = -0.5 * (b + (b >= 0.0 ? sq : -sq));
            const double r1 = t / a, rr2 = t != 0.0 ? cc / t : HUGE_VAL;
            if (r1 > 0.0 && r1 < best)
                best = r1;
            if (rr2 > 0.0 && rr2 < best)
                best = rr2;
        }
    }
    if (dv[0] < 0.0 && -v[0] / dv[0] < best)
        best = -v[0] / dv[0];
    return best;
}

static double cone_step(const ck *c, const double *v, const double *dv)
{
    double best = HUGE_VAL;
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start;
        if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < cn->dim; t++)
                if (dv[o + t] < 0.0 && -v[o + t] / dv[o + t] < best)
                    best = -v[o + t] / dv[o + t];
        } else if (cn->kind == CK_SOC) {
            const double a = soc_step(v + o, dv + o, cn->dim);
            if (a < best)
                best = a;
        }
    }
    return best;
}

static bool build_kkt(ck *c)
{
    const int64_t n = c->n, m = c->m;
    int64_t kn = n + m;
    for (int64_t k = 0; k < c->ncone; k++) {
        c->cone[k].aux = -1;
        if (c->cone[k].kind == CK_SOC) {
            c->cone[k].aux = kn;
            kn += 2;
        }
    }
    c->kn = kn;
    int64_t *cnt = jm_calloc_array(kn + 1, sizeof *cnt);
    if (cnt == nullptr)
        return false;
    for (int64_t j = 0; j < n; j++) {
        bool diag = false;
        for (int64_t p = c->ps[j]; p < c->ps[j + 1]; p++)
            diag |= c->pi[p] == j;
        cnt[j + 1] += (c->ps[j + 1] - c->ps[j]) + (diag ? 0 : 1) +
                      (c->as[j + 1] - c->as[j]);
    }
    for (int64_t i = 0; i < m; i++) {
        const ck_cone *cn = &c->cone[c->cone_of[i]];
        cnt[n + i + 1] += (c->rs[i + 1] - c->rs[i]) +
                          (cn->kind == CK_SOC ? 3 : 1);
    }
    for (int64_t k = 0; k < c->ncone; k++)
        if (c->cone[k].aux >= 0) {
            cnt[c->cone[k].aux + 1] += c->cone[k].dim + 1;
            cnt[c->cone[k].aux + 2] += c->cone[k].dim + 1;
        }
    for (int64_t k = 0; k < kn; k++)
        cnt[k + 1] += cnt[k];
    const int64_t nnz = cnt[kn];
    c->ks = cnt;
    c->ki = jm_alloc_array(nnz > 0 ? nnz : 1, sizeof *c->ki);
    c->kv = jm_calloc_array(nnz > 0 ? nnz : 1, sizeof *c->kv);
    c->ksign = jm_alloc_array(kn > 0 ? kn : 1, sizeof *c->ksign);
    c->kdiag = jm_alloc_array(kn > 0 ? kn : 1, sizeof *c->kdiag);
    int64_t hn = 0;
    for (int64_t k = 0; k < c->ncone; k++)
        hn += c->cone[k].kind == CK_SOC ? 5 * c->cone[k].dim + 2
                                         : c->cone[k].dim;
    c->hn = hn;
    c->hpos = jm_alloc_array(hn > 0 ? hn : 1, sizeof *c->hpos);
    if (c->ki == nullptr || c->kv == nullptr || c->ksign == nullptr ||
        c->kdiag == nullptr || c->hpos == nullptr)
        return false;
    for (int64_t j = 0; j < n; j++) {
        int64_t at = c->ks[j];
        bool diag = false;
        for (int64_t p = c->ps[j]; p < c->ps[j + 1]; p++) {
            c->ki[at] = c->pi[p];
            c->kv[at] = c->pv[p];
            if (c->pi[p] == j) {
                diag = true;
                c->kdiag[j] = at;
            }
            at++;
        }
        if (!diag) {
            c->ki[at] = j;
            c->kv[at] = 0.0;
            c->kdiag[j] = at;
            at++;
        }
        for (int64_t p = c->as[j]; p < c->as[j + 1]; p++) {
            c->ki[at] = n + c->ai[p];
            c->kv[at] = c->av[p];
            at++;
        }
        c->ksign[j] = 1;
    }
    int64_t h = 0;
    for (int64_t i = 0; i < m; i++) {
        int64_t at = c->ks[n + i];
        for (int64_t p = c->rs[i]; p < c->rs[i + 1]; p++) {
            c->ki[at] = c->ri[p];
            c->kv[at] = c->rv[p];
            at++;
        }
        const ck_cone *cn = &c->cone[c->cone_of[i]];
        c->ki[at] = n + i;
        c->kdiag[n + i] = at;
        at++;
        if (cn->aux >= 0) {
            c->ki[at++] = cn->aux;
            c->ki[at++] = cn->aux + 1;
        }
        c->ksign[n + i] = -1;
    }
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        for (int64_t e = 0; cn->aux >= 0 && e < 2; e++) {
            const int64_t col = cn->aux + e;
            int64_t at = c->ks[col];
            for (int64_t t = 0; t < cn->dim; t++)
                c->ki[at++] = n + cn->start + t;
            c->ki[at] = col;
            c->kdiag[col] = at;
            c->ksign[col] = e == 0 ? 1 : -1;
        }
    }
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        if (cn->aux >= 0) {
            for (int64_t t = 0; t < cn->dim; t++) {
                const int64_t i = cn->start + t;
                c->hpos[h++] = c->kdiag[n + i];
                c->hpos[h++] = c->ks[n + i + 1] - 2;
                c->hpos[h++] = c->ks[n + i + 1] - 1;
                c->hpos[h++] = c->ks[cn->aux] + t;
                c->hpos[h++] = c->ks[cn->aux + 1] + t;
            }
            c->hpos[h++] = c->kdiag[cn->aux];
            c->hpos[h++] = c->kdiag[cn->aux + 1];
        } else {
            for (int64_t t = 0; t < cn->dim; t++)
                c->hpos[h++] = c->kdiag[n + cn->start + t];
        }
    }
    return jm_chol_symbolic_dense(&c->ldl, kn, c->ks, c->ki,
                                  jm_chol_dense_limit(kn), &c->work) == JAOS_OK;
}

static void fill_h(ck *c)
{
    int64_t h = 0;
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start, d = cn->dim;
        if (cn->kind == CK_ZERO) {
            for (int64_t t = 0; t < d; t++)
                c->kv[c->hpos[h++]] = -CONIC_REG;
        } else if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < d; t++) {
                const double wi = c->w[o + t];
                c->kv[c->hpos[h++]] = -(wi * wi + CONIC_REG);
            }
        } else {
            const double *wb = c->w + o;
            const double eta = cn->eta, e2 = eta * eta;
            double r2 = 0.0;
            for (int64_t t = 1; t < d; t++)
                r2 += wb[t] * wb[t];
            const double r = sqrt(r2);
            const double lm = (r + r2 / (wb[0] + 1.0)) * (wb[0] + r + 1.0);
            const double pa = sqrt(0.5 * lm), pb = sqrt(0.5 * lm / (1.0 + lm));
            for (int64_t t = 0; t < d; t++) {
                const double e = t == 0 ? 1.0 : r > 0.0 ? wb[t] / r : 0.0;
                const double ut = pa * e, vt = t == 0 ? pb : -pb * e;
                c->kv[c->hpos[h++]] = -(e2 + CONIC_REG);
                c->kv[c->hpos[h++]] = -eta * ut;
                c->kv[c->hpos[h++]] = eta * vt;
                c->kv[c->hpos[h++]] = -eta * ut;
                c->kv[c->hpos[h++]] = eta * vt;
            }
            c->kv[c->hpos[h++]] = 1.0;
            c->kv[c->hpos[h++]] = -1.0;
        }
    }
    for (int64_t j = 0; j < c->n; j++) {
        double pjj = 0.0;
        for (int64_t p = c->ps[j]; p < c->ps[j + 1]; p++)
            if (c->pi[p] == j)
                pjj += c->pv[p];
        c->kv[c->kdiag[j]] = pjj + CONIC_REG;
    }
    jm_work_add(&c->work, c->hn * JM_WORK_NONZERO);
}

static void mul_h(const ck *c, const double *v, double *out)
{
    for (int64_t k = 0; k < c->ncone; k++) {
        const ck_cone *cn = &c->cone[k];
        const int64_t o = cn->start, d = cn->dim;
        if (cn->kind == CK_ZERO) {
            for (int64_t t = 0; t < d; t++)
                out[o + t] = 0.0;
        } else if (cn->kind == CK_NONNEG) {
            for (int64_t t = 0; t < d; t++)
                out[o + t] = c->w[o + t] * c->w[o + t] * v[o + t];
        } else {
            const double *wb = c->w + o;
            const double e2 = cn->eta * cn->eta;
            const double wv = dot2(wb, v + o, d);
            out[o] = e2 * (2.0 * wb[0] * wv - v[o]);
            for (int64_t t = 1; t < d; t++)
                out[o + t] = e2 * (2.0 * wb[t] * wv + v[o + t]);
        }
    }
}

static void kkt_solve(ck *c, const double *rhs_x, const double *rhs_z,
                      double *sx, double *sz)
{
    const int64_t n = c->n, m = c->m, kn = c->kn;
    double *sol = c->ksol, *res = c->kres;
    for (int64_t j = 0; j < n; j++)
        sol[j] = rhs_x[j];
    for (int64_t i = 0; i < m; i++)
        sol[n + i] = rhs_z[i];
    for (int64_t k = n + m; k < kn; k++)
        sol[k] = 0.0;
    jm_ldlt_solve(&c->ldl, sol, &c->work);
    for (int64_t pass = 0; pass < CONIC_REFINE; pass++) {
        mul_p(c, sol, c->t1);
        mul_at(c, sol + n, c->t2);
        double worst = 0.0, scale = 1.0;
        for (int64_t j = 0; j < n; j++) {
            res[j] = rhs_x[j] - c->t1[j] - c->t2[j];
            if (fabs(res[j]) > worst)
                worst = fabs(res[j]);
            if (fabs(rhs_x[j]) > scale)
                scale = fabs(rhs_x[j]);
        }
        mul_a(c, sol, c->t3);
        mul_h(c, sol + n, c->t1);
        for (int64_t i = 0; i < m; i++) {
            res[n + i] = rhs_z[i] - c->t3[i] + c->t1[i];
            if (fabs(res[n + i]) > worst)
                worst = fabs(res[n + i]);
            if (fabs(rhs_z[i]) > scale)
                scale = fabs(rhs_z[i]);
        }
        if (worst <= 1e-14 * scale)
            break;
        for (int64_t k = n + m; k < kn; k++)
            res[k] = 0.0;
        jm_ldlt_solve(&c->ldl, res, &c->work);
        for (int64_t k = 0; k < kn; k++)
            sol[k] += res[k];
    }
    for (int64_t j = 0; j < n; j++)
        sx[j] = sol[j];
    for (int64_t i = 0; i < m; i++)
        sz[i] = sol[n + i];
}

static void ruiz(ck *c)
{
    const int64_t n = c->n, m = c->m;
    for (int64_t j = 0; j < n; j++)
        c->dcol[j] = 1.0;
    for (int64_t i = 0; i < m; i++)
        c->erow[i] = 1.0;
    double *cn = c->t1, *rn = c->t3;
    for (int64_t round = 0; round < CONIC_RUIZ; round++) {
        for (int64_t j = 0; j < n; j++)
            cn[j] = 0.0;
        for (int64_t i = 0; i < m; i++)
            rn[i] = 0.0;
        for (int64_t j = 0; j < n; j++) {
            for (int64_t p = c->ps[j]; p < c->ps[j + 1]; p++)
                if (fabs(c->pv[p]) > cn[j])
                    cn[j] = fabs(c->pv[p]);
            for (int64_t p = c->as[j]; p < c->as[j + 1]; p++) {
                const double v = fabs(c->av[p]);
                if (v > cn[j])
                    cn[j] = v;
                if (v > rn[c->ai[p]])
                    rn[c->ai[p]] = v;
            }
        }
        for (int64_t k = 0; k < c->ncone; k++) {
            const ck_cone *cc = &c->cone[k];
            if (cc->kind != CK_SOC)
                continue;
            double mx = 0.0;
            for (int64_t t = 0; t < cc->dim; t++)
                if (rn[cc->start + t] > mx)
                    mx = rn[cc->start + t];
            for (int64_t t = 0; t < cc->dim; t++)
                rn[cc->start + t] = mx;
        }
        for (int64_t j = 0; j < n; j++) {
            double f = cn[j] > 0.0 ? 1.0 / sqrt(cn[j]) : 1.0;
            if (f < CONIC_SCALE_MIN) f = CONIC_SCALE_MIN;
            if (f > CONIC_SCALE_MAX) f = CONIC_SCALE_MAX;
            cn[j] = f;
            c->dcol[j] *= f;
        }
        for (int64_t i = 0; i < m; i++) {
            double f = rn[i] > 0.0 ? 1.0 / sqrt(rn[i]) : 1.0;
            if (f < CONIC_SCALE_MIN) f = CONIC_SCALE_MIN;
            if (f > CONIC_SCALE_MAX) f = CONIC_SCALE_MAX;
            rn[i] = f;
            c->erow[i] *= f;
        }
        for (int64_t j = 0; j < n; j++) {
            for (int64_t p = c->ps[j]; p < c->ps[j + 1]; p++)
                c->pv[p] *= cn[c->pi[p]] * cn[j];
            for (int64_t p = c->as[j]; p < c->as[j + 1]; p++)
                c->av[p] *= rn[c->ai[p]] * cn[j];
        }
        jm_work_add(&c->work, 2 * (c->ps[n] + c->as[n] + n + m) *
                                  JM_WORK_NONZERO);
    }
    for (int64_t j = 0; j < n; j++)
        c->q[j] *= c->dcol[j];
    for (int64_t i = 0; i < m; i++)
        c->b[i] *= c->erow[i];
    double qm = inf_norm(c->q, n);
    for (int64_t p = 0; p < c->ps[n]; p++)
        if (fabs(c->pv[p]) > qm)
            qm = fabs(c->pv[p]);
    double cs = 1.0;
    if (qm > 0.0) {
        cs = 1.0 / qm;
        if (cs < CONIC_SCALE_MIN) cs = CONIC_SCALE_MIN;
        if (cs > CONIC_SCALE_MAX) cs = CONIC_SCALE_MAX;
    }
    c->cscale = cs;
    for (int64_t j = 0; j < n; j++)
        c->q[j] *= cs;
    for (int64_t p = 0; p < c->ps[n]; p++)
        c->pv[p] *= cs;
}

static bool rowwise(ck *c)
{
    const int64_t n = c->n, m = c->m, nz = c->as[n];
    c->rs = jm_calloc_array(m + 1, sizeof *c->rs);
    c->ri = jm_alloc_array(nz > 0 ? nz : 1, sizeof *c->ri);
    c->rv = jm_alloc_array(nz > 0 ? nz : 1, sizeof *c->rv);
    int64_t *fill = jm_alloc_array(m > 0 ? m : 1, sizeof *fill);
    if (c->rs == nullptr || c->ri == nullptr || c->rv == nullptr ||
        fill == nullptr) {
        free(fill);
        return false;
    }
    for (int64_t p = 0; p < nz; p++)
        c->rs[c->ai[p] + 1]++;
    for (int64_t i = 0; i < m; i++)
        c->rs[i + 1] += c->rs[i];
    for (int64_t i = 0; i < m; i++)
        fill[i] = c->rs[i];
    for (int64_t j = 0; j < n; j++)
        for (int64_t p = c->as[j]; p < c->as[j + 1]; p++) {
            const int64_t i = c->ai[p];
            c->ri[fill[i]] = j;
            c->rv[fill[i]] = c->av[p];
            fill[i]++;
        }
    free(fill);
    return true;
}

static bool ck_vectors(ck *c)
{
    const int64_t n = c->n > 0 ? c->n : 1, m = c->m > 0 ? c->m : 1;
    int64_t k = n + m;
    for (int64_t t = 0; t < c->ncone; t++)
        k += c->cone[t].kind == CK_SOC ? 2 : 0;
    double **nv[] = {&c->q, &c->dcol, &c->x, &c->rx, &c->dx1, &c->dx2,
                     &c->dxa, &c->px, &c->atz, &c->xi, &c->pxi, &c->rhx};
    double **mv[] = {&c->b, &c->erow, &c->s, &c->z, &c->w, &c->lambda,
                     &c->rz, &c->dz1, &c->dz2, &c->dza, &c->dsa, &c->ds,
                     &c->g, &c->wg, &c->rhz, &c->e};
    double **kv[] = {&c->t1, &c->t2, &c->t3, &c->kr, &c->ksol, &c->kres,
                     &c->tm, &c->tm2};
    for (size_t t = 0; t < sizeof nv / sizeof *nv; t++)
        if ((*nv[t] = jm_calloc_array(n, sizeof(double))) == nullptr)
            return false;
    for (size_t t = 0; t < sizeof mv / sizeof *mv; t++)
        if ((*mv[t] = jm_calloc_array(m, sizeof(double))) == nullptr)
            return false;
    for (size_t t = 0; t < sizeof kv / sizeof *kv; t++)
        if ((*kv[t] = jm_calloc_array(k, sizeof(double))) == nullptr)
            return false;
    return true;
}

static double step_to_boundary(const ck *c, const double *ds,
                               const double *dz, double dt, double dk)
{
    double a = cone_step(c, c->s, ds);
    const double az = cone_step(c, c->z, dz);
    if (az < a)
        a = az;
    if (dt < 0.0 && -c->tau / dt < a)
        a = -c->tau / dt;
    if (dk < 0.0 && -c->kappa / dk < a)
        a = -c->kappa / dk;
    return a;
}

static double direction(ck *c, const double *ds, double dk, double scale,
                        double rt, double den, double *dx, double *dz,
                        double *dsout, double *dkout)
{
    const int64_t n = c->n, m = c->m;
    jordan_div(c, c->lambda, ds, c->g);
    apply_w(c, c->g, c->wg, false);
    for (int64_t j = 0; j < n; j++)
        c->rhx[j] = -scale * c->rx[j];
    for (int64_t i = 0; i < m; i++)
        c->rhz[i] = -scale * c->rz[i] + c->wg[i];
    kkt_solve(c, c->rhx, c->rhz, c->dx1, c->dz1);
    double num = -scale * rt + dk / c->tau;
    for (int64_t j = 0; j < n; j++)
        num -= (c->q[j] + 2.0 * c->pxi[j]) * c->dx1[j];
    num -= dot(c->b, c->dz1, m);
    const double dt = num / den;
    for (int64_t j = 0; j < n; j++)
        dx[j] = c->dx1[j] + dt * c->dx2[j];
    for (int64_t i = 0; i < m; i++)
        dz[i] = c->dz1[i] + dt * c->dz2[i];
    apply_w(c, dz, c->tm, false);
    for (int64_t i = 0; i < m; i++)
        c->tm[i] += c->g[i];
    apply_w(c, c->tm, dsout, false);
    for (int64_t i = 0; i < m; i++)
        dsout[i] = -dsout[i];
    *dkout = -(dk + c->kappa * dt) / c->tau;
    jm_work_add(&c->work, 12 * (n + m) * JM_WORK_NONZERO);
    return dt;
}

static void set_infeasible(const ck *c, jm_cone_result *out, double bz)
{
    out->status = JAOS_SOLVE_INFEASIBLE;
    for (int64_t i = 0; i < c->m; i++) {
        out->z[i] = c->z[i] * c->erow[i] / -bz;
        out->s[i] = 0.0;
    }
    for (int64_t j = 0; j < c->n; j++)
        out->x[j] = 0.0;
}

static void set_unbounded(const ck *c, jm_cone_result *out, double rate)
{
    out->status = JAOS_SOLVE_UNBOUNDED;
    for (int64_t j = 0; j < c->n; j++)
        out->x[j] = c->x[j] * c->dcol[j] / rate;
    for (int64_t i = 0; i < c->m; i++) {
        out->s[i] = c->s[i] / c->erow[i] / rate;
        out->z[i] = 0.0;
    }
}

jaos_status jm_cone_solve(const jm_cone_problem *pb, const jaos_model *log,
                          jm_work *work, jm_cone_result *out)
{
    ck cc;
    ck *c = &cc;
    memset(c, 0, sizeof *c);
    jm_chol_init(&c->ldl);
    c->log = log;
    c->work = *work;
    const double started = jm_monotonic_seconds();
    const int64_t n = pb->n, m = pb->m;
    c->n = n;
    c->m = m;
    out->status = JAOS_SOLVE_NUMERICAL_ERROR;
    out->iters = 0;
    out->pobj = out->dobj = 0.0;
    out->relaxed = false;
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;

    c->ncone = (pb->nzero > 0) + (pb->nnonneg > 0) + pb->nsoc;
    c->cone = jm_calloc_array(c->ncone > 0 ? c->ncone : 1, sizeof *c->cone);
    c->cone_of = jm_alloc_array(m > 0 ? m : 1, sizeof *c->cone_of);
    if (c->cone == nullptr || c->cone_of == nullptr)
        goto done;
    {
        int64_t k = 0, at = 0;
        if (pb->nzero > 0)
            c->cone[k++] = (ck_cone){CK_ZERO, at, pb->nzero, 1.0, -1};
        at += pb->nzero;
        if (pb->nnonneg > 0)
            c->cone[k++] = (ck_cone){CK_NONNEG, at, pb->nnonneg, 1.0, -1};
        at += pb->nnonneg;
        for (int64_t t = 0; t < pb->nsoc; t++) {
            if (pb->soc_dim[t] < 1) {
                st = JAOS_ERR_INVALID_INPUT;
                goto done;
            }
            c->cone[k++] = (ck_cone){CK_SOC, at, pb->soc_dim[t], 1.0, -1};
            at += pb->soc_dim[t];
        }
        if (at != m) {
            st = JAOS_ERR_INVALID_INPUT;
            goto done;
        }
        for (int64_t t = 0; t < c->ncone; t++)
            for (int64_t r = 0; r < c->cone[t].dim; r++)
                c->cone_of[c->cone[t].start + r] = t;
        c->degree = pb->nnonneg + pb->nsoc;
    }

    {
        int64_t *cnt = jm_calloc_array(n + 1, sizeof *cnt);
        if (cnt == nullptr)
            goto done;
        for (int64_t j = 0; j < n; j++)
            for (int64_t p = pb->p_start[j]; p < pb->p_start[j + 1]; p++) {
                const int64_t i = pb->p_index[p];
                cnt[j + 1]++;
                if (i != j)
                    cnt[i + 1]++;
            }
        for (int64_t j = 0; j < n; j++)
            cnt[j + 1] += cnt[j];
        const int64_t pn = cnt[n];
        c->ps = cnt;
        c->pi = jm_alloc_array(pn > 0 ? pn : 1, sizeof *c->pi);
        c->pv = jm_alloc_array(pn > 0 ? pn : 1, sizeof *c->pv);
        int64_t *fill = jm_alloc_array(n > 0 ? n : 1, sizeof *fill);
        if (c->pi == nullptr || c->pv == nullptr || fill == nullptr) {
            free(fill);
            goto done;
        }
        for (int64_t j = 0; j < n; j++)
            fill[j] = c->ps[j];
        for (int64_t j = 0; j < n; j++)
            for (int64_t p = pb->p_start[j]; p < pb->p_start[j + 1]; p++) {
                const int64_t i = pb->p_index[p];
                c->pi[fill[j]] = i;
                c->pv[fill[j]++] = pb->p_value[p];
                if (i != j) {
                    c->pi[fill[i]] = j;
                    c->pv[fill[i]++] = pb->p_value[p];
                }
            }
        free(fill);
    }
    {
        const int64_t an = pb->a_start[n];
        c->as = jm_alloc_array(n + 1, sizeof *c->as);
        c->ai = jm_alloc_array(an > 0 ? an : 1, sizeof *c->ai);
        c->av = jm_alloc_array(an > 0 ? an : 1, sizeof *c->av);
        if (c->as == nullptr || c->ai == nullptr || c->av == nullptr)
            goto done;
        memcpy(c->as, pb->a_start, (size_t)(n + 1) * sizeof *c->as);
        if (an > 0) {
            memcpy(c->ai, pb->a_index, (size_t)an * sizeof *c->ai);
            memcpy(c->av, pb->a_value, (size_t)an * sizeof *c->av);
        }
    }
    if (!ck_vectors(c))
        goto done;
    if (n > 0)
        memcpy(c->q, pb->q, (size_t)n * sizeof *c->q);
    if (m > 0)
        memcpy(c->b, pb->b, (size_t)m * sizeof *c->b);

    ruiz(c);
    if (!rowwise(c) || !build_kkt(c))
        goto done;
    st = JAOS_OK;

    cone_identity(c, c->e);
    cone_identity(c, c->s);
    cone_identity(c, c->z);
    c->tau = 1.0;
    c->kappa = 1.0;

    double norm_q = 0.0, norm_b = 0.0;
    for (int64_t j = 0; j < n; j++)
        if (fabs(pb->q[j]) > norm_q)
            norm_q = fabs(pb->q[j]);
    for (int64_t i = 0; i < m; i++)
        if (fabs(pb->b[i]) > norm_b)
            norm_b = fabs(pb->b[i]);

    const double *dcol = c->dcol, *erow = c->erow;
    const double cs = c->cscale;
    bool near = false, rough = false;
    double best_merit = HUGE_VAL;
    int64_t since = 0;
    int64_t it;
    for (it = 0; it < CONIC_MAX_ITER; it++) {
        mul_p(c, c->x, c->px);
        mul_at(c, c->z, c->atz);
        for (int64_t j = 0; j < n; j++)
            c->rx[j] = c->px[j] + c->atz[j] + c->q[j] * c->tau;
        mul_a(c, c->x, c->t3);
        for (int64_t i = 0; i < m; i++)
            c->rz[i] = c->t3[i] + c->s[i] - c->b[i] * c->tau;
        const double xpx = dot(c->x, c->px, n);
        const double qx = dot(c->q, c->x, n), bz = dot(c->b, c->z, m);
        const double rt = qx + bz + c->kappa + xpx / c->tau;

        double pres = 0.0, dres = 0.0, xn = 0.0, sn = 0.0, zn = 0.0;
        for (int64_t i = 0; i < m; i++) {
            const double r = fabs(c->rz[i] / erow[i]) / c->tau;
            if (r > pres) pres = r;
            const double sv = fabs(c->s[i] / erow[i]) / c->tau;
            if (sv > sn) sn = sv;
            const double zv = fabs(c->z[i] * erow[i] / cs) / c->tau;
            if (zv > zn) zn = zv;
        }
        for (int64_t j = 0; j < n; j++) {
            const double r = fabs(c->rx[j] / (dcol[j] * cs)) / c->tau;
            if (r > dres) dres = r;
            const double xv = fabs(c->x[j] * dcol[j]) / c->tau;
            if (xv > xn) xn = xv;
        }
        const double t2 = c->tau * c->tau;
        const double pobj = (0.5 * xpx / t2 + qx / c->tau) / cs;
        const double dobj = (-0.5 * xpx / t2 - bz / c->tau) / cs;
        const double prel = pres / (1.0 + norm_b + xn + sn);
        const double drel = dres / (1.0 + norm_q + xn + zn);
        const double small = fabs(pobj) < fabs(dobj) ? fabs(pobj) : fabs(dobj);
        const double comp = dot(c->s, c->z, m) / cs / t2 / (1.0 + small);
        const double gap = fmax(fabs(pobj - dobj) / (1.0 + small), comp);
        const double mu = (dot(c->s, c->z, m) + c->tau * c->kappa) /
                          (double)(c->degree + 1);
        jm_log(log, JAOS_LOG_DETAIL,
               "conic %3lld: pobj %+.10e dobj %+.10e pres %.2e dres %.2e "
               "gap %.2e mu %.2e tau %.2e kappa %.2e",
               (long long)it, pobj, dobj, prel, drel, gap, mu, c->tau,
               c->kappa);
        const bool within = prel <= CONIC_TOL_STALL &&
                            drel <= CONIC_TOL_STALL && gap <= CONIC_TOL_STALL;
        if (within || (!near && prel <= CONIC_TOL_ROUGH &&
                       drel <= CONIC_TOL_ROUGH && gap <= CONIC_TOL_ROUGH)) {
            near = near || within;
            rough = true;
            out->pobj = pobj;
            out->dobj = dobj;
            for (int64_t j = 0; j < n; j++)
                out->x[j] = c->x[j] * dcol[j] / c->tau;
            for (int64_t i = 0; i < m; i++) {
                out->s[i] = c->s[i] / erow[i] / c->tau;
                out->z[i] = c->z[i] * erow[i] / cs / c->tau;
            }
            if (prel <= CONIC_TOL && drel <= CONIC_TOL && gap <= CONIC_TOL) {
                out->status = JAOS_SOLVE_OPTIMAL;
                break;
            }
        }
        double inf_ratio = HUGE_VAL, unb_ratio = HUGE_VAL;
        if (bz < 0.0 && c->tau < c->kappa) {
            double worst = 0.0;
            for (int64_t j = 0; j < n; j++) {
                const double v = fabs(c->atz[j] / dcol[j]);
                if (v > worst)
                    worst = v;
            }
            inf_ratio = worst / -bz;
        }
        if (qx < 0.0 && c->tau < c->kappa) {
            double worst = 0.0;
            for (int64_t i = 0; i < m; i++) {
                const double v = fabs((c->t3[i] + c->s[i]) / erow[i]);
                if (v > worst)
                    worst = v;
            }
            for (int64_t j = 0; j < n; j++) {
                const double v = fabs(c->px[j] / (dcol[j] * cs));
                if (v > worst)
                    worst = v;
            }
            unb_ratio = worst / (-qx / cs);
        }
        if (inf_ratio <= CONIC_TOL_INFEAS) {
            set_infeasible(c, out, bz);
            break;
        }
        if (unb_ratio <= CONIC_TOL_INFEAS) {
            set_unbounded(c, out, -qx / cs);
            break;
        }
        const double merit = fmax(fmax(prel, drel), gap);
        if (merit < best_merit) {
            best_merit = merit;
            since = 0;
        } else if (rough && ++since >= CONIC_STALL_ITERS) {
            jm_log(log, JAOS_LOG_SUMMARY, "conic: no progress for %lld "
                   "iterations; stopping", (long long)since);
            break;
        }

        if (log != nullptr && log->cfg.work_limit > 0 &&
            c->work.units >= log->cfg.work_limit) {
            out->status = JAOS_SOLVE_WORK_LIMIT;
            break;
        }
        if (log != nullptr && log->cfg.time_limit > 0.0 &&
            jm_monotonic_seconds() - started >= log->cfg.time_limit) {
            out->status = JAOS_SOLVE_TIME_LIMIT;
            break;
        }

        nt_scaling(c);
        fill_h(c);
        st = jm_ldlt_numeric(&c->ldl, c->kv, c->ksign, CONIC_PIVOT, &c->work);
        if (st != JAOS_OK)
            break;

        for (int64_t j = 0; j < n; j++)
            c->rhx[j] = -c->q[j];
        for (int64_t i = 0; i < m; i++)
            c->rhz[i] = c->b[i];
        kkt_solve(c, c->rhx, c->rhz, c->dx2, c->dz2);
        for (int64_t j = 0; j < n; j++)
            c->xi[j] = c->x[j] / c->tau;
        mul_p(c, c->xi, c->pxi);
        double den;
        {
            for (int64_t j = 0; j < n; j++)
                c->tm[j] = c->xi[j] - c->dx2[j];
            mul_p(c, c->tm, c->tm2);
            const double apart = dot(c->tm, c->tm2, n);
            mul_p(c, c->dx2, c->tm2);
            const double aown = dot(c->dx2, c->tm2, n);
            den = -(c->kappa / c->tau - dot(c->q, c->dx2, n) -
                    dot(c->b, c->dz2, m) + apart - aown);
        }

        jordan_prod(c, c->lambda, c->lambda, c->kr);
        for (int64_t i = 0; i < m; i++)
            c->ds[i] = c->kr[i];
        double dka = 0.0;
        const double dta = direction(c, c->ds, c->tau * c->kappa, 1.0, rt,
                                     den, c->dxa, c->dza, c->dsa, &dka);
        const double aa = step_to_boundary(c, c->dsa, c->dza, dta, dka);
        const double om = 1.0 - (aa < 1.0 ? aa : 1.0);
        const double sigma = om * om * om;

        apply_w(c, c->dsa, c->tm, true);
        apply_w(c, c->dza, c->tm2, false);
        jordan_prod(c, c->tm, c->tm2, c->ds);
        for (int64_t i = 0; i < m; i++)
            c->ds[i] += c->kr[i] - sigma * mu * c->e[i];
        const double dk = c->tau * c->kappa + dta * dka - sigma * mu;
        double dkk = 0.0;
        double *dx = c->dxa, *dz = c->dza, *dsn = c->dsa;
        const double dt = direction(c, c->ds, dk, 1.0 - sigma, rt, den, dx,
                                    dz, dsn, &dkk);
        double alpha = CONIC_STEP * step_to_boundary(c, dsn, dz, dt, dkk);
        if (alpha > 1.0)
            alpha = 1.0;
        bool finite = isfinite(dt) && isfinite(dkk);
        for (int64_t j = 0; j < n && finite; j++)
            finite = isfinite(dx[j]);
        for (int64_t i = 0; i < m && finite; i++)
            finite = isfinite(dz[i]) && isfinite(dsn[i]);
        if (!finite || !(alpha > CONIC_STALL_STEP)) {
            jm_log(log, JAOS_LOG_SUMMARY,
                   "conic: the step fell to %.3e at iteration %lld%s; stopping",
                   finite ? alpha : 0.0, (long long)it,
                   finite ? "" : ", the direction not finite");
            if (inf_ratio <= CONIC_TOL_INFEAS_STALL) {
                set_infeasible(c, out, bz);
                out->relaxed = true;
            } else if (unb_ratio <= CONIC_TOL_INFEAS_STALL) {
                set_unbounded(c, out, -qx / cs);
                out->relaxed = true;
            }
            it++;
            break;
        }
        for (int64_t j = 0; j < n; j++)
            c->x[j] += alpha * dx[j];
        for (int64_t i = 0; i < m; i++) {
            c->z[i] += alpha * dz[i];
            c->s[i] += alpha * dsn[i];
        }
        c->tau += alpha * dt;
        c->kappa += alpha * dkk;
    }
    out->iters = it;
    if (st == JAOS_OK && rough && out->status == JAOS_SOLVE_NUMERICAL_ERROR) {
        out->status = JAOS_SOLVE_OPTIMAL;
        out->relaxed = !near;
        jm_log(log, JAOS_LOG_SUMMARY,
               "conic: the walk stopped short of %.0e and its last point "
               "within %.0e stands%s", CONIC_TOL,
               near ? CONIC_TOL_STALL : CONIC_TOL_ROUGH,
               near ? "" : " if the checker passes it");
    }

done:
    *work = c->work;
    ck_free(c);
    return st;
}

constexpr double  CONIC_PSD_TOL  = 1e-10;
constexpr int64_t CONIC_QC_DENSE = 3000;
constexpr double  CONIC_RAY_ZERO = 1e-7;
constexpr int64_t CONIC_CERT_TILT = 32;
constexpr int64_t CONIC_CERT_SWEEPS = 1;
constexpr int64_t CONIC_CERT_CALLS = 1024;
constexpr double  CONIC_RAY_ACTIVE = 1e-6;
constexpr int64_t CONIC_RAY_ITERS = 100;
constexpr double  CONIC_RAY_TOL   = 1e-24;

typedef struct {
    int64_t row, col;
    double v;
} cm_trip;

typedef struct {
    cm_trip *t;
    int64_t n, cap;
    double *b;
    int64_t nb, bcap;
} cm_rows;

static bool cm_put(cm_rows *r, int64_t col, double v)
{
    if (!JM_GROW(r->t, r->cap, r->n + 1))
        return false;
    r->t[r->n++] = (cm_trip){r->nb, col, v};
    return true;
}

static bool cm_end(cm_rows *r, double b)
{
    if (!JM_GROW(r->b, r->bcap, r->nb + 1))
        return false;
    r->b[r->nb++] = b;
    return true;
}

typedef struct {
    int sgn;
    int64_t k, r;
    int64_t *cols;
    double *f;
    double u;
} cm_qc;

static void cm_qc_free(cm_qc *q)
{
    free(q->cols);
    free(q->f);
    memset(q, 0, sizeof *q);
}

int64_t jm_row_quadratic_count(const jaos_model *m, int64_t i)
{
    return m->rq_start != nullptr ? m->rq_start[i + 1] - m->rq_start[i] : 0;
}

static int cmp_i64(const void *a, const void *b)
{
    const int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

jaos_status jm_row_quadratic_factor(jaos_model *m, int64_t i, int sgn,
                                    int64_t *k_out, int64_t *r_out,
                                    int64_t **cols_out, double **f_out)
{
    *k_out = *r_out = 0;
    *cols_out = nullptr;
    *f_out = nullptr;
    const int64_t b = m->rq_start[i], e = m->rq_start[i + 1];
    int64_t *cols = jm_alloc_array(2 * (e - b) > 0 ? 2 * (e - b) : 1,
                                   sizeof *cols);
    if (cols == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    int64_t k = 0;
    for (int64_t p = b; p < e; p++) {
        cols[k++] = m->rq_i[p];
        cols[k++] = m->rq_j[p];
    }
    qsort(cols, (size_t)k, sizeof *cols, cmp_i64);
    int64_t u = 0;
    for (int64_t t = 0; t < k; t++)
        if (u == 0 || cols[u - 1] != cols[t])
            cols[u++] = cols[t];
    k = u;
    if (k > CONIC_QC_DENSE) {
        free(cols);
        jm_set_err(m, "row %lld has a quadratic part over %lld "
                   "columns; JAOS factors one of at most %lld",
                   (long long)i, (long long)k, (long long)CONIC_QC_DENSE);
        return JAOS_ERR_INVALID_INPUT;
    }
    double *a = jm_calloc_array(k * k > 0 ? k * k : 1, sizeof *a);
    double *l = jm_calloc_array(k * k > 0 ? k * k : 1, sizeof *l);
    int64_t *perm = jm_alloc_array(k > 0 ? k : 1, sizeof *perm);
    if (a == nullptr || l == nullptr || perm == nullptr) {
        free(cols); free(a); free(l); free(perm);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t p = b; p < e; p++) {
        int64_t ri = 0, ci = 0;
        for (int64_t lo = 0, hi = k - 1; lo <= hi;) {
            const int64_t mid = lo + (hi - lo) / 2;
            if (cols[mid] == m->rq_i[p]) { ri = mid; break; }
            if (cols[mid] < m->rq_i[p]) lo = mid + 1; else hi = mid - 1;
        }
        for (int64_t lo = 0, hi = k - 1; lo <= hi;) {
            const int64_t mid = lo + (hi - lo) / 2;
            if (cols[mid] == m->rq_j[p]) { ci = mid; break; }
            if (cols[mid] < m->rq_j[p]) lo = mid + 1; else hi = mid - 1;
        }
        const double v = sgn * m->rq_v[p];
        a[ri * k + ci] += v;
        if (ri != ci)
            a[ci * k + ri] += v;
    }
    double big = 0.0;
    for (int64_t t = 0; t < k * k; t++)
        if (fabs(a[t]) > big)
            big = fabs(a[t]);
    const double tol = CONIC_PSD_TOL * (big > 0.0 ? big : 1.0);
    for (int64_t t = 0; t < k; t++)
        perm[t] = t;
    int64_t r = 0;
    jaos_status st = JAOS_OK;
    for (; r < k; r++) {
        int64_t piv = r;
        for (int64_t t = r + 1; t < k; t++)
            if (a[t * k + t] > a[piv * k + piv])
                piv = t;
        if (a[piv * k + piv] <= tol) {
            for (int64_t s = r; s < k && st == JAOS_OK; s++)
                for (int64_t t = r; t < k; t++)
                    if (fabs(a[s * k + t]) > tol ||
                        (s == t && a[s * k + t] < -tol)) {
                        st = JAOS_ERR_INVALID_INPUT;
                        break;
                    }
            break;
        }
        if (piv != r) {
            for (int64_t t = 0; t < k; t++) {
                double x = a[r * k + t];
                a[r * k + t] = a[piv * k + t];
                a[piv * k + t] = x;
            }
            for (int64_t t = 0; t < k; t++) {
                double x = a[t * k + r];
                a[t * k + r] = a[t * k + piv];
                a[t * k + piv] = x;
            }
            for (int64_t t = 0; t < r; t++) {
                double x = l[r * k + t];
                l[r * k + t] = l[piv * k + t];
                l[piv * k + t] = x;
            }
            int64_t x = perm[r];
            perm[r] = perm[piv];
            perm[piv] = x;
        }
        const double d = sqrt(a[r * k + r]);
        l[r * k + r] = d;
        for (int64_t s = r + 1; s < k; s++)
            l[s * k + r] = a[s * k + r] / d;
        for (int64_t s = r + 1; s < k; s++)
            for (int64_t t = r + 1; t < k; t++)
                a[s * k + t] -= l[s * k + r] * l[t * k + r];
    }
    free(a);
    if (st != JAOS_OK) {
        free(cols); free(l); free(perm);
        return st;
    }
    double *f = jm_calloc_array(r * k > 0 ? r * k : 1, sizeof *f);
    if (f == nullptr) {
        free(cols); free(l); free(perm);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t t = 0; t < r; t++)
        for (int64_t s = t; s < k; s++)
            f[t * k + perm[s]] = l[s * k + t];
    free(l);
    free(perm);
    *k_out = k;
    *r_out = r;
    *cols_out = cols;
    *f_out = f;
    return JAOS_OK;
}

constexpr int64_t CONIC_NEWTON_STEPS = 2;
constexpr int64_t CONIC_NEWTON_ROUNDS = 4;

static void cm_add_product(double *s, double *c, double a, double b)
{
    const double p = a * b;
    jm_obj_add(s, c, p);
    const double e = jm_two_product_residue(a, b, p);
    if (e != 0.0)
        jm_obj_add(s, c, e);
}

static bool cm_active(double v, double lo, double hi, double dual,
                      double *target)
{
    if (isfinite(lo) && lo == hi) {
        *target = lo;
        return true;
    }
    const double dl = isfinite(lo) ? fabs(v - lo) : HUGE_VAL;
    const double du = isfinite(hi) ? fabs(v - hi) : HUGE_VAL;
    const double slack = dl < du ? dl : du;
    if (!(slack <= fabs(dual)))
        return false;
    *target = dl < du ? lo : hi;
    return true;
}

static double cm_soc_gap(const jaos_model *m, int64_t k, const double *v,
                         bool by_col, double scale, double *big)
{
    const int64_t b = m->cone_start[k], d = m->cone_start[k + 1] - b;
    constexpr double RT = 0.70710678118654752440;
    double head = 0.0, r2 = 0.0;
    *big = 0.0;
    for (int64_t t = 0; t < d; t++) {
        const double e = scale * (by_col ? v[m->cone_col[b + t]] : v[b + t]);
        if (fabs(e) > *big)
            *big = fabs(e);
        if (t == 0)
            head = e;
        else if (t > 1 || m->cone_type[k] != JAOS_CONE_ROTATED)
            r2 += e * e;
    }
    if (m->cone_type[k] == JAOS_CONE_ROTATED) {
        const double e0 = head;
        const double e1 = scale * (by_col ? v[m->cone_col[b + 1]] : v[b + 1]);
        head = RT * (e0 + e1);
        const double q = RT * (e0 - e1);
        r2 += q * q;
    }
    return head - sqrt(r2);
}

static int cm_cone_face(const jaos_model *m, int64_t k, const double *x,
                        const double *z, double sigma, double *w)
{
    const int64_t b = m->cone_start[k], d = m->cone_start[k + 1] - b;
    const int64_t *c = m->cone_col + b;
    double xbig = 0.0, zbig = 0.0;
    const double xgap = cm_soc_gap(m, k, x, true, 1.0, &xbig);
    const double zgap = cm_soc_gap(m, k, z, false, sigma, &zbig);
    if (zbig <= xgap)
        return 1;
    if (xbig <= zgap)
        return 0;
    if (m->cone_type[k] == JAOS_CONE_ROTATED) {
        w[0] = x[c[1]];
        w[1] = x[c[0]];
        for (int64_t t = 2; t < d; t++)
            w[t] = -x[c[t]];
    } else {
        w[0] = x[c[0]];
        for (int64_t t = 1; t < d; t++)
            w[t] = -x[c[t]];
    }
    return 2;
}

typedef struct {
    int64_t r, c, seq;
    double v;
} cm_ent;

typedef struct {
    cm_ent *e;
    int64_t n, cap;
} cm_ents;

static int cmp_ent(const void *pa, const void *pb)
{
    const cm_ent *a = pa, *b = pb;
    if (a->c != b->c)
        return (a->c > b->c) - (a->c < b->c);
    if (a->r != b->r)
        return (a->r > b->r) - (a->r < b->r);
    return (a->seq > b->seq) - (a->seq < b->seq);
}

static bool ent_put(cm_ents *s, int64_t r, int64_t c, double v)
{
    if (!JM_GROW(s->e, s->cap, s->n + 2))
        return false;
    s->e[s->n] = (cm_ent){r, c, s->n, v};
    s->n++;
    if (r != c) {
        s->e[s->n] = (cm_ent){c, r, s->n, v};
        s->n++;
    }
    return true;
}

static const double cm_rt = 0.70710678118654752440;

static double cm_head(bool rot, int64_t t)
{
    if (rot)
        return t < 2 ? cm_rt : 0.0;
    return t == 0 ? 1.0 : 0.0;
}

static int64_t cm_member(bool rot, int64_t t, double *coef)
{
    *coef = 1.0;
    if (rot && t < 2) {
        *coef = t == 0 ? cm_rt : -cm_rt;
        return 0;
    }
    return t - 1;
}

static void cm_cone_grad(const jaos_model *m, int64_t k, const double *x,
                         double *v, double *g, double *vn, double *hval)
{
    const int64_t b = m->cone_start[k], d = m->cone_start[k + 1] - b;
    const int64_t *c = m->cone_col + b;
    const bool rot = m->cone_type[k] == JAOS_CONE_ROTATED;
    double h = 0.0;
    for (int64_t t = 0; t < d; t++)
        if (cm_head(rot, t) != 0.0)
            h += cm_head(rot, t) * x[c[t]];
    for (int64_t a = 0; a + 1 < d; a++)
        v[a] = 0.0;
    for (int64_t t = 0; t < d; t++) {
        double coef;
        const int64_t a = cm_member(rot, t, &coef);
        if (a >= 0)
            v[a] += coef * x[c[t]];
    }
    double r2 = 0.0;
    for (int64_t a = 0; a + 1 < d; a++)
        r2 += v[a] * v[a];
    const double nv = sqrt(r2);
    for (int64_t t = 0; t < d; t++) {
        double coef;
        const int64_t a = cm_member(rot, t, &coef);
        double s = -cm_head(rot, t);
        if (a >= 0)
            s += coef * v[a] / nv;
        g[t] = s;
    }
    *vn = nv;
    *hval = h;
}

static jaos_status newton_polish(jaos_model *m, jm_work *work,
                                 const bool *skip, bool *moved)
{
    const int64_t n = m->num_col, nr = m->num_row, nk = m->num_cone;
    const int64_t members = nk > 0 ? m->cone_start[nk] : 0;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    jaos_status st = JAOS_OK;
    int64_t na = 0, widest = 1;
    for (int64_t k = 0; k < nk; k++) {
        const int64_t d = m->cone_start[k + 1] - m->cone_start[k];
        if (d > widest)
            widest = d;
    }
    const int64_t acap = nr + n + nk + members + 1;
    int64_t *akind = jm_alloc_array(acap, sizeof *akind);
    int64_t *aidx = jm_alloc_array(acap, sizeof *aidx);
    int64_t *yat = jm_alloc_array(acap, sizeof *yat);
    double *atgt = jm_alloc_array(acap, sizeof *atgt);
    int *face = jm_calloc_array(nk > 0 ? nk : 1, sizeof *face);
    double *cv = jm_alloc_array(widest, sizeof *cv);
    double *cg = jm_alloc_array(widest, sizeof *cg);
    double *x = jm_alloc_array(n > 0 ? n : 1, sizeof *x);
    double *best = jm_alloc_array(n > 0 ? n : 1, sizeof *best);
    double *fr = jm_alloc_array(n > 0 ? n : 1, sizeof *fr);
    double *frc = jm_alloc_array(n > 0 ? n : 1, sizeof *frc);
    double *ny = jm_calloc_array(nr > 0 ? nr : 1, sizeof *ny);
    double *nz = jm_calloc_array(members > 0 ? members : 1, sizeof *nz);
    double *nd = jm_calloc_array(n > 0 ? n : 1, sizeof *nd);
    double *nact = jm_calloc_array(nr > 0 ? nr : 1, sizeof *nact);
    double *lam = nullptr, *lbest = nullptr, *rhs = nullptr, *sol = nullptr;
    double *res = nullptr, *kv = nullptr;
    int64_t *ks = nullptr, *ki = nullptr;
    int8_t *sign = nullptr;
    cm_ents ents = {0};
    jm_chol ldl;
    jm_chol_init(&ldl);
    if (akind == nullptr || aidx == nullptr || yat == nullptr ||
        atgt == nullptr || face == nullptr || cv == nullptr ||
        cg == nullptr || x == nullptr || best == nullptr || fr == nullptr ||
        frc == nullptr || ny == nullptr || nz == nullptr || nd == nullptr ||
        nact == nullptr) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    memcpy(x, m->sol_col, (size_t)n * sizeof *x);
    for (int64_t i = 0; i < nr; i++) {
        if (!cm_active(m->sol_row[i], m->row_lower[i], m->row_upper[i],
                       m->sol_dual[i], &atgt[na]))
            continue;
        akind[na] = 0;
        aidx[na++] = i;
    }
    for (int64_t k = 0; k < nk; k++) {
        face[k] = cm_cone_face(m, k, x, m->sol_cone, sigma,
                               nz + m->cone_start[k]);
        if (face[k] != 2)
            continue;
        akind[na] = 1;
        aidx[na] = k;
        atgt[na++] = 0.0;
    }
    for (int64_t t = 0; t < members; t++)
        nz[t] = 0.0;
    for (int64_t k = 0; k < nk; k++)
        for (int64_t t = m->cone_start[k];
             face[k] == 0 && t < m->cone_start[k + 1]; t++) {
            akind[na] = 3;
            aidx[na] = t;
            atgt[na++] = 0.0;
        }
    for (int64_t j = 0; j < n; j++) {
        if (skip != nullptr && skip[j])
            continue;
        if (!cm_active(x[j], m->col_lower[j], m->col_upper[j],
                       m->sol_redcost[j], &atgt[na]))
            continue;
        akind[na] = 2;
        aidx[na++] = j;
    }
    if (na == 0)
        goto done;
    int64_t kn = n + na;
    for (int64_t a = 0; a < na; a++) {
        yat[a] = -1;
        if (akind[a] == 1 && m->cone_start[aidx[a] + 1] -
                                     m->cone_start[aidx[a]] > CONIC_NEWTON_WIDE)
            yat[a] = kn++;
    }
    lam = jm_alloc_array(na, sizeof *lam);
    lbest = jm_alloc_array(na, sizeof *lbest);
    rhs = jm_alloc_array(kn, sizeof *rhs);
    sol = jm_alloc_array(kn, sizeof *sol);
    res = jm_alloc_array(kn, sizeof *res);
    sign = jm_alloc_array(kn, sizeof *sign);
    if (lam == nullptr || lbest == nullptr || rhs == nullptr ||
        sol == nullptr || res == nullptr || sign == nullptr) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    for (int64_t k = 0; k < kn; k++)
        sign[k] = k < n ? 1 : -1;
    for (int64_t a = 0; a < na; a++) {
        if (akind[a] == 0) {
            lam[a] = sigma * m->sol_dual[aidx[a]];
        } else if (akind[a] == 2) {
            lam[a] = sigma * m->sol_redcost[aidx[a]];
        } else if (akind[a] == 3) {
            lam[a] = sigma * m->sol_cone[aidx[a]];
        } else {
            const int64_t k = aidx[a], b = m->cone_start[k];
            const int64_t d = m->cone_start[k + 1] - b;
            double vn, hv;
            cm_cone_grad(m, k, x, cv, cg, &vn, &hv);
            double zg = 0.0, gg = 0.0;
            for (int64_t t = 0; t < d; t++) {
                zg += sigma * m->sol_cone[b + t] * cg[t];
                gg += cg[t] * cg[t];
            }
            lam[a] = gg > 0.0 ? zg / gg : 0.0;
        }
    }

    double worst_best = HUGE_VAL;
    int64_t step = 0;
    bool symbolic = false;
    for (;; step++) {
        for (int64_t j = 0; j < n; j++) {
            fr[j] = 0.0;
            frc[j] = 0.0;
        }
        for (int64_t j = 0; j < n; j++) {
            jm_obj_add(&fr[j], &frc[j], sigma * m->col_cost[j]);
            if (m->col_quad != nullptr && m->col_quad[j] != 0.0)
                cm_add_product(&fr[j], &frc[j], sigma * m->col_quad[j], x[j]);
            for (int64_t p = m->q_start != nullptr ? m->q_start[j] : 0;
                 m->q_start != nullptr && p < m->q_start[j + 1]; p++) {
                const int64_t i = m->q_index[p];
                cm_add_product(&fr[i], &frc[i], sigma * m->q_value[p], x[j]);
                cm_add_product(&fr[j], &frc[j], sigma * m->q_value[p], x[i]);
            }
        }
        ents.n = 0;
        bool ok = true;
        for (int64_t j = 0; j < n && ok; j++) {
            double h = CONIC_REG;
            if (m->col_quad != nullptr)
                h += sigma * m->col_quad[j];
            ok = ent_put(&ents, j, j, h);
            for (int64_t p = m->q_start != nullptr ? m->q_start[j] : 0;
                 ok && m->q_start != nullptr && p < m->q_start[j + 1]; p++)
                ok = ent_put(&ents, m->q_index[p], j, sigma * m->q_value[p]);
        }
        double worst = 0.0;
        for (int64_t a = 0; a < na && ok; a++) {
            const int64_t row = n + a;
            double cval = 0.0, cvalc = 0.0;
            if (akind[a] == 0) {
                const int64_t i = aidx[a];
                for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
                    const int64_t j = m->ar_index[p];
                    cm_add_product(&cval, &cvalc, m->ar_value[p], x[j]);
                    cm_add_product(&fr[j], &frc[j], -lam[a], m->ar_value[p]);
                    ok = ok && ent_put(&ents, row, j, m->ar_value[p]);
                }
                for (int64_t p = m->rq_start != nullptr ? m->rq_start[i] : 0;
                     ok && m->rq_start != nullptr && p < m->rq_start[i + 1];
                     p++) {
                    const int64_t r = m->rq_i[p], c = m->rq_j[p];
                    const double v = m->rq_v[p];
                    cm_add_product(&cval, &cvalc,
                                   (r == c ? 0.5 : 1.0) * v * x[r], x[c]);
                    cm_add_product(&fr[r], &frc[r], -lam[a] * v, x[c]);
                    ok = ent_put(&ents, row, r, v * x[c]) &&
                         ent_put(&ents, r, c, -lam[a] * v);
                    if (r != c) {
                        cm_add_product(&fr[c], &frc[c], -lam[a] * v, x[r]);
                        ok = ok && ent_put(&ents, row, c, v * x[r]);
                    }
                }
                jm_obj_add(&cval, &cvalc, -atgt[a]);
            } else if (akind[a] == 1) {
                const int64_t k = aidx[a], b = m->cone_start[k];
                const int64_t d = m->cone_start[k + 1] - b;
                const bool rot = m->cone_type[k] == JAOS_CONE_ROTATED;
                double vn, hv;
                cm_cone_grad(m, k, x, cv, cg, &vn, &hv);
                cval = vn - hv;
                const double gam = -lam[a] / vn, root = sqrt(fabs(gam));
                for (int64_t t = 0; t < d && ok; t++) {
                    const int64_t j = m->cone_col[b + t];
                    cm_add_product(&fr[j], &frc[j], -lam[a], cg[t]);
                    ok = ent_put(&ents, row, j, cg[t]);
                    double ct;
                    const int64_t at = cm_member(rot, t, &ct);
                    const double ut = at >= 0 ? 0.0 + ct * cv[at] / vn : 0.0;
                    if (yat[a] >= 0) {
                        ok = ok && ent_put(&ents, j, yat[a],
                                           gam >= 0.0 ? -root * ut : root * ut);
                        for (int64_t s = rot && t == 1 ? 0 : t; s <= t && ok;
                             s++) {
                            double cs;
                            const int64_t as = cm_member(rot, s, &cs);
                            const double p = at >= 0 && at == as ? ct * cs : 0.0;
                            ok = ent_put(&ents, j, m->cone_col[b + s], gam * p);
                        }
                        continue;
                    }
                    for (int64_t s = 0; s <= t && ok; s++) {
                        double cs;
                        const int64_t as = cm_member(rot, s, &cs);
                        const double us = as >= 0 ? 0.0 + cs * cv[as] / vn : 0.0;
                        double hts = at >= 0 && at == as ? 0.0 + ct * cs : 0.0;
                        hts = (hts - ut * us) / vn;
                        ok = ent_put(&ents, j, m->cone_col[b + s],
                                     -lam[a] * hts);
                    }
                }
                if (yat[a] >= 0) {
                    ok = ok && ent_put(&ents, yat[a], yat[a],
                                       gam >= 0.0 ? 1.0 : -1.0);
                    sign[yat[a]] = gam >= 0.0 ? 1 : -1;
                    rhs[yat[a]] = 0.0;
                }
            } else {
                const int64_t j = akind[a] == 2 ? aidx[a]
                                                : m->cone_col[aidx[a]];
                cval = x[j] - atgt[a];
                cm_add_product(&fr[j], &frc[j], -lam[a], 1.0);
                ok = ent_put(&ents, row, j, 1.0);
            }
            ok = ok && ent_put(&ents, row, row, -CONIC_REG);
            const double c = cval + cvalc;
            rhs[row] = -c;
            if (fabs(c) > worst)
                worst = fabs(c);
        }
        if (!ok) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        for (int64_t j = 0; j < n; j++) {
            const double f = fr[j] + frc[j];
            rhs[j] = -f;
            if (fabs(f) > worst)
                worst = fabs(f);
        }
        jm_log(m, JAOS_LOG_DETAIL, "conic: Newton polish step %lld on %lld "
               "active constraints, KKT residual %.3e", (long long)step,
               (long long)na, worst);
        if (!(worst < worst_best)) {
            memcpy(x, best, (size_t)n * sizeof *x);
            memcpy(lam, lbest, (size_t)na * sizeof *lam);
            break;
        }
        worst_best = worst;
        memcpy(best, x, (size_t)n * sizeof *x);
        memcpy(lbest, lam, (size_t)na * sizeof *lam);
        if (step >= CONIC_NEWTON_STEPS || worst == 0.0)
            break;

        qsort(ents.e, (size_t)ents.n, sizeof *ents.e, cmp_ent);
        int64_t u = 0;
        for (int64_t t = 0; t < ents.n; t++) {
            if (u > 0 && ents.e[u - 1].r == ents.e[t].r &&
                ents.e[u - 1].c == ents.e[t].c) {
                ents.e[u - 1].v += ents.e[t].v;
                continue;
            }
            ents.e[u++] = ents.e[t];
        }
        if (!symbolic) {
            ks = jm_calloc_array(kn + 1, sizeof *ks);
            ki = jm_alloc_array(u > 0 ? u : 1, sizeof *ki);
            kv = jm_alloc_array(u > 0 ? u : 1, sizeof *kv);
            if (ks == nullptr || ki == nullptr || kv == nullptr) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }
            for (int64_t t = 0; t < u; t++) {
                ks[ents.e[t].c + 1]++;
                ki[t] = ents.e[t].r;
            }
            for (int64_t k = 0; k < kn; k++)
                ks[k + 1] += ks[k];
            if (jm_chol_symbolic_dense(&ldl, kn, ks, ki, jm_chol_dense_limit(kn),
                                       work) != JAOS_OK)
                goto done;
            symbolic = true;
        } else if (u != ks[kn]) {
            break;
        }
        for (int64_t t = 0; t < u; t++)
            kv[t] = ents.e[t].v;
        if (jm_ldlt_numeric(&ldl, kv, sign, CONIC_PIVOT, work) != JAOS_OK)
            break;
        memcpy(sol, rhs, (size_t)kn * sizeof *sol);
        jm_ldlt_solve(&ldl, sol, work);
        for (int64_t pass = 0; pass < CONIC_REFINE; pass++) {
            for (int64_t k = 0; k < kn; k++)
                res[k] = rhs[k];
            for (int64_t col = 0; col < kn; col++)
                for (int64_t p = ks[col]; p < ks[col + 1]; p++) {
                    double v = kv[p];
                    if (ki[p] == col && col < n + na)
                        v -= col < n ? CONIC_REG : -CONIC_REG;
                    res[ki[p]] -= v * sol[col];
                }
            double rmax = 0.0, bmax = 0.0;
            for (int64_t k = 0; k < kn; k++) {
                if (fabs(res[k]) > rmax)
                    rmax = fabs(res[k]);
                if (fabs(rhs[k]) > bmax)
                    bmax = fabs(rhs[k]);
            }
            if (rmax <= 1e-15 * bmax)
                break;
            jm_ldlt_solve(&ldl, res, work);
            for (int64_t k = 0; k < kn; k++)
                sol[k] += res[k];
        }
        jm_work_add(work, (CONIC_REFINE + 2) * u * JM_WORK_NONZERO);
        for (int64_t j = 0; j < n; j++)
            x[j] += sol[j];
        for (int64_t a = 0; a < na; a++)
            lam[a] -= sol[n + a];
    }

    for (int64_t a = 0; a < na; a++)
        if (akind[a] == 2)
            x[aidx[a]] = atgt[a];
        else if (akind[a] == 3)
            x[m->cone_col[aidx[a]]] = 0.0;
    for (int64_t a = 0; a < na; a++) {
        if (akind[a] == 0) {
            ny[aidx[a]] = sigma * lam[a] == 0.0 ? 0.0 : sigma * lam[a];
        } else if (akind[a] == 3) {
            nz[aidx[a]] = sigma * lam[a] == 0.0 ? 0.0 : sigma * lam[a];
        } else if (akind[a] == 1) {
            const int64_t k = aidx[a], b = m->cone_start[k];
            const int64_t d = m->cone_start[k + 1] - b;
            double vn, hv;
            cm_cone_grad(m, k, x, cv, cg, &vn, &hv);
            for (int64_t t = 0; t < d; t++) {
                const double v = sigma * lam[a] * cg[t];
                nz[b + t] = v == 0.0 ? 0.0 : v;
            }
        }
    }

    const double tol = jm_dual_tolerance(m);
    jaos_check_report was, now;
    st = jaos_check_conic_solution(m, m->sol_col, m->sol_dual,
                                   nk > 0 ? m->sol_cone : nullptr, tol, &was);
    if (st != JAOS_OK ||
        jaos_check_conic_solution(m, x, ny, nk > 0 ? nz : nullptr, tol,
                                  &now) != JAOS_OK) {
        st = JAOS_OK;
        goto done;
    }
    const double wb = fmax(fmax(was.max_row_violation_relative,
                                was.max_col_violation),
                           fmax(was.max_cone_violation, was.max_dual_violation));
    const double nb = fmax(fmax(now.max_row_violation_relative,
                                now.max_col_violation),
                           fmax(now.max_cone_violation, now.max_dual_violation));
    const bool was_ok = was.primal_feasible && was.dual_feasible;
    const bool better = now.primal_feasible && now.dual_feasible
                            ? !was_ok || nb < wb
                            : !was_ok && nb < wb;
    jm_log(m, JAOS_LOG_DETAIL, "conic: Newton polish %s: worst violation "
           "%.3e to %.3e", better ? "taken" : "refused", wb, nb);
    if (!better)
        goto done;

    for (int64_t i = 0; i < nr; i++) {
        double act = 0.0, comp = 0.0;
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            cm_add_product(&act, &comp, m->ar_value[p], x[m->ar_index[p]]);
        for (int64_t p = m->rq_start != nullptr ? m->rq_start[i] : 0;
             m->rq_start != nullptr && p < m->rq_start[i + 1]; p++) {
            const int64_t r = m->rq_i[p], c = m->rq_j[p];
            cm_add_product(&act, &comp,
                           (r == c ? 0.5 : 1.0) * m->rq_v[p] * x[r], x[c]);
        }
        const double a = act + comp;
        nact[i] = a == 0.0 ? 0.0 : a;
    }
    for (int64_t j = 0; j < n; j++) {
        fr[j] = 0.0;
        frc[j] = 0.0;
    }
    for (int64_t a = 0; a < na; a++)
        if (akind[a] == 2)
            nd[aidx[a]] = sigma * lam[a] == 0.0 ? 0.0 : sigma * lam[a];
    for (int64_t j = 0; j < n; j++)
        m->sol_col[j] = x[j] == 0.0 ? 0.0 : x[j];
    memcpy(m->sol_row, nact, (size_t)nr * sizeof *nact);
    memcpy(m->sol_dual, ny, (size_t)nr * sizeof *ny);
    memcpy(m->sol_redcost, nd, (size_t)n * sizeof *nd);
    if (members > 0)
        memcpy(m->sol_cone, nz, (size_t)members * sizeof *nz);
    *moved = true;

done:
    jm_chol_free(&ldl);
    free(ents.e);
    free(akind); free(aidx); free(yat); free(atgt); free(face);
    free(cv); free(cg); free(x); free(best); free(fr); free(frc); free(ny);
    free(nz); free(nd); free(nact); free(lam); free(lbest); free(rhs);
    free(sol); free(res); free(kv); free(ks); free(ki); free(sign);
    return st;
}

static void cm_free_rows(cm_rows *r)
{
    free(r->t);
    free(r->b);
    memset(r, 0, sizeof *r);
}

static int cmp_trip(const void *pa, const void *pb)
{
    const cm_trip *a = pa, *b = pb;
    if (a->col != b->col)
        return (a->col > b->col) - (a->col < b->col);
    return (a->row > b->row) - (a->row < b->row);
}

static void cm_ray_apply(const int64_t *rs, const int64_t *rc,
                         const double *rv, int64_t nrow, const double *w,
                         double *out, int64_t n)
{
    for (int64_t j = 0; j < n; j++)
        out[j] = 0.0;
    for (int64_t t = 0; t < nrow; t++)
        for (int64_t p = rs[t]; p < rs[t + 1]; p++)
            out[rc[p]] += rv[p] * w[t];
}

static void cm_ray_rows(const int64_t *rs, const int64_t *rc, const double *rv,
                        int64_t nrow, const double *v, double *out)
{
    for (int64_t t = 0; t < nrow; t++) {
        double s = 0.0;
        for (int64_t p = rs[t]; p < rs[t + 1]; p++)
            s += rv[p] * v[rc[p]];
        out[t] = s;
    }
}

static jaos_status ray_polish(jaos_model *m, const cm_qc *qc, double *d,
                              jm_work *work)
{
    const int64_t n = m->num_col, nr = m->num_row;
    int64_t nrow = 0, nent = 0;
    for (int64_t i = 0; i < nr; i++) {
        nrow += 1 + qc[i].r;
        nent += m->ar_start[i + 1] - m->ar_start[i] + qc[i].r * qc[i].k;
    }
    int64_t *rs = jm_alloc_array(nrow + 1, sizeof *rs);
    int64_t *rc = jm_alloc_array(nent > 0 ? nent : 1, sizeof *rc);
    double *rv = jm_alloc_array(nent > 0 ? nent : 1, sizeof *rv);
    double *b = jm_alloc_array(nrow > 0 ? nrow : 1, sizeof *b);
    double *w = jm_calloc_array(nrow > 0 ? nrow : 1, sizeof *w);
    double *r = jm_alloc_array(nrow > 0 ? nrow : 1, sizeof *r);
    double *p = jm_alloc_array(nrow > 0 ? nrow : 1, sizeof *p);
    double *q = jm_alloc_array(nrow > 0 ? nrow : 1, sizeof *q);
    double *v = jm_alloc_array(n > 0 ? n : 1, sizeof *v);
    jaos_status st = JAOS_OK;
    if (rs == nullptr || rc == nullptr || rv == nullptr || b == nullptr ||
        w == nullptr || r == nullptr || p == nullptr || q == nullptr ||
        v == nullptr) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    int64_t t = 0, at = 0;
    rs[0] = 0;
    for (int64_t i = 0; i < nr; i++) {
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        double move = 0.0, traffic = 0.0;
        for (int64_t e = m->ar_start[i]; e < m->ar_start[i + 1]; e++) {
            move += m->ar_value[e] * d[m->ar_index[e]];
            traffic += fabs(m->ar_value[e] * d[m->ar_index[e]]);
        }
        const bool sided = isfinite(lo) || isfinite(hi);
        if (sided && ((isfinite(lo) && lo == hi) ||
                      fabs(move) <= CONIC_RAY_ACTIVE * traffic)) {
            for (int64_t e = m->ar_start[i]; e < m->ar_start[i + 1]; e++)
                if (d[m->ar_index[e]] != 0.0) {
                    rc[at] = m->ar_index[e];
                    rv[at++] = m->ar_value[e];
                }
            if (at > rs[t])
                rs[++t] = at;
        }
        for (int64_t f = 0; f < qc[i].r; f++) {
            for (int64_t c = 0; c < qc[i].k; c++) {
                const double fv = qc[i].f[f * qc[i].k + c];
                if (fv != 0.0 && d[qc[i].cols[c]] != 0.0) {
                    rc[at] = qc[i].cols[c];
                    rv[at++] = fv;
                }
            }
            if (at > rs[t])
                rs[++t] = at;
        }
    }
    nrow = t;
    if (nrow == 0)
        goto done;
    cm_ray_rows(rs, rc, rv, nrow, d, b);
    double rr = 0.0;
    for (int64_t k = 0; k < nrow; k++) {
        r[k] = p[k] = b[k];
        rr += b[k] * b[k];
    }
    const double rr0 = rr;
    int64_t it = 0;
    for (; it < CONIC_RAY_ITERS && rr > CONIC_RAY_TOL * rr0; it++) {
        cm_ray_apply(rs, rc, rv, nrow, p, v, n);
        cm_ray_rows(rs, rc, rv, nrow, v, q);
        const double pq = dot(p, q, nrow);
        if (!(pq > 0.0))
            break;
        const double alpha = rr / pq;
        double next = 0.0;
        for (int64_t k = 0; k < nrow; k++) {
            w[k] += alpha * p[k];
            r[k] -= alpha * q[k];
            next += r[k] * r[k];
        }
        const double beta = next / rr;
        for (int64_t k = 0; k < nrow; k++)
            p[k] = r[k] + beta * p[k];
        rr = next;
    }
    jm_work_add(work, (it + 2) * (2 * at + n + nrow) * JM_WORK_NONZERO);
    cm_ray_apply(rs, rc, rv, nrow, w, v, n);
    for (int64_t j = 0; j < n; j++)
        if (d[j] != 0.0)
            d[j] -= v[j];

done:
    free(rs); free(rc); free(rv); free(b); free(w); free(r); free(p);
    free(q); free(v);
    return st;
}

static jaos_status conic_solve(jaos_model *m, int64_t work0, int64_t iters0);

jaos_status jm_conic(jaos_model *m)
{
    return conic_solve(m, 0, 0);
}

jaos_status jm_conic_after_barrier(jaos_model *m)
{
    const double limit = m->cfg.time_limit, spent = m->solve_time;
    if (limit > 0.0 && spent >= limit)
        return JAOS_OK;
    const bool node = m->cfg.node_solve;
    jm_log(m, JAOS_LOG_SUMMARY, "the barrier ended without an answer after "
           "%lld iterations and %lld work units; the conic interior point "
           "solves the model again", (long long)m->solve_iters,
           (long long)m->solve_work);
    m->err[0] = '\0';
    m->cfg.node_solve = false;
    if (limit > 0.0)
        m->cfg.time_limit = limit - spent;
    const jaos_status st = conic_solve(m, m->solve_work, m->solve_iters);
    m->cfg.node_solve = node;
    m->cfg.time_limit = limit;
    m->solve_time += spent;
    return st;
}

enum { CM_ZERO = 1, CM_HEAD = 2, CM_FREE = 4, CM_ONCE = 8, CM_TWICE = 16,
       CM_QUAD = 32 };
enum { CM_DEAD = 1, CM_IDLE = 2 };

static int64_t cm_drop_cones(const jaos_model *m, uint8_t *drop,
                             uint8_t *flag)
{
    const int64_t nk = m->num_cone, n = m->num_col;
    for (int64_t k = 0; k < nk; k++) {
        const int64_t b = m->cone_start[k];
        flag[m->cone_col[b]] |= CM_HEAD;
        if (m->cone_type[k] == JAOS_CONE_ROTATED)
            flag[m->cone_col[b + 1]] |= CM_HEAD;
        for (int64_t t = b; t < m->cone_start[k + 1]; t++)
            flag[m->cone_col[t]] |= flag[m->cone_col[t]] & CM_ONCE ? CM_TWICE
                                                                    : CM_ONCE;
    }
    for (int64_t p = 0; p < m->rq_nz; p++) {
        flag[m->rq_i[p]] |= CM_QUAD;
        flag[m->rq_j[p]] |= CM_QUAD;
    }
    for (int64_t j = 0; j < n; j++) {
        if (m->col_quad != nullptr && m->col_quad[j] != 0.0)
            flag[j] |= CM_QUAD;
        for (int64_t p = m->q_start != nullptr ? m->q_start[j] : 0;
             m->q_start != nullptr && p < m->q_start[j + 1]; p++) {
            flag[j] |= CM_QUAD;
            flag[m->q_index[p]] |= CM_QUAD;
        }
    }
    int64_t count = 0;
    for (int64_t k = 0; k < nk; k++) {
        const int64_t b = m->cone_start[k], e = m->cone_start[k + 1];
        const int64_t h = m->cone_col[b];
        if (m->cone_type[k] != JAOS_CONE_QUADRATIC)
            continue;
        if (m->col_upper[h] == INFINITY && m->col_cost[h] == 0.0 &&
            m->a_start[h + 1] == m->a_start[h] &&
            (flag[h] & (CM_TWICE | CM_QUAD)) == 0) {
            drop[k] = CM_IDLE;
            flag[h] |= CM_FREE;
            count++;
            continue;
        }
        if (m->col_upper[h] != 0.0)
            continue;
        bool ok = true;
        for (int64_t t = b + 1; t < e && ok; t++) {
            const int64_t c = m->cone_col[t];
            ok = (flag[c] & CM_HEAD) == 0 && m->col_lower[c] <= 0.0 &&
                 m->col_upper[c] >= 0.0;
        }
        if (!ok)
            continue;
        drop[k] = CM_DEAD;
        count++;
        for (int64_t t = b; t < e; t++)
            flag[m->cone_col[t]] |= CM_ZERO;
    }
    return count;
}

static void cm_idle_heads(const jaos_model *m, const uint8_t *drop, double *v,
                          bool ray)
{
    for (int64_t k = 0; k < m->num_cone; k++) {
        const int64_t b = m->cone_start[k], e = m->cone_start[k + 1];
        if (drop[k] != CM_IDLE)
            continue;
        double rest = 0.0;
        for (int64_t t = b + 1; t < e; t++)
            rest += v[m->cone_col[t]] * v[m->cone_col[t]];
        const double lo = m->col_lower[m->cone_col[b]];
        const double r = sqrt(rest);
        v[m->cone_col[b]] = !ray && lo > r ? lo : r;
    }
}

static void cm_dead_fill(const jaos_model *m, const uint8_t *dead,
                         uint8_t *seen, double *r, double *z)
{
    const int64_t nk = m->num_cone;
    for (int64_t k = 0; k < nk; k++) {
        const int64_t b = m->cone_start[k], e = m->cone_start[k + 1];
        if (dead[k] != CM_DEAD)
            continue;
        for (int64_t t = b + 1; t < e; t++) {
            const int64_t c = m->cone_col[t];
            z[t] = seen[c] ? 0.0 : r[c];
            if (!seen[c])
                r[c] = 0.0;
            seen[c] = 1;
        }
        double rest = 0.0;
        for (int64_t t = b + 1; t < e; t++)
            rest += z[t] * z[t];
        z[b] = sqrt(rest);
        r[m->cone_col[b]] -= z[b];
    }
    for (int64_t k = 0; k < nk; k++) {
        const int64_t b = m->cone_start[k], h = m->cone_col[b];
        if (dead[k] != CM_DEAD || seen[h] == 2)
            continue;
        seen[h] = 2;
        if (m->col_lower[h] < 0.0 && r[h] > 0.0) {
            z[b] += r[h];
            r[h] = 0.0;
        }
    }
}

static jaos_status cm_dead_duals(jaos_model *m, const uint8_t *dead,
                                 const uint8_t *flag, double sigma)
{
    const int64_t n = m->num_col, members = m->cone_start[m->num_cone];
    uint8_t *seen = jm_calloc_array(n > 0 ? n : 1, sizeof *seen);
    if (seen == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    for (int64_t j = 0; j < n; j++) {
        if (flag[j] & CM_ZERO)
            m->sol_col[j] = 0.0;
        if (flag[j] & CM_FREE)
            m->sol_redcost[j] = 0.0;
        m->sol_redcost[j] *= sigma;
    }
    cm_idle_heads(m, dead, m->sol_col, false);
    for (int64_t t = 0; t < members; t++)
        m->sol_cone[t] *= sigma;
    cm_dead_fill(m, dead, seen, m->sol_redcost, m->sol_cone);
    for (int64_t j = 0; j < n; j++)
        m->sol_redcost[j] = sigma * m->sol_redcost[j] == 0.0
                                ? 0.0 : sigma * m->sol_redcost[j];
    for (int64_t t = 0; t < members; t++)
        m->sol_cone[t] = sigma * m->sol_cone[t] == 0.0
                             ? 0.0 : sigma * m->sol_cone[t];
    free(seen);
    return JAOS_OK;
}

static jaos_status cm_dead_certificate(jaos_model *m, const uint8_t *dead,
                                       const uint8_t *flag)
{
    const int64_t n = m->num_col, nk = m->num_cone;
    uint8_t *seen = jm_calloc_array(n > 0 ? n : 1, sizeof *seen);
    double *a = jm_calloc_array(n > 0 ? n : 1, sizeof *a);
    double *ac = jm_calloc_array(n > 0 ? n : 1, sizeof *ac);
    if (seen == nullptr || a == nullptr || ac == nullptr) {
        free(seen); free(a); free(ac);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < n; j++) {
        if (!(flag[j] & CM_ZERO))
            continue;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            cm_add_product(&a[j], &ac[j], m->a_value[p],
                           m->sol_farkas[m->a_index[p]]);
    }
    for (int64_t k = 0; k < nk; k++)
        for (int64_t t = m->cone_start[k];
             !dead[k] && t < m->cone_start[k + 1]; t++)
            if (flag[m->cone_col[t]] & CM_ZERO)
                jm_obj_add(&a[m->cone_col[t]], &ac[m->cone_col[t]],
                           m->sol_cone[t]);
    for (int64_t j = 0; j < n; j++)
        a[j] = -(a[j] + ac[j]);
    cm_dead_fill(m, dead, seen, a, m->sol_cone);
    free(seen); free(a); free(ac);
    return JAOS_OK;
}

constexpr double CONIC_RAY_PROBE_TOL = 1e-6;

static jaos_status cm_zero_row(jaos_model *r, double *row, int64_t *idx,
                               int64_t nc)
{
    int64_t n = 0;
    for (int64_t k = 0; k < nc; k++)
        if (row[k] != 0.0) {
            idx[n] = k;
            row[n] = row[k];
            n++;
        }
    if (n == 0)
        return JAOS_OK;
    const double zero = 0.0;
    const int64_t rs[2] = {0, n};
    return jaos_add_rows(r, 1, &zero, &zero, n, rs, idx, row);
}

static jaos_status cm_ray_probe(jaos_model *m, jm_work *work, bool *found)
{
    *found = false;
    const int64_t nc = m->num_col, nr = m->num_row;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    jaos_model *r = nullptr;
    double *cost = jm_alloc_array(nc > 0 ? nc : 1, sizeof *cost);
    double *lo = jm_alloc_array(nc > 0 ? nc : 1, sizeof *lo);
    double *hi = jm_alloc_array(nc > 0 ? nc : 1, sizeof *hi);
    double *d = jm_alloc_array(nc > 0 ? nc : 1, sizeof *d);
    double *qrow = jm_calloc_array(nc > 0 ? nc : 1, sizeof *qrow);
    uint8_t *touched = jm_calloc_array(nc > 0 ? nc : 1, sizeof *touched);
    int64_t *idx = jm_alloc_array(nc > 0 ? nc : 1, sizeof *idx);
    jaos_status st = JAOS_ERR_OUT_OF_MEMORY;
    if (cost == nullptr || lo == nullptr || hi == nullptr || d == nullptr ||
        qrow == nullptr || touched == nullptr || idx == nullptr)
        goto out;
    for (int64_t j = 0; j < nc; j++) {
        cost[j] = sigma * m->col_cost[j];
        lo[j] = isfinite(m->col_lower[j]) ? 0.0 : -1.0;
        hi[j] = isfinite(m->col_upper[j]) ? 0.0 : 1.0;
    }
    st = jaos_model_new(&r);
    if (st != JAOS_OK)
        goto out;
    st = jaos_load_lp(r, nc, 0, JAOS_MINIMIZE, 0.0, cost, lo, hi, nullptr,
                      nullptr, 0, nullptr, nullptr, nullptr);
    if (st != JAOS_OK)
        goto out;
    st = jm_model_ensure_rowwise(m);
    if (st != JAOS_OK)
        goto out;
    for (int64_t i = 0; i < nr && st == JAOS_OK; i++) {
        const double rl = isfinite(m->row_lower[i]) ? 0.0 : -INFINITY;
        const double ru = isfinite(m->row_upper[i]) ? 0.0 : INFINITY;
        const int64_t n = m->ar_start[i + 1] - m->ar_start[i];
        const int64_t rs[2] = {0, n};
        if (isfinite(rl) || isfinite(ru))
            st = jaos_add_rows(r, 1, &rl, &ru, n, rs,
                               m->ar_index + m->ar_start[i],
                               m->ar_value + m->ar_start[i]);
    }
    for (int64_t j = 0; j < nc; j++) {
        if (m->col_quad != nullptr && m->col_quad[j] != 0.0)
            touched[j] = 1;
        for (int64_t p = m->q_start != nullptr ? m->q_start[j] : 0;
             m->q_start != nullptr && p < m->q_start[j + 1]; p++)
            touched[j] = touched[m->q_index[p]] = 1;
    }
    for (int64_t j = 0; j < nc && st == JAOS_OK; j++) {
        if (!touched[j])
            continue;
        for (int64_t k = 0; k < nc; k++)
            qrow[k] = 0.0;
        if (m->col_quad != nullptr)
            qrow[j] += m->col_quad[j];
        for (int64_t p = m->q_start != nullptr ? m->q_start[j] : 0;
             m->q_start != nullptr && p < m->q_start[j + 1]; p++)
            qrow[m->q_index[p]] += m->q_value[p];
        for (int64_t k = 0; k < nc; k++)
            for (int64_t p = m->q_start != nullptr ? m->q_start[k] : 0;
                 m->q_start != nullptr && p < m->q_start[k + 1]; p++)
                if (m->q_index[p] == j)
                    qrow[k] += m->q_value[p];
        st = cm_zero_row(r, qrow, idx, nc);
    }
    for (int64_t i = 0; m->rq_start != nullptr && i < nr && st == JAOS_OK;
         i++) {
        for (int64_t p = m->rq_start[i]; p < m->rq_start[i + 1]; p++)
            touched[m->rq_i[p]] = touched[m->rq_j[p]] = 2;
        for (int64_t j = 0; j < nc && st == JAOS_OK; j++) {
            if (touched[j] != 2)
                continue;
            for (int64_t k = 0; k < nc; k++)
                qrow[k] = 0.0;
            for (int64_t p = m->rq_start[i]; p < m->rq_start[i + 1]; p++) {
                if (m->rq_i[p] == j)
                    qrow[m->rq_j[p]] += m->rq_v[p];
                else if (m->rq_j[p] == j)
                    qrow[m->rq_i[p]] += m->rq_v[p];
            }
            st = cm_zero_row(r, qrow, idx, nc);
        }
        for (int64_t p = m->rq_start[i]; p < m->rq_start[i + 1]; p++)
            touched[m->rq_i[p]] = touched[m->rq_j[p]] = 0;
    }
    for (int64_t k = 0; k < m->num_cone && st == JAOS_OK; k++)
        st = jaos_add_cone(r, m->cone_type[k],
                           m->cone_start[k + 1] - m->cone_start[k],
                           m->cone_col + m->cone_start[k]);
    if (st != JAOS_OK)
        goto out;
    r->cfg.log_cb = nullptr;
    r->cfg.progress_cb = nullptr;
    r->cfg.incumbent_cb = nullptr;
    if (m->cfg.work_limit > 0) {
        const int64_t left = m->cfg.work_limit - work->units;
        r->cfg.work_limit = left > 0 ? left : 1;
    }
    st = jaos_solve(r);
    jm_work_add(work, r->solve_work);
    if (st != JAOS_OK || jaos_status_of(r) != JAOS_SOLVE_OPTIMAL)
        goto out;
    st = jaos_solution(r, d, nullptr, nullptr, nullptr);
    if (st != JAOS_OK)
        goto out;
    double big = 0.0;
    for (int64_t j = 0; j < nc; j++)
        if (fabs(d[j]) > big)
            big = fabs(d[j]);
    for (int64_t j = 0; j < nc; j++) {
        if (fabs(d[j]) <= CONIC_RAY_ZERO * big)
            d[j] = 0.0;
        d[j] = d[j] < lo[j] ? lo[j] : d[j] > hi[j] ? hi[j] : d[j];
    }
    jaos_ray_report rr;
    st = jaos_check_ray(m, d, CONIC_RAY_PROBE_TOL, &rr);
    jm_log(m, JAOS_LOG_DETAIL, "conic: the direction solve ends %s, its "
           "direction rated %.3g, past a column bound %.3g, past a row side "
           "%.3g, curvature %.3g",
           jaos_solve_status_str(jaos_status_of(r)), rr.rate,
           rr.max_col_escape, rr.max_row_escape, rr.curvature);
    if (st == JAOS_OK && rr.certified) {
        for (int64_t j = 0; j < nc; j++)
            m->sol_ray[j] = d[j] == 0.0 ? 0.0 : d[j];
        m->ray_ok = true;
        *found = true;
    }
out:
    jaos_model_free(r);
    free(cost); free(lo); free(hi); free(d); free(qrow); free(touched);
    free(idx);
    return st;
}

static double cm_cert_gap(jaos_model *m, const double *base, double t,
                          double tol, jaos_certificate_report *cr,
                          int64_t *calls)
{
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_farkas[i] = m->rq_start[i + 1] > m->rq_start[i]
                               ? t * base[i] : base[i];
    (*calls)++;
    if (jaos_check_conic_certificate(m, m->sol_farkas,
                                     m->num_cone > 0 ? m->sol_cone : nullptr,
                                     tol, cr) != JAOS_OK)
        return -INFINITY;
    return cr->gap;
}

static bool cm_cert_tilt(jaos_model *m, double tol,
                         jaos_certificate_report *cr, int64_t *calls)
{
    if (m->rq_start == nullptr || m->rq_nz == 0 || m->num_row == 0)
        return false;
    double *base = jm_alloc_array(m->num_row, sizeof *base);
    if (base == nullptr)
        return false;
    memcpy(base, m->sol_farkas, (size_t)m->num_row * sizeof *base);
    double lo = 0.0, at = 1.0, best = -INFINITY;
    for (int64_t k = -CONIC_CERT_TILT; k <= CONIC_CERT_TILT; k++) {
        const double t = ldexp(1.0, (int)k);
        const double g = cm_cert_gap(m, base, t, tol, cr, calls);
        if (g > best) {
            best = g;
            at = t;
        }
    }
    double hi = 2.0 * at;
    lo = 0.5 * at;
    for (int64_t k = 0; k < CONIC_CERT_TILT; k++) {
        const double ml = lo + (hi - lo) / 3.0, mr = hi - (hi - lo) / 3.0;
        const double gl = cm_cert_gap(m, base, ml, tol, cr, calls);
        const double gr = cm_cert_gap(m, base, mr, tol, cr, calls);
        if (gl > best) { best = gl; at = ml; }
        if (gr > best) { best = gr; at = mr; }
        if (gl < gr)
            lo = ml;
        else
            hi = mr;
    }
    const bool ok = cm_cert_gap(m, base, at, tol, cr, calls) > -INFINITY &&
                    cr->certified;
    if (!ok)
        memcpy(m->sol_farkas, base, (size_t)m->num_row * sizeof *base);
    free(base);
    return ok;
}

static bool cm_row_curves(const jaos_model *m, int64_t i, double y)
{
    if (m->rq_start == nullptr || y == 0.0)
        return true;
    for (int64_t p = m->rq_start[i]; p < m->rq_start[i + 1]; p++)
        if (m->rq_i[p] != m->rq_j[p] || y * m->rq_v[p] > 0.0)
            return false;
    return true;
}

static bool cm_cert_signs(jaos_model *m)
{
    bool moved = false;
    for (int64_t i = 0; i < m->num_row; i++) {
        if (cm_row_curves(m, i, m->sol_farkas[i]) ||
            !cm_row_curves(m, i, -m->sol_farkas[i]))
            continue;
        m->sol_farkas[i] = -m->sol_farkas[i];
        moved = true;
    }
    return moved;
}

static void cm_cert_project(jaos_model *m)
{
    constexpr double RT = 0.70710678118654752440;
    for (int64_t k = 0; k < m->num_cone; k++) {
        const int64_t b = m->cone_start[k], e = m->cone_start[k + 1];
        const bool rot = m->cone_type[k] == JAOS_CONE_ROTATED && b + 1 < e;
        double head = m->sol_cone[b], second = 0.0, rest = 0.0;
        if (rot) {
            head = (m->sol_cone[b] + m->sol_cone[b + 1]) * RT;
            second = (m->sol_cone[b] - m->sol_cone[b + 1]) * RT;
            rest = second * second;
        }
        for (int64_t t = rot ? b + 2 : b + 1; t < e; t++)
            rest += m->sol_cone[t] * m->sol_cone[t];
        const double norm = sqrt(rest);
        if (norm <= head)
            continue;
        const double s = norm <= -head ? 0.0 : 0.5 * (head + norm) / norm;
        const double h = norm <= -head ? 0.0 : 0.5 * (head + norm);
        if (rot) {
            const double u = second * s;
            m->sol_cone[b] = (h + u) * RT;
            m->sol_cone[b + 1] = (h - u) * RT;
        } else {
            m->sol_cone[b] = h;
        }
        for (int64_t t = rot ? b + 2 : b + 1; t < e; t++)
            m->sol_cone[t] *= s;
    }
}

static double cm_cert_at(jaos_model *m, int64_t i, double v, double tol,
                         jaos_certificate_report *cr, int64_t *calls)
{
    const double keep = m->sol_farkas[i];
    m->sol_farkas[i] = v;
    (*calls)++;
    const bool ok = jaos_check_conic_certificate(
                        m, m->sol_farkas,
                        m->num_cone > 0 ? m->sol_cone : nullptr, tol, cr) ==
                    JAOS_OK;
    m->sol_farkas[i] = keep;
    return ok ? cr->gap : -INFINITY;
}

static bool cm_cert_climb(jaos_model *m, double tol,
                          jaos_certificate_report *cr, int64_t *calls)
{
    const int64_t nr = m->num_row;
    if (nr == 0 || *calls >= CONIC_CERT_CALLS)
        return false;
    double big = 0.0;
    for (int64_t i = 0; i < nr; i++)
        if (fabs(m->sol_farkas[i]) > big)
            big = fabs(m->sol_farkas[i]);
    if (!(big > 0.0))
        return false;
    double best = cm_cert_at(m, 0, m->sol_farkas[0], tol, cr, calls);
    bool certified = best > -INFINITY && cr->certified;
    for (int64_t sweep = 0; sweep < CONIC_CERT_SWEEPS && !certified; sweep++) {
        bool moved = false;
        for (int64_t i = 0; i < nr && !certified; i++) {
            const double at = m->sol_farkas[i];
            double pick = at;
            for (int64_t k = -CONIC_CERT_TILT;
                 k <= CONIC_CERT_TILT && *calls < CONIC_CERT_CALLS; k++) {
                const double step = ldexp(big, (int)k);
                for (int s = -1; s <= 1; s += 2) {
                    const double v = at + (double)s * step;
                    const double g = cm_cert_at(m, i, v, tol, cr, calls);
                    if (g > best) {
                        best = g;
                        pick = v;
                        certified = cr->certified;
                    }
                    if (certified)
                        break;
                }
                if (certified)
                    break;
            }
            if (pick != at) {
                m->sol_farkas[i] = pick;
                moved = true;
            }
        }
        if (!moved)
            break;
    }
    if (!certified)
        return false;
    return cm_cert_at(m, 0, m->sol_farkas[0], tol, cr, calls) > -INFINITY &&
           cr->certified;
}

static bool cm_cert_search(jaos_model *m, double tol, int64_t members,
                           jaos_certificate_report *cr, int64_t *calls)
{
    if (cm_cert_tilt(m, tol, cr, calls))
        return true;
    if (cm_cert_climb(m, tol, cr, calls))
        return true;
    if (cm_cert_signs(m) && cm_cert_climb(m, tol, cr, calls))
        return true;
    if (members > 0) {
        cm_cert_project(m);
        if (cm_cert_climb(m, tol, cr, calls))
            return true;
    }
    return false;
}

static jaos_status cm_cert_trim(jaos_model *m, double big, double tol)
{
    const int64_t n = m->num_col, nk = m->num_cone;
    const int64_t members = nk > 0 ? m->cone_start[nk] : 0;
    double *a = jm_calloc_array(n > 0 ? n : 1, sizeof *a);
    double *traf = jm_calloc_array(n > 0 ? n : 1, sizeof *traf);
    if (a == nullptr || traf == nullptr) {
        free(a); free(traf);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < n; j++)
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const double v = m->a_value[p] * m->sol_farkas[m->a_index[p]];
            a[j] += v;
            traf[j] += fabs(v);
        }
    for (int64_t t = 0; t < members; t++) {
        a[m->cone_col[t]] += m->sol_cone[t];
        traf[m->cone_col[t]] += fabs(m->sol_cone[t]);
    }
    const double tiny = CONIC_RAY_ZERO * big;
    for (int64_t j = 0; j < n; j++) {
        if (!(fabs(a[j]) > tol * traf[j]) || traf[j] > tiny ||
            isfinite(a[j] > 0.0 ? m->col_upper[j] : m->col_lower[j]))
            continue;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++)
            if (fabs(m->sol_farkas[m->a_index[p]]) <= tiny)
                m->sol_farkas[m->a_index[p]] = 0.0;
        for (int64_t k = 0; k < nk; k++) {
            const int64_t b = m->cone_start[k], e = m->cone_start[k + 1];
            const int64_t heads = m->cone_type[k] == JAOS_CONE_ROTATED ? 2 : 1;
            for (int64_t t = b; t < e; t++) {
                if (m->cone_col[t] != j)
                    continue;
                bool small = true;
                for (int64_t u = b; u < e && t < b + heads; u++)
                    small = small && fabs(m->sol_cone[u]) <= tiny;
                if (t >= b + heads)
                    m->sol_cone[t] = 0.0;
                else if (small)
                    for (int64_t u = b; u < e; u++)
                        m->sol_cone[u] = 0.0;
            }
        }
    }
    free(a); free(traf);
    return JAOS_OK;
}

static jaos_status cm_without_cones(jaos_model *m, const uint8_t *drop,
                                    const uint8_t *flag, int64_t work0,
                                    int64_t iters0)
{
    const int64_t n = m->num_col, nr = m->num_row;
    const int64_t nk = m->num_cone, members = m->cone_start[nk];
    const double started = jm_monotonic_seconds();
    jaos_model *red = nullptr;
    int64_t *all = jm_alloc_array(nk, sizeof *all);
    jaos_status st = jaos_model_copy(m, &red);
    if (st != JAOS_OK || all == nullptr) {
        free(all);
        jaos_model_free(red);
        return st == JAOS_OK ? JAOS_ERR_OUT_OF_MEMORY : st;
    }
    for (int64_t k = 0; k < nk; k++)
        all[k] = k;
    st = jaos_delete_cones(red, nk, all);
    free(all);
    for (int64_t j = 0; j < n && st == JAOS_OK; j++) {
        if (flag[j] & CM_ZERO)
            st = jaos_set_col_bounds(red, j, 0.0, 0.0);
        else if (flag[j] & CM_FREE) {
            const double v = isfinite(m->col_lower[j]) ? m->col_lower[j] : 0.0;
            st = jaos_set_col_bounds(red, j, v, v);
        }
    }
    if (st == JAOS_OK && m->cfg.work_limit > 0) {
        const int64_t left = m->cfg.work_limit - work0;
        red->cfg.work_limit = left > 0 ? left : 1;
    }
    if (st == JAOS_OK) {
        jm_log(m, JAOS_LOG_SUMMARY, "conic: every cone is left out and no row "
               "is quadratic, so the model goes to the algorithm it would "
               "have taken without them");
        st = jaos_solve(red);
    }
    if (st != JAOS_OK) {
        jm_set_err(m, "%s", jaos_model_error(red));
        jaos_model_free(red);
        return st;
    }
    st = jm_model_ensure_solution_arrays(m);
    if (st == JAOS_OK && members > 0 && m->sol_cone == nullptr &&
        (m->sol_cone = jm_calloc_array(members, sizeof *m->sol_cone)) ==
            nullptr)
        st = JAOS_ERR_OUT_OF_MEMORY;
    if (st != JAOS_OK) {
        jaos_model_free(red);
        return st;
    }
    m->solve_status = jaos_status_of(red);
    m->solve_iters = iters0 + jaos_iterations(red);
    m->solve_barrier_iters = m->solve_iters;
    m->solve_work = work0 + jaos_work_units(red);
    m->solve_time = jm_monotonic_seconds() - started;
    m->sol_basis_ok = false;
    m->conic_rough = false;
    m->farkas_ok = false;
    m->ray_ok = false;
    m->cone_ok = false;
    for (int64_t t = 0; t < members; t++)
        m->sol_cone[t] = 0.0;
    if (n > 0) {
        memcpy(m->sol_col, red->sol_col, (size_t)n * sizeof *m->sol_col);
        memcpy(m->sol_redcost, red->sol_redcost,
               (size_t)n * sizeof *m->sol_redcost);
        memcpy(m->sol_ray, red->sol_ray, (size_t)n * sizeof *m->sol_ray);
    }
    if (nr > 0) {
        memcpy(m->sol_row, red->sol_row, (size_t)nr * sizeof *m->sol_row);
        memcpy(m->sol_dual, red->sol_dual, (size_t)nr * sizeof *m->sol_dual);
        memcpy(m->sol_farkas, red->sol_farkas,
               (size_t)nr * sizeof *m->sol_farkas);
    }
    for (int64_t j = 0; j < n; j++)
        m->sol_col_status[j] = JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < nr; i++)
        m->sol_row_status[i] = JAOS_BASIS_BASIC;
    m->objective = red->objective;
    if (m->solve_status == JAOS_SOLVE_OPTIMAL) {
        const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
        st = cm_dead_duals(m, drop, flag, sigma);
        m->cone_ok = st == JAOS_OK;
        if (st == JAOS_OK) {
            for (int64_t i = 0; i < nr; i++) {
                double act = 0.0, comp = 0.0;
                for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
                    cm_add_product(&act, &comp, m->ar_value[p],
                                   m->sol_col[m->ar_index[p]]);
                const double a = act + comp;
                m->sol_row[i] = a == 0.0 ? 0.0 : a;
            }
            jm_model_publish_objective(m);
        }
    } else if (m->solve_status == JAOS_SOLVE_INFEASIBLE && red->farkas_ok) {
        st = cm_dead_certificate(m, drop, flag);
        m->farkas_ok = st == JAOS_OK;
        m->cone_ok = m->farkas_ok;
    } else if (m->solve_status == JAOS_SOLVE_UNBOUNDED && red->ray_ok) {
        for (int64_t j = 0; j < n; j++)
            if (flag[j] & CM_ZERO)
                m->sol_ray[j] = 0.0;
        cm_idle_heads(m, drop, m->sol_ray, true);
        jaos_ray_report rr;
        m->ray_ok = jaos_check_ray(m, m->sol_ray, jm_primal_tolerance(m),
                                   &rr) == JAOS_OK && rr.certified;
    }
    if (m->solve_status != JAOS_SOLVE_OPTIMAL && m->err[0] == '\0' &&
        jaos_model_error(red)[0] != '\0')
        jm_set_err(m, "%s", jaos_model_error(red));
    jaos_model_free(red);
    return st;
}

static jaos_status conic_solve(jaos_model *m, int64_t work0, int64_t iters0)
{
    const int64_t n = m->num_col, nr = m->num_row;
    const double sigma = m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    constexpr double RT = 0.70710678118654752440;
    jaos_status st = JAOS_OK;
    cm_rows zr = {0}, lr = {0}, sr = {0};
    cm_qc *qc = jm_calloc_array(nr > 0 ? nr : 1, sizeof *qc);
    int64_t *soc_dim = nullptr, nsoc = 0;
    int64_t *slot_lo = jm_alloc_array(nr + n > 0 ? nr + n : 1, sizeof *slot_lo);
    int64_t *slot_up = jm_alloc_array(nr + n > 0 ? nr + n : 1, sizeof *slot_up);
    int64_t *slot_eq = jm_alloc_array(nr + n > 0 ? nr + n : 1, sizeof *slot_eq);
    int64_t *qc_at = jm_alloc_array(nr > 0 ? nr : 1, sizeof *qc_at);
    int64_t *cone_at = jm_alloc_array(m->num_cone > 0 ? m->num_cone : 1,
                                      sizeof *cone_at);
    int64_t *ps = nullptr, *pi = nullptr, *as = nullptr, *ai = nullptr;
    double *pv = nullptr, *av = nullptr, *q = nullptr, *b = nullptr;
    double *x = nullptr, *s = nullptr, *z = nullptr;
    uint8_t *dead = jm_calloc_array(m->num_cone > 0 ? m->num_cone : 1,
                                    sizeof *dead);
    uint8_t *flag = jm_calloc_array(n > 0 ? n : 1, sizeof *flag);
    jm_work work = {work0};
    const double started = jm_monotonic_seconds();
    if (qc == nullptr || slot_lo == nullptr || slot_up == nullptr ||
        slot_eq == nullptr || qc_at == nullptr || cone_at == nullptr ||
        dead == nullptr || flag == nullptr) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    st = jm_model_ensure_rowwise(m);
    if (st != JAOS_OK)
        goto done;
    const int64_t ndead = cm_drop_cones(m, dead, flag);
    if (ndead == m->num_cone && m->num_cone > 0 && m->rq_nz == 0) {
        st = cm_without_cones(m, dead, flag, work0, iters0);
        goto done;
    }
    if (ndead > 0) {
        int64_t idle = 0;
        for (int64_t k = 0; k < m->num_cone; k++)
            idle += dead[k] == CM_IDLE;
        jm_log(m, JAOS_LOG_SUMMARY, "conic: the walk leaves out %lld cones "
               "whose head an upper bound of 0 holds at 0, their columns "
               "fixed at 0, and %lld whose head is free above, costs "
               "nothing and appears nowhere else, set after the walk to the "
               "norm of the rest", (long long)(ndead - idle),
               (long long)idle);
    }
    for (int64_t t = 0; t < nr + n; t++)
        slot_lo[t] = slot_up[t] = slot_eq[t] = -1;
    for (int64_t i = 0; i < nr; i++) {
        qc_at[i] = -1;
        if (jm_row_quadratic_count(m, i) == 0)
            continue;
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        if (isfinite(lo) && isfinite(hi)) {
            jm_set_err(m, "row %lld has a quadratic part and two finite "
                          "bounds, which is not a convex set; JAOS takes a "
                          "quadratic row with one side", (long long)i);
            st = JAOS_ERR_INVALID_INPUT;
            goto done;
        }
        if (!isfinite(lo) && !isfinite(hi))
            continue;
        qc[i].sgn = isfinite(hi) ? 1 : -1;
        qc[i].u = isfinite(hi) ? hi : -lo;
        st = jm_row_quadratic_factor(m, i, qc[i].sgn, &qc[i].k, &qc[i].r,
                                     &qc[i].cols, &qc[i].f);
        if (st == JAOS_ERR_INVALID_INPUT && m->err[0] == '\0')
            jm_set_err(m, "row %lld has a quadratic part that is not convex "
                          "on its %s side", (long long)i,
                       qc[i].sgn > 0 ? "upper" : "lower");
        if (st != JAOS_OK)
            goto done;
    }

    for (int64_t i = 0; i < nr && st == JAOS_OK; i++) {
        if (jm_row_quadratic_count(m, i) > 0)
            continue;
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        if (isfinite(lo) && lo == hi) {
            slot_eq[i] = zr.nb;
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
                if (!cm_put(&zr, m->ar_index[p], m->ar_value[p]))
                    st = JAOS_ERR_OUT_OF_MEMORY;
            if (!cm_end(&zr, hi))
                st = JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }
        if (isfinite(hi)) {
            slot_up[i] = lr.nb;
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
                if (!cm_put(&lr, m->ar_index[p], m->ar_value[p]))
                    st = JAOS_ERR_OUT_OF_MEMORY;
            if (!cm_end(&lr, hi))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        if (isfinite(lo)) {
            slot_lo[i] = lr.nb;
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
                if (!cm_put(&lr, m->ar_index[p], -m->ar_value[p]))
                    st = JAOS_ERR_OUT_OF_MEMORY;
            if (!cm_end(&lr, -lo))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
    }
    for (int64_t j = 0; j < n && st == JAOS_OK; j++) {
        double lo = m->col_lower[j], hi = m->col_upper[j];
        if (flag[j] & CM_ZERO)
            lo = hi = 0.0;
        else if (flag[j] & CM_FREE)
            lo = hi = isfinite(lo) ? lo : 0.0;
        if (isfinite(lo) && lo == hi) {
            slot_eq[nr + j] = zr.nb;
            if (!cm_put(&zr, j, 1.0) || !cm_end(&zr, lo))
                st = JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }
        if (isfinite(hi)) {
            slot_up[nr + j] = lr.nb;
            if (!cm_put(&lr, j, 1.0) || !cm_end(&lr, hi))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        if (isfinite(lo)) {
            slot_lo[nr + j] = lr.nb;
            if (!cm_put(&lr, j, -1.0) || !cm_end(&lr, -lo))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
    }
    if (st != JAOS_OK)
        goto done;

    {
        int64_t count = m->num_cone;
        for (int64_t i = 0; i < nr; i++)
            count += qc[i].sgn != 0;
        soc_dim = jm_alloc_array(count > 0 ? count : 1, sizeof *soc_dim);
        if (soc_dim == nullptr) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
    }
    for (int64_t k = 0; k < m->num_cone && st == JAOS_OK; k++) {
        const int64_t bb = m->cone_start[k], ee = m->cone_start[k + 1];
        cone_at[k] = -1;
        if (dead[k])
            continue;
        cone_at[k] = sr.nb;
        if (m->cone_type[k] == JAOS_CONE_ROTATED) {
            const int64_t c0 = m->cone_col[bb], c1 = m->cone_col[bb + 1];
            if (!cm_put(&sr, c0, -RT) || !cm_put(&sr, c1, -RT) ||
                !cm_end(&sr, 0.0) ||
                !cm_put(&sr, c0, -RT) || !cm_put(&sr, c1, RT) ||
                !cm_end(&sr, 0.0))
                st = JAOS_ERR_OUT_OF_MEMORY;
            for (int64_t t = bb + 2; t < ee && st == JAOS_OK; t++)
                if (!cm_put(&sr, m->cone_col[t], -1.0) || !cm_end(&sr, 0.0))
                    st = JAOS_ERR_OUT_OF_MEMORY;
        } else {
            for (int64_t t = bb; t < ee && st == JAOS_OK; t++)
                if (!cm_put(&sr, m->cone_col[t], -1.0) || !cm_end(&sr, 0.0))
                    st = JAOS_ERR_OUT_OF_MEMORY;
        }
        soc_dim[nsoc++] = ee - bb;
    }
    for (int64_t i = 0; i < nr && st == JAOS_OK; i++) {
        if (qc[i].sgn == 0)
            continue;
        const double sg = qc[i].sgn, u = qc[i].u;
        qc_at[i] = sr.nb;
        for (int half = 0; half < 2 && st == JAOS_OK; half++) {
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
                if (!cm_put(&sr, m->ar_index[p], sg * m->ar_value[p] * RT))
                    st = JAOS_ERR_OUT_OF_MEMORY;
            if (!cm_end(&sr, (half == 0 ? u + 1.0 : u - 1.0) * RT))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        for (int64_t t = 0; t < qc[i].r && st == JAOS_OK; t++) {
            for (int64_t c = 0; c < qc[i].k; c++) {
                const double f = qc[i].f[t * qc[i].k + c];
                if (f != 0.0 && !cm_put(&sr, qc[i].cols[c], -f))
                    st = JAOS_ERR_OUT_OF_MEMORY;
            }
            if (!cm_end(&sr, 0.0))
                st = JAOS_ERR_OUT_OF_MEMORY;
        }
        soc_dim[nsoc++] = qc[i].r + 2;
    }
    if (st != JAOS_OK)
        goto done;

    const int64_t mz = zr.nb, ml = lr.nb, msoc = sr.nb, mt = mz + ml + msoc;
    const int64_t nz = zr.n + lr.n + sr.n;
    {
        cm_trip *all = jm_alloc_array(nz > 0 ? nz : 1, sizeof *all);
        as = jm_calloc_array(n + 1, sizeof *as);
        ai = jm_alloc_array(nz > 0 ? nz : 1, sizeof *ai);
        av = jm_alloc_array(nz > 0 ? nz : 1, sizeof *av);
        b = jm_alloc_array(mt > 0 ? mt : 1, sizeof *b);
        if (all == nullptr || as == nullptr || ai == nullptr || av == nullptr ||
            b == nullptr) {
            free(all);
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        int64_t at = 0;
        for (int64_t t = 0; t < zr.n; t++)
            all[at++] = zr.t[t];
        for (int64_t t = 0; t < lr.n; t++) {
            all[at] = lr.t[t];
            all[at++].row += mz;
        }
        for (int64_t t = 0; t < sr.n; t++) {
            all[at] = sr.t[t];
            all[at++].row += mz + ml;
        }
        qsort(all, (size_t)nz, sizeof *all, cmp_trip);
        for (int64_t t = 0; t < nz; t++) {
            as[all[t].col + 1]++;
            ai[t] = all[t].row;
            av[t] = all[t].v;
        }
        for (int64_t j = 0; j < n; j++)
            as[j + 1] += as[j];
        free(all);
        for (int64_t t = 0; t < mz; t++)
            b[t] = zr.b[t];
        for (int64_t t = 0; t < ml; t++)
            b[mz + t] = lr.b[t];
        for (int64_t t = 0; t < msoc; t++)
            b[mz + ml + t] = sr.b[t];
    }
    {
        int64_t pn = 0;
        for (int64_t j = 0; j < n; j++) {
            pn += m->col_quad != nullptr && m->col_quad[j] != 0.0;
            if (m->q_start != nullptr)
                pn += m->q_start[j + 1] - m->q_start[j];
        }
        cm_trip *pt = jm_alloc_array(pn > 0 ? pn : 1, sizeof *pt);
        ps = jm_calloc_array(n + 1, sizeof *ps);
        pi = jm_alloc_array(pn > 0 ? pn : 1, sizeof *pi);
        pv = jm_alloc_array(pn > 0 ? pn : 1, sizeof *pv);
        q = jm_alloc_array(n > 0 ? n : 1, sizeof *q);
        if (pt == nullptr || ps == nullptr || pi == nullptr || pv == nullptr ||
            q == nullptr) {
            free(pt);
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        int64_t at = 0;
        for (int64_t j = 0; j < n; j++) {
            q[j] = sigma * m->col_cost[j];
            if (m->col_quad != nullptr && m->col_quad[j] != 0.0)
                pt[at++] = (cm_trip){j, j, sigma * m->col_quad[j]};
            if (m->q_start != nullptr)
                for (int64_t p = m->q_start[j]; p < m->q_start[j + 1]; p++) {
                    const int64_t i = m->q_index[p];
                    pt[at++] = (cm_trip){i > j ? i : j, i > j ? j : i,
                                         sigma * m->q_value[p]};
                }
        }
        qsort(pt, (size_t)pn, sizeof *pt, cmp_trip);
        for (int64_t t = 0; t < pn; t++) {
            ps[pt[t].col + 1]++;
            pi[t] = pt[t].row;
            pv[t] = pt[t].v;
        }
        for (int64_t j = 0; j < n; j++)
            ps[j + 1] += ps[j];
        free(pt);
    }

    x = jm_calloc_array(n > 0 ? n : 1, sizeof *x);
    s = jm_calloc_array(mt > 0 ? mt : 1, sizeof *s);
    z = jm_calloc_array(mt > 0 ? mt : 1, sizeof *z);
    if (x == nullptr || s == nullptr || z == nullptr) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        goto done;
    }
    const jm_cone_problem pb = {
        .n = n, .m = mt, .p_start = ps, .p_index = pi, .p_value = pv, .q = q,
        .a_start = as, .a_index = ai, .a_value = av, .b = b, .nzero = mz,
        .nnonneg = ml, .nsoc = nsoc, .soc_dim = soc_dim};
    jm_cone_result res = {.x = x, .s = s, .z = z};
    jm_log(m, JAOS_LOG_SUMMARY,
           "conic interior point: %lld columns, %lld rows in %lld equalities, "
           "%lld inequalities and %lld second-order cones",
           (long long)n, (long long)mt, (long long)mz, (long long)ml,
           (long long)nsoc);
    st = jm_cone_solve(&pb, m, &work, &res);
    if (st != JAOS_OK)
        goto done;
    const bool stalled = res.status == JAOS_SOLVE_NUMERICAL_ERROR;
    if (res.status == JAOS_SOLVE_UNBOUNDED || stalled) {
        double *q0 = jm_calloc_array(n > 0 ? n : 1, sizeof *q0);
        int64_t *p0 = jm_calloc_array(n + 1, sizeof *p0);
        double *fx = jm_calloc_array(n > 0 ? n : 1, sizeof *fx);
        double *fs = jm_calloc_array(mt > 0 ? mt : 1, sizeof *fs);
        double *fz = jm_calloc_array(mt > 0 ? mt : 1, sizeof *fz);
        if (q0 != nullptr && p0 != nullptr && fx != nullptr && fs != nullptr &&
            fz != nullptr) {
            jm_cone_problem fp = pb;
            fp.q = q0;
            fp.p_start = p0;
            jm_cone_result fr = {.x = fx, .s = fs, .z = fz};
            jm_log(m, JAOS_LOG_SUMMARY, "conic: %s; looking for a feasible "
                   "point first", stalled ? "the walk ends with no answer"
                                          : "an improving ray");
            st = jm_cone_solve(&fp, m, &work, &fr);
            res.iters += fr.iters;
            if (st == JAOS_OK && fr.status == JAOS_SOLVE_INFEASIBLE) {
                memcpy(x, fx, (size_t)(n > 0 ? n : 1) * sizeof *x);
                memcpy(s, fs, (size_t)(mt > 0 ? mt : 1) * sizeof *s);
                memcpy(z, fz, (size_t)(mt > 0 ? mt : 1) * sizeof *z);
                res.status = JAOS_SOLVE_INFEASIBLE;
                res.relaxed = fr.relaxed;
            } else if (st == JAOS_OK && fr.status != JAOS_SOLVE_OPTIMAL) {
                res.status = fr.status == JAOS_SOLVE_WORK_LIMIT ||
                             fr.status == JAOS_SOLVE_TIME_LIMIT
                                 ? fr.status : JAOS_SOLVE_NUMERICAL_ERROR;
            } else if (stalled) {
                res.status = JAOS_SOLVE_UNBOUNDED;
                for (int64_t j = 0; j < n; j++)
                    x[j] = 0.0;
            }
        } else {
            st = JAOS_ERR_OUT_OF_MEMORY;
        }
        free(q0); free(p0); free(fx); free(fs); free(fz);
        if (st != JAOS_OK)
            goto done;
    }

    st = jm_model_ensure_solution_arrays(m);
    if (st != JAOS_OK)
        goto done;
    if (m->num_cone > 0 && m->sol_cone == nullptr) {
        m->sol_cone = jm_calloc_array(m->cone_start[m->num_cone],
                                      sizeof *m->sol_cone);
        if (m->sol_cone == nullptr) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
    }
    m->solve_status = res.status;
    m->solve_iters = iters0 + res.iters;
    m->solve_barrier_iters = iters0 + res.iters;
    m->solve_work = work.units;
    m->solve_time = jm_monotonic_seconds() - started;
    m->farkas_ok = false;
    m->ray_ok = false;
    m->cone_ok = false;
    m->conic_rough = false;
    m->sol_basis_ok = false;
    for (int64_t j = 0; j < n; j++) {
        m->sol_col[j] = 0.0;
        m->sol_redcost[j] = 0.0;
        m->sol_col_status[j] = JAOS_BASIS_BASIC;
        m->sol_ray[j] = 0.0;
    }
    for (int64_t i = 0; i < nr; i++) {
        m->sol_row[i] = 0.0;
        m->sol_dual[i] = 0.0;
        m->sol_row_status[i] = JAOS_BASIS_BASIC;
        m->sol_farkas[i] = 0.0;
    }
    for (int64_t t = 0; m->num_cone > 0 && t < m->cone_start[m->num_cone]; t++)
        m->sol_cone[t] = 0.0;
    m->objective = 0.0;

    const double *zz = z;
    const double *zl = zz + mz;
    const double *zs = zz + mz + ml;
    const double yscale = res.status == JAOS_SOLVE_OPTIMAL ? sigma : 1.0;
    if (res.status == JAOS_SOLVE_OPTIMAL || res.status == JAOS_SOLVE_INFEASIBLE) {
        for (int64_t i = 0; i < nr; i++) {
            double y = 0.0;
            if (slot_eq[i] >= 0)
                y -= zz[slot_eq[i]];
            if (slot_lo[i] >= 0)
                y += zl[slot_lo[i]];
            if (slot_up[i] >= 0)
                y -= zl[slot_up[i]];
            if (qc_at[i] >= 0) {
                const double lam = (zs[qc_at[i]] + zs[qc_at[i] + 1]) * RT;
                y = -qc[i].sgn * lam;
            }
            if (res.status == JAOS_SOLVE_OPTIMAL)
                m->sol_dual[i] = yscale * y == 0.0 ? 0.0 : yscale * y;
            else
                m->sol_farkas[i] = y == 0.0 ? 0.0 : y;
        }
        for (int64_t k = 0; k < m->num_cone; k++) {
            if (dead[k])
                continue;
            const int64_t bb = m->cone_start[k], ee = m->cone_start[k + 1];
            const double *zk = zs + cone_at[k];
            double *out = m->sol_cone + bb;
            if (m->cone_type[k] == JAOS_CONE_ROTATED) {
                out[0] = yscale * (zk[0] + zk[1]) * RT;
                out[1] = yscale * (zk[0] - zk[1]) * RT;
                for (int64_t t = 2; t < ee - bb; t++)
                    out[t] = yscale * zk[t];
            } else {
                for (int64_t t = 0; t < ee - bb; t++)
                    out[t] = yscale * zk[t];
            }
        }
        m->cone_ok = m->num_cone > 0;
    }
    if (res.status == JAOS_SOLVE_OPTIMAL) {
        for (int64_t j = 0; j < n; j++) {
            m->sol_col[j] = x[j] == 0.0 ? 0.0 : x[j];
            double d = 0.0;
            if (slot_eq[nr + j] >= 0)
                d -= zz[slot_eq[nr + j]];
            if (slot_lo[nr + j] >= 0)
                d += zl[slot_lo[nr + j]];
            if (slot_up[nr + j] >= 0)
                d -= zl[slot_up[nr + j]];
            m->sol_redcost[j] = sigma * d == 0.0 ? 0.0 : sigma * d;
        }
        if (ndead > 0) {
            st = cm_dead_duals(m, dead, flag, sigma);
            if (st != JAOS_OK)
                goto done;
        }
        for (int64_t i = 0; i < nr; i++) {
            double act = 0.0, comp = 0.0;
            for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
                jm_obj_add(&act, &comp, m->ar_value[p] * m->sol_col[m->ar_index[p]]);
            for (int64_t p = m->rq_start != nullptr ? m->rq_start[i] : 0;
                 m->rq_start != nullptr && p < m->rq_start[i + 1]; p++) {
                const double v = m->rq_v[p] * m->sol_col[m->rq_i[p]] *
                                 m->sol_col[m->rq_j[p]];
                jm_obj_add(&act, &comp, m->rq_i[p] == m->rq_j[p] ? 0.5 * v : v);
            }
            const double a = act + comp;
            m->sol_row[i] = a == 0.0 ? 0.0 : a;
        }
        bool *loose = jm_calloc_array(n > 0 ? n : 1, sizeof *loose);
        if (loose == nullptr) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            goto done;
        }
        for (int64_t r = 0; r < CONIC_NEWTON_ROUNDS; r++) {
            bool moved = false;
            st = newton_polish(m, &work, r > 0 ? loose : nullptr, &moved);
            if (st != JAOS_OK) {
                free(loose);
                goto done;
            }
            int64_t wrong = 0;
            for (int64_t j = 0; moved && j < n; j++) {
                const double w = m->sol_redcost[j];
                if (loose[j] || w == 0.0 ||
                    m->col_lower[j] == m->col_upper[j])
                    continue;
                if ((w > 0.0 && m->sol_col[j] == m->col_upper[j]) ||
                    (w < 0.0 && m->sol_col[j] == m->col_lower[j])) {
                    loose[j] = true;
                    wrong++;
                }
            }
            if (wrong == 0)
                break;
            jm_log(m, JAOS_LOG_DETAIL, "conic: %lld columns leave the Newton "
                   "finish's active set with the wrong sign", (long long)wrong);
        }
        free(loose);
        m->solve_work = work.units;
        jaos_check_report ck;
        if (res.relaxed &&
            (jaos_check_conic_solution(m, m->sol_col, m->sol_dual,
                                       m->num_cone > 0 ? m->sol_cone
                                                       : nullptr,
                                       jm_primal_tolerance(m), &ck) !=
                 JAOS_OK || !ck.primal_feasible || !ck.dual_feasible)) {
            if (m->cfg.node_solve) {
                m->conic_rough = true;
                jm_model_publish_objective(m);
            } else {
                m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
                m->cone_ok = false;
                jm_set_err(m, "the conic interior point stopped near an "
                              "optimum that the checker does not pass "
                              "(columns off by %.3g, rows by %.3g, %.3g of "
                              "their traffic, cones by %.3g, duals by %.3g)",
                           ck.max_col_violation, ck.max_row_violation,
                           ck.max_row_violation_relative,
                           ck.max_cone_violation, ck.max_dual_violation);
            }
        } else {
            jm_model_publish_objective(m);
        }
    } else if (res.status == JAOS_SOLVE_INFEASIBLE) {
        jaos_certificate_report cr;
        int64_t calls = 0;
        const int64_t members = m->num_cone > 0 ? m->cone_start[m->num_cone]
                                                : 0;
        if (ndead > 0) {
            st = cm_dead_certificate(m, dead, flag);
            if (st != JAOS_OK)
                goto done;
        }
        bool certified = jaos_check_conic_certificate(
                             m, m->sol_farkas,
                             m->num_cone > 0 ? m->sol_cone : nullptr,
                             jm_primal_tolerance(m), &cr) == JAOS_OK &&
                         cr.certified;
        if (!certified) {
            double big = 0.0;
            for (int64_t i = 0; i < nr; i++)
                if (fabs(m->sol_farkas[i]) > big)
                    big = fabs(m->sol_farkas[i]);
            for (int64_t t = 0; t < members; t++)
                if (fabs(m->sol_cone[t]) > big)
                    big = fabs(m->sol_cone[t]);
            double *keep = jm_alloc_array(nr + members > 0 ? nr + members : 1,
                                          sizeof *keep);
            if (keep == nullptr) {
                st = JAOS_ERR_OUT_OF_MEMORY;
                goto done;
            }
            if (nr > 0)
                memcpy(keep, m->sol_farkas, (size_t)nr * sizeof *keep);
            if (members > 0)
                memcpy(keep + nr, m->sol_cone, (size_t)members * sizeof *keep);
            for (int round = 0; round < 2 && !certified; round++) {
                if (round == 0) {
                    st = cm_cert_trim(m, big, jm_primal_tolerance(m));
                    if (st != JAOS_OK) {
                        free(keep);
                        goto done;
                    }
                } else {
                    if (nr > 0)
                        memcpy(m->sol_farkas, keep, (size_t)nr * sizeof *keep);
                    if (members > 0)
                        memcpy(m->sol_cone, keep + nr,
                               (size_t)members * sizeof *keep);
                    for (int64_t i = 0; i < nr; i++)
                        if (fabs(m->sol_farkas[i]) <= CONIC_RAY_ZERO * big)
                            m->sol_farkas[i] = 0.0;
                    for (int64_t t = 0; t < members; t++)
                        if (fabs(m->sol_cone[t]) <= CONIC_RAY_ZERO * big)
                            m->sol_cone[t] = 0.0;
                }
                if (ndead > 0) {
                    st = cm_dead_certificate(m, dead, flag);
                    if (st != JAOS_OK) {
                        free(keep);
                        goto done;
                    }
                }
                certified = jaos_check_conic_certificate(
                                m, m->sol_farkas,
                                m->num_cone > 0 ? m->sol_cone : nullptr,
                                jm_primal_tolerance(m), &cr) == JAOS_OK &&
                            cr.certified;
                if (!certified && !m->cfg.node_solve)
                    certified = cm_cert_search(m, jm_primal_tolerance(m),
                                               members, &cr, &calls);
            }
            free(keep);
        }
        m->solve_work += calls * (m->num_nz + m->num_col + m->num_row + 1);
        if (certified) {
            m->farkas_ok = true;
        } else if (res.relaxed) {
            m->cone_ok = false;
            m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
            jm_set_err(m, "the conic interior point stalled next to an "
                          "infeasibility certificate that the certificate "
                          "checker does not confirm");
        } else if (m->rq_nz == 0) {
            m->cone_ok = false;
            m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
            jm_set_err(m, "the conic interior point ended at an "
                          "infeasibility certificate that the certificate "
                          "checker does not confirm (columns reach %.3g, "
                          "rows need %.3g), and with no quadratic row the "
                          "checker's test is exact", cr.sup_columns,
                       cr.inf_rows);
        } else {
            m->cone_ok = false;
            jm_log(m, JAOS_LOG_SUMMARY, "conic: the certificate does not "
                   "pass the certificate checker (columns reach %.3g, rows "
                   "need %.3g); none is published", cr.sup_columns,
                   cr.inf_rows);
        }
    } else if (res.status == JAOS_SOLVE_UNBOUNDED) {
        double big = 0.0;
        for (int64_t j = 0; j < n; j++)
            if (fabs(x[j]) > big)
                big = fabs(x[j]);
        for (int64_t j = 0; j < n; j++)
            m->sol_ray[j] = fabs(x[j]) <= CONIC_RAY_ZERO * big ||
                                    (flag[j] & CM_ZERO) ? 0.0 : x[j];
        if (ndead > 0)
            cm_idle_heads(m, dead, m->sol_ray, true);
        jaos_ray_report rr;
        m->ray_ok = jaos_check_ray(m, m->sol_ray, jm_primal_tolerance(m),
                                   &rr) == JAOS_OK && rr.certified;
        if (!m->ray_ok) {
            st = ray_polish(m, qc, m->sol_ray, &work);
            if (st != JAOS_OK)
                goto done;
            big = 0.0;
            for (int64_t j = 0; j < n; j++)
                if (fabs(m->sol_ray[j]) > big)
                    big = fabs(m->sol_ray[j]);
            for (int64_t j = 0; j < n; j++)
                if (fabs(m->sol_ray[j]) <= CONIC_RAY_ZERO * big ||
                    (flag[j] & CM_ZERO))
                    m->sol_ray[j] = 0.0;
            if (ndead > 0)
                cm_idle_heads(m, dead, m->sol_ray, true);
            m->ray_ok = jaos_check_ray(m, m->sol_ray, jm_primal_tolerance(m),
                                       &rr) == JAOS_OK && rr.certified;
            m->solve_work = work.units;
        }
        if (!m->ray_ok) {
            bool probed = false;
            st = cm_ray_probe(m, &work, &probed);
            if (st != JAOS_OK)
                goto done;
            m->solve_work = work.units;
            if (probed)
                jm_log(m, JAOS_LOG_SUMMARY, "conic: the walk's direction "
                       "does not pass the ray checker; a solve over the "
                       "directions the rows, the bounds and the cones leave "
                       "open found one that does");
        }
        if (!m->ray_ok) {
            m->solve_status = JAOS_SOLVE_NUMERICAL_ERROR;
            if (stalled)
                jm_set_err(m, "the conic interior point stopped after %lld "
                              "iterations without an answer, and the model's "
                              "rows, bounds and cones leave open no improving "
                              "direction the ray checker takes",
                           (long long)res.iters);
            else
                jm_set_err(m, "the conic interior point found an improving "
                              "direction that the ray checker does not "
                              "confirm: rate %.3g, push past a column bound "
                              "%.3g, past a row side %.3g", rr.rate,
                           rr.max_col_escape, rr.max_row_escape);
        }
    } else {
        jm_set_err(m, "the conic interior point stopped after %lld "
                      "iterations without an answer", (long long)res.iters);
    }
    jm_log(m, JAOS_LOG_SUMMARY, "%s after %lld conic iterations, %lld work "
           "units", jaos_solve_status_str(m->solve_status),
           (long long)res.iters, (long long)work.units);

done:
    for (int64_t i = 0; qc != nullptr && i < nr; i++)
        cm_qc_free(&qc[i]);
    free(qc);
    cm_free_rows(&zr);
    cm_free_rows(&lr);
    cm_free_rows(&sr);
    free(soc_dim); free(slot_lo); free(slot_up); free(slot_eq); free(qc_at);
    free(cone_at); free(dead); free(flag);
    free(ps); free(pi); free(pv); free(as); free(ai); free(av);
    free(q); free(b); free(x); free(s); free(z);
    return st;
}
