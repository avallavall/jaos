#include "jaos_internal.h"

#include <assert.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct jm_tableau {
    jaos_model *m;
    int64_t nrow, ncol, nvar;
    double sigma;
    const double *rho;
    const double *gam;
    int64_t *basis;
    int64_t *pos;
    jm_lu lu;
    double *xb;
    double *y;
    double *d;
    double *vec;
    int64_t *pat;
    uint64_t *mark;
    double *alpha;
    int64_t *touched;
    unsigned char *seen;
    jm_work w;
};
typedef struct jm_tableau rg;

static void rg_free(rg *g)
{
    jm_lu_free(&g->lu);
    free(g->basis);
    free(g->pos);
    free(g->xb);
    free(g->y);
    free(g->d);
    free(g->vec);
    free(g->pat);
    free(g->mark);
    free(g->alpha);
    free(g->touched);
    free(g->seen);
}

static double rg_lower(const rg *g, int64_t v)
{
    return v < g->ncol ? g->m->col_lower[v] : g->m->row_lower[v - g->ncol];
}

static double rg_upper(const rg *g, int64_t v)
{
    return v < g->ncol ? g->m->col_upper[v] : g->m->row_upper[v - g->ncol];
}

static jaos_basis_status rg_status(const rg *g, int64_t v)
{
    return v < g->ncol ? g->m->sol_col_status[v]
                       : g->m->sol_row_status[v - g->ncol];
}

static double rg_vscale(const rg *g, int64_t v)
{
    return v < g->ncol ? g->gam[v] : 1.0 / g->rho[v - g->ncol];
}

static bool rg_rest(const rg *g, int64_t v, double *out)
{
    switch (rg_status(g, v)) {
    case JAOS_BASIS_AT_LOWER: *out = rg_lower(g, v); break;
    case JAOS_BASIS_AT_UPPER: *out = rg_upper(g, v); break;
    case JAOS_BASIS_FREE:     *out = 0.0; break;
    default:                  return false;
    }
    return isfinite(*out);
}

static void rg_scatter(const rg *g, int64_t v, double *vec)
{
    const jaos_model *m = g->m;
    if (v < g->ncol) {
        for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
            vec[m->a_index[k]] = m->a_value[k];
    } else {
        vec[v - g->ncol] = -1.0;
    }
}

static bool rg_has_optimum(const jaos_model *m)
{
    return m->solve_status == JAOS_SOLVE_OPTIMAL &&
           m->sol_col_status != nullptr && m->sol_row_status != nullptr;
}

static jaos_status rg_build(jaos_model *m, rg *g)
{
    memset(g, 0, sizeof *g);
    jm_lu_init(&g->lu);
    g->m = m;
    g->nrow = m->num_row;
    g->ncol = m->num_col;
    g->nvar = m->num_col + m->num_row;
    g->sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    const int64_t nrow = g->nrow, ncol = g->ncol, nvar = g->nvar;

    jaos_status st = jm_model_ensure_rowwise(m);
    if (st != JAOS_OK)
        return st;

    if (!m->scale_valid) {
        st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }
    g->rho = m->row_scale;
    g->gam = m->col_scale;

    int64_t nb = 0;
    for (int64_t j = 0; j < ncol; j++)
        nb += m->sol_col_status[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < nrow; i++)
        nb += m->sol_row_status[i] == JAOS_BASIS_BASIC;
    if (nb != nrow) {
        jm_set_err(m, "the basis behind the answer has %lld basic variables "
                      "and a basis of this model has %lld",
                   (long long)nb, (long long)nrow);
        return JAOS_ERR_NUMERICAL;
    }

    g->basis   = jm_alloc_array(nrow, sizeof *g->basis);
    g->pos     = jm_alloc_array(nvar, sizeof *g->pos);
    g->xb      = jm_calloc_array(nrow, sizeof *g->xb);
    g->y       = jm_calloc_array(nrow, sizeof *g->y);
    g->d       = jm_calloc_array(nvar, sizeof *g->d);
    g->vec     = jm_calloc_array(nrow, sizeof *g->vec);
    g->pat     = jm_alloc_array(nrow, sizeof *g->pat);
    g->mark    = jm_calloc_array((nrow + 63) / 64 + 1, sizeof *g->mark);
    g->alpha   = jm_calloc_array(ncol, sizeof *g->alpha);
    g->touched = jm_alloc_array(ncol, sizeof *g->touched);
    g->seen    = jm_calloc_array(ncol, sizeof *g->seen);
    if (!g->basis || !g->pos || !g->xb || !g->y || !g->d || !g->vec ||
        !g->pat || !g->mark || !g->alpha || !g->touched || !g->seen)
        return JAOS_ERR_OUT_OF_MEMORY;

    int64_t p = 0, nnz = 0;
    for (int64_t j = 0; j < ncol; j++) {
        g->pos[j] = -1;
        if (m->sol_col_status[j] == JAOS_BASIS_BASIC) {
            g->basis[p] = j;
            g->pos[j] = p++;
            nnz += m->a_start[j + 1] - m->a_start[j];
        }
    }
    for (int64_t i = 0; i < nrow; i++) {
        const int64_t v = ncol + i;
        g->pos[v] = -1;
        if (m->sol_row_status[i] == JAOS_BASIS_BASIC) {
            g->basis[p] = v;
            g->pos[v] = p++;
            nnz += 1;
        }
    }
    assert(p == nrow);

    const int64_t room = nnz > 0 ? nnz : 1;
    int64_t *bs = jm_alloc_array(nrow + 1, sizeof *bs);
    int64_t *bi = jm_alloc_array(room, sizeof *bi);
    double  *bv = jm_alloc_array(room, sizeof *bv);
    if (!bs || !bi || !bv) {
        free(bs); free(bi); free(bv);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    int64_t q = 0;
    for (p = 0; p < nrow; p++) {
        bs[p] = q;
        const int64_t v = g->basis[p];
        if (v < ncol) {
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                bi[q] = m->a_index[k];
                bv[q] = g->rho[m->a_index[k]] * m->a_value[k] * g->gam[v];
                q++;
            }
        } else {

            bi[q] = v - ncol;
            bv[q] = -1.0;
            q++;
        }
    }
    bs[nrow] = q;
    st = jm_lu_factor(&g->lu, nrow, bs, bi, bv, LU_PIVOT_TOL, &g->w);
    free(bs); free(bi); free(bv);
    if (st != JAOS_OK)
        return st;
    if (g->lu.rank < nrow) {
        jm_set_err(m, "the basis behind the answer is singular on the model "
                      "as loaded: rank %lld of %lld",
                   (long long)g->lu.rank, (long long)nrow);
        return JAOS_ERR_NUMERICAL;
    }

    for (int64_t v = 0; v < nvar; v++) {
        if (g->pos[v] >= 0)
            continue;
        double xv;
        if (!rg_rest(g, v, &xv)) {
            jm_set_err(m, "%s %lld is nonbasic on a bound it does not have",
                       v < ncol ? "column" : "row",
                       (long long)(v < ncol ? v : v - ncol));
            return JAOS_ERR_NUMERICAL;
        }
        if (xv == 0.0)
            continue;
        if (v < ncol) {
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                g->vec[m->a_index[k]] -= m->a_value[k] * xv;
        } else {
            g->vec[v - ncol] += xv;
        }
    }

    for (int64_t i = 0; i < nrow; i++)
        g->vec[i] *= g->rho[i];
    jm_lu_ftran(&g->lu, g->vec, &g->w);
    for (p = 0; p < nrow; p++)
        g->xb[p] = g->vec[p] * rg_vscale(g, g->basis[p]);
    memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);

    for (p = 0; p < nrow; p++) {
        const int64_t v = g->basis[p];
        g->vec[p] = v < ncol ? g->sigma * m->col_cost[v] * g->gam[v] : 0.0;
    }
    jm_lu_btran(&g->lu, g->vec, &g->w);
    for (int64_t i = 0; i < nrow; i++)
        g->y[i] = g->vec[i] * g->rho[i];
    memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);

    for (int64_t j = 0; j < ncol; j++) {
        if (g->pos[j] >= 0)
            continue;
        double t = g->sigma * m->col_cost[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            t -= m->a_value[k] * g->y[m->a_index[k]];
        g->d[j] = t;
    }
    for (int64_t i = 0; i < nrow; i++) {
        const int64_t v = ncol + i;
        g->d[v] = g->pos[v] >= 0 ? 0.0 : g->y[i];
    }
    return JAOS_OK;
}

static void rg_cost_limit(const rg *g, int64_t v, double alpha,
                          double *dmin, double *dmax)
{
    if (alpha == 0.0 || rg_lower(g, v) == rg_upper(g, v))
        return;
    const double ratio = g->d[v] / alpha;
    switch (rg_status(g, v)) {
    case JAOS_BASIS_AT_LOWER:
        if (alpha > 0.0) { if (ratio < *dmax) *dmax = ratio; }
        else             { if (ratio > *dmin) *dmin = ratio; }
        break;
    case JAOS_BASIS_AT_UPPER:
        if (alpha > 0.0) { if (ratio > *dmin) *dmin = ratio; }
        else             { if (ratio < *dmax) *dmax = ratio; }
        break;
    case JAOS_BASIS_FREE:
        if (*dmin < 0.0) *dmin = 0.0;
        if (*dmax > 0.0) *dmax = 0.0;
        break;
    default:
        break;
    }
}

static void rg_publish(double sigma, double L, double U,
                       double *lo, double *hi)
{
    if (sigma > 0.0) {
        if (lo) *lo = L;
        if (hi) *hi = U;
    } else {
        if (lo) *lo = -U;
        if (hi) *hi = -L;
    }
}

jaos_status jaos_cost_ranging(jaos_model *m, double *lower, double *upper)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!rg_has_optimum(m)) {
        jm_set_err(m, "ranging needs the optimum of the last solve, and "
                      "there is none");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (lower == nullptr && upper == nullptr)
        return JAOS_OK;

    rg g;
    jaos_status st = rg_build(m, &g);
    if (st != JAOS_OK) {
        rg_free(&g);
        return st;
    }
    const double sigma = g.sigma;
    const int64_t nrow = g.nrow, ncol = g.ncol;

    for (int64_t j = 0; j < ncol; j++) {
        const double cc = sigma * m->col_cost[j];
        double L, U;
        if (m->col_lower[j] == m->col_upper[j]) {

            L = -INFINITY;
            U = INFINITY;
        } else if (g.pos[j] < 0) {

            const double base = cc - g.d[j];
            const double lo_end = base < cc ? base : cc;
            const double hi_end = base > cc ? base : cc;
            switch (m->sol_col_status[j]) {
            case JAOS_BASIS_AT_LOWER: L = lo_end;    U = INFINITY; break;
            case JAOS_BASIS_AT_UPPER: L = -INFINITY; U = hi_end;   break;
            default:                  L = lo_end;    U = hi_end;   break;
            }
        } else {

            memset(g.vec, 0, (size_t)nrow * sizeof *g.vec);
            g.vec[g.pos[j]] = 1.0;
            int64_t npat = 0, words = 0;
            jm_lu_btran_sparse(&g.lu, g.vec, &g.w, g.pat, &npat);
            npat = jm_pattern_order(npat, g.pat, g.mark, nrow, &words);

            const double gj = g.gam[j];
            int64_t nt = 0;
            for (int64_t t = 0; t < npat; t++) {
                const int64_t i = g.pat[t];
                g.vec[i] *= g.rho[i] * gj;
                const double ri = g.vec[i];
                if (ri == 0.0)
                    continue;
                for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
                    const int64_t col = m->ar_index[k];
                    if (g.pos[col] >= 0)
                        continue;
                    if (!g.seen[col]) {
                        g.seen[col] = 1;
                        g.touched[nt++] = col;
                    }
                    g.alpha[col] += ri * m->ar_value[k];
                }
            }
            double dmin = -INFINITY, dmax = INFINITY;
            for (int64_t t = 0; t < nt; t++) {
                const int64_t col = g.touched[t];
                const double a = g.alpha[col];
                g.alpha[col] = 0.0;
                g.seen[col] = 0;
                rg_cost_limit(&g, col, a, &dmin, &dmax);
            }
            for (int64_t t = 0; t < npat; t++) {
                const int64_t i = g.pat[t];
                const int64_t v = ncol + i;
                if (g.pos[v] < 0)
                    rg_cost_limit(&g, v, -g.vec[i], &dmin, &dmax);
            }

            if (dmin > 0.0) dmin = 0.0;
            if (dmax < 0.0) dmax = 0.0;
            L = cc + dmin;
            U = cc + dmax;
        }
        rg_publish(sigma, L, U, lower ? &lower[j] : nullptr,
                   upper ? &upper[j] : nullptr);
    }
    rg_free(&g);
    return JAOS_OK;
}

static void rg_bound_range(rg *g, int64_t v, double *lo_lo, double *lo_hi,
                           double *hi_lo, double *hi_hi)
{
    const double l = rg_lower(g, v), u = rg_upper(g, v);
    const jaos_basis_status s = rg_status(g, v);
    double LL, LU, UL, UU;

    if (s == JAOS_BASIS_BASIC) {

        const double x = g->xb[g->pos[v]];
        LL = -INFINITY; LU = x > l ? x : l;
        UL = x < u ? x : u; UU = INFINITY;
    } else if (s == JAOS_BASIS_FREE) {
        LL = -INFINITY; LU = 0.0;
        UL = 0.0;       UU = INFINITY;
    } else {

        const bool at_lo = (l == u) ? !(g->d[v] < 0.0)
                                    : (s == JAOS_BASIS_AT_LOWER);
        const int64_t nrow = g->nrow;
        memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);
        rg_scatter(g, v, g->vec);

        if (v < g->ncol) {
            const jaos_model *m = g->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                g->vec[m->a_index[k]] *= g->rho[m->a_index[k]];
        } else {
            g->vec[v - g->ncol] *= g->rho[v - g->ncol];
        }
        int64_t npat = 0;
        jm_lu_ftran_sparse(&g->lu, g->vec, &g->w, g->pat, &npat);

        double dmin = -INFINITY, dmax = INFINITY;
        for (int64_t t = 0; t < npat; t++) {
            const int64_t p = g->pat[t];
            const int64_t q = g->basis[p];
            const double wp = g->vec[p] * rg_vscale(g, q);
            if (wp == 0.0)
                continue;
            const double lq = rg_lower(g, q), uq = rg_upper(g, q);
            const double x = g->xb[p];

            if (wp > 0.0) {
                if (isfinite(lq)) { const double r = (x - lq) / wp; if (r < dmax) dmax = r; }
                if (isfinite(uq)) { const double r = (x - uq) / wp; if (r > dmin) dmin = r; }
            } else {
                if (isfinite(lq)) { const double r = (x - lq) / wp; if (r > dmin) dmin = r; }
                if (isfinite(uq)) { const double r = (x - uq) / wp; if (r < dmax) dmax = r; }
            }
        }
        if (at_lo) {
            if (isfinite(u)) { const double r = u - l; if (r < dmax) dmax = r; }
        } else {
            if (isfinite(l)) { const double r = l - u; if (r > dmin) dmin = r; }
        }

        if (dmin > 0.0) dmin = 0.0;
        if (dmax < 0.0) dmax = 0.0;
        if (at_lo) {
            LL = l + dmin; LU = l + dmax;
            UL = l;        UU = INFINITY;
        } else {
            UL = u + dmin; UU = u + dmax;
            LL = -INFINITY; LU = u;
        }
    }
    if (lo_lo) *lo_lo = LL;
    if (lo_hi) *lo_hi = LU;
    if (hi_lo) *hi_lo = UL;
    if (hi_hi) *hi_hi = UU;
}

static jaos_status rg_bounds(jaos_model *m, bool rows, double *lo_lo,
                             double *lo_hi, double *hi_lo, double *hi_hi)
{
    if (m == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!rg_has_optimum(m)) {
        jm_set_err(m, "ranging needs the optimum of the last solve, and "
                      "there is none");
        return JAOS_ERR_INVALID_INPUT;
    }
    if (!lo_lo && !lo_hi && !hi_lo && !hi_hi)
        return JAOS_OK;

    rg g;
    jaos_status st = rg_build(m, &g);
    if (st != JAOS_OK) {
        rg_free(&g);
        return st;
    }
    const int64_t n = rows ? g.nrow : g.ncol;
    const int64_t base = rows ? g.ncol : 0;
    for (int64_t t = 0; t < n; t++)
        rg_bound_range(&g, base + t, lo_lo ? &lo_lo[t] : nullptr,
                       lo_hi ? &lo_hi[t] : nullptr,
                       hi_lo ? &hi_lo[t] : nullptr,
                       hi_hi ? &hi_hi[t] : nullptr);
    rg_free(&g);
    return JAOS_OK;
}

jaos_status jaos_rhs_ranging(jaos_model *m, double *lower_lo,
                             double *lower_hi, double *upper_lo,
                             double *upper_hi)
{
    return rg_bounds(m, true, lower_lo, lower_hi, upper_lo, upper_hi);
}

jaos_status jaos_bound_ranging(jaos_model *m, double *lower_lo,
                               double *lower_hi, double *upper_lo,
                               double *upper_hi)
{
    return rg_bounds(m, false, lower_lo, lower_hi, upper_lo, upper_hi);
}

jaos_status jm_tableau_build(jaos_model *m, jm_tableau **out)
{
    *out = nullptr;
    if (!rg_has_optimum(m)) {
        jm_set_err(m, "no optimal basis to read a tableau from");
        return JAOS_ERR_INVALID_INPUT;
    }
    rg *g = calloc(1, sizeof *g);
    if (g == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    const jaos_status st = rg_build(m, g);
    if (st != JAOS_OK) {
        rg_free(g);
        free(g);
        return st;
    }
    *out = g;
    return JAOS_OK;
}

void jm_tableau_free(jm_tableau *g)
{
    if (g == nullptr)
        return;
    rg_free(g);
    free(g);
}

int64_t jm_tableau_position(const jm_tableau *g, int64_t v)
{
    return g->pos[v];
}

int64_t jm_tableau_variable(const jm_tableau *g, int64_t p)
{
    return g->basis[p];
}

double jm_tableau_value(const jm_tableau *g, int64_t p)
{
    return g->xb[p];
}

int64_t jm_tableau_work(const jm_tableau *g)
{
    return g->w.units;
}

jaos_status jm_tableau_row(jm_tableau *g, int64_t p, double *row)
{
    const jaos_model *m = g->m;
    const int64_t nrow = g->nrow, ncol = g->ncol;
    memset(row, 0, (size_t)g->nvar * sizeof *row);
    memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);
    g->vec[p] = 1.0;
    int64_t npat = 0, words = 0;
    jm_lu_btran_sparse(&g->lu, g->vec, &g->w, g->pat, &npat);
    npat = jm_pattern_order(npat, g->pat, g->mark, nrow, &words);
    const double sb = rg_vscale(g, g->basis[p]);
    for (int64_t t = 0; t < npat; t++) {
        const int64_t i = g->pat[t];
        const double ri = g->vec[i];
        g->vec[i] = 0.0;
        if (ri == 0.0)
            continue;

        const double t_i = ri * g->rho[i] * sb;
        for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
            const int64_t j = m->ar_index[k];
            if (g->pos[j] < 0)
                row[j] += t_i * m->ar_value[k];
        }

        if (g->pos[ncol + i] < 0)
            row[ncol + i] = -ri * sb * g->rho[i];
    }
    jm_work_add(&g->w, npat);
    return JAOS_OK;
}
