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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Draft tolerances (PLAN.md 2.6); frozen when the Netlib gate closes. They
 * are specified in scaled space and that is where they act: the solver
 * works on a scaled copy of the model throughout (see sx_init), and the
 * checker judges the original. */
constexpr double PRIMAL_TOL    = 1e-7;
constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */
/* How far a reduced cost may be pushed past feasible in exchange for a
 * better pivot. This is the width of the Harris window and the only place
 * dual feasibility is traded for anything; outside the ratio test the dual
 * simplex keeps it by construction. */
constexpr double DUAL_TOL      = 1e-7;
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

/* How far a carried weight may sit from the exact one before the whole set
 * is thrown away and restarted (PLAN 2.6). A draft, like the tolerances:
 * too tight and the method keeps discarding usable information, too loose
 * and it prices on numbers that no longer mean anything. A factor of ten
 * is well outside what rounding produces and well inside what a single
 * badly conditioned pivot can. */
constexpr double DSE_DRIFT = 10.0;

/* Refactorization interval. PLAN 2.5.5 also calls for stability triggers
 * (FTRAN/BTRAN residual checks); only the interval and the reactive
 * fallback on a failed update exist so far. */
constexpr int64_t REFACTOR_EVERY = 64;

/* Anti-stall cost perturbation (Q10). When grow15 stalls, a small
 * deterministic perturbation breaks degenerate cycles.
 * Strategy: detect degenerate steps (|theta_dual| < threshold) and
 * apply progressively larger perturbations when stuck in a cycle.
 * Perturbation is tracked in shift[] and removed at settlement. */
constexpr double PERTURB_BASE = 1e-10;   /* base perturbation magnitude */
constexpr int DEGENERATE_THRESHOLD = 1000; /* consecutive degenerate steps before perturb - tuned for grow22 */
constexpr double DEGENERATE_EPS = 1e-14;  /* threshold for detecting degenerate steps - stricter than before */

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
 * and the answer is the original problem's. If one is active, the loan was
 * reached, and what that means is classify_optimum's question rather than
 * this constant's.
 *
 * Koberstein [21] compares this against subproblem and cost-shifting
 * methods; those converge better on hard models, and replacing this is a
 * later step. What matters now is that a whole class of models becomes
 * solvable instead of refused.
 *
 * The value wants to be large enough to keep the loan out of the way and
 * small enough to stay numerically sane against the tolerances above; it
 * is a draft, like the tolerances themselves. What it is deliberately not
 * is load-bearing for any verdict — an optimum reached only because the
 * loan was too tight is refused rather than answered, and unboundedness is
 * proven against the model's own bounds. Sizing it is what makes a model
 * solvable, not what makes an answer true. */
constexpr double ARTIFICIAL_BOUND = 1e10;

/* A stop that is not a real limit, only a guard against a loop that fails
 * to terminate through a bug. Hitting it is a defect in JAOS, so it is
 * reported as a library error rather than as a solve outcome: a caller
 * must be able to tell "this model is hard" from "this code is wrong". */
constexpr int64_t ITER_SANITY_FACTOR = 200;

/* Which simplex method is currently running. */
enum { SX_PRIMAL = 0, SX_DUAL };

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    /* The matrix the solver actually works on: the model's values with the
     * scaling applied, sharing the model's sparsity pattern because
     * scaling never moves a nonzero. The model's own copy stays as loaded
     * — it is what the checker judges against, and a solver that could
     * rewrite it would be a solver marking its own homework. */
    double *av;              /* [num_nz] */

    /* Bounds and costs over all variables, likewise scaled: structurals
     * first, then the logicals that carry row activities. `cost` is the
     * working cost, which is the model's plus whatever has been shifted
     * into it; `shift` is the record of exactly that, so the loan can be
     * called in at the end. */
    double *lo, *up, *cost;
    double *shift;           /* [nvar] */

    jm_var_status *status;   /* [nvar] */
    int64_t *basis;          /* [nrow] variable occupying each position */
    int64_t *where;          /* [nvar] basis position, or -1 */

    /* Bounds JAOS invented to get a dual feasible start. A column is
     * caught by exactly one branch of the cost-sign test, so one array
     * says both whether a bound was lent and which side it went on — two
     * parallel flags would encode an invariant the types do not. It is
     * also what real_lower/real_upper undo the loan from. */
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

    /* The ratio test's candidate set, filled once per iteration: which
     * variables may enter (`cand`), how far each one's reduced cost is
     * from infeasibility (`rnum`) and how big its pivot would be
     * (`rden`). Kept apart from alpha because the eligibility rule is
     * solver state and the choice between the eligible is pure
     * arithmetic — jm_harris_pick sees only the second. */
    int64_t *cand;           /* [nvar] */
    double *rnum, *rden;     /* [nvar] */
    double *rrange;          /* [nvar] width of the box, or infinity */

    /* Refactorization buffers, grown once and reused: a refactorization
     * every REFACTOR_EVERY iterations should not also be an allocation. */
    int64_t *bs, *bi;
    double *bv;
    int64_t bi_cap, bv_cap;

    struct timespec started;
    int64_t iters;
    int phase;
    int64_t primal_iters;
    int64_t dual_iters;
    bool needs_refactor;

    /* Anti-stall state for grow15 (Q10) */
    int last_perturb_iter;
    int degenerate_count;       /* consecutive degenerate steps */
    int perturb_level;          /* current perturbation magnitude level */

    /* Crash basis: build a near-triangular initial basis with structural
     * columns. Enabled by default; only the simplest models gain nothing
     * from it. Set false to force the slack basis. */
    bool crash_enabled;

    /* Scratch for the Koberstein-style crash basis construction.
     * row_count[i] and col_count[j] hold sparsity counts of structural
     * columns remaining to be selected; row_done/col_done mark which ones
     * are already accounted for. crash_perm carries the selected column
     * order. Freed after build or never allocated when disabled. */
    int64_t *row_count;
    int64_t *col_count;
    int64_t *crash_perm;
    bool *row_done;
    bool *col_done;

    /* Has optimality been re-checked against a freshly computed point since
     * the last basis change? See the r < 0 branch in run(). */
    bool verified;
} sx;

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

static void sx_free(sx *s)
{
    free(s->av);
    free(s->lo); free(s->up); free(s->cost); free(s->shift);
    free(s->status); free(s->basis); free(s->where);
    free(s->xb); free(s->d); free(s->dse);
    free(s->col); free(s->raw); free(s->rho); free(s->tau); free(s->alpha);
    free(s->cand); free(s->rnum); free(s->rden); free(s->rrange);
    free(s->bs); free(s->bi); free(s->bv);
    free(s->row_count);
    free(s->col_count);
    free(s->crash_perm);
    free(s->row_done);
    free(s->col_done);
    free(s->fake);
    jm_lu_free(&s->lu);
    memset(s, 0, sizeof *s);
}

/* Sets up the scaled working copy the whole solve runs on.
 *
 * Scaling row i by rho_i and column j by gamma_j is a change of variable,
 * x_j = gamma_j * xhat_j, not an approximation: every factor is an exact
 * power of two (see docs/scaling.md), so applying one adds no rounding
 * error of its own and the scaled problem has exactly the solutions the
 * original has. What it buys is that the tolerances of PLAN 2.6 mean the
 * same thing on every row, which on a model whose coefficients span ten
 * orders of magnitude is the difference between a solve and a guess.
 *
 * The model as loaded is never touched. The answers are put back into its
 * units in publish(), and the checker reads the original.
 *
 * A caller who has already chosen a scaling keeps it; otherwise the
 * default is Curtis-Reid (PLAN 2.5.3). */
static jaos_status sx_init(sx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    jm_lu_init(&s->lu);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }

    s->av     = jm_alloc_array(m->num_nz, sizeof(double));
    s->lo     = jm_alloc_array(s->nvar, sizeof(double));
    s->up     = jm_alloc_array(s->nvar, sizeof(double));
    s->cost   = jm_calloc_array(s->nvar, sizeof(double));
    s->shift  = jm_calloc_array(s->nvar, sizeof(double));
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
    s->cand   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->rnum   = jm_alloc_array(s->nvar, sizeof(double));
    s->rden   = jm_alloc_array(s->nvar, sizeof(double));
    s->rrange = jm_alloc_array(s->nvar, sizeof(double));
    s->bs     = jm_alloc_array(s->nrow + 1, sizeof(int64_t));
    s->fake   = jm_calloc_array(s->nvar, sizeof *s->fake);

    if (!s->av || !s->lo || !s->up || !s->cost || !s->shift ||
        !s->status || !s->basis ||
        !s->where || !s->xb || !s->d || !s->dse || !s->col || !s->raw ||
        !s->rho || !s->tau || !s->alpha || !s->cand || !s->rnum ||
        !s->rden || !s->rrange || !s->bs || !s->fake) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    s->crash_enabled = false;
    s->phase         = SX_DUAL;

    const double *rho = m->row_scale, *gamma = m->col_scale;

    /* ahat_ij = rho_i * a_ij * gamma_j. */
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];

    /* A column's bounds are its own units divided out; its cost is what
     * keeps the objective the same number. A row's bounds move with the
     * row's factor, since that is what its activity was multiplied by.
     * Infinities survive all of it — every factor is finite and positive,
     * so no bound changes side. */
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    for (int64_t j = 0; j < s->ncol; j++) {
        s->lo[j] = m->col_lower[j] / gamma[j];
        s->up[j] = m->col_upper[j] / gamma[j];
        s->cost[j] = sigma * m->col_cost[j] * gamma[j];
    }
    for (int64_t i = 0; i < s->nrow; i++) {
        s->lo[s->ncol + i] = m->row_lower[i] * rho[i];
        s->up[s->ncol + i] = m->row_upper[i] * rho[i];
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

/* The bounds the model declared, as against the ones dual phase 1 lent.
 *
 * No copy is kept because none is needed: `lo` and `up` are written in
 * exactly two places — sx_init, from the model, and build_initial_basis,
 * where a loan overwrites one side — and a loan only ever replaced an
 * infinity, since having no bound on that side is what made the column
 * need one. So `fake` is enough to undo it. */
static double real_lower(const sx *s, int64_t v)
{
    return s->fake[v] == FAKE_LO ? -HUGE_VAL : s->lo[v];
}

static double real_upper(const sx *s, int64_t v)
{
    return s->fake[v] == FAKE_UP ? HUGE_VAL : s->up[v];
}

/* Scatters variable v's column of M into a dense vector. */
static void var_column(const sx *s, int64_t v, double *out)
{
    memset(out, 0, (size_t)s->nrow * sizeof *out);
    if (v < s->ncol) {
        const jaos_model *m = s->m;
        for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
            out[m->a_index[k]] = s->av[k];
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
        a += s->rho[m->a_index[k]] * s->av[k];
    jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                          JM_WORK_NONZERO);
    return a;
}

/* Koberstein-style dual crash basis [Koberstein 2008, §6.3].
 *
 * Builds a near-triangular initial basis by selecting structural columns
 * with favourable cost/sign patterns via a greedy Maros-style algorithm.
 * Only falls back to the slack basis if the crash fails (singular basis)
 * or is disabled. On success every basis position gets a structural or
 * logical column, and the DSE weights are initialised exactly.
 *
 * The algorithm:
 *   1. Count nonzeros per row and per structural column.
 *   2. Greedy triangular selection: repeatedly pick the remaining row with
 *      fewest nonzeros, choose its column by minimum fill-in, pivot it in.
 *   3. Fill remaining basis slots with logical columns.
 *   4. Compute exact DSE weights via m BTRAN solves.
 *   5. Return JAOS_OK so the caller calls refresh() to factor and compute
 *      the initial point.
 *
 * Returns JAOS_ERR_OUT_OF_MEMORY on allocation failure, JAOS_OK otherwise
 * (a singular basis is handled by falling back to the slack basis). */
static jaos_status build_crash_basis(sx *s)
{
    const int64_t nrow = s->nrow, ncol = s->ncol;
    const jaos_model *m = s->m;

    /* Ensure CSR mirror is available for row-major traversal. */
    jaos_status st = jm_model_ensure_rowwise(s->m);
    if (st != JAOS_OK)
        return st;

    /* ---- scratch allocation ---- */
    s->row_count = jm_calloc_array(nrow, sizeof(int64_t));
    s->col_count = jm_calloc_array(ncol, sizeof(int64_t));
    s->crash_perm = jm_alloc_array(nrow, sizeof(int64_t));
    s->row_done = jm_calloc_array(nrow, sizeof(bool));
    s->col_done = jm_calloc_array(ncol, sizeof(bool));
    if (!s->row_count || !s->col_count || !s->crash_perm ||
        !s->row_done || !s->col_done)
        return JAOS_ERR_OUT_OF_MEMORY;

    /* ---- step 1: count nonzeros per row and column ---- */
    int64_t *R = s->row_count;
    int64_t *C = s->col_count;

    for (int64_t j = 0; j < ncol; j++) {
        int64_t cnt = 0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            R[m->a_index[k]]++;
            cnt++;
        }
        C[j] = cnt;
    }

    /* ---- step 2: greedy triangular selection (Maros-style) ---- */
    int64_t *selected = s->crash_perm;   /* first nsel entries are the selected columns */
    int64_t nsel = 0;

    /* Precompute column scores for tie-breaking: |c_j| * (u_j - l_j) / ||A[:,j]||_1.
     * Wide bounds and large costs attract a column to the basis. */
    double *col_score = jm_alloc_array(ncol, sizeof(double));
    if (!col_score)
        return JAOS_ERR_OUT_OF_MEMORY;

    for (int64_t j = 0; j < ncol; j++) {
        double norm1 = 0.0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            norm1 += fabs(s->av[k]);

        double width = s->up[j] - s->lo[j];
        if (!isfinite(width)) {
            /* Free or semi-infinite: wide enough to never be the blocker. */
            col_score[j] = fabs(s->cost[j]) * 1e10 / (norm1 > 0.0 ? norm1 : 1.0);
        } else {
            col_score[j] = fabs(s->cost[j]) * fmax(width, 0.0) / (norm1 > 0.0 ? norm1 : 1.0);
        }
    }

    /* Greedy loop: pick the row with fewest remaining structural nonzeros,
     * then pick the column in that row with the smallest column count
     * (minimum fill-in), tie-breaking by the cost-quality score. */
    for (int64_t step = 0; step < nrow; step++) {
        /* Find remaining row with smallest R[i]. */
        int64_t best_row = -1;
        int64_t best_cnt = INT64_MAX;
        for (int64_t i = 0; i < nrow; i++) {
            if (s->row_done[i])
                continue;
            if (R[i] < best_cnt) {
                best_cnt = R[i];
                best_row = i;
            }
        }
        if (best_row < 0)
            break;

        if (best_cnt == 0) {
            /* Row has no remaining structural nonzeros: leave for a logical. */
            s->row_done[best_row] = true;
            continue;
        }

        /* Find the best column in this row using the CSR mirror. */
        int64_t best_col = -1;
        int64_t best_col_cnt = INT64_MAX;
        double best_col_score = -1.0;

        for (int64_t p = m->ar_start[best_row]; p < m->ar_start[best_row + 1]; p++) {
            int64_t j = m->ar_index[p];
            if (s->col_done[j])
                continue;
            int64_t cnt = C[j];
            double score = col_score[j];
            if (cnt < best_col_cnt || (cnt == best_col_cnt && score > best_col_score)) {
                best_col_cnt = cnt;
                best_col_score = score;
                best_col = j;
            }
        }

        if (best_col < 0) {
            /* No structural column left: leave for a logical. */
            s->row_done[best_row] = true;
            continue;
        }

        /* Record the selection and remove row + column from consideration.
         * Place the column directly at the row's basis position to preserve
         * triangularity (position best_row gets the column selected for it). */
        s->basis[best_row] = best_col;
        s->status[best_col] = JM_BASIC;
        s->where[best_col] = best_row;
        nsel++;
        s->row_done[best_row] = true;
        s->col_done[best_col] = true;

        /* Update row counts: column best_col's nonzeros are no longer available. */
        for (int64_t p = m->a_start[best_col]; p < m->a_start[best_col + 1]; p++) {
            int64_t i = m->a_index[p];
            if (!s->row_done[i])
                R[i]--;
        }
    }

    free(col_score);

    /* ---- step 3: fill remaining basis positions with logicals ----
     * Structural columns were placed at their row positions during selection.
     * Rows that didn't get a structural column need a logical (slack). */
    for (int64_t i = 0; i < nrow; i++) {
        if (s->basis[i] == 0 && s->status[0] != JM_BASIC) {
            /* This position wasn't filled - add logical for row i */
            int64_t v = s->ncol + i;
            s->basis[i] = v;
            s->status[v] = JM_BASIC;
            s->where[v] = i;
        }
    }
    /* Count of structural columns selected */
    int64_t nsel = 0;
    for (int64_t i = 0; i < nrow; i++)
        if (s->basis[i] < s->ncol)
            nsel++;
    /* Mark non-basic structural columns. */
    for (int64_t j = 0; j < ncol; j++) {
        if (s->status[j] != JM_BASIC)
            s->where[j] = -1;
    }

    /* Set nonbasic status for all structural columns not in the basis,
     * using the same cost-sign logic as build_initial_basis. */
    for (int64_t j = 0; j < ncol; j++) {
        if (s->status[j] == JM_BASIC)
            continue;
        bool has_lo = isfinite(s->lo[j]);
        bool has_up = isfinite(s->up[j]);

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
            s->status[j] = JM_AT_LOWER;
        } else if (has_up) {
            s->status[j] = JM_AT_UPPER;
        } else {
            s->status[j] = JM_FREE;
        }
    }

    /* Set DSE weights to a reasonable default — the exact values will
     * be computed by the caller after factorizing the basis. */
    for (int64_t i = 0; i < nrow; i++)
        s->dse[i] = 1.0;

    return JAOS_OK;
}

/* The slack basis: every logical basic, every structural pinned to the
 * bound that makes its reduced cost feasible. With B = -I this factors by
 * inspection.
 *
 * A structural whose cost asks for a bound it does not have gets an
 * artificial one, which is dual phase 1 (see ARTIFICIAL_BOUND above). The
 * loan is recorded so the outcome can be read honestly afterwards.
 *
 * If crash basis is enabled, build a near-triangular basis from structural
 * columns instead. Falls back to the slack basis if the crash fails. */
static void build_initial_basis(sx *s)
{
    if (s->crash_enabled) {
        jaos_status st = build_crash_basis(s);
        if (st == JAOS_OK)
            return;
        /* Fall through to the slack basis on failure. */
    }
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
                s->bv[p] = s->av[k];
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
                rhs[m->a_index[k]] -= s->av[k] * val;
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
 * two violations is the only candidate. *violation carries the size of the
 * chosen one out: the ratio test spends it. */
static int64_t price_row(sx *s, bool *below, double *violation)
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
            *violation = viol;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return best;
}

/* Has a carried weight lost touch with the truth? A weight is the squared
 * norm of a row of the inverse of a nonsingular matrix, so anything that
 * is not finite and positive is already wrong, whichever side it sits on
 * of any factor. */
static bool weight_drifted(double carried, double exact, double factor)
{
    if (!isfinite(carried) || carried <= 0.0)
        return true;
    if (!isfinite(exact) || exact <= 0.0)
        return true;
    return carried > exact * factor || carried * factor < exact;
}

/* The weight recurrence. Row i of the new B^-1 is row i minus
 * (alpha_i / alpha_r) times row r, and row r becomes row r over alpha_r;
 * expanding the squared norms of those two statements gives everything
 * below, with tau_i = rho_i . rho_r supplying the cross term. The exact
 * weight and the restart it can trigger are documented in the header,
 * which is also where the exported-for-testing rationale lives. */
void jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau,
                   double exact_r, double drift_factor)
{
    if (weight_drifted(w[r], exact_r, drift_factor)) {
        for (int64_t i = 0; i < n; i++)
            w[i] = 1.0;
        return;
    }
    w[r] = exact_r;

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

/* Bound flipping [19][1]. A candidate with two finite bounds does not have
 * to stop the dual step. Swapping it to its other bound keeps it dual
 * feasible — its reduced cost crosses zero exactly as the variable crosses
 * to the side where the opposite sign is what feasibility means — and it
 * moves row r towards its bound by |alpha| times the width of the box.
 * While that still leaves the row short, the step can pass the breakpoint
 * and carry on, so one long iteration replaces a run of short ones.
 *
 * `remaining` is the row's violation, spent down by each swap. The walk
 * pops candidates in ascending ratio order with a linear scan per swap
 * rather than sorting the whole set once: the number of swaps is small in
 * practice and the sort would be paid on every iteration. That is a guess
 * about the common case, and PLAN 2.11 records it as one.
 *
 * Retired candidates are swapped to the tail: [0, live) are still in play
 * and [live, n) are to be flipped. A returned zero means every candidate
 * was passed and the row is still short — no step blocks it, the dual is
 * unbounded, and the primal has no feasible point.
 *
 * A fixed column, whose bounds coincide, falls out of this for free: its
 * box has no width, so passing it costs nothing and it never blocks a step
 * it could not constrain anyway. */
static int64_t bfrt_walk(sx *s, int64_t n, double remaining)
{
    int64_t live = n;

    while (live > 0) {
        int64_t k = 0;
        double least = HUGE_VAL;
        for (int64_t j = 0; j < live; j++) {
            double t = s->rnum[j] / s->rden[j];
            if (t < least) {
                least = t;
                k = j;
            }
        }
        jm_work_add(&s->work, live * JM_WORK_NONZERO);

        double width = s->rrange[k];
        if (!isfinite(width))
            break;                     /* no other bound to swap to */
        if (!(remaining - s->rden[k] * width > 0.0))
            break;                     /* swapping would overshoot: it blocks */
        remaining -= s->rden[k] * width;

        live--;
        int64_t ci = s->cand[k];
        double a = s->rnum[k], b = s->rden[k], c = s->rrange[k];
        s->cand[k]   = s->cand[live];   s->cand[live]   = ci;
        s->rnum[k]   = s->rnum[live];   s->rnum[live]   = a;
        s->rden[k]   = s->rden[live];   s->rden[live]   = b;
        s->rrange[k] = s->rrange[live]; s->rrange[live] = c;
    }
    return live;
}

/* Swaps the retired candidates bound to bound and moves the primal point
 * with them. x_B = -B^-1 N x_N, so a nonbasic moving by delta moves the
 * basics by -B^-1 M_v delta; the moves are accumulated into one column and
 * transformed once, because the cost of a solve is in the solve and not in
 * the vector it is given. */
static void apply_flips(sx *s, int64_t at, int64_t n)
{
    /* Borrowed: pivot() overwrites col with the entering column before
     * reading it, and this is the last use before that. */
    double *rhs = s->col;
    memset(rhs, 0, (size_t)s->nrow * sizeof *rhs);

    for (int64_t k = at; k < n; k++) {
        int64_t v = s->cand[k];
        double from = nonbasic_value(s, v);
        s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                   : JM_AT_LOWER;
        double delta = nonbasic_value(s, v) - from;
        if (delta == 0.0)
            continue;                  /* a fixed column: nothing moved */

        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t p = m->a_start[v]; p < m->a_start[v + 1]; p++)
                rhs[m->a_index[p]] += s->av[p] * delta;
            jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                                  JM_WORK_NONZERO);
        } else {
            rhs[v - s->ncol] -= delta;   /* column is -e_i */
            jm_work_add(&s->work, JM_WORK_NONZERO);
        }
    }

    jm_lu_ftran(&s->lu, rhs, &s->work);
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= rhs[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
}

/* The ratio test: who may enter, how far the step may go, and which
 * candidate takes it.
 *
 * The eligible set is a matter of signs: dx_B[r] = -a * dx_v, and dx_v's
 * direction is fixed by which bound v sits at, so only some entries push
 * row r the way it has to go. That part is solver state and lives here.
 * How far the step reaches is bfrt_walk, and which of the candidates still
 * standing wins is jm_harris_pick — arithmetic, both of them, and neither
 * needs to know what a basis is.
 *
 * The numerator is the distance from v's reduced cost to infeasibility,
 * not its magnitude: they differ when d has already drifted a hair past
 * zero, and reading such a cost as "nearly blocking" rather than "already
 * blocking" is what turns a rounding error into a step in the wrong
 * direction. Clamped at zero, an already-infeasible cost blocks at once,
 * and the step that follows repairs it exactly.
 *
 * The flips are applied here rather than reported outwards: they are part
 * of the step, and a caller holding a half-taken step is a caller who can
 * forget to finish it. */
static int64_t dual_ratio_test(sx *s, bool below, double violation,
                               double *theta_out)
{
    int64_t n = 0;

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        double a = s->alpha[v];
        if (fabs(a) < PIVOT_MIN)
            continue;

        bool ok;
        double dist;
        if (s->status[v] == JM_AT_LOWER) {
            ok = below ? (a < 0.0) : (a > 0.0);
            dist = s->d[v];          /* must stay non-negative */
        } else if (s->status[v] == JM_AT_UPPER) {
            ok = below ? (a > 0.0) : (a < 0.0);
            dist = -s->d[v];         /* must stay non-positive */
        } else {
            ok = true;               /* free: may move either way */
            dist = 0.0;              /* and must stay at zero */
        }
        if (!ok)
            continue;

        s->cand[n] = v;
        s->rnum[n] = dist > 0.0 ? dist : 0.0;
        s->rden[n] = fabs(a);
        s->rrange[n] = s->up[v] - s->lo[v];
        n++;
    }
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);

    if (n == 0)
        return -1;

    int64_t live = bfrt_walk(s, n, violation);
    if (live == 0)
        return -1;   /* nothing blocks the step; the model is infeasible */

    int64_t k = jm_harris_pick(live, s->rnum, s->rden, s->cand, DUAL_TOL);
    jm_work_add(&s->work, 2 * live * JM_WORK_NONZERO);
    int64_t best = s->cand[k];

    if (live < n)
        apply_flips(s, live, n);

    /* The step that lands the winner's reduced cost exactly on zero. Every
     * other candidate inside the window ends at worst DUAL_TOL past
     * feasible, which is the whole of what Harris trades away. */
    *theta_out = s->d[best] / s->alpha[best];
    return best;
}

/* Harris' window and the best-conditioned pivot inside it. Documented in
 * the header, which is also where the reachable-from-outside rationale
 * lives. */
int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       const int64_t *cand, double dual_tol)
{
    if (n <= 0)
        return -1;

    double window = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        double t = (num[k] + dual_tol) / den[k];
        if (t < window)
            window = t;
    }

    int64_t best = 0;
    double best_den = 0.0;
    for (int64_t k = 0; k < n; k++) {
        if (num[k] / den[k] <= window) {
            if (den[k] > best_den + 1e-15) {
                best_den = den[k];
                best = k;
            } else if (cand[k] < cand[best]) {
                best_den = den[k];
                best = k;
            }
        }
    }
    return best;
}

/* Builds row r of B^-1 M and picks the entering variable from it. May also
 * swap nonbasic variables between their bounds on the way — see
 * dual_ratio_test — which moves the primal point but not the basis. */
static int64_t price_and_select(sx *s, int64_t r, bool below,
                                double violation, double *theta_dual)
{
    memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
    s->rho[r] = 1.0;
    jm_lu_btran(&s->lu, s->rho, &s->work);

    for (int64_t v = 0; v < s->nvar; v++)
        s->alpha[v] = s->status[v] == JM_BASIC ? 0.0 : price_entry(s, v);

    return dual_ratio_test(s, below, violation, theta_dual);
}

/* Cost shifting [1]. Puts one nonbasic reduced cost back on the feasible
 * side by moving its cost there, and writes down what it moved by.
 *
 * The Harris window is the one place the method spends dual feasibility,
 * and until now nothing bought it back: a reduced cost pushed a tolerance
 * past zero stayed there, and the next iteration could push it further.
 * That is worse than untidy. The ratio test reads a cost already past zero
 * as blocking immediately, and if such a candidate is the one chosen, the
 * step it computes runs backwards — a dual step with the wrong sign, sized
 * by however small the pivot happened to be.
 *
 * Shifting the cost of a *nonbasic* variable changes that variable's
 * reduced cost and nothing else: the duals come from the basic costs
 * alone. So the repair is exactly local, and the loan is recorded to be
 * repaid in settle_shifts. */
static void shift_to_feasible(sx *s, int64_t v)
{
    double need = 0.0;
    if (s->status[v] == JM_AT_LOWER) {
        if (s->d[v] < 0.0)
            need = -s->d[v];        /* must stay non-negative */
    } else if (s->status[v] == JM_AT_UPPER) {
        if (s->d[v] > 0.0)
            need = -s->d[v];        /* must stay non-positive */
    } else if (s->status[v] == JM_FREE) {
        need = -s->d[v];            /* must stay at zero */
    } else {
        return;                     /* basic: its reduced cost is zero */
    }
    if (need == 0.0)
        return;

    s->cost[v] += need;
    s->shift[v] += need;
    s->d[v] = 0.0;
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

    /* Reduced costs shift by the dual step along the pricing row, and any
     * that the window pushed past feasible are bought back on the spot. */
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC || v == q)
            continue;
        s->d[v] -= theta_dual * s->alpha[v];
        shift_to_feasible(s, v);
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

    /* One weight is known exactly at no cost: rho is row r of B^-1, so its
     * squared norm is the very quantity the recurrence has been estimating
     * for that row. Handing it over lets the step start from a true value,
     * and lets the recurrence be caught when it has drifted away from one. */
    double exact = 0.0;
    for (int64_t i = 0; i < s->nrow; i++)
        exact += s->rho[i] * s->rho[i];

    jm_dse_update(s->nrow, s->dse, r, s->col, s->tau, exact, DSE_DRIFT);
    jm_work_add(&s->work, 2 * s->nrow * JM_WORK_NONZERO);

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

    /* The leaving variable's reduced cost is minus the dual step, which is
     * feasible for the bound it left to — unless the step itself came out
     * of a cost that was already past zero, which is the case the shifting
     * exists to make impossible. Checked rather than assumed. */
    shift_to_feasible(s, leaving);

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
/* Settling up                                                           */
/* --------------------------------------------------------------------- */

/* A nonbasic variable whose reduced cost came out on the wrong side can
 * sometimes be put right for nothing: sitting at its other bound, the
 * opposite sign is what feasibility means, so swapping it over fixes the
 * condition exactly. What it does not fix is the primal — the variable
 * moves by the whole width of its box, and the basics move with it — so
 * the swap is only taken when it leaves every basic inside its bounds.
 *
 * Any wrong sign at all is worth swapping, with no tolerance to clear
 * first, because a swap that passes the primal test cannot make the answer
 * worse: the objective changes by the reduced cost times the move, and the
 * two have opposite signs in both directions, so it can only go down. A
 * threshold here would decline free improvements to avoid churn that costs
 * nothing.
 *
 * A column carrying an invented bound is left alone, and that exclusion is
 * load-bearing twice over. Parking one on a bound the model never declared
 * would publish a value nothing authorised; and it would plant exactly the
 * evidence classify_optimum reads immediately after this runs, which is a
 * solver arranging the proof of its own verdict.
 *
 * Whatever cannot be repaired this way stays in the reported reduced
 * costs, where the independent checker will see it. Removing it properly
 * means moving a nonbasic variable until something blocks, which is a
 * primal simplex iteration, and there is no primal simplex before M6. */
static void repair_dual_infeasibility(sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC || s->fake[v] != NOT_FAKE)
            continue;

        double to;
        if (s->status[v] == JM_AT_LOWER && s->d[v] < 0.0)
            to = s->up[v];
        else if (s->status[v] == JM_AT_UPPER && s->d[v] > 0.0)
            to = s->lo[v];
        else
            continue;
        if (!isfinite(to))
            continue;

        double delta = to - nonbasic_value(s, v);
        var_column(s, v, s->col);
        for (int64_t i = 0; i < s->nrow; i++)
            s->col[i] *= delta;
        jm_lu_ftran(&s->lu, s->col, &s->work);

        bool safe = true;
        for (int64_t i = 0; i < s->nrow; i++) {
            double x = s->xb[i] - s->col[i];
            int64_t b = s->basis[i];
            if (x < s->lo[b] - PRIMAL_TOL || x > s->up[b] + PRIMAL_TOL) {
                safe = false;
                break;
            }
            /* The bounds just tested include the invented ones, and a
             * basic left sitting *on* one would be published at a value
             * the model never allowed. The swap that would do that is
             * refused; no test constructs this, it takes a box ~1e10
             * wide, but an answer must not depend on nobody ever
             * building one. */
            if ((s->fake[b] == FAKE_LO && x <= s->lo[b] + PRIMAL_TOL) ||
                (s->fake[b] == FAKE_UP && x >= s->up[b] - PRIMAL_TOL)) {
                safe = false;
                break;
            }
        }
        jm_work_add(&s->work, 2 * s->nrow * JM_WORK_NONZERO);
        if (!safe)
            continue;

        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= s->col[i];
        s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                   : JM_AT_LOWER;
    }
}

/* Calls in every cost the solve borrowed, and recomputes the duals from
 * the model's own costs so that what is published belongs to the problem
 * that was asked about rather than to the one that was convenient. Then
 * repairs what the true costs turn out to leave infeasible. */
static void settle_shifts(sx *s)
{
    bool any = false;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->shift[v] == 0.0)
            continue;
        s->cost[v] -= s->shift[v];
        s->shift[v] = 0.0;
        any = true;
    }
    if (!any)
        return;

    compute_duals(s);
    repair_dual_infeasibility(s);
}

/* --------------------------------------------------------------------- */
/* Anti-stall: deterministic cost perturbation (Q10)                   */
/* --------------------------------------------------------------------- */

/* Deterministic hash for anti-stall perturbation. Maps iteration count
 * to a perturbation pattern that is the same across runs (D8). */
static double perturb_hash(int64_t seed, int64_t v)
{
    uint64_t x = (uint64_t)seed * 0x9e3779b97f4a7c15ULL + (uint64_t)v * 0x6a09e667f3bcc909ULL;
    x = (x ^ (x >> 33)) * 0xff51afd7ed558ccdULL;
    x = (x ^ (x >> 33)) * 0xc4ceb9fe1a85ec53ULL;
    x = x ^ (x >> 33);
    return (double)((int64_t)(x % 2001)) / 1000.0 - 1.0;
}

/* Apply anti-stall perturbation when degenerate steps are detected.
 * Called after each pivot with the dual step size. */
static void check_and_perturb(sx *s, double theta_dual)
{
    /* A "degenerate" step is one with very small dual step size.
     * This indicates the entering variable had near-zero reduced cost,
     * which is the hallmark of degenerate cycling in dual simplex.
     * Use DEGENERATE_EPS (1e-14) for stricter detection than before (1e-12). */
    bool degenerate = fabs(theta_dual) < DEGENERATE_EPS;
    
    if (degenerate) {
        s->degenerate_count++;
    } else {
        /* Reset counter on non-degenerate step */
        s->degenerate_count = 0;
        return;
    }
    
    /* Only perturb if we've seen many consecutive degenerate steps */
    if (s->degenerate_count < DEGENERATE_THRESHOLD)
        return;
    
    /* Reset counter and bump perturbation level */
    s->degenerate_count = 0;
    
    /* Cap perturb_level to prevent overflow and excessive perturbation */
    if (s->perturb_level < 20)
        s->perturb_level++;
    
    /* Apply perturbation with magnitude based on perturb_level.
     * Use gradual growth (1.5x per level) instead of doubling to avoid
     * overly aggressive perturbation that can break grow22. */
    double eps = PERTURB_BASE * pow(1.5, s->perturb_level);
    if (eps > 1e-8) eps = 1e-8;  /* cap below DUAL_TOL (1e-7) with margin */
    
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        double delta = eps * perturb_hash(s->iters, v);
        s->cost[v] += delta;
        s->shift[v] += delta;
    }
    
    /* Recompute duals after perturbation */
    compute_duals(s);
    s->verified = false;
}

/* --------------------------------------------------------------------- */
/* Reading the verdict                                                   */
/* --------------------------------------------------------------------- */

/* Is this column still being held back by a bound JAOS invented?
 *
 * The bounded problem is at its optimum, and it differs from the original
 * only where a bound was lent. So the two can only disagree where a loan is
 * what stopped the objective, and the evidence for that is narrow: a
 * *nonbasic* column resting on its invented bound whose reduced cost still
 * points outwards. Every other nonbasic is held by a bound the model
 * actually declared. A basic variable is not evidence at all — its reduced
 * cost is zero by definition, so nothing improves by moving it, and one
 * sitting on an invented bound is degeneracy rather than a blocked
 * objective.
 *
 * "Points outwards" is measured against DUAL_TOL, the same tolerance the
 * rest of the solve uses to decide whether a reduced cost is zero. Reading
 * a cost this solver calls zero as a direction of improvement would be a
 * verdict its own arithmetic disagrees with.
 *
 * The reduced costs are the model's own: this runs after settle_shifts has
 * called in every borrowed cost, so `d` prices the problem that was asked
 * about rather than the one the ratio test found convenient. */
static bool held_by_an_invented_bound(const sx *s, int64_t j)
{
    if (s->fake[j] == FAKE_LO)
        return s->status[j] == JM_AT_LOWER && s->d[j] > DUAL_TOL;
    if (s->fake[j] == FAKE_UP)
        return s->status[j] == JM_AT_UPPER && s->d[j] < -DUAL_TOL;
    return false;
}

/* Would the objective actually run away if that bound were lifted?
 *
 * Letting column j leave its invented bound by t moves the basics along
 * dx_B = -B^-1 M_j dx_j, so the point travels a straight line and the
 * objective falls at a constant rate along it. The line is a ray of the
 * *original* problem exactly when no basic runs into a bound the model
 * itself declared — lent bounds do not count, and the one being lifted is
 * infinite on that side by construction, which is why it was lent.
 *
 * This is what makes the verdict independent of ARTIFICIAL_BOUND. Resting
 * on a lent bound says only that the loan was reached; a ray says the
 * objective has nowhere to stop, and that is a statement about the model.
 *
 * An entry below PIVOT_MIN is not treated as a blocker, on the grounds the
 * ratio test already refuses one as a pivot: it cannot be told apart from
 * the zero exact arithmetic would have produced. The line has to be drawn
 * somewhere and neither side is free — honouring noise refuses the verdict
 * on models that genuinely run away, ignoring a true tiny entry claims one
 * where a very distant bound really does block — so it is drawn where this
 * solver already draws it rather than at a new number of its own. */
static bool improves_without_limit(sx *s, int64_t j)
{
    var_column(s, j, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    /* dx_j leaves a lower loan downwards and an upper loan upwards, and
     * dx_B = -B^-1 M_j dx_j carries that sign across. */
    const double sgn = (s->fake[j] == FAKE_LO) ? 1.0 : -1.0;

    bool unlimited = true;
    for (int64_t i = 0; i < s->nrow; i++) {
        double step = sgn * s->col[i];
        if (fabs(step) < PIVOT_MIN)
            continue;
        int64_t b = s->basis[i];
        double limit = step > 0.0 ? real_upper(s, b) : real_lower(s, b);
        if (isfinite(limit)) {
            unlimited = false;
            break;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return unlimited;
}

/* The verdict on a point the bounded problem calls optimal.
 *
 * Three outcomes, and the third is the honest one. If some column can leave
 * its invented bound along a ray, the model is unbounded and that is now
 * proven rather than inferred from where a variable came to rest. If no
 * column is held by a loan at all, the loans never mattered and the answer
 * is the original problem's.
 *
 * What is left is a column the objective wants to push further, stopped by
 * a real constraint rather than by infinity: the true optimum lies past the
 * bound phase 1 lent, and this method cannot reach it. Reaching it means
 * lifting the loan and re-solving, and the degenerate case of that — a
 * basic already pressed against a real bound in the ray's direction — needs
 * a primal pivot, which does not exist before M6. So the solve refuses out
 * loud instead. That is the whole change in kind: this used to be reported
 * as UNBOUNDED, silently and wrongly, on a model with a perfectly good
 * finite optimum. */
static jaos_solve_status classify_optimum(sx *s)
{
    int64_t blocked = -1;

    for (int64_t j = 0; j < s->ncol; j++) {
        if (!held_by_an_invented_bound(s, j))
            continue;
        if (improves_without_limit(s, j))
            return JAOS_SOLVE_UNBOUNDED;
        if (blocked < 0)
            blocked = j;
    }

    if (blocked < 0)
        return JAOS_SOLVE_OPTIMAL;

    jm_set_err(s->m, "column %lld improves past the bound dual phase 1 lent "
                     "it, and a constraint stops it short of infinity: the "
                     "optimum is finite but lies beyond the reach of this "
                     "phase 1", (long long)blocked);
    return JAOS_SOLVE_NUMERICAL_ERROR;
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

    /* Initialize anti-stall tracking */
    s->last_perturb_iter = 0;

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
        double violation = 0.0;
        int64_t r = price_row(s, &below, &violation);
        if (r < 0) {
            /* Nothing violates a bound — but the values that statement was
             * read off are carried, not computed. x_B is updated in place
             * by every pivot and the factorization is patched rather than
             * rebuilt, so both drift, and the drift is invisible from the
             * inside: the test that would notice it is the one being run.
             * A solve can therefore stop on numbers that no basis supports,
             * and it stops precisely when they are wrong in the direction
             * of looking feasible.
             *
             * So a declaration of optimality is not accepted on carried
             * numbers. Recompute the point from a fresh factorization and
             * price it again; only a second opinion, taken from arithmetic
             * that owes nothing to the first, ends the solve. If the fresh
             * numbers do violate something, the loop simply carries on and
             * repairs it — the iterations that the drift was hiding.
             *
             * This is PLAN 2.5.5's stability trigger arriving through the
             * back door. It fires once per solve rather than watching a
             * residual every iteration, which is the cheap half; whether
             * the other half is needed is a question for instances, not for
             * argument. The cost is one refactorization per solve, and the
             * work counter bills it (D16). */
            if (!s->verified) {
                st = refresh(s, &ok);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                s->verified = true;
                continue;
            }
            /* Optimal for the problem as bounded. Whether that is also the
             * original's answer is classify_optimum's question, and it is
             * asked once the borrowed costs have been called in. */
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        double theta_dual = 0.0;
        int64_t q = price_and_select(s, r, below, violation, &theta_dual);
        if (q < 0) {
            /* No entering column can repair row r: the dual is unbounded,
             * so the primal has no feasible point. */
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;
        }

        /* The basis is about to change, so any verification of the point
         * it implied is spent. */
        s->verified = false;
        st = pivot(s, r, q, below, theta_dual);
        if (st != JAOS_OK)
            return st;

        s->iters++;

        /* Anti-stall: check for degenerate steps and apply perturbation if needed.
         * A degenerate step has |theta_dual| < DEGENERATE_EPS (1e-14), indicating near-zero
         * reduced cost and potential cycling. Perturb costs to break the cycle.
         * Applied AFTER the pivot so the state remains consistent. */
        check_and_perturb(s, theta_dual);
        if (s->phase == SX_PRIMAL)
            s->primal_iters++;
        else
            s->dual_iters++;
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
    if (m->sol_col != nullptr && m->sol_row != nullptr &&
        m->sol_dual != nullptr && m->sol_redcost != nullptr)
        return JAOS_OK;

    /* All four or none. A partial set — left behind when one of these
     * allocations failed on an earlier solve — must not read as "already
     * there", or this publish writes through the missing ones. */
    free(m->sol_col);     m->sol_col = nullptr;
    free(m->sol_row);     m->sol_row = nullptr;
    free(m->sol_dual);    m->sol_dual = nullptr;
    free(m->sol_redcost); m->sol_redcost = nullptr;

    m->sol_col     = jm_alloc_array(m->num_col, sizeof(double));
    m->sol_row     = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_dual    = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_redcost = jm_alloc_array(m->num_col, sizeof(double));
    if (!m->sol_col || !m->sol_row || !m->sol_dual || !m->sol_redcost) {
        free(m->sol_col);     m->sol_col = nullptr;
        free(m->sol_row);     m->sol_row = nullptr;
        free(m->sol_dual);    m->sol_dual = nullptr;
        free(m->sol_redcost); m->sol_redcost = nullptr;
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    return JAOS_OK;
}

static jaos_status publish(sx *s, jaos_solve_status status)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    m->solve_status = status;
    m->solve_iters = s->iters;
    m->solve_primal_iters = s->primal_iters;
    m->solve_dual_iters   = s->dual_iters;
    /* The work snapshot is taken at the *end* of each path below, not
     * here: publishing itself runs a kernel (the BTRAN for the duals),
     * and a counter that reported everything except the last thing it
     * did would be lying by one solve (D16). */

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
        m->solve_work = s->work.units;
        return JAOS_OK;
    }

    /* Out of the scaled copy and back into the model's own units. A column
     * carries its factor, a row activity divides its own out; the duals go
     * the other way, because a dual is a rate per unit of the thing it
     * prices. Every entry is written, so no pre-zeroing is needed. */
    const double *rho = m->row_scale, *gamma = m->col_scale;

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = gamma[j] * var_value(s, j);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row[i] = var_value(s, m->num_col + i) / rho[i];

    /* y = B^-T c_B, then undo the internal minimisation. */
    double *y = s->rho;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_dual[i] = sigma * y[i] * rho[i];
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_redcost[j] = sigma * s->d[j] / gamma[j];

    double obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++)
        obj += m->col_cost[j] * m->sol_col[j];
    m->objective = obj;
    m->solve_work = s->work.units;
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

    if (s.crash_enabled) {
        st = build_crash_basis(&s);
        if (st == JAOS_OK) {
            /* Factor so we can compute exact DSE weights. */
            st = refactorize(&s);
            if (st == JAOS_OK && s.lu.rank != s.nrow)
                st = JAOS_ERR_NUMERICAL;   /* singular; fall back */
        }
        if (st == JAOS_OK) {
            /* ---- exact DSE weight initialization ---- */
            /* m BTRAN solves: ||e_i^T B^-1||^2 for each basis row. */
            for (int64_t i = 0; i < s.nrow; i++) {
                memset(s.rho, 0, (size_t)s.nrow * sizeof(double));
                s.rho[i] = 1.0;
                jm_lu_btran(&s.lu, s.rho, &s.work);
                double norm2 = 0.0;
                for (int64_t k = 0; k < s.nrow; k++)
                    norm2 += s.rho[k] * s.rho[k];
                s.dse[i] = norm2 > DSE_MIN ? norm2 : DSE_MIN;
            }
        }
        if (st != JAOS_OK) {
            /* Crash failed — build the ordinary slack basis. */
            build_initial_basis(&s);
        }
    } else {
        build_initial_basis(&s);
    }
    st = run(&s, &outcome);
    if (st == JAOS_OK) {
        /* Settle first, then judge. The verdict turns on reduced costs, so
         * it has to read the model's own and not the shifted ones the
         * ratio test worked with. */
        if (outcome == JAOS_SOLVE_OPTIMAL) {
            settle_shifts(&s);
            outcome = classify_optimum(&s);
        }
        st = publish(&s, outcome);
    }
    sx_free(&s);
    return st;
}

void jaos_get_iter_counts(const jaos_model *m,
    int64_t *primal, int64_t *dual)
{
    if (!m) {
        if (primal) *primal = 0;
        if (dual)   *dual   = 0;
        return;
    }
    if (primal) *primal = m->solve_primal_iters;
    if (dual)   *dual   = m->solve_dual_iters;
}