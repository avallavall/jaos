/* Dual simplex with bounds.
 *
 * The problem is held as M z = 0 with M = [A | -I] and z = [x; s]: every
 * row gets a logical variable carrying its activity, so a row bound and a
 * column bound are the same kind of object and there is one code path
 * instead of four. A basis is m columns of M; the nonbasic variables are
 * pinned to bounds, which is what makes the basis determine a point.
 *
 * The dual method keeps reduced costs feasible and drives out primal
 * infeasibility, the mirror of the primal method. It is the right default
 * because it warm-starts well after a bound change — the situation branch
 * and bound creates thousands of times per solve — and because dual
 * steepest-edge pricing is what makes large models tractable [1].
 *
 * This is the correctness-first skeleton: largest-infeasibility pricing
 * and a textbook ratio test. Dual steepest edge [8], the Harris two-pass
 * test with bound flipping [7][19] and dual phase 1 [21] land on top of
 * it, each replacing one clearly separated decision.
 *
 * Sign conventions, stated once because every bug here is a sign bug:
 *   - x_B = -B^-1 N x_N, so moving a nonbasic by dx moves the basics by
 *     -B^-1 M_q dx.
 *   - alpha_j is row r of B^-1 M, so dx_B[r] = -alpha_q * dx_q.
 *   - A basic below its lower bound must rise, so the entering move must
 *     make dx_B[r] positive.
 *   - Internally the objective is always minimised; a maximisation model
 *     has its costs negated on the way in and its duals on the way out.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Draft tolerances (PLAN.md 2.6); frozen when the Netlib gate closes. */
constexpr double PRIMAL_TOL    = 1e-7;
constexpr double DUAL_TOL      = 1e-7;
constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */
constexpr double LU_PIVOT_TOL  = 0.1;    /* Markowitz threshold */
constexpr double LU_UPDATE_TOL = 1e-9;
constexpr int64_t REFACTOR_EVERY = 64;

/* A stop that is not a real limit, only a guard against a loop that fails
 * to terminate through a bug. Hitting it is a defect, not an answer. */
constexpr int64_t ITER_SANITY_FACTOR = 200;

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    /* Bounds and costs over all variables: structurals first, then the
     * logicals that carry row activities. */
    double *lo, *up, *cost;

    jm_var_status *status;   /* [nvar] */
    int64_t *basis;          /* [nrow] variable occupying each position */
    int64_t *where;          /* [nvar] basis position, or -1 */

    double *xb;              /* [nrow] basic values */
    double *d;               /* [nvar] reduced costs */

    jm_lu lu;
    jm_work work;

    /* Scratch, all owned. */
    double *col;             /* [nrow] a column, or an FTRAN result */
    double *rho;             /* [nrow] row r of B^-1 */
    double *alpha;           /* [nvar] pricing row */

    int64_t iters;
    bool needs_refactor;
} sx;

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

static void sx_free(sx *s)
{
    free(s->lo); free(s->up); free(s->cost);
    free(s->status); free(s->basis); free(s->where);
    free(s->xb); free(s->d);
    free(s->col); free(s->rho); free(s->alpha);
    jm_lu_free(&s->lu);
    memset(s, 0, sizeof *s);
}

static jaos_status sx_init(sx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    jm_lu_init(&s->lu);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    s->lo     = jm_alloc_array(s->nvar, sizeof(double));
    s->up     = jm_alloc_array(s->nvar, sizeof(double));
    s->cost   = jm_calloc_array(s->nvar, sizeof(double));
    s->status = jm_alloc_array(s->nvar, sizeof(jm_var_status));
    s->basis  = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->where  = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->xb     = jm_calloc_array(s->nrow, sizeof(double));
    s->d      = jm_calloc_array(s->nvar, sizeof(double));
    s->col    = jm_calloc_array(s->nrow, sizeof(double));
    s->rho    = jm_calloc_array(s->nrow, sizeof(double));
    s->alpha  = jm_calloc_array(s->nvar, sizeof(double));

    if (!s->lo || !s->up || !s->cost || !s->status || !s->basis ||
        !s->where || !s->xb || !s->d || !s->col || !s->rho || !s->alpha) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    for (int64_t j = 0; j < s->ncol; j++) {
        s->lo[j] = m->col_lower[j];
        s->up[j] = m->col_upper[j];
        s->cost[j] = sigma * m->col_cost[j];
    }
    for (int64_t i = 0; i < s->nrow; i++) {
        s->lo[s->ncol + i] = m->row_lower[i];
        s->up[s->ncol + i] = m->row_upper[i];
        s->cost[s->ncol + i] = 0.0;
    }
    return JAOS_OK;
}

/* Value a nonbasic variable is pinned at. */
static double nonbasic_value(const sx *s, int64_t v)
{
    switch (s->status[v]) {
    case JM_AT_LOWER: return s->lo[v];
    case JM_AT_UPPER: return s->up[v];
    case JM_FREE:     return 0.0;
    case JM_BASIC:    break;
    }
    return 0.0;
}

/* Scatters variable v's column of M into a dense vector. */
static void var_column(const sx *s, int64_t v, double *out)
{
    memset(out, 0, (size_t)s->nrow * sizeof *out);
    if (v < s->ncol) {
        const jaos_model *m = s->m;
        for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
            out[m->a_index[k]] = m->a_value[k];
    } else {
        out[v - s->ncol] = -1.0;   /* logicals enter as -I */
    }
}

/* rho' M_v, the pricing-row entry for variable v. */
static double price_entry(const sx *s, int64_t v)
{
    if (v >= s->ncol)
        return -s->rho[v - s->ncol];
    const jaos_model *m = s->m;
    double a = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        a += s->rho[m->a_index[k]] * m->a_value[k];
    return a;
}

/* The slack basis: every logical basic, every structural pinned to the
 * bound that makes its reduced cost feasible. With B = -I this is
 * factorizable by inspection and dual feasible whenever each structural
 * has the bound its cost asks for — when one does not, the model needs a
 * dual phase 1, which is a later step of the plan. */
static bool build_initial_basis(sx *s)
{
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->ncol + i;
        s->basis[i] = v;
        s->status[v] = JM_BASIC;
        s->where[v] = i;
    }
    for (int64_t j = 0; j < s->ncol; j++) {
        s->where[j] = -1;
        bool has_lo = isfinite(s->lo[j]);
        bool has_up = isfinite(s->up[j]);

        if (s->cost[j] >= 0.0 && has_lo)
            s->status[j] = JM_AT_LOWER;
        else if (s->cost[j] <= 0.0 && has_up)
            s->status[j] = JM_AT_UPPER;
        else if (has_lo)
            s->status[j] = JM_AT_LOWER;
        else if (has_up)
            s->status[j] = JM_AT_UPPER;
        else if (s->cost[j] == 0.0)
            s->status[j] = JM_FREE;
        else
            return false;   /* needs dual phase 1 */

        /* A bound chosen against the cost's sign leaves the start dual
         * infeasible, which this skeleton cannot repair. */
        double dj = s->cost[j];
        if (s->status[j] == JM_AT_LOWER && dj < -DUAL_TOL)
            return false;
        if (s->status[j] == JM_AT_UPPER && dj > DUAL_TOL)
            return false;
    }
    return true;
}

/* --------------------------------------------------------------------- */
/* Recomputation from the factorization                                  */
/* --------------------------------------------------------------------- */

static jaos_status refactorize(sx *s)
{
    /* Gather the basis columns into CSC and factor them. */
    int64_t nz = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        nz += v < s->ncol
            ? s->m->a_start[v + 1] - s->m->a_start[v]
            : 1;
    }
    int64_t *bs = jm_alloc_array(s->nrow + 1, sizeof(int64_t));
    int64_t *bi = jm_alloc_array(nz, sizeof(int64_t));
    double *bv = jm_alloc_array(nz, sizeof(double));
    if (!bs || !bi || !bv) {
        free(bs); free(bi); free(bv);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    int64_t p = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        bs[i] = p;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            for (int64_t k = s->m->a_start[v]; k < s->m->a_start[v + 1]; k++) {
                bi[p] = s->m->a_index[k];
                bv[p] = s->m->a_value[k];
                p++;
            }
        } else {
            bi[p] = v - s->ncol;
            bv[p] = -1.0;
            p++;
        }
    }
    bs[s->nrow] = p;

    jaos_status st = jm_lu_factor(&s->lu, s->nrow, bs, bi, bv,
                                  LU_PIVOT_TOL, &s->work);
    free(bs); free(bi); free(bv);
    if (st != JAOS_OK)
        return st;
    s->needs_refactor = false;
    return JAOS_OK;
}

/* x_B = -B^-1 (N x_N). */
static void compute_primal(sx *s)
{
    double *rhs = s->col;
    memset(rhs, 0, (size_t)s->nrow * sizeof *rhs);

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        double val = nonbasic_value(s, v);
        if (val == 0.0)
            continue;
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                rhs[m->a_index[k]] -= m->a_value[k] * val;
        } else {
            rhs[v - s->ncol] += val;   /* column is -e_i */
        }
    }
    jm_lu_ftran(&s->lu, rhs, &s->work);
    memcpy(s->xb, rhs, (size_t)s->nrow * sizeof *rhs);
}

/* d_N = c_N - y' M_N, with y = B^-T c_B. */
static void compute_duals(sx *s)
{
    double *y = s->rho;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC) {
            s->d[v] = 0.0;
            continue;
        }
        s->d[v] = s->cost[v] - price_entry(s, v);
    }
}

/* --------------------------------------------------------------------- */
/* One iteration                                                         */
/* --------------------------------------------------------------------- */

/* Largest bound violation among the basics. Returns -1 when primal
 * feasible; otherwise sets *below to which bound was breached. */
static int64_t price_row(const sx *s, bool *below)
{
    int64_t best = -1;
    double worst = PRIMAL_TOL;

    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        double viol_lo = isfinite(s->lo[v]) ? s->lo[v] - s->xb[i] : 0.0;
        double viol_up = isfinite(s->up[v]) ? s->xb[i] - s->up[v] : 0.0;
        if (viol_lo > worst) {
            worst = viol_lo;
            best = i;
            *below = true;
        }
        if (viol_up > worst) {
            worst = viol_up;
            best = i;
            *below = false;
        }
    }
    return best;
}

/* Textbook dual ratio test: among the entries whose sign lets the basic in
 * row r move the right way, take the one whose reduced cost runs out
 * first. Harris and bound flipping replace exactly this function. */
static int64_t dual_ratio_test(const sx *s, bool below, double *theta_out)
{
    int64_t best = -1;
    double best_ratio = HUGE_VAL;
    double best_pivot = 0.0;

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        double a = s->alpha[v];
        if (fabs(a) < PIVOT_MIN)
            continue;

        /* dx_B[r] = -a * dx_v, and dx_v's sign is fixed by which bound v
         * sits at. Keep only the entries that push row r the right way. */
        bool ok;
        if (s->status[v] == JM_AT_LOWER)
            ok = below ? (a < 0.0) : (a > 0.0);
        else if (s->status[v] == JM_AT_UPPER)
            ok = below ? (a > 0.0) : (a < 0.0);
        else
            ok = true;   /* free: may move either way */
        if (!ok)
            continue;

        double ratio = fabs(s->d[v]) / fabs(a);
        /* Ties go to the larger pivot: same step, better conditioning. */
        if (ratio < best_ratio - 1e-12 ||
            (ratio < best_ratio + 1e-12 && fabs(a) > fabs(best_pivot))) {
            best_ratio = ratio;
            best_pivot = a;
            best = v;
        }
    }
    if (best < 0)
        return -1;
    *theta_out = s->d[best] / s->alpha[best];
    return best;
}

/* --------------------------------------------------------------------- */
/* Driver                                                                */
/* --------------------------------------------------------------------- */

static jaos_status run(sx *s, jaos_solve_status *out)
{
    jaos_status st = refactorize(s);
    if (st != JAOS_OK)
        return st;
    if (s->lu.rank != s->nrow) {
        /* The slack basis is nonsingular by construction, so this would
         * mean the factorization itself is wrong, not the model. */
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }
    compute_primal(s);
    compute_duals(s);

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->work_limit > 0 && s->work.units >= s->m->work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters > iter_cap) {
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        if (s->needs_refactor) {
            st = refactorize(s);
            if (st != JAOS_OK)
                return st;
            if (s->lu.rank != s->nrow) {
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
            compute_primal(s);
            compute_duals(s);
        }

        bool below = false;
        int64_t r = price_row(s, &below);
        if (r < 0) {
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        /* Row r of B^-1, then the pricing row over the nonbasics. */
        memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
        s->rho[r] = 1.0;
        jm_lu_btran(&s->lu, s->rho, &s->work);
        for (int64_t v = 0; v < s->nvar; v++)
            s->alpha[v] = s->status[v] == JM_BASIC ? 0.0 : price_entry(s, v);

        double theta_dual = 0.0;
        int64_t q = dual_ratio_test(s, below, &theta_dual);
        if (q < 0) {
            /* No entering column can repair row r: the dual is unbounded,
             * so the primal has no feasible point. */
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;
        }

        int64_t leaving = s->basis[r];
        double bound = below ? s->lo[leaving] : s->up[leaving];
        double alpha_q = s->alpha[q];

        /* Primal step: how far the entering variable moves so that row r
         * lands exactly on the bound it violated. */
        double theta_primal = (s->xb[r] - bound) / alpha_q;

        /* Reduced costs shift by the dual step along the pricing row. */
        for (int64_t v = 0; v < s->nvar; v++) {
            if (s->status[v] == JM_BASIC || v == q)
                continue;
            s->d[v] -= theta_dual * s->alpha[v];
        }
        s->d[leaving] = -theta_dual;
        s->d[q] = 0.0;

        /* Basic values shift by dx_B = -B^-1 M_q dx_q — note the sign, it
         * comes straight from x_B = -B^-1 N x_N. */
        var_column(s, q, s->col);
        jm_lu_ftran(&s->lu, s->col, &s->work);
        double q_value = nonbasic_value(s, q);
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= theta_primal * s->col[i];
        /* Position r now holds the entering variable, at its new value. */
        s->xb[r] = q_value + theta_primal;

        /* Swap. The leaving variable settles on the bound it violated. */
        s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
        s->where[leaving] = -1;
        s->basis[r] = q;
        s->status[q] = JM_BASIC;
        s->where[q] = r;

        /* Repair the factorization, or schedule a rebuild. */
        if (s->lu.n_updates >= REFACTOR_EVERY) {
            s->needs_refactor = true;
        } else {
            var_column(s, q, s->col);
            jaos_status ust = jm_lu_update(&s->lu, r, s->col,
                                           LU_UPDATE_TOL, &s->work);
            if (ust == JAOS_ERR_NUMERICAL || ust == JAOS_ERR_OUT_OF_MEMORY)
                s->needs_refactor = true;
            else if (ust != JAOS_OK)
                return ust;
        }

        s->iters++;
    }
}

/* --------------------------------------------------------------------- */
/* Entry point                                                           */
/* --------------------------------------------------------------------- */

static jaos_status publish(sx *s, jaos_solve_status status)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    m->solve_status = status;
    m->solve_work = s->work.units;
    m->solve_iters = s->iters;

    free(m->sol_col); free(m->sol_row);
    free(m->sol_dual); free(m->sol_redcost);
    m->sol_col     = jm_calloc_array(m->num_col, sizeof(double));
    m->sol_row     = jm_calloc_array(m->num_row, sizeof(double));
    m->sol_dual    = jm_calloc_array(m->num_row, sizeof(double));
    m->sol_redcost = jm_calloc_array(m->num_col, sizeof(double));
    if (!m->sol_col || !m->sol_row || !m->sol_dual || !m->sol_redcost)
        return JAOS_ERR_OUT_OF_MEMORY;

    if (status != JAOS_SOLVE_OPTIMAL) {
        m->objective = 0.0;
        return JAOS_OK;
    }

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = s->status[j] == JM_BASIC
            ? s->xb[s->where[j]] : nonbasic_value(s, j);
    for (int64_t i = 0; i < m->num_row; i++) {
        int64_t v = m->num_col + i;
        m->sol_row[i] = s->status[v] == JM_BASIC
            ? s->xb[s->where[v]] : nonbasic_value(s, v);
    }

    /* y = B^-T c_B, then undo the internal minimisation. */
    double *y = s->rho;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_dual[i] = sigma * y[i];
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_redcost[j] = sigma * s->d[j];

    double obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++)
        obj += m->col_cost[j] * m->sol_col[j];
    m->objective = obj;
    return JAOS_OK;
}

jaos_status jm_dual_simplex(jaos_model *m)
{
    sx s;
    jaos_status st = sx_init(&s, m);
    if (st != JAOS_OK)
        return st;

    jaos_solve_status outcome = JAOS_SOLVE_NUMERICAL_ERROR;
    if (!build_initial_basis(&s)) {
        /* Dual phase 1 is a later step; say so rather than guessing. */
        jm_set_err(m, "this model needs a dual phase 1, which is not "
                      "implemented yet");
        sx_free(&s);
        return JAOS_ERR_INVALID_INPUT;
    }

    st = run(&s, &outcome);
    if (st == JAOS_OK)
        st = publish(&s, outcome);
    sx_free(&s);
    return st;
}
