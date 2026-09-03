/* Sensitivity and ranging on the basis behind the last optimum, over the
 * caller's own model (D258).
 *
 * The published basis is a basis of the model as loaded, count and all
 * (D257), so it is refactored here on the unscaled matrix, and every range
 * is a statement about that factorization: how far one number in the model
 * may move, everything else held, before this basis stops being optimal.
 * Cost ranging keeps primal feasibility for free and asks every nonbasic
 * reduced cost to keep the sign its status requires; bound ranging keeps
 * dual feasibility for free and asks every basic value to stay inside its
 * bounds. Both are the textbook ratio tests (Chvatal, Linear Programming,
 * 1983, ch. 10), taken on values recomputed from the factorization rather
 * than read from the published arrays, so a range and the basis it
 * describes come from one arithmetic.
 *
 * Signs are stated for MINIMIZE, the canonical space the checker judges in;
 * a maximised model's costs and reduced costs are negated on the way in
 * and its cost intervals flipped on the way out. Rows enter as their
 * logicals, whose column in the basis matrix is -e_i and whose bounds are
 * the row's own, so "the bound of a row" and "the bound of a column" are
 * one question asked of one routine.
 *
 * Nothing here is billed to jaos_work_units, which belongs to the solve;
 * jaos.h states the cost. Every solve and every sum runs in a fixed order,
 * so the ranges are bit-identical on every machine and every run (D8). */

#include "jaos_internal.h"

#include <assert.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;   /* nvar = ncol + nrow; variable v >= ncol
                                   is the logical of row v - ncol */
    double sigma;               /* +1 minimise, -1 maximise */
    int64_t *basis;             /* [nrow] the variable at each position */
    int64_t *pos;               /* [nvar] a basic's position, -1 nonbasic */
    jm_lu lu;
    double *xb;                 /* [nrow] basic values, by position */
    double *y;                  /* [nrow] canonical duals, by row */
    double *d;                  /* [nvar] canonical reduced costs, 0 basic */
    double *vec;                /* [nrow] dense solve vector */
    int64_t *pat;               /* [nrow] a sparse solve's pattern */
    uint64_t *mark;             /* bitmap for jm_pattern_order, zero at rest */
    double *alpha;              /* [ncol] pricing scratch, zero at rest */
    int64_t *touched;           /* [ncol] which alpha slots are live */
    unsigned char *seen;        /* [ncol] the same, as a flag */
    jm_work w;                  /* counted and not reported (jaos.h) */
} rg;

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

/* The value a nonbasic variable rests at. False when its status names a
 * bound the variable does not have, which the solver's own publication
 * never does (D257) and a basis is not without. */
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

/* The column of variable v in [A | -I], scattered by row into a zero vec. */
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

/* Factors the published basis on the model as loaded and recomputes the
 * basic values, the canonical duals and the canonical reduced costs from
 * it. Positions are structurals in index order, then logicals. */
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

    /* The basis matrix, compressed by column, position by position. At
     * least one slot: jm_lu_factor takes non-null arrays whenever dim > 0. */
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
                bv[q] = m->a_value[k];
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
    /* No early return on a row-less model: both solves are no-ops at
     * dimension zero and the reduced costs below are still the costs. */

    /* x_B = B^-1 (-N x_N): the nonbasics' columns, at their resting values,
     * scattered by row with the sign the equation Ax - s = 0 gives them. */
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
    jm_lu_ftran(&g->lu, g->vec, &g->w);
    memcpy(g->xb, g->vec, (size_t)nrow * sizeof *g->xb);
    memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);

    /* y' = c_B' B^-1 in the canonical space; a logical costs nothing. */
    for (p = 0; p < nrow; p++) {
        const int64_t v = g->basis[p];
        g->vec[p] = v < ncol ? g->sigma * m->col_cost[v] : 0.0;
    }
    jm_lu_btran(&g->lu, g->vec, &g->w);
    memcpy(g->y, g->vec, (size_t)nrow * sizeof *g->y);
    memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);

    /* d_N = c_N - N' y, canonical; a logical's is its row's dual, since its
     * column is -e_i and its cost zero. */
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

/* --- Cost ranging ------------------------------------------------------ */

/* One nonbasic variable's say on how far a basic column's cost may move:
 * its canonical reduced cost changes by -delta * alpha and must keep the
 * sign its status requires. A fixed variable's sign is free (check.c,
 * "fixed -> anything"), and a free nonbasic's must stay zero. */
static void rg_cost_limit(const rg *g, int64_t v, double alpha,
                          double *dmin, double *dmax)
{
    if (alpha == 0.0 || rg_lower(g, v) == rg_upper(g, v))
        return;
    const double ratio = g->d[v] / alpha;
    switch (rg_status(g, v)) {
    case JAOS_BASIS_AT_LOWER:            /* d - delta * alpha >= 0 */
        if (alpha > 0.0) { if (ratio < *dmax) *dmax = ratio; }
        else             { if (ratio > *dmin) *dmin = ratio; }
        break;
    case JAOS_BASIS_AT_UPPER:            /* d - delta * alpha <= 0 */
        if (alpha > 0.0) { if (ratio > *dmin) *dmin = ratio; }
        else             { if (ratio < *dmax) *dmax = ratio; }
        break;
    case JAOS_BASIS_FREE:                /* d - delta * alpha == 0 */
        if (*dmin < 0.0) *dmin = 0.0;
        if (*dmax > 0.0) *dmax = 0.0;
        break;
    default:
        break;
    }
}

/* A canonical interval into the model's own sense. */
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
            /* A fixed column's cost decides nothing. */
            L = -INFINITY;
            U = INFINITY;
        } else if (g.pos[j] < 0) {
            /* Nonbasic: its own reduced cost moves one for one with its
             * cost and must keep its sign; `base` is where it reaches 0.
             * The current cost stays inside: a reduced cost the solve
             * accepted on the wrong side of zero inside its tolerance
             * would otherwise put `base` past it. */
            const double base = cc - g.d[j];
            const double lo_end = base < cc ? base : cc;
            const double hi_end = base > cc ? base : cc;
            switch (m->sol_col_status[j]) {
            case JAOS_BASIS_AT_LOWER: L = lo_end;    U = INFINITY; break;
            case JAOS_BASIS_AT_UPPER: L = -INFINITY; U = hi_end;   break;
            default:                  L = lo_end;    U = hi_end;   break;
            }
        } else {
            /* Basic at position p: the row r = e_p' B^-1 says how every
             * nonbasic reduced cost moves per unit of this cost,
             * alpha_k = r' a_k, and each of them limits the move. The
             * row is priced over the rows r reaches, ascending (D35). */
            memset(g.vec, 0, (size_t)nrow * sizeof *g.vec);
            g.vec[g.pos[j]] = 1.0;
            int64_t npat = 0, words = 0;
            jm_lu_btran_sparse(&g.lu, g.vec, &g.w, g.pat, &npat);
            npat = jm_pattern_order(npat, g.pat, g.mark, nrow, &words);

            int64_t nt = 0;
            for (int64_t t = 0; t < npat; t++) {
                const int64_t i = g.pat[t];
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
            /* The current cost is inside its own range: a limit the other
             * side of it is a reduced cost the solve accepted inside its
             * tolerance, and the range says so by stopping at zero. */
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

/* --- Bound ranging ----------------------------------------------------- */

/* The interval each of variable v's two bounds may take. A basic variable
 * is held by the basis and not by either bound, so each bound may close
 * in on the value and no further. A nonbasic one rests on a bound, moves
 * with it, and drags the basics along by w = B^-1 a_v per unit; those must
 * stay inside their own bounds, and the two bounds of v must not cross.
 * The bound it does not rest on may close in on the value. */
static void rg_bound_range(rg *g, int64_t v, double *lo_lo, double *lo_hi,
                           double *hi_lo, double *hi_hi)
{
    const double l = rg_lower(g, v), u = rg_upper(g, v);
    const jaos_basis_status s = rg_status(g, v);
    double LL, LU, UL, UU;

    if (s == JAOS_BASIS_BASIC) {
        /* Each bound may close in on the value; the current bound stays
         * inside when the recomputed value sits a rounding past it. */
        const double x = g->xb[g->pos[v]];
        LL = -INFINITY; LU = x > l ? x : l;
        UL = x < u ? x : u; UU = INFINITY;
    } else if (s == JAOS_BASIS_FREE) {
        LL = -INFINITY; LU = 0.0;
        UL = 0.0;       UU = INFINITY;
    } else {
        /* A fixed variable's status names either bound and its reduced
         * cost obeys no sign, so which bound holds it is read from that
         * sign: a negative canonical reduced cost wants to rise and is
         * held by the upper bound. */
        const bool at_lo = (l == u) ? !(g->d[v] < 0.0)
                                    : (s == JAOS_BASIS_AT_LOWER);
        const int64_t nrow = g->nrow;
        memset(g->vec, 0, (size_t)nrow * sizeof *g->vec);
        rg_scatter(g, v, g->vec);
        int64_t npat = 0;
        jm_lu_ftran_sparse(&g->lu, g->vec, &g->w, g->pat, &npat);

        double dmin = -INFINITY, dmax = INFINITY;
        for (int64_t t = 0; t < npat; t++) {
            const int64_t p = g->pat[t];
            const double wp = g->vec[p];
            if (wp == 0.0)
                continue;
            const int64_t q = g->basis[p];
            const double lq = rg_lower(g, q), uq = rg_upper(g, q);
            const double x = g->xb[p];
            /* lq <= x - delta * wp <= uq */
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
        /* The current bound is inside its own range, as for costs. */
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
