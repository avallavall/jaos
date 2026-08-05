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
 * Pricing is dual steepest edge [8] and phase 1 is by artificial bounds
 * (both below). The ratio test is still the textbook one; the Harris
 * two-pass test with bound flipping [7][19] replaces exactly that.
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
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Draft tolerances (PLAN.md 2.6); frozen when the Netlib gate closes.
 * PLAN 2.6 specifies these in scaled space — see Q7: the solver does not
 * apply the scaling it computes yet, so today they act on raw values. */
constexpr double PRIMAL_TOL    = 1e-7;
constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */
/* PLAN 2.6 also specifies a dual feasibility tolerance. It has no use
 * here yet: the dual simplex maintains dual feasibility by construction,
 * and the ratio test compares ratios rather than testing costs against a
 * threshold. It arrives with the Harris test, which needs exactly that
 * kind of threshold to widen its first pass. Declaring it now and leaving
 * it unapplied would suggest a check that is not happening. */
constexpr double LU_PIVOT_TOL  = 0.1;    /* Markowitz threshold */
constexpr double LU_UPDATE_TOL = 1e-9;

/* Floor on a steepest-edge weight. Every weight is the squared norm of a
 * row of B^-1, so it is positive by construction; the recurrence that
 * carries it forward subtracts, and subtraction can cancel a small true
 * value to zero or below. The floor exists so that cancellation cannot
 * divide by zero, and it is set far below any weight a nonsingular basis
 * produces — it is a guard, not a tuning knob. Weights that drift from
 * the truth are a separate matter, handled by resetting them (PLAN 2.8). */
constexpr double DSE_MIN = 1e-12;

/* Refactorization interval. PLAN 2.5.5 also calls for stability triggers
 * (FTRAN/BTRAN residual checks); only the interval and the reactive
 * fallback on a failed update exist so far. */
constexpr int64_t REFACTOR_EVERY = 64;

/* The clock is read once every this many iterations rather than every
 * iteration. Reading it cannot change which pivot is chosen (D8) — it only
 * decides whether to stop — and at this granularity the syscall cost
 * disappears while the cutoff stays responsive. */
constexpr int64_t TIME_CHECK_EVERY = 64;

/* Dual phase 1 by artificial bounds.
 *
 * The slack basis is dual feasible only if every column has the bound its
 * cost asks for. A column whose cost pushes it towards a bound that does
 * not exist leaves the start dual infeasible, and the dual simplex has
 * nowhere to begin.
 *
 * The repair: lend that column a finite bound. Solve the bounded problem —
 * which is now dual feasible by construction — and read the answer. If no
 * artificial bound is active at the optimum, it never constrained anything
 * and the answer is the original problem's. If one is active, the objective
 * wanted to keep going past a bound that was never real: the original
 * problem is unbounded.
 *
 * Koberstein [21] compares this against subproblem and cost-shifting
 * methods; those converge better on hard models, and replacing this is a
 * later step. What matters now is that a whole class of models becomes
 * solvable instead of refused, and that the unbounded verdict is read off
 * evidence rather than guessed.
 *
 * The bound has to be large enough not to cut off a genuine optimum and
 * small enough to stay numerically sane against the tolerances above. This
 * value is a draft, like the tolerances themselves. */
constexpr double ARTIFICIAL_BOUND = 1e10;

/* A stop that is not a real limit, only a guard against a loop that fails
 * to terminate through a bug. Hitting it is a defect in JAOS, so it is
 * reported as a library error rather than as a solve outcome: a caller
 * must be able to tell "this model is hard" from "this code is wrong". */
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

    /* Bounds JAOS invented to get a dual feasible start. A column is
     * caught by exactly one branch of the cost-sign test, so one array
     * says both whether a bound was lent and which side it went on — two
     * parallel flags would encode an invariant the types do not. An
     * active one at the optimum means unbounded. */
    enum { NOT_FAKE = 0, FAKE_LO, FAKE_UP } *fake;

    double *xb;              /* [nrow] basic values */
    double *d;               /* [nvar] reduced costs */

    /* Dual steepest-edge weights, one per basis position: dse[i] tracks
     * ||row i of B^-1||^2. Carried across refactorizations — recomputing
     * them exactly would cost one solve per row, which is the whole point
     * of the recurrence. */
    double *dse;             /* [nrow] */

    jm_lu lu;
    jm_work work;

    /* Scratch, all owned. `col` carries an FTRAN result; `raw` keeps the
     * untransformed column the LU update needs. */
    double *col;
    double *raw;
    double *rho;             /* [nrow] row r of B^-1 */
    double *tau;             /* [nrow] B^-1 rho, for the weight update */
    double *alpha;           /* [nvar] pricing row */

    /* Refactorization buffers, grown once and reused: a refactorization
     * every REFACTOR_EVERY iterations should not also be an allocation. */
    int64_t *bs, *bi;
    double *bv;
    int64_t bi_cap, bv_cap;

    struct timespec started;
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
    free(s->xb); free(s->d); free(s->dse);
    free(s->col); free(s->raw); free(s->rho); free(s->tau); free(s->alpha);
    free(s->bs); free(s->bi); free(s->bv);
    free(s->fake);
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
    s->dse    = jm_alloc_array(s->nrow, sizeof(double));
    s->col    = jm_calloc_array(s->nrow, sizeof(double));
    s->raw    = jm_calloc_array(s->nrow, sizeof(double));
    s->rho    = jm_calloc_array(s->nrow, sizeof(double));
    s->tau    = jm_calloc_array(s->nrow, sizeof(double));
    s->alpha  = jm_calloc_array(s->nvar, sizeof(double));
    s->bs     = jm_alloc_array(s->nrow + 1, sizeof(int64_t));
    s->fake   = jm_calloc_array(s->nvar, sizeof *s->fake);

    if (!s->lo || !s->up || !s->cost || !s->status || !s->basis ||
        !s->where || !s->xb || !s->d || !s->dse || !s->col || !s->raw ||
        !s->rho || !s->tau || !s->alpha || !s->bs || !s->fake) {
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

/* Value of any variable, basic or not. */
static double var_value(const sx *s, int64_t v)
{
    return s->status[v] == JM_BASIC ? s->xb[s->where[v]]
                                    : nonbasic_value(s, v);
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

/* rho' M_v, the pricing-row entry for variable v, charging the nonzeros it
 * touches (PLAN 2.7 weights pricing the same as any other solve traffic).
 *
 * src/check.c has a loop of similar shape. They stay apart because sharing
 * would make the checker link against solver internals — this function
 * takes solver state, handles logicals and bills work units, none of which
 * the checker has any business seeing. See the header of src/check.c for
 * what the checker's independence actually rests on; it is not this. */
static double price_entry(sx *s, int64_t v)
{
    if (v >= s->ncol) {
        jm_work_add(&s->work, JM_WORK_NONZERO);
        return -s->rho[v - s->ncol];
    }
    const jaos_model *m = s->m;
    double a = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        a += s->rho[m->a_index[k]] * m->a_value[k];
    jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                          JM_WORK_NONZERO);
    return a;
}

/* The slack basis: every logical basic, every structural pinned to the
 * bound that makes its reduced cost feasible. With B = -I this factors by
 * inspection.
 *
 * A structural whose cost asks for a bound it does not have gets an
 * artificial one, which is dual phase 1 (see ARTIFICIAL_BOUND above). The
 * loan is recorded so the outcome can be read honestly afterwards. */
static void build_initial_basis(sx *s)
{
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->ncol + i;
        s->basis[i] = v;
        s->status[v] = JM_BASIC;
        s->where[v] = i;
        /* B = -I, so row i of B^-1 is -e_i and its squared norm is exactly
         * one. The steepest-edge recurrence starts from truth here rather
         * than from an approximation, which is the reason the dual method
         * is started from the slack basis at all. */
        s->dse[i] = 1.0;
    }
    for (int64_t j = 0; j < s->ncol; j++) {
        s->where[j] = -1;
        bool has_lo = isfinite(s->lo[j]);
        bool has_up = isfinite(s->up[j]);

        /* A positive cost wants the variable low, a negative one wants it
         * high; that is the bound its reduced cost is feasible at. */
        if (s->cost[j] > 0.0) {
            if (!has_lo) {
                s->lo[j] = -ARTIFICIAL_BOUND;
                s->fake[j] = FAKE_LO;
            }
            s->status[j] = JM_AT_LOWER;
        } else if (s->cost[j] < 0.0) {
            if (!has_up) {
                s->up[j] = ARTIFICIAL_BOUND;
                s->fake[j] = FAKE_UP;
            }
            s->status[j] = JM_AT_UPPER;
        } else if (has_lo) {
            s->status[j] = JM_AT_LOWER;   /* zero cost: either bound is fine */
        } else if (has_up) {
            s->status[j] = JM_AT_UPPER;
        } else {
            s->status[j] = JM_FREE;       /* zero cost, no bounds: d = 0 */
        }
    }
}

/* Did the optimum come to rest against a bound JAOS invented? Runs once,
 * when optimality is declared. If so the
 * objective wanted to keep going past something that was never there, and
 * the original problem is unbounded. Checked on the values as solved, so
 * the verdict rests on evidence rather than on a guess about which
 * variables looked suspicious. */
static bool leans_on_an_invented_bound(const sx *s)
{
    for (int64_t j = 0; j < s->ncol; j++) {
        if (s->fake[j] == NOT_FAKE)
            continue;
        double v = var_value(s, j);
        if (s->fake[j] == FAKE_LO && v <= s->lo[j] + PRIMAL_TOL)
            return true;
        if (s->fake[j] == FAKE_UP && v >= s->up[j] - PRIMAL_TOL)
            return true;
    }
    return false;
}

/* --------------------------------------------------------------------- */
/* Recomputation from the factorization                                  */
/* --------------------------------------------------------------------- */

static jaos_status refactorize(sx *s)
{
    int64_t nz = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        nz += v < s->ncol ? s->m->a_start[v + 1] - s->m->a_start[v] : 1;
    }
    if (!JM_GROW(s->bi, s->bi_cap, nz) || !JM_GROW(s->bv, s->bv_cap, nz))
        return JAOS_ERR_OUT_OF_MEMORY;

    int64_t p = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        s->bs[i] = p;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            for (int64_t k = s->m->a_start[v]; k < s->m->a_start[v + 1]; k++) {
                s->bi[p] = s->m->a_index[k];
                s->bv[p] = s->m->a_value[k];
                p++;
            }
        } else {
            s->bi[p] = v - s->ncol;
            s->bv[p] = -1.0;
            p++;
        }
    }
    s->bs[s->nrow] = p;

    jaos_status st = jm_lu_factor(&s->lu, s->nrow, s->bs, s->bi, s->bv,
                                  LU_PIVOT_TOL, &s->work);
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
            jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                                  JM_WORK_NONZERO);
        } else {
            rhs[v - s->ncol] += val;   /* column is -e_i */
            jm_work_add(&s->work, JM_WORK_NONZERO);
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

/* Rebuild the factorization and everything derived from it. Returns false
 * when the basis will not factor, which for a basis the algorithm itself
 * assembled means the numerics have failed rather than the model. */
static jaos_status refresh(sx *s, bool *ok)
{
    jaos_status st = refactorize(s);
    if (st != JAOS_OK)
        return st;
    if (s->lu.rank != s->nrow) {
        *ok = false;
        return JAOS_OK;
    }
    compute_primal(s);
    compute_duals(s);
    *ok = true;
    return JAOS_OK;
}

/* --------------------------------------------------------------------- */
/* One iteration                                                         */
/* --------------------------------------------------------------------- */

/* Dual steepest-edge pricing [8]: among the basics that violate a bound,
 * the one whose violation is largest measured along the direction the
 * method would actually move — violation squared over the squared norm of
 * that row of B^-1. Returns -1 when primal feasible; otherwise sets *below
 * to which bound was breached.
 *
 * The raw violation on its own is not a distance: two rows a metre apart
 * from feasibility differ by a factor of a thousand if one is written in
 * millimetres, and the largest-violation rule would pick the millimetre
 * row every time. The weight divides that unit out. This is the single
 * largest determinant of how many iterations a real model takes [1].
 *
 * A variable cannot break both of its bounds at once, so the larger of the
 * two violations is the only candidate. */
static int64_t price_row(sx *s, bool *below)
{
    int64_t best = -1;
    double best_score = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        double viol_lo = isfinite(s->lo[v]) ? s->lo[v] - s->xb[i] : 0.0;
        double viol_up = isfinite(s->up[v]) ? s->xb[i] - s->up[v] : 0.0;

        bool under = viol_lo >= viol_up;
        double viol = under ? viol_lo : viol_up;
        if (viol <= PRIMAL_TOL)
            continue;

        double score = viol * viol / s->dse[i];
        if (score > best_score) {
            best_score = score;
            best = i;
            *below = under;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return best;
}

/* The weight recurrence. Row i of the new B^-1 is row i minus
 * (alpha_i / alpha_r) times row r, and row r becomes row r over alpha_r;
 * expanding the squared norms of those two statements gives everything
 * below, with tau_i = rho_i . rho_r supplying the cross term. Documented
 * in the header, which is also where the exported-for-testing rationale
 * lives. */
void jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau)
{
    double pivot = alpha[r];
    if (pivot == 0.0)
        return;   /* the ratio test never picks one, and weights are a
                     heuristic: leaving them stale beats infinities */

    double wr = w[r];
    for (int64_t i = 0; i < n; i++) {
        if (i == r || alpha[i] == 0.0)
            continue;
        double k = alpha[i] / pivot;
        double wi = w[i] - 2.0 * k * tau[i] + k * k * wr;
        w[i] = wi > DSE_MIN ? wi : DSE_MIN;
    }
    double wnew = wr / (pivot * pivot);
    w[r] = wnew > DSE_MIN ? wnew : DSE_MIN;
}

/* Textbook dual ratio test: among the entries whose sign lets the basic in
 * row r move the right way, take the one whose reduced cost runs out
 * first. Harris and bound flipping replace exactly this function. */
static int64_t dual_ratio_test(sx *s, bool below, double *theta_out)
{
    int64_t best = -1;
    double best_ratio = HUGE_VAL;
    double best_pivot = 0.0;
    int64_t examined = 0;

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        examined++;
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
    jm_work_add(&s->work, examined * JM_WORK_NONZERO);

    if (best < 0)
        return -1;
    *theta_out = s->d[best] / s->alpha[best];
    return best;
}

/* Builds row r of B^-1 M and picks the entering variable from it. */
static int64_t price_and_select(sx *s, int64_t r, bool below,
                                double *theta_dual)
{
    memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
    s->rho[r] = 1.0;
    jm_lu_btran(&s->lu, s->rho, &s->work);

    for (int64_t v = 0; v < s->nvar; v++)
        s->alpha[v] = s->status[v] == JM_BASIC ? 0.0 : price_entry(s, v);

    return dual_ratio_test(s, below, theta_dual);
}

/* Applies the basis change: q enters at position r, the variable there
 * leaves to the bound it violated. */
static jaos_status pivot(sx *s, int64_t r, int64_t q, bool below,
                         double theta_dual)
{
    int64_t leaving = s->basis[r];
    double bound = below ? s->lo[leaving] : s->up[leaving];
    double alpha_q = s->alpha[q];

    /* Primal step: how far the entering variable moves so that row r lands
     * exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;

    /* Reduced costs shift by the dual step along the pricing row. */
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC || v == q)
            continue;
        s->d[v] -= theta_dual * s->alpha[v];
    }
    s->d[leaving] = -theta_dual;
    s->d[q] = 0.0;
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);

    /* Basic values shift by dx_B = -B^-1 M_q dx_q — note the sign, it
     * comes straight from x_B = -B^-1 N x_N. The raw column is kept
     * because the LU update wants it untransformed. */
    var_column(s, q, s->raw);
    memcpy(s->col, s->raw, (size_t)s->nrow * sizeof *s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    /* Steepest-edge weights, while the old basis is still in force: both
     * vectors the recurrence needs are solves against it, so this has to
     * happen before the factorization is repaired below. rho still holds
     * row r of B^-1 from price_and_select, which is the one piece of state
     * this function inherits rather than derives.
     *
     * The second FTRAN is what exact weights cost, and it is the reason
     * approximations of this rule exist at all; they trade iterations for
     * it [8]. */
    memcpy(s->tau, s->rho, (size_t)s->nrow * sizeof *s->tau);
    jm_lu_ftran(&s->lu, s->tau, &s->work);
    jm_dse_update(s->nrow, s->dse, r, s->col, s->tau);
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    double q_value = nonbasic_value(s, q);
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= theta_primal * s->col[i];
    /* Position r now holds the entering variable, at its new value. */
    s->xb[r] = q_value + theta_primal;

    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
    s->where[leaving] = -1;
    s->basis[r] = q;
    s->status[q] = JM_BASIC;
    s->where[q] = r;

    /* Repair the factorization, or schedule a rebuild. */
    if (s->lu.n_updates >= REFACTOR_EVERY) {
        s->needs_refactor = true;
        return JAOS_OK;
    }
    jaos_status ust = jm_lu_update(&s->lu, r, s->raw, LU_UPDATE_TOL,
                                   &s->work);
    if (ust == JAOS_ERR_NUMERICAL || ust == JAOS_ERR_OUT_OF_MEMORY) {
        s->needs_refactor = true;
        return JAOS_OK;
    }
    return ust;
}

/* --------------------------------------------------------------------- */
/* Driver                                                                */
/* --------------------------------------------------------------------- */

static bool out_of_time(const sx *s)
{
    if (s->m->time_limit <= 0.0)
        return false;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return false;
    double elapsed = (double)(now.tv_sec - s->started.tv_sec) +
                     1e-9 * (double)(now.tv_nsec - s->started.tv_nsec);
    return elapsed >= s->m->time_limit;
}

static jaos_status run(sx *s, jaos_solve_status *out)
{
    bool ok = false;
    jaos_status st = refresh(s, &ok);
    if (st != JAOS_OK)
        return st;
    if (!ok) {
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->work_limit > 0 && s->work.units >= s->m->work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (s->iters > iter_cap) {
            /* A defect in JAOS, not a property of the model. Reporting it
             * as a solve outcome would put it in the same bucket as an
             * honestly hard problem. */
            jm_set_err(s->m, "internal iteration guard tripped after "
                             "%lld iterations; this is a JAOS defect",
                       (long long)s->iters);
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            st = refresh(s, &ok);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }

        bool below = false;
        int64_t r = price_row(s, &below);
        if (r < 0) {
            *out = leans_on_an_invented_bound(s) ? JAOS_SOLVE_UNBOUNDED
                                                 : JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        double theta_dual = 0.0;
        int64_t q = price_and_select(s, r, below, &theta_dual);
        if (q < 0) {
            /* No entering column can repair row r: the dual is unbounded,
             * so the primal has no feasible point. */
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;
        }

        st = pivot(s, r, q, below, theta_dual);
        if (st != JAOS_OK)
            return st;

        s->iters++;
    }
}

/* --------------------------------------------------------------------- */
/* Entry point                                                           */
/* --------------------------------------------------------------------- */

/* Solution buffers are kept across solves and only resized when the model
 * changes shape: branch and bound re-solves the same model thousands of
 * times, and an allocation per solve would be pure churn. */
static jaos_status ensure_solution_arrays(jaos_model *m)
{
    if (m->sol_col != nullptr)
        return JAOS_OK;
    m->sol_col     = jm_alloc_array(m->num_col, sizeof(double));
    m->sol_row     = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_dual    = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_redcost = jm_alloc_array(m->num_col, sizeof(double));
    if (!m->sol_col || !m->sol_row || !m->sol_dual || !m->sol_redcost)
        return JAOS_ERR_OUT_OF_MEMORY;
    return JAOS_OK;
}

static jaos_status publish(sx *s, jaos_solve_status status)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    m->solve_status = status;
    m->solve_work = s->work.units;
    m->solve_iters = s->iters;

    jaos_status st = ensure_solution_arrays(m);
    if (st != JAOS_OK)
        return st;

    if (status != JAOS_SOLVE_OPTIMAL) {
        /* Nothing to report; zero rather than leave the previous solve's
         * answer sitting where a caller might read it. */
        m->objective = 0.0;
        memset(m->sol_col, 0, (size_t)m->num_col * sizeof(double));
        memset(m->sol_row, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_dual, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_redcost, 0, (size_t)m->num_col * sizeof(double));
        return JAOS_OK;
    }

    /* Every entry below is written, so no pre-zeroing is needed. */
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = var_value(s, j);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row[i] = var_value(s, m->num_col + i);

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
    clock_gettime(CLOCK_MONOTONIC, &s.started);

    jaos_solve_status outcome;
    build_initial_basis(&s);
    st = run(&s, &outcome);
    if (st == JAOS_OK)
        st = publish(&s, outcome);
    sx_free(&s);
    return st;
}
