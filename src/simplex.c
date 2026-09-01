/* Dual simplex with bounds.
 *
 * The problem is held as M z = 0 with M = [A | -I] and z = [x; s]: every
 * row gets a logical variable carrying its activity, so a row bound and a
 * column bound are the same kind of object and there is one code path
 * instead of four. A basis is m columns of M; the nonbasic variables are
 * pinned to bounds, which is what makes the basis determine a point.
 *
 * Pricing is dual steepest edge [8] and phase 1 is by artificial bounds.
 *
 * Sign conventions:
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

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Specified in scaled space, where they act: the solver works on a scaled
 * copy of the model (see sx_init), and the checker judges the original. */
constexpr double PRIMAL_TOL    = 1e-7;
constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */
/* On top of PIVOT_MIN, in the two primal ratio tests only: how far an entry
 * of `B^-1 M_q` has to stand above the rounding of the solve that produced
 * it, in multiples of one ulp of that column's largest entry. A row below it
 * is dropped from the candidate list, so it neither pivots nor blocks
 * (D207, D212). Dimensionless, so it belongs to neither space;
 * the threshold it builds is in scaled space, with `s->col`. Swept in
 * `bench/measurements/02-122/`. */
constexpr double PIVOT_MARGIN  = 1.0;
/* The width of the Harris window in the two PRIMAL ratio tests, as a multiple
 * of `s->primal_tol` — the per-model field and not `PRIMAL_TOL`, so a model
 * that tightens the tolerance tightens the window with it. The phase-1
 * argument in `docs/research/harris-primal.md` bounds the width above by that
 * tolerance, and `primal_pick` asserts it on the widened value. 0.5 is a power
 * of two, so the product is exact for every normal tolerance and no
 * contraction can round it differently; otherwise `-ffp-contract=off` would be
 * the only thing holding the window's bits fixed across architectures. The
 * seven-setting sweep, the plateau it found and the reason the value is argued
 * rather than fitted are D213's (`bench/measurements/02-127/`). */
constexpr double PRIMAL_HARRIS_DELTA = 0.5;
/* The width of the Harris window, and what the solve calls zero for a
 * reduced cost (D174, D184); per-model override is D64. */
constexpr double DUAL_TOL      = 1e-9;
constexpr double LU_PIVOT_TOL  = 0.1;    /* Markowitz threshold */
constexpr double LU_UPDATE_TOL = 1e-9;

/* Floor on a steepest-edge weight: the recurrence subtracts and can cancel
 * a small true value to zero or below. A guard, not a knob. */
constexpr double DSE_MIN = 1e-12;

/* How far a carried weight may sit from the exact one before the whole set
 * is thrown away and restarted (PLAN 2.6). */
constexpr double DSE_DRIFT = 10.0;

/* When the pricing row is read through its pattern instead of in full. A
 * divisor rather than a fraction so the crossover can be swept (D40). */
constexpr int64_t SPARSE_ALPHA_DEN = 4;

/* The same question for `rho`, and it needed asking separately (D43). */
constexpr int64_t SPARSE_RHO_DEN = 4;

/* The same question for the entering column's FTRAN (D44, D45). */
constexpr int64_t SPARSE_COL_DEN = 8;

/* Refactorization interval (D180). */
constexpr int64_t REFACTOR_EVERY = 64;

/* How far the two computations of the pivot element may disagree before the
 * factorization they both came through stops being trusted (D86). */
constexpr double LU_AGREE_TOL = 1e-5;

/* The clock is read once every this many iterations (D8). */
constexpr int64_t TIME_CHECK_EVERY = 64;

/* How often a progress line is offered, in iterations (D8). */
constexpr int64_t LOG_EVERY = 1000;

/* How often a watcher is asked whether to carry on, in iterations (D8). */
constexpr int64_t PROGRESS_EVERY = 64;

/* Dual phase 1 by artificial bounds [21]: a column whose cost asks for a
 * bound it does not have is lent this one, and classify_optimum reads
 * whether the loan was reached. Not load-bearing for any verdict: an
 * optimum reached only because the loan was too tight is refused. */
constexpr double ARTIFICIAL_BOUND = 1e10;

/* A guard against a loop that fails to terminate through a bug. */
constexpr int64_t ITER_SANITY_FACTOR = 200;
/* The cap is CUMULATIVE: phase 1, phase 2 and the dual re-entry all test the
 * same `s->iters` against it. D196 measured the headroom -- phase 1 spends at
 * most 1.68% of it, on `pilot-ja` -- and said to rebase the cap per phase if
 * this ever drops below about 60. Below that a legitimate long solve is
 * reported as a JAOS defect, which is a wrong answer about the solver (D232). */
static_assert(ITER_SANITY_FACTOR >= 60,
              "the iteration cap is shared across phases (D196)");

/* How long a solve may fail to improve before it is treated as cycling, as
 * a multiple of `nrow + ncol + 1` (D17). */
constexpr int64_t STALL_FACTOR = 10;

/* How far the primal phase 1's total infeasibility may rise above its own
 * running minimum before the basis is called unrepairable, as a fraction of
 * that minimum. 1.0 is "it may double". The quantity is a sum of bound
 * violations and cannot rise at all under an exact pivot (D218). */
constexpr double PHASE1_RISE_MAX = 1.0;

/* How far above the rounding of its own dot product a reduced cost has to
 * stand before the re-entry will act on it (D23). */
constexpr double NOISE_MARGIN = 1e5;

/* How many times a settled point may be handed back to the dual simplex. A
 * backstop, not a limit meant to bind (D30). */
constexpr int64_t SETTLE_ROUNDS = 32;

/* How short a mapped starting basis may be and still be repaired rather than
 * refused (D149, D151). */
constexpr int64_t WARM_REPAIR_MAX_SHORT = 4;

/* Bounds JAOS invented to get a dual feasible start. One value says both
 * whether a bound was lent and which side; real_lower/real_upper undo it. */
typedef enum { NOT_FAKE = 0, FAKE_LO, FAKE_UP } jm_fake;

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    /* The model's values with the scaling applied, sharing the model's
     * sparsity pattern. The model's own copy stays as loaded. */
    double *av;              /* [num_nz] */

    /* The same scaled values by row, over the model's CSR mirror (D35). */
    double *arv;             /* [num_nz], parallel to m->ar_index */

    /* Bounds and costs over all variables, likewise scaled: structurals
     * first, then the logicals. `cost` is the working cost, the model's
     * plus whatever has been shifted into it; `shift` is the record.
     * `cost0` is the model's own scaled cost, written once and never again.
     * Calling a loan in RESTORES from it rather than subtracting the record
     * back out (D121). There is no `cost[v] == cost0[v] + shift[v]`
     * invariant: the two arrays round apart. Every reader who needs to know
     * how much a cost moved must compute `cost[v] - cost0[v]` and never
     * read `shift[v]`. `shift` is written at three sites and nowhere else:
     * the lend in `shift_to_feasible`, and the two repayments (D124). */
    double *lo, *up, *cost;
    double *cost0;           /* [nvar] the model's own, never written twice */
    double *shift;           /* [nvar] */

    jm_var_status *status;   /* [nvar] */
    int64_t *basis;          /* [nrow] variable occupying each position */
    int64_t *where;          /* [nvar] basis position, or -1 */

    /* Which bound, if any, JAOS lent each variable; see jm_fake. */
    jm_fake *fake;

    double *xb;              /* [nrow] basic values */
    double *d;               /* [nvar] reduced costs */

    /* Dual steepest-edge weights, one per basis position: dse[i] tracks
     * ||row i of B^-1||^2. Carried across refactorizations. */
    double *dse;             /* [nrow] */

    jm_lu lu;
    jm_work work;

    /* Scratch, all owned. `col` carries an FTRAN result; `raw` keeps the
     * untransformed column the LU update needs. */
    double *col;
    double *raw;
    /* Two compensation terms, one per row each: `rhsc` for the sum
     * compute_primal builds in `col`, `resc` for the residual
     * subtract_basis_times forms (D171). Separate arrays: `rhsc` is still
     * live while subtract_basis_times runs. Each is `memset` at the entry
     * of its single reader and dead at its exit. A borrower whose value has
     * to survive a call to `compute_primal(s, true)` is not safe. */
    double *rhsc;
    double *resc;
    /* The duals and the pricing row are two different quantities (D30). */
    double *y;               /* [nrow] the duals, B^-T c_B */
    double *rho;             /* [nrow] row r of B^-1 */
    double *tau;             /* [nrow] B^-1 rho, for the weight update */
    double *alpha;           /* [nvar] pricing row */

    /* Where `alpha` can be nonzero, ascending and without repeats, or
     * `anpat < 0` when the array has to be read in full. `amark` is the
     * bitmap jm_pattern_order orders through; zero between iterations. */
    int64_t *apat;           /* [nvar] */
    int64_t anpat;
    uint64_t *amark;         /* [(nvar + 63) / 64] */

    /* Which variables are not basic, one bit each: bit v is set exactly when
     * `status[v] != JM_BASIC`. Membership, never "has a finite bound".
     * Persistent, unlike `amark`. jm_nonbasic_build is the only routine
     * that writes it wholesale; the eight sites that move a variable into
     * or out of the basis each maintain it by hand. */
    uint64_t *nbmark;        /* [(nvar + 63) / 64] */

    /* Where `rho` is nonzero, ascending, or `nrpat < 0` when nobody has
     * looked. `rmark` is what puts it in ascending order. */
    int64_t *rpat;           /* [nrow] */
    int64_t nrpat;
    uint64_t *rmark;         /* [(nrow + 63) / 64] */

    /* Where the entering column's FTRAN is nonzero, or `ncpat < 0` when too
     * dense to be worth carrying. Unordered: every reader is elementwise. */
    int64_t *cpat;           /* [nrow] */
    int64_t ncpat;

    /* The ratio test's candidate set: who may enter (`cand`), distance of
     * its reduced cost from infeasibility (`rnum`), its pivot (`rden`). */
    int64_t *cand;           /* [nvar] */
    double *rnum, *rden;     /* [nvar] */
    /* The primal ratio tests' candidates (D212): row, exact distance to
     * the blocking bound, pivot magnitude. Own arrays, never the dual's:
     * primal_cleanup runs inside the dual's settle loop and iterates
     * `cand` while calling primal_ratio_test. */
    int64_t *prow;           /* [nrow] */
    double *pnum, *pden;     /* [nrow] */
    double *rrange;          /* [nvar] width of the box, or infinity */

#ifndef NDEBUG
    /* Where the bitmap walk's candidate set is parked for the cross-check
     * in dual_ratio_test. Dev and sanitizer builds only. */
    int64_t *dbg_cand;               /* [nvar] */
    double *dbg_rnum, *dbg_rden;     /* [nvar] */
    double *dbg_rrange;              /* [nvar] */
    /* The scratch primal_bound_flip's s->col cross-check computes into. Its
     * own buffer, not one of the four above. */
    double *dbg_col;                 /* [nrow] */
    /* Pivots since the last write to `verified`, so `verified_fresh` can say
     * whether the flag still describes the point in front of it (D233). */
    int64_t dbg_piv_since_verify;
#endif

    /* Refactorization buffers, grown once and reused. */
    int64_t *bs, *bi;
    double *bv;
    int64_t bi_cap, bv_cap;

    /* The settled point, kept so that a re-entry which ends worse than it
     * started can be undone. These five arrays are the whole of what a
     * re-entry may write (see save_settled). Allocated on first use. */
    jm_var_status *sav_status;
    int64_t *sav_basis;
    double *sav_lo, *sav_up;
    jm_fake *sav_fake;

    /* The best point any round reached, a different question from the one
     * above (D89). `bst_obj` is its objective on the model's own costs. */
    jm_var_status *bst_status;
    int64_t *bst_basis;
    double *bst_lo, *bst_up;
    jm_fake *bst_fake;
    double bst_obj;
    /* Its worst dual sign violation in the model's own space. */
    double bst_dviol;
    bool bst_valid;

    struct timespec started;
    int64_t iters;
    bool needs_refactor;

    /* Has optimality been re-checked against a freshly computed point since
     * the last basis change? See the r < 0 branch in run(). Written only
     * through `set_verified` and read only through `verified_fresh`, which
     * is what keeps the debug counter beside it honest (D233). */
    bool verified;

    /* Does the next refresh owe a full sweep of shift_to_feasible? Set by a
     * warm start and by nothing else. The cold start is dual feasible by
     * construction and its first refresh must leave the costs untouched. */
    bool shift_pending;

    /* May some nonbasic reduced cost be dual infeasible? A dual step driven
     * by the row's pattern can skip the rest only while this is clear.
     * compute_duals and primal_cleanup write a reduced cost without a
     * pivot; either arms this, and one full sweep disarms it. */
    bool duals_dirty;

    /* Cycle detection. `infeas_best` is the smallest total primal
     * infeasibility this solve has reached and `last_gain` the iteration
     * that reached it; past STALL_FACTOR times the model's size without
     * improving, `bland` goes on. Improving turns it off (price_row). */
    double infeas_best;
    int64_t last_gain;
    bool bland;

    /* The same detector's measure for the primal method: the sum of the
     * wrong-signed reduced costs. A different quantity, so a different
     * field; `jaos_progress.primal_infeasibility` still reads `infeas_best`. */
    double dinfeas_best;

    /* The caller's two tolerances, resolved once. A solve works on one set
     * from start to finish, whatever anyone does to the model meanwhile. */
    double primal_tol;
    double dual_tol;

    /* Counted always, reported only if someone is listening. */
    int64_t n_refactor;
    int64_t n_weight_restart;
    int64_t n_bland;
    /* Pivots declined because the factorization contradicted itself (D86). */
    int64_t n_stability;

    /* Iterations `run_primal` took; `iters` counts every basis change
     * whichever method made it. Only this count tells the two apart. */
    int64_t n_primal_iters;

    /* How many of those `n_primal_iters` were phase 1's. Set on every exit
     * from phase 1; 0 means phase 1 genuinely did not run (D195). */
    int64_t n_phase1_iters;

    /* The primal phase-1 cost vector: `-1` on a basic below a bound the
     * model declared, `+1` on one above, `0` everywhere else. Swapped into
     * `s->cost` for the duration of one `compute_duals` call and swapped
     * straight back, so `d` comes back holding phase-1 reduced costs.
     * Allocated on the first phase 1. */
    double *c1;

    /* Which positions of `c1` the last `primal_phase1_costs` set, and how
     * many. At most `nrow`, because only a basic can be infeasible (D199).
     * `c1` is allocated zeroed, because nothing else initialises it. */
    int64_t *c1_at;
    int64_t n_c1_at;

    /* Is the primal phase 1 in flight? Read by the three places a cost is
     * lent: `update_dual`, the tail of `pivot()`, and `refresh`'s sweep
     * after a singular-basis repair. In phase 1 `d` holds gradients of the
     * sum of bound violations, so a lend against them corrupts the model's
     * objective (D191, D193). Phase 2 is deliberately not guarded (D191). */
    bool in_phase1;
} sx;


/* The only writer of `verified`, and the only reader, so the debug counter
 * beside the flag cannot drift from it the way a hand-maintained one does
 * (D201 is the receipt for that failure mode).
 *
 * Both a set and a clear end the stretch: a clear because the flag is gone,
 * a set because the verification is fresh again.
 *
 * `pivot()` may run with the flag still set. That is measured rather than
 * assumed: 6 entries out of 1033526 over the 139 gate instances, on
 * `etamacro`, `wood1p` and `pilot87`, all of them through
 * `reenter_after_settling` calling `primal_cleanup` before it clears the
 * flag. The prose used to say every caller clears it first, which is false.
 * What is true is what `verified_fresh` asserts: no READER ever sees a
 * verification a pivot has spent, 0 times in that same census (D233,
 * `bench/measurements/02-146/`). */
static inline void set_verified(sx *s, bool v)
{
    s->verified = v;
#ifndef NDEBUG
    s->dbg_piv_since_verify = 0;
#endif
}

static inline bool verified_fresh(const sx *s)
{
    assert(!s->verified || s->dbg_piv_since_verify == 0);
    return s->verified;
}

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

static void sx_free(sx *s)
{
    free(s->av); free(s->arv);
    free(s->lo); free(s->up); free(s->cost); free(s->cost0); free(s->shift);
    free(s->status); free(s->basis); free(s->where);
    free(s->xb); free(s->d); free(s->dse);
    free(s->col); free(s->raw); free(s->rhsc); free(s->resc);
    free(s->y); free(s->rho);
    free(s->tau); free(s->alpha); free(s->apat); free(s->amark);
    free(s->nbmark);
    free(s->rpat); free(s->rmark); free(s->cpat);
    free(s->cand); free(s->rnum); free(s->rden); free(s->rrange);
    free(s->prow); free(s->pnum); free(s->pden);
#ifndef NDEBUG
    free(s->dbg_cand); free(s->dbg_rnum); free(s->dbg_rden);
    free(s->dbg_rrange); free(s->dbg_col);
#endif
    free(s->bs); free(s->bi); free(s->bv);
    free(s->fake); free(s->c1); free(s->c1_at);
    free(s->sav_status); free(s->sav_basis);
    free(s->sav_lo); free(s->sav_up); free(s->sav_fake);
    free(s->bst_status); free(s->bst_basis);
    free(s->bst_lo); free(s->bst_up); free(s->bst_fake);
    jm_lu_free(&s->lu);
    memset(s, 0, sizeof *s);
}

/* Sets up the scaled working copy the whole solve runs on. Every factor is
 * an exact power of two (docs/scaling.md). The model as loaded is never
 * touched; publish() puts the answers back into its units. */
static jaos_status sx_init(sx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    jm_lu_init(&s->lu);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    /* Zero on the model means the caller never set one. */
    s->primal_tol = m->cfg.primal_tol > 0.0 ? m->cfg.primal_tol : PRIMAL_TOL;
    s->dual_tol   = m->cfg.dual_tol   > 0.0 ? m->cfg.dual_tol   : DUAL_TOL;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }

    /* Pricing walks the matrix by row (D35), so the mirror has to exist. */
    {
        jaos_status st = jm_model_ensure_rowwise(m);
        if (st != JAOS_OK)
            return st;
    }

    s->av     = jm_alloc_array(m->num_nz, sizeof(double));
    s->arv    = jm_alloc_array(m->num_nz, sizeof(double));
    s->lo     = jm_alloc_array(s->nvar, sizeof(double));
    s->up     = jm_alloc_array(s->nvar, sizeof(double));
    s->cost   = jm_calloc_array(s->nvar, sizeof(double));
    s->cost0  = jm_calloc_array(s->nvar, sizeof(double));
    s->shift  = jm_calloc_array(s->nvar, sizeof(double));
    s->status = jm_alloc_array(s->nvar, sizeof(jm_var_status));
    s->basis  = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->where  = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->xb     = jm_calloc_array(s->nrow, sizeof(double));
    s->d      = jm_calloc_array(s->nvar, sizeof(double));
    s->dse    = jm_alloc_array(s->nrow, sizeof(double));
    s->col    = jm_calloc_array(s->nrow, sizeof(double));
    s->raw    = jm_calloc_array(s->nrow, sizeof(double));
    s->rhsc   = jm_calloc_array(s->nrow, sizeof(double));
    s->resc   = jm_calloc_array(s->nrow, sizeof(double));
    s->y      = jm_calloc_array(s->nrow, sizeof(double));
    s->rho    = jm_calloc_array(s->nrow, sizeof(double));
    s->tau    = jm_calloc_array(s->nrow, sizeof(double));
    s->alpha  = jm_calloc_array(s->nvar, sizeof(double));
    s->apat   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->amark  = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
    s->nbmark = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
    s->rpat   = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->rmark  = jm_calloc_array((s->nrow + 63) / 64, sizeof(uint64_t));
    s->cpat   = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->ncpat  = -1;
    s->anpat  = -1;          /* alpha is all zero, but nothing has said where */
    s->nrpat  = -1;
    s->duals_dirty = true;   /* nothing has established the costs are feasible */
    s->cand   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->rnum   = jm_alloc_array(s->nvar, sizeof(double));
    s->rden   = jm_alloc_array(s->nvar, sizeof(double));
    s->rrange = jm_alloc_array(s->nvar, sizeof(double));
    s->prow   = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->pnum   = jm_alloc_array(s->nrow, sizeof(double));
    s->pden   = jm_alloc_array(s->nrow, sizeof(double));
    s->bs     = jm_alloc_array(s->nrow + 1, sizeof(int64_t));
    s->fake   = jm_calloc_array(s->nvar, sizeof *s->fake);

    if (!s->av || !s->arv || !s->lo || !s->up || !s->cost || !s->cost0 ||
        !s->shift ||
        !s->status || !s->basis ||
        !s->where || !s->xb || !s->d || !s->dse ||
        !s->col || !s->raw || !s->rhsc || !s->resc ||
        !s->y || !s->rho || !s->tau || !s->alpha || !s->apat || !s->amark ||
        !s->nbmark ||
        !s->rpat || !s->rmark || !s->cpat || !s->cand || !s->rnum ||
        !s->rden || !s->rrange || !s->prow || !s->pnum || !s->pden ||
        !s->bs || !s->fake) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

#ifndef NDEBUG
    /* Kept out of the chain above so the release build's allocation list
     * is unchanged. */
    s->dbg_cand   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->dbg_rnum   = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_rden   = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_rrange = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_col    = jm_alloc_array(s->nrow, sizeof(double));
    if (!s->dbg_cand || !s->dbg_rnum || !s->dbg_rden || !s->dbg_rrange ||
        !s->dbg_col) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
#endif

    const double *rho = m->row_scale, *gamma = m->col_scale;

    /* ahat_ij = rho_i * a_ij * gamma_j. */
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];

    /* The same product in the same order of operations, laid out by row:
     * that is what makes the row-wise pricing sums bit-identical. */
    for (int64_t i = 0; i < s->nrow; i++)
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            s->arv[p] = rho[i] * m->ar_value[p] * gamma[m->ar_index[p]];

    /* A column's bounds are its own units divided out; a row's bounds move
     * with the row's factor. Infinities survive: no bound changes side. */
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
    /* The one write to cost0 (D121). */
    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
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

/* The bounds the model declared, as against the ones dual phase 1 lent. A
 * loan only ever replaced an infinity, so `fake` is enough to undo it. */
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

/* w' M_v for a dense row vector w, charging the nonzeros it touches.
 * src/check.c has a similar loop; they stay apart so the checker does not
 * link against solver internals. */
static double price_entry(sx *s, const double *w, int64_t v)
{
    if (v >= s->ncol) {
        jm_work_add(&s->work, JM_WORK_NONZERO);
        return -w[v - s->ncol];
    }
    const jaos_model *m = s->m;
    double a = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        a += w[m->a_index[k]] * s->av[k];
    jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                          JM_WORK_NONZERO);
    return a;
}

/* The slack basis: every logical basic, every structural pinned to the
 * bound that makes its reduced cost feasible, lent one if it has none. */
static void build_initial_basis(sx *s)
{
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->ncol + i;
        s->basis[i] = v;
        s->status[v] = JM_BASIC;
        s->where[v] = i;
        /* B = -I, so row i of B^-1 is -e_i and its squared norm is one. */
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

    /* Built rather than patched: the loops above are the whole membership
     * state. */
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);
}

/* The basis a previous solve or a caller left on the model, installed as the
 * point this solve starts from. Returns false when there is none, or when
 * what is there cannot be started from; the caller then builds the slack
 * basis. A status naming a bound that is no longer finite is moved to its
 * other bound. A nonbasic with no bounds rests free at zero (D90). A set
 * of columns that no longer factors is repair_singular_basis's job.
 * It cannot establish dual feasibility: the first refresh shifts every
 * breached cost to the feasible side. No artificial bounds are lent here.
 * The weights start at one. */
static bool build_warm_basis(sx *s)
{
    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return false;

    /* A SHORT count is repaired rather than refused (D144). While short BY
     * AT MOST WARM_REPAIR_MAX_SHORT, promote the logical of an UNCOVERED
     * row first (without its own e_i, B is structurally singular), then
     * logicals in fixed row order (D8). Past the cap, fall back to cold
     * (D148, D151). A LONG count is still refused. The stored arrays are
     * the model's and are never written. OOM below falls back to cold and
     * reports JAOS_OK. */
    jaos_basis_status *want_arr =
        jm_alloc_array(s->nvar > 0 ? s->nvar : 1, sizeof *want_arr);
    if (want_arr == nullptr)
        return false;
    for (int64_t v = 0; v < s->nvar; v++)
        want_arr[v] = v < s->ncol ? m->start_col_status[v]
                                  : m->start_row_status[v - s->ncol];

    int64_t nbasic = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        nbasic += want_arr[v] == JAOS_BASIS_BASIC;

    if (nbasic < s->nrow && s->nrow - nbasic <= WARM_REPAIR_MAX_SHORT) {
        unsigned char *cov = jm_calloc_array(s->nrow > 0 ? s->nrow : 1,
                                             sizeof *cov);
        if (cov == nullptr) {
            free(want_arr);
            return false;
        }
        int64_t covnz = 0;
        for (int64_t v = 0; v < s->nvar; v++) {
            if (want_arr[v] != JAOS_BASIS_BASIC)
                continue;
            if (v < s->ncol) {
                for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                    cov[m->a_index[k]] = 1;
                covnz += m->a_start[v + 1] - m->a_start[v];
            } else {
                cov[v - s->ncol] = 1;
                covnz++;
            }
        }
        jm_work_add(&s->work, covnz * JM_WORK_NONZERO);
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (!cov[i] && want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
            }
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
            }
        free(cov);
        jm_log(s->m, JAOS_LOG_DETAIL,
               "the mapped starting basis arrived short and was repaired "
               "by promoting logicals");
    }
    if (nbasic != s->nrow) {
        free(want_arr);
        return false;
    }

    int64_t p = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        jaos_basis_status want = want_arr[v];
        if (want == JAOS_BASIS_BASIC) {
            s->basis[p] = v;
            s->status[v] = JM_BASIC;
            s->where[v] = p;
            p++;
            continue;
        }

        /* The stored side is kept when it is still there, the other one
         * taken when it is not, and free when there is neither (D90). */
        s->where[v] = -1;
        if (want == JAOS_BASIS_AT_UPPER && isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else if (isfinite(s->lo[v]))
            s->status[v] = JM_AT_LOWER;
        else if (isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else
            s->status[v] = JM_FREE;
    }
    free(want_arr);

    /* As in build_initial_basis. Every return before this point is taken
     * before any status is written. */
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    for (int64_t i = 0; i < s->nrow; i++)
        s->dse[i] = 1.0;

    s->shift_pending = true;
    return true;
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
    /* At least one slot even for a basis with no entries: jm_lu_factor is
     * entitled to non-null arrays whenever dim > 0. */
    int64_t room = nz > 0 ? nz : 1;
    if (!JM_GROW(s->bi, s->bi_cap, room) || !JM_GROW(s->bv, s->bv_cap, room))
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

/* r -= B z, for a dense vector over the rows; walks the basis columns and
 * scatters. Compensated, like the sum it subtracts from (D171). */
static void subtract_basis_times(sx *s, double *r, const double *z)
{
    int64_t nz = 0;
    double *comp = s->resc;
    memset(comp, 0, (size_t)s->nrow * sizeof *comp);
    for (int64_t i = 0; i < s->nrow; i++) {
        double zi = z[i];
        if (zi == 0.0)
            continue;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                const int64_t ii = m->a_index[k];
                const double t = -(s->av[k] * zi);
                const double a = r[ii], u = a + t;
                comp[ii] += (fabs(a) >= fabs(t)) ? ((a - u) + t)
                                                 : ((t - u) + a);
                r[ii] = u;
            }
            nz += m->a_start[v + 1] - m->a_start[v];
        } else {
            const int64_t ii = v - s->ncol;   /* the column is -e_i */
            const double a = r[ii], u = a + zi;
            comp[ii] += (fabs(a) >= fabs(zi)) ? ((a - u) + zi)
                                              : ((zi - u) + a);
            r[ii] = u;
            nz++;
        }
    }
    /* Same guard and same reason as compute_primal's. */
    for (int64_t i = 0; i < s->nrow; i++)
        if (isfinite(r[i]) && isfinite(comp[i]))
            r[i] += comp[i];
    jm_work_add(&s->work, nz * JM_WORK_NONZERO);
}

/* x_B = -B^-1 (N x_N). `refine` asks for one step of iterative refinement
 * against the basis columns themselves; see refresh() for which solves get
 * it. The right-hand side is accumulated with Neumaier compensation
 * (D168). `long double` would break cross-machine determinism (D34).
 * `apply_flips` is still uncompensated, deliberately: the final
 * `refine = true` refresh rebuilds `x_B` from scratch. */
static void compute_primal(sx *s, bool refine)
{
    double *rhs = s->col;
    double *comp = s->rhsc;
    memset(rhs, 0, (size_t)s->nrow * sizeof *rhs);
    memset(comp, 0, (size_t)s->nrow * sizeof *comp);

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        double val = nonbasic_value(s, v);
        if (val == 0.0)
            continue;
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                const int64_t i = m->a_index[k];
                const double t = -(s->av[k] * val);
                const double a = rhs[i], u = a + t;
                comp[i] += (fabs(a) >= fabs(t)) ? ((a - u) + t)
                                                : ((t - u) + a);
                rhs[i] = u;
            }
            jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                                  JM_WORK_NONZERO);
        } else {
            const int64_t i = v - s->ncol;   /* column is -e_i */
            const double a = rhs[i], u = a + val;
            comp[i] += (fabs(a) >= fabs(val)) ? ((a - u) + val)
                                              : ((val - u) + a);
            rhs[i] = u;
            jm_work_add(&s->work, JM_WORK_NONZERO);
        }
    }

    /* Guarded once, after the loop: `inf + (inf - inf)` is a NaN (D165).
     * This pass bills no work units. */
    for (int64_t i = 0; i < s->nrow; i++)
        if (isfinite(rhs[i]) && isfinite(comp[i]))
            rhs[i] += comp[i];

    /* Borrowed: `raw` belongs to pivot(), which rebuilds it from scratch
     * before every use, and no pivot is in flight while this runs. */
    if (refine)
        memcpy(s->raw, rhs, (size_t)s->nrow * sizeof *s->raw);

    jm_lu_ftran(&s->lu, rhs, &s->work);
    memcpy(s->xb, rhs, (size_t)s->nrow * sizeof *rhs);

    if (!refine)
        return;

    double *r = s->raw;                /* holds b; becomes b - B x_B */
    subtract_basis_times(s, r, s->xb);
    jm_lu_ftran(&s->lu, r, &s->work);
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] += r[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
}

/* d_N = c_N - y' M_N, with y = B^-T c_B. `refine` as above, on the
 * transposed solve: the two travel together (D29). */
static void compute_duals(sx *s, bool refine)
{
    double *y = s->y;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);

    if (refine) {
        /* Borrowed: `tau` is pivot()'s weight-update scratch, overwritten
         * from `rho` before each use, so nothing here outlives this block. */
        double *r = s->tau;
        int64_t nz = 0;
        for (int64_t i = 0; i < s->nrow; i++) {
            int64_t v = s->basis[i];
            double dot;
            if (v < s->ncol) {
                const jaos_model *m = s->m;
                dot = 0.0;
                for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                    dot += s->av[k] * y[m->a_index[k]];
                nz += m->a_start[v + 1] - m->a_start[v];
            } else {
                dot = -y[v - s->ncol];
                nz++;
            }
            r[i] = s->cost[v] - dot;
        }
        jm_work_add(&s->work, nz * JM_WORK_NONZERO);
        jm_lu_btran(&s->lu, r, &s->work);
        for (int64_t i = 0; i < s->nrow; i++)
            y[i] += r[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC) {
            s->d[v] = 0.0;
            continue;
        }
        s->d[v] = s->cost[v] - price_entry(s, s->y, v);
    }

    /* These costs owe nothing to the shifting, so any of them may now sit
     * on the infeasible side of its bound. */
    s->duals_dirty = true;
}

/* Needed by refresh below; defined with the settling code. */
static void shift_to_feasible(sx *s, int64_t v);

/* How many times one refresh will repair and refactor before giving up. */
constexpr int REPAIR_ATTEMPTS = 4;

/* Puts a basis back together after the factorization finds it singular.
 * The LU's contract (jaos_internal.h) is that rank < dim is a fact rather
 * than an error, and that the caller replaces basis columns. Slots
 * 0..rank-1 of the permutations name the pivoted rows and the used
 * positions. The repair pairs each uncovered row with a dependent position
 * and puts the row's logical there. The logical of an uncovered row cannot
 * already be in the basis; the check below is kept anyway. Returns false
 * when nothing was repaired, which the caller must treat as the numerical
 * failure it then is. */
static bool repair_singular_basis(sx *s)
{
    const int64_t n = s->nrow;
    const int64_t rank = s->lu.rank;

    /* rank < 0 marks a factorization wrecked by a failed update. */
    if (rank < 0 || rank >= n)
        return false;

    bool *row_covered = jm_calloc_array(n, sizeof(bool));
    bool *pos_used    = jm_calloc_array(n, sizeof(bool));
    if (row_covered == nullptr || pos_used == nullptr) {
        free(row_covered);
        free(pos_used);
        return false;
    }

    for (int64_t k = 0; k < rank; k++) {
        row_covered[s->lu.perm_row[k]] = true;
        pos_used[s->lu.perm_col[k]] = true;
    }

    bool done = true;
    int64_t i = 0;
    for (int64_t p = 0; p < n; p++) {
        if (pos_used[p])
            continue;
        while (i < n && row_covered[i])
            i++;
        if (i >= n) {
            /* Fewer uncovered rows than dependent columns. */
            done = false;
            break;
        }

        int64_t leaving  = s->basis[p];
        int64_t entering = s->ncol + i;
        if (s->status[entering] == JM_BASIC) {
            done = false;
            break;
        }

        /* Lower first, as build_initial_basis does. A variable with neither
         * becomes nonbasic free; the shift in refresh keeps that feasible. */
        if (isfinite(s->lo[leaving]))
            s->status[leaving] = JM_AT_LOWER;
        else if (isfinite(s->up[leaving]))
            s->status[leaving] = JM_AT_UPPER;
        else
            s->status[leaving] = JM_FREE;
        jm_nonbasic_insert(s->nbmark, leaving);
        s->where[leaving] = -1;

        s->basis[p] = entering;
        s->status[entering] = JM_BASIC;
        jm_nonbasic_remove(s->nbmark, entering);
        s->where[entering] = p;
        i++;
    }

    free(row_covered);
    free(pos_used);
    if (!done)
        return false;

    /* B^-1 changed in several columns at once: restart the weights. */
    for (int64_t k = 0; k < n; k++)
        s->dse[k] = 1.0;
    return true;
}

/* Rebuild the factorization and everything derived from it. Returns false
 * when the basis will not factor and the repair above cannot put it right.
 * `refine` asks the two solves for one step of iterative refinement, and
 * only callers whose result can be published ask (D20, D29). */
static jaos_status refresh(sx *s, bool *ok, bool refine)
{
    bool repaired = false;
    s->n_refactor++;

    for (int attempt = 0;; attempt++) {
        jaos_status st = refactorize(s);
        if (st != JAOS_OK)
            return st;
        if (s->lu.rank == s->nrow)
            break;
        if (attempt + 1 >= REPAIR_ATTEMPTS || !repair_singular_basis(s)) {
            jm_set_err(s->m, "the basis went singular at iteration %lld and "
                             "could not be repaired: rank %lld of %lld",
                       (long long)s->iters, (long long)s->lu.rank,
                       (long long)s->nrow);
            *ok = false;
            return JAOS_OK;
        }
        repaired = true;
    }

    compute_primal(s, refine);
    compute_duals(s, refine);

    /* The repair chose bounds for the evicted variables before their
     * reduced costs existed, so some are now on the wrong side. Only after
     * a repair, or on the first refresh of a warm start: a cold solve that
     * never went singular is left bit for bit. Never while the primal phase
     * 1 runs: this is the third site that lends a cost (D193).
     * `shift_pending` is left standing rather than cleared. */
    if (!s->in_phase1) {
        bool sweep = repaired || s->shift_pending;
        s->shift_pending = false;
        if (sweep)
            for (int64_t v = 0; v < s->nvar; v++)
                shift_to_feasible(s, v);
    }

    *ok = true;
    return JAOS_OK;
}

/* --------------------------------------------------------------------- */
/* One iteration                                                         */
/* --------------------------------------------------------------------- */

/* Dual steepest-edge pricing [8]: among the basics that violate a bound,
 * the one with the largest violation squared over the squared norm of its
 * row of B^-1. Returns -1 when primal feasible; otherwise sets *below to
 * which bound was breached and *violation to its size. */
static int64_t price_row(sx *s, bool *below, double *violation)
{
    /* Decided before the loop, so one iteration uses one rule throughout. */
    if (!s->bland &&
        s->iters - s->last_gain > STALL_FACTOR * (s->nrow + s->ncol + 1)) {
        s->bland = true;
        s->n_bland++;
        jm_log(s->m, JAOS_LOG_DETAIL,
               "iter %lld: no progress for %lld iterations, switching to "
               "Bland's rule", (long long)s->iters,
               (long long)(s->iters - s->last_gain));
    }

    int64_t best = -1;
    double best_score = 0.0;
    double total = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        double viol_lo = isfinite(s->lo[v]) ? s->lo[v] - s->xb[i] : 0.0;
        double viol_up = isfinite(s->up[v]) ? s->xb[i] - s->up[v] : 0.0;

        bool under = viol_lo >= viol_up;
        double viol = under ? viol_lo : viol_up;
        if (viol <= s->primal_tol)
            continue;

        total += viol;

        if (s->bland) {
            if (best < 0 || v < s->basis[best]) {
                best = i;
                *below = under;
                *violation = viol;
            }
            continue;
        }

        double score = viol * viol / s->dse[i];
        if (score > best_score) {
            best_score = score;
            best = i;
            *below = under;
            *violation = viol;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    /* Improving turns Bland's rule off (D26). */
    if (total < s->infeas_best) {
        s->infeas_best = total;
        s->last_gain = s->iters;
        s->bland = false;
    }
    return best;
}

/* Anything that is not finite and positive is already wrong. */
static bool weight_drifted(double carried, double exact, double factor)
{
    if (!isfinite(carried) || carried <= 0.0)
        return true;
    if (!isfinite(exact) || exact <= 0.0)
        return true;
    return carried > exact * factor || carried * factor < exact;
}

/* The weight recurrence: row i of the new B^-1 is row i minus
 * (alpha_i / alpha_r) times row r, row r becomes row r over alpha_r, and
 * tau_i = rho_i . rho_r supplies the cross term. See the header. */
bool jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau,
                   double exact_r, double drift_factor,
                   const int64_t *pat, int64_t npat)
{
    if (weight_drifted(w[r], exact_r, drift_factor)) {
        for (int64_t i = 0; i < n; i++)
            w[i] = 1.0;
        return true;  /* a restart is every weight by definition, pattern or no */
    }
    w[r] = exact_r;

    double pivot = alpha[r];
    if (pivot == 0.0)
        return false;   /* the ratio test never picks one, and weights are a
                           heuristic: leaving them stale beats infinities */

    double wr = w[r];
    /* Each row's new weight depends on its own old one and nothing else,
     * so the dense and sparse forms compute the same numbers. */
    const int64_t nvisit = pat != nullptr ? npat : n;
    for (int64_t k = 0; k < nvisit; k++) {
        int64_t i = pat != nullptr ? pat[k] : k;
        if (i == r || alpha[i] == 0.0)
            continue;
        double kk = alpha[i] / pivot;
        double wi = w[i] - 2.0 * kk * tau[i] + kk * kk * wr;
        w[i] = wi > DSE_MIN ? wi : DSE_MIN;
    }
    double wnew = wr / (pivot * pivot);
    w[r] = wnew > DSE_MIN ? wnew : DSE_MIN;
    return false;
}

/* Bound flipping [19][1]. A candidate with two finite bounds need not stop
 * the dual step: swapped to its other bound it stays dual feasible and
 * moves row r by |alpha| times the width of the box. `remaining` is the
 * row's violation, spent down by each swap. Retired candidates are swapped
 * to the tail: [0, live) are still in play, [live, n) are to be flipped.
 * Zero means the dual is unbounded, the primal has no feasible point. */
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
 * with them, accumulated into one column and transformed once. */
static void apply_flips(sx *s, int64_t at, int64_t n)
{
    /* Borrowed: pivot() overwrites col before reading it. */
    double *rhs = s->col;
    memset(rhs, 0, (size_t)s->nrow * sizeof *rhs);

    for (int64_t k = at; k < n; k++) {
        /* `bfrt_walk` retires a candidate only after `if (!isfinite(width))
         * break;`, so everything in [live, n) has a finite box. Flipping one
         * that does not makes `nonbasic_value` return an infinity and `xb`
         * NaN, which is a wrong answer and not a crash (D232). */
        assert(isfinite(s->rrange[k]));
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

    /* Same borrowing: pivot fills `cpat` from its own FTRAN first. */
    int64_t nc = 0;
    jm_lu_ftran_sparse(&s->lu, rhs, &s->work, s->cpat, &nc);
    if (nc * SPARSE_COL_DEN <= s->nrow) {
        for (int64_t k = 0; k < nc; k++)
            s->xb[s->cpat[k]] -= rhs[s->cpat[k]];
        jm_work_add(&s->work, nc * JM_WORK_NONZERO);
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= rhs[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }
}

/* One variable's eligibility, and its place in the candidate arrays. */
static void admit_candidate(sx *s, int64_t v, bool below, int64_t *n)
{
    if (s->status[v] == JM_BASIC)
        return;
    double a = s->alpha[v];
    if (fabs(a) < PIVOT_MIN)
        return;

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
        return;

    int64_t k = (*n)++;
    s->cand[k] = v;
    s->rnum[k] = dist > 0.0 ? dist : 0.0;
    s->rden[k] = fabs(a);
    s->rrange[k] = s->up[v] - s->lo[v];
}

/* The ratio test: who may enter, how far the step may go, and which
 * candidate takes it. The numerator is the distance from v's reduced cost
 * to infeasibility, not its magnitude: clamped at zero, an
 * already-infeasible cost blocks at once and the step repairs it exactly.
 * The flips are applied here: they are part of the step. */
static int64_t dual_ratio_test(sx *s, bool below, double violation,
                               double *theta_out)
{
    int64_t n = 0;

    /* A variable outside the pattern has alpha exactly zero, which the
     * PIVOT_MIN test rejects, so the two scans admit the same candidates in
     * the same array positions. bfrt_walk and jm_harris_pick break an exact
     * tie by whichever candidate they meet first, and apply_flips adds
     * columns in the order they stand. The bitmap walk is ascending. */
    if (s->anpat >= 0) {
        for (int64_t t = 0; t < s->anpat; t++)
            admit_candidate(s, s->apat[t], below, &n);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    } else {
        /* `nbmark` is maintained by hand at eight sites and rebuilt
         * wholesale at four, and this is the only place that reads it. A
         * bit out of step with `status` silently drops a candidate from the
         * ratio test or offers a basic one, and no predicate any gate
         * reports can see either. Rebuilt and compared here rather than
         * trusted (D223). O(nvar) and no allocation, on the branch that is
         * already the dense one. */
#ifndef NDEBUG
        for (int64_t v = 0; v < s->nvar; v++) {
            const bool in_map = (s->nbmark[v >> 6] >> (v & 63)) & 1;
            assert(in_map == (s->status[v] != JM_BASIC));
        }
#endif
        /* Charged per variable handed to admit_candidate (D93). */
        int64_t visited = 0;
        int64_t nwords = (s->nvar + 63) / 64;
        for (int64_t w = 0; w < nwords; w++) {
            uint64_t bits = s->nbmark[w];
            while (bits != 0) {
                admit_candidate(s, (w << 6) + __builtin_ctzll(bits), below,
                                &n);
                bits &= bits - 1;
                visited++;
            }
        }
        jm_work_add(&s->work, visited * JM_WORK_NONZERO);

        /* Exactly nrow variables carry JM_BASIC. Not redundant with the
         * dn == n cross-check: an inflated `visited` moves s->work.units. */
        assert(visited == s->nvar - s->nrow);
    }

#ifndef NDEBUG
    /* Both scans, over the state that produced them (D30). Charges no work,
     * so a dev build bills what the release build bills. */
    {
        for (int64_t k = 0; k < n; k++) {
            s->dbg_cand[k]   = s->cand[k];
            s->dbg_rnum[k]   = s->rnum[k];
            s->dbg_rden[k]   = s->rden[k];
            s->dbg_rrange[k] = s->rrange[k];
        }
        int64_t dn = 0;
        for (int64_t v = 0; v < s->nvar; v++)
            admit_candidate(s, v, below, &dn);
        assert(dn == n);
        for (int64_t k = 0; k < n; k++) {
            assert(s->cand[k] == s->dbg_cand[k]);
            assert(s->rnum[k] == s->dbg_rnum[k]);
            assert(s->rden[k] == s->dbg_rden[k]);
            assert(s->rrange[k] == s->dbg_rrange[k]);
        }
    }
#endif

    if (n == 0)
        return -1;

    /* Bland's rule takes the exact minimum quotient: no window, no flips. */
    if (s->bland) {
        int64_t b = jm_bland_pick(n, s->cand, s->rnum, s->rden);
        jm_work_add(&s->work, 2 * n * JM_WORK_NONZERO);
        int64_t bv = s->cand[b];
        *theta_out = s->d[bv] / s->alpha[bv];
        return bv;
    }

    int64_t live = bfrt_walk(s, n, violation);
    if (live == 0)
        return -1;   /* nothing blocks the step; the model is infeasible */

    int64_t k = jm_harris_pick(live, s->rnum, s->rden, s->dual_tol);
    jm_work_add(&s->work, 2 * live * JM_WORK_NONZERO);
    int64_t best = s->cand[k];

    if (live < n)
        apply_flips(s, live, n);

    /* The step that lands the winner's reduced cost exactly on zero. */
    *theta_out = s->d[best] / s->alpha[best];
    return best;
}

/* Bland's rule over the same candidates. Documented in the header. */
int64_t jm_bland_pick(int64_t n, const int64_t *var, const double *num,
                      const double *den)
{
    if (n <= 0)
        return -1;

    double least = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        double t = num[k] / den[k];
        if (t < least)
            least = t;
    }

    int64_t best = -1;
    for (int64_t k = 0; k < n; k++)
        if (num[k] / den[k] <= least && (best < 0 || var[k] < var[best]))
            best = k;
    return best;
}

/* Bland's rule on the primal side. Documented in the header. */
bool jm_primal_row_wins(double step, int64_t var,
                        double best_step, int64_t best_var, bool bland)
{
    if (step < best_step)
        return true;
    return bland && best_var >= 0 && step == best_step && var < best_var;
}

/* A scatter's record of where it wrote, made ascending and distinct. */
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words)
{
    *words = 0;
    if (n <= 0 || limit <= 0)
        return 0;

    /* `mark` is borrowed scratch: all zero on entry, all zero again on
     * return. A word left set by a previous call would put a position in
     * this call's output that this call's input never named, and the
     * pattern is what the next FTRAN trusts. Checked over the whole bitmap
     * on entry and over the touched range on return, because that range is
     * the only part this call may have written (D223). */
#ifndef NDEBUG
    const int64_t nwords_dbg = (limit + 63) / 64;
    for (int64_t w = 0; w < nwords_dbg; w++)
        assert(mark[w] == 0);
#endif

    /* The touched range, so a small pattern does not pay for the bitmap. */
    int64_t lo = (limit + 63) / 64, hi = -1;
    for (int64_t t = 0; t < n; t++) {
        int64_t p = pos[t];
        if (p < 0 || p >= limit)
            continue;
        int64_t w = p >> 6;
        mark[w] |= UINT64_C(1) << (p & 63);
        if (w < lo) lo = w;
        if (w > hi) hi = w;
    }

    /* Reading back over the input is safe: the distinct count can only be
     * smaller than what went in. */
    int64_t k = 0;
    for (int64_t w = lo; w <= hi; w++) {
        uint64_t bits = mark[w];
        if (bits == 0)
            continue;
        mark[w] = 0;
        while (bits != 0) {
            pos[k++] = (w << 6) + __builtin_ctzll(bits);
            bits &= bits - 1;
        }
    }
    *words = hi >= lo ? hi - lo + 1 : 0;
#ifndef NDEBUG
    /* "Reading back over the input is safe: the distinct count can only be
     * smaller than what went in." A pattern longer than its input names
     * positions no FTRAN scattered, and `price_all` then sums over whatever
     * `alpha` happened to hold (D232). */
    assert(k <= n);
    for (int64_t w = lo; w <= hi; w++)
        assert(mark[w] == 0);
    /* Ascending and each position once: the read-back walks words upward and
     * takes bits from low to high inside each, so this states what the loop
     * shape already gives and would catch a rewrite that stopped giving it. */
    for (int64_t t = 1; t < k; t++)
        assert(pos[t] > pos[t - 1]);
#endif
    return k;
}

/* The nonbasic set as a bitmap. Unlike jm_pattern_order's, this bitmap is
 * persistent and nothing here clears it (see `nbmark`). */
int64_t jm_nonbasic_build(int64_t nvar, const jm_var_status *status,
                          uint64_t *mark)
{
    int64_t nwords = (nvar + 63) / 64;
    for (int64_t w = 0; w < nwords; w++)
        mark[w] = 0;

    /* Membership, not bounds: JM_FREE included. */
    int64_t k = 0;
    for (int64_t v = 0; v < nvar; v++) {
        if (status[v] == JM_BASIC)
            continue;
        mark[v >> 6] |= UINT64_C(1) << (v & 63);
        k++;
    }
    return k;
}

void jm_nonbasic_insert(uint64_t *mark, int64_t v)
{
    mark[v >> 6] |= UINT64_C(1) << (v & 63);
}

void jm_nonbasic_remove(uint64_t *mark, int64_t v)
{
    mark[v >> 6] &= ~(UINT64_C(1) << (v & 63));
}

/* The testable mirror of the walk in dual_ratio_test, which does not call
 * it. */
int64_t jm_nonbasic_expand(int64_t nvar, const uint64_t *mark, int64_t *out)
{
    int64_t nwords = (nvar + 63) / 64, k = 0;
    for (int64_t w = 0; w < nwords; w++) {
        uint64_t bits = mark[w];
        while (bits != 0) {
            out[k++] = (w << 6) + __builtin_ctzll(bits);
            bits &= bits - 1;
        }
    }
    return k;
}

/* Harris' window and the best-conditioned pivot inside it. */
int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol)
{
    if (n <= 0)
        return -1;

    /* The header's two preconditions, checked rather than written (D223).
     * A negative numerator widens the window the wrong way and a
     * non-positive denominator divides by zero or flips the quotient's
     * sign; either produces a step this test would then call the smallest,
     * which is a wrong pivot and not a crash. */
#ifndef NDEBUG
    for (int64_t k = 0; k < n; k++) {
        assert(num[k] >= 0.0);
        assert(den[k] > 0.0);
    }
#endif

    double window = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        double t = (num[k] + dual_tol) / den[k];
        if (t < window)
            window = t;
    }

    int64_t best = 0;
    double best_den = 0.0;
    for (int64_t k = 0; k < n; k++) {
        if (num[k] / den[k] <= window && den[k] > best_den) {
            best_den = den[k];
            best = k;
        }
    }
    /* "The set it chooses from is never empty for n > 0" — so a caller may
     * use the return as an index without testing it, and several do (D223). */
    assert(best >= 0 && best < n);
    return best;
}

/* alpha = rho' M, for every variable at once, walking the row-wise mirror
 * (D35). The sums are bit-identical to the column-wise ones by
 * construction: each CSC column is sorted by row index and the rows are
 * visited in increasing order, so every column accumulates its terms in
 * the same order. A skipped row would have contributed `0.0 * a_ij`. */
static void price_all(sx *s)
{
    const jaos_model *m = s->m;

    /* Erasing the previous row, through the pattern where one is known. */
    if (s->anpat < 0)
        memset(s->alpha, 0, (size_t)s->nvar * sizeof *s->alpha);
    else
        for (int64_t k = 0; k < s->anpat; k++)
            s->alpha[s->apat[k]] = 0.0;

    /* A slot can be recorded twice (cancelled back to exactly zero), so
     * what comes out is a list, not a set; jm_pattern_order makes it one. */
    const int64_t cap = s->nvar / SPARSE_ALPHA_DEN;
    int64_t np = 0, touched = 0;

    /* The rows to visit, ascending either way. */
    const bool sparse_rows = s->nrpat >= 0;
    const int64_t nvisit = sparse_rows ? s->nrpat : s->nrow;
    int64_t nfound = 0;

    for (int64_t k = 0; k < nvisit; k++) {
        int64_t i = sparse_rows ? s->rpat[k] : k;
        double w = s->rho[i];
        if (w == 0.0)
            continue;
        /* The dense branch leaves the pattern behind it, ascending and
         * complete, which is what pivot's exact weight reads (D42). */
        if (!sparse_rows)
            s->rpat[nfound++] = i;
        /* A logical's column is -e_i: row i alone writes slot ncol+i. */
        int64_t lg = s->ncol + i;
        if (np < cap)
            s->apat[np] = lg;
        np++;
        s->alpha[lg] = -w;
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            int64_t c = m->ar_index[p];
            double prev = s->alpha[c];
            if (prev == 0.0) {
                if (np < cap)
                    s->apat[np] = c;
                np++;
            }
            s->alpha[c] = prev + w * s->arv[p];
        }
        touched += m->ar_start[i + 1] - m->ar_start[i];
    }
    if (!sparse_rows)
        s->nrpat = nfound;

    /* A basic variable prices to zero by definition. They stay in the
     * pattern: the next clear works from it, and every consumer skips a
     * basic on its status. Which walk is cheaper is two loop lengths. */
    const bool sparse_zero = np <= cap && np < s->nrow;
    if (sparse_zero) {
        for (int64_t k = 0; k < np; k++) {
            int64_t v = s->apat[k];
            if (s->status[v] == JM_BASIC)
                s->alpha[v] = 0.0;
        }
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            s->alpha[s->basis[i]] = 0.0;
    }

    /* The reset above and the clear at the top are not charged (PLAN 2.11). */
    jm_work_add(&s->work, (touched + nvisit) * JM_WORK_NONZERO);

    if (np > cap) {
        s->anpat = -1;   /* too dense to be worth walking, and incomplete */
        return;
    }
    int64_t words = 0;
    s->anpat = jm_pattern_order(np, s->apat, s->amark, s->nvar, &words);
    jm_work_add(&s->work, (np + words + s->anpat) * JM_WORK_NONZERO);
}

/* Row r of `B^-1` into `rho`, and row r of `B^-1 M` into `alpha`, shared
 * by three callers. Leaves `nrpat` and `anpat` describing the patterns,
 * or negative where one was too dense to be worth carrying. */
static void build_pricing_row(sx *s, int64_t r)
{
    memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
    s->rho[r] = 1.0;

    /* Ordered so price_all's sums come out the way the column-wise pass
     * produced them (D35). */
    int64_t nr = 0, words = 0;
    jm_lu_btran_sparse(&s->lu, s->rho, &s->work, s->rpat, &nr);
    if (nr * SPARSE_RHO_DEN <= s->nrow) {
        s->nrpat = jm_pattern_order(nr, s->rpat, s->rmark, s->nrow, &words);
        jm_work_add(&s->work, (nr + words + s->nrpat) * JM_WORK_NONZERO);
    } else {
        s->nrpat = -1;   /* too dense to be worth ordering; scan instead */
    }

    price_all(s);
}

/* Builds row r of B^-1 M and picks the entering variable from it. May also
 * swap nonbasics between their bounds. */
static int64_t price_and_select(sx *s, int64_t r, bool below,
                                double violation, double *theta_dual)
{
    build_pricing_row(s, r);
    return dual_ratio_test(s, below, violation, theta_dual);
}

/* Cost shifting [1]. Puts one nonbasic reduced cost back on the feasible
 * side by moving its cost there, and writes down what it moved by.
 * Shifting a nonbasic's cost changes only its own reduced cost. Repaid in
 * settle_shifts. The record is what the cost actually moved by, not what
 * was asked for (D125). `d[v] = 0.0` stays (D126). */
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

    const double before = s->cost[v];
    s->cost[v] += need;
    s->shift[v] += s->cost[v] - before;
    s->d[v] = 0.0;
}

/* One variable's share of the dual step, and the repair that follows it. */
static void update_dual(sx *s, int64_t v, int64_t q, double theta_dual)
{
    if (s->status[v] == JM_BASIC || v == q)
        return;
    s->d[v] -= theta_dual * s->alpha[v];
    if (!s->in_phase1)
        shift_to_feasible(s, v);
}

/* Applies the basis change: q enters at position r, the variable there
 * leaves to the bound it violated. `*took` says whether it happened: a
 * declined pivot leaves every field exactly as it found them and asks for
 * a refactorization, so the caller must not bill an iteration for it. */
static jaos_status pivot(sx *s, int64_t r, int64_t q, bool below,
                         double theta_dual, bool *took)
{
#ifndef NDEBUG
    /* Counted on entry rather than at the basis change, so a declined pivot
     * counts too. That is the conservative direction and it is the version
     * 02-146 measured (D233). */
    s->dbg_piv_since_verify++;
#endif
    int64_t leaving = s->basis[r];
    double bound = below ? s->lo[leaving] : s->up[leaving];
    double alpha_q = s->alpha[q];

    /* The entering column, transformed, before anything is mutated: that
     * ordering is the stability trigger (D86). `raw` is kept because the
     * LU update wants the column untransformed. */
    var_column(s, q, s->raw);
    memcpy(s->col, s->raw, (size_t)s->nrow * sizeof *s->col);
    {
        int64_t nc = 0;
        jm_lu_ftran_sparse(&s->lu, s->col, &s->work, s->cpat, &nc);
        s->ncpat = nc * SPARSE_COL_DEN <= s->nrow ? nc : -1;
    }

    /* If the two disagree, ask for a rebuild and hand the iteration back
     * unspent. Only when `n_updates > 0`: on a fresh factorization refusing
     * would loop forever, so there the pivot is taken. */
    {
        double a = fabs(alpha_q), c = fabs(s->col[r]);
        double big = a > c ? a : c;
        if (big > 0.0 && fabs(alpha_q - s->col[r]) > LU_AGREE_TOL * big &&
            s->lu.n_updates > 0) {
            s->needs_refactor = true;
            s->n_stability++;
            *took = false;
            return JAOS_OK;
        }
    }
    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;

    /* A variable the pricing row does not touch takes no step, so the
     * pattern is enough to move every cost that moves. It is not enough for
     * the repair: `shift_to_feasible` is a no-op only on a cost already
     * feasible, which holds for the skipped ones only while `duals_dirty`
     * is clear. */
    if (s->duals_dirty || s->anpat < 0) {
        for (int64_t v = 0; v < s->nvar; v++)
            update_dual(s, v, q, theta_dual);
        s->duals_dirty = false;
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    } else {
        for (int64_t t = 0; t < s->anpat; t++)
            update_dual(s, s->apat[t], q, theta_dual);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    }
    s->d[leaving] = -theta_dual;
    s->d[q] = 0.0;

    /* Steepest-edge weights, while the old basis is still in force. rho
     * still holds row r of B^-1 from build_pricing_row, the one piece of
     * state this function inherits rather than derives. */
    memcpy(s->tau, s->rho, (size_t)s->nrow * sizeof *s->tau);
    jm_lu_ftran(&s->lu, s->tau, &s->work);

    /* rho is row r of B^-1, so its squared norm is the exact weight for
     * that row. Summed over rho's pattern where price_all left one; the
     * skipped zeros contribute `0.0 * 0.0` to a sum of squares, so the
     * total is bit for bit the same. */
    double exact = 0.0;
    if (s->nrpat >= 0) {
        for (int64_t k = 0; k < s->nrpat; k++) {
            double v = s->rho[s->rpat[k]];
            exact += v * v;
        }
        jm_work_add(&s->work, s->nrpat * JM_WORK_NONZERO);
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            exact += s->rho[i] * s->rho[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }

    const bool sparse_col = s->ncpat >= 0;
    if (jm_dse_update(s->nrow, s->dse, r, s->col, s->tau, exact, DSE_DRIFT,
                      sparse_col ? s->cpat : nullptr, s->ncpat))
        s->n_weight_restart++;
    jm_work_add(&s->work,
                (sparse_col ? s->ncpat : s->nrow) * JM_WORK_NONZERO);

    /* A row the column does not reach does not move. */
    double q_value = nonbasic_value(s, q);
    if (sparse_col) {
        for (int64_t k = 0; k < s->ncpat; k++) {
            int64_t i = s->cpat[k];
            s->xb[i] -= theta_primal * s->col[i];
        }
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= theta_primal * s->col[i];
    }
    s->xb[r] = q_value + theta_primal;

    /* The bitmap moves with the status, on the same lines. */
    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
    jm_nonbasic_insert(s->nbmark, leaving);
    s->where[leaving] = -1;
    s->basis[r] = q;
    s->status[q] = JM_BASIC;
    jm_nonbasic_remove(s->nbmark, q);
    s->where[q] = r;

    /* The leaving variable's reduced cost is minus the dual step. Checked
     * rather than assumed; skipped in phase 1. */
    if (!s->in_phase1)
        shift_to_feasible(s, leaving);

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

/* A nonbasic with a wrong-signed reduced cost can sometimes be put right
 * for nothing: at its other bound the opposite sign is what feasibility
 * means. Taken only when every basic stays inside its bounds; any wrong
 * sign at all qualifies. A column carrying an invented bound is left
 * alone: parking it there would publish a value nothing authorised. The
 * rest is `primal_cleanup`'s (D30). */
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
            if (x < s->lo[b] - s->primal_tol ||
                x > s->up[b] + s->primal_tol) {
                safe = false;
                break;
            }
            /* A basic left sitting on an invented bound would be published
             * at a value the model never allowed. Refused. */
            if ((s->fake[b] == FAKE_LO && x <= s->lo[b] + s->primal_tol) ||
                (s->fake[b] == FAKE_UP && x >= s->up[b] - s->primal_tol)) {
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

/* Calls in every cost the solve borrowed. Says whether anything was owed.
 * RESTORED from `cost0` rather than subtracted back out (D121). The test is
 * on the COST and not on the record alone: a cost can move while its
 * record cancels back to exactly zero. */
static bool repay_shifts(sx *s)
{
    bool any = false;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->cost[v] == s->cost0[v] && s->shift[v] == 0.0)
            continue;
        s->cost[v] = s->cost0[v];
        s->shift[v] = 0.0;
        any = true;
    }
    return any;
}

/* Calls in the loans, recomputes the duals from the model's own costs,
 * then repairs what the true costs leave infeasible. */
static void settle_shifts(sx *s)
{
    if (!repay_shifts(s))
        return;

    compute_duals(s, false);
    repair_dual_infeasibility(s);
}

/* --------------------------------------------------------------------- */
/* Re-entry after settling                                               */
/* --------------------------------------------------------------------- */

/* The solve loop, which a re-entry runs again. Defined with the driver. */
static jaos_status run(sx *s, jaos_solve_status *out);

/* How far this nonbasic's reduced cost points the wrong way, or zero.
 * Measured against DUAL_TOL, what the rest of the solve calls zero. */
static double dual_breach(const sx *s, int64_t v)
{
    switch (s->status[v]) {
    case JM_AT_LOWER: return s->d[v] < -s->dual_tol ? -s->d[v] : 0.0;
    case JM_AT_UPPER: return s->d[v] > s->dual_tol ? s->d[v] : 0.0;
    case JM_FREE:     return fabs(s->d[v]) > s->dual_tol ? fabs(s->d[v]) : 0.0;
    case JM_BASIC:    break;
    }
    return 0.0;   /* basic: its reduced cost is zero by definition */
}

/* The same condition, read in the space the answer is published in. The
 * two readings disagree about whether there is anything there (D27), and
 * neither may replace the other (D92). The scale factors are always
 * populated, 1.0 when unscaled. */
static double published_breach(const sx *s, int64_t v)
{
    const jaos_model *m = s->m;
    const double d = v < s->ncol ? s->d[v] / m->col_scale[v]
                                 : s->d[v] * m->row_scale[v - s->ncol];
    switch (s->status[v]) {
    case JM_AT_LOWER: return d < -s->dual_tol ? -d : 0.0;
    case JM_AT_UPPER: return d > s->dual_tol ? d : 0.0;
    case JM_FREE:     return fabs(d) > s->dual_tol ? fabs(d) : 0.0;
    case JM_BASIC:    break;
    }
    return 0.0;   /* basic: its reduced cost is zero by definition */
}

/* Is there a sign-condition breach here at all, in either space? The only
 * breach question that takes the union (D92). */
static bool breached(const sx *s, int64_t v)
{
    return dual_breach(s, v) != 0.0 || published_breach(s, v) != 0.0;
}

/* Everything a re-entry is allowed to write. Restoring these five and
 * rebuilding lands on exactly the saved point: `where` is the inverse of
 * `basis`, `xb` and `d` are derived by compute_primal and compute_duals,
 * and the factorization is of `basis`. */
static bool save_settled(sx *s)
{
    if (s->sav_status == nullptr) {
        s->sav_status = jm_alloc_array(s->nvar, sizeof *s->sav_status);
        s->sav_basis  = jm_alloc_array(s->nrow, sizeof *s->sav_basis);
        s->sav_lo     = jm_alloc_array(s->nvar, sizeof *s->sav_lo);
        s->sav_up     = jm_alloc_array(s->nvar, sizeof *s->sav_up);
        s->sav_fake   = jm_alloc_array(s->nvar, sizeof *s->sav_fake);
        if (!s->sav_status || !s->sav_basis || !s->sav_lo || !s->sav_up ||
            !s->sav_fake)
            return false;
    }
    memcpy(s->sav_status, s->status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->sav_basis, s->basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->sav_lo, s->lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->sav_up, s->up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->sav_fake, s->fake, (size_t)s->nvar * sizeof *s->fake);
    return true;
}

/* The objective of the point as it stands, on the model's own costs
 * (`cost0`, not `cost - shift`, D121). Scaling cancels. */
static double settled_objective(const sx *s)
{
#ifndef NDEBUG
    /* The precondition: every caller reaches this with the loans settled. */
    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]);
#endif
    /* Compensated, because this number RANKS two points (D175). */
    double sum = 0.0, comp = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double x = s->status[v] == JM_BASIC ? s->xb[s->where[v]]
                                                  : nonbasic_value(s, v);
        const double c = s->cost0[v];
        const double t = c * x;
        jm_obj_add(&sum, &comp, t);
        const double e = jm_two_product_residue(c, x, t);
        if (e != 0.0)
            jm_obj_add(&sum, &comp, e);
    }
    /* An inf or NaN partial sum carries no residue (D165). */
    return (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

/* The worst dual sign violation the point carries, in the model's own
 * space (D50); this is what the checker judges. The tolerance is applied
 * after the conversion, not before (D92). */
static double settled_dual_violation(const sx *s)
{
    double worst = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        double br = published_breach(s, v);
        if (br > worst)
            worst = br;
    }
    return worst;
}

/* Ranks two points (D89), lexicographically: defensible first, close
 * second. A dual violation inside tolerance beats one outside, whatever
 * the objectives; between two inside, the lower objective wins; between
 * two outside, the smaller violation. Both are in the model's own space. */
static bool better_point(double tol, double dviol_a, double obj_a,
                         double dviol_b, double obj_b)
{
    const bool a_ok = dviol_a <= tol, b_ok = dviol_b <= tol;
    if (a_ok != b_ok)
        return a_ok;
    return a_ok ? obj_a < obj_b : dviol_a < dviol_b;
}

static bool save_best(sx *s)
{
    if (s->bst_status == nullptr) {
        s->bst_status = jm_alloc_array(s->nvar, sizeof *s->bst_status);
        s->bst_basis  = jm_alloc_array(s->nrow, sizeof *s->bst_basis);
        s->bst_lo     = jm_alloc_array(s->nvar, sizeof *s->bst_lo);
        s->bst_up     = jm_alloc_array(s->nvar, sizeof *s->bst_up);
        s->bst_fake   = jm_alloc_array(s->nvar, sizeof *s->bst_fake);
        if (!s->bst_status || !s->bst_basis || !s->bst_lo || !s->bst_up ||
            !s->bst_fake)
            return false;
    }
    memcpy(s->bst_status, s->status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->bst_basis, s->basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->bst_lo, s->lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->bst_up, s->up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->bst_fake, s->fake, (size_t)s->nvar * sizeof *s->fake);
    s->bst_obj = settled_objective(s);
    s->bst_dviol = settled_dual_violation(s);
    s->bst_valid = true;
    return true;
}

/* Takes the best point only if it beats where the loop stopped. */
static jaos_status take_best_if_better(sx *s, bool *ok)
{
    *ok = true;
    if (!s->bst_valid ||
        !better_point(s->dual_tol, s->bst_dviol, s->bst_obj,
                      settled_dual_violation(s), settled_objective(s)))
        return JAOS_OK;

    repay_shifts(s);
    memcpy(s->status, s->bst_status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->basis, s->bst_basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->lo, s->bst_lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->up, s->bst_up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->fake, s->bst_fake, (size_t)s->nvar * sizeof *s->fake);

    for (int64_t v = 0; v < s->nvar; v++)
        s->where[v] = -1;
    for (int64_t i = 0; i < s->nrow; i++)
        s->where[s->basis[i]] = i;
    /* The memcpy replaced every status at once, so the bitmap is rebuilt. */
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    s->needs_refactor = true;
    set_verified(s, false);
    return refresh(s, ok, true);
}

/* Puts back the saved point and rebuilds everything that hangs off it. The
 * costs are returned to the model's own first. */
static jaos_status restore_settled(sx *s, bool *ok)
{
    repay_shifts(s);
    memcpy(s->status, s->sav_status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->basis, s->sav_basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->lo, s->sav_lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->up, s->sav_up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->fake, s->sav_fake, (size_t)s->nvar * sizeof *s->fake);

    for (int64_t v = 0; v < s->nvar; v++)
        s->where[v] = -1;
    for (int64_t i = 0; i < s->nrow; i++)
        s->where[s->basis[i]] = i;
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    s->needs_refactor = true;
    return refresh(s, ok, true);
}

/* Everything that went into `d_j`: `|c_j|` plus the magnitudes of the terms
 * of `y' M_j`. `y` is whatever compute_duals last left in `s->y`, which is
 * why the candidates are chosen before any of them moves. */
static double column_traffic(const sx *s, int64_t v)
{
    double t = fabs(s->cost[v]);
    if (v >= s->ncol)
        return t + fabs(s->y[v - s->ncol]);     /* logicals enter as -I */

    const jaos_model *m = s->m;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        t += fabs(s->y[m->a_index[k]] * s->av[k]);
    return t;
}

/* Worth a flip when the wrong sign carries objective behind it. The term in
 * `P − D` is `|d|` times the width of the box (D24), a product `publish`
 * leaves invariant, so it has no space. `|d|` counts only above the
 * rounding of its own dot product (NOISE_MARGIN). A column with no other
 * real bound contributes nothing; what it needs is a primal pivot. */
/* Everything that went into `alpha[q] = rho' M_q`: the magnitudes of its own
 * terms. `column_traffic`'s shape with the pricing row in place of `y`, and
 * billed the way `price_entry` bills the same walk. */
static double alpha_traffic(sx *s, int64_t v)
{
    if (v >= s->ncol) {
        jm_work_add(&s->work, JM_WORK_NONZERO);
        return fabs(s->rho[v - s->ncol]);   /* logicals enter as -I */
    }
    const jaos_model *m = s->m;
    double t = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        t += fabs(s->rho[m->a_index[k]] * s->av[k]);
    jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                          JM_WORK_NONZERO);
    return t;
}

/* Is the pricing row's pivot element unusable? Two separate questions, and
 * the constant they share is a coincidence of value and not of meaning.
 *
 * `PIVOT_MIN` is a STABILITY floor: `pivot` divides by this number and so
 * does `theta_dual`, and 1e-10 is as dangerous to divide by when it is exact
 * as when it is not. Every one of the thirteen calls it rejects over the
 * standard 94 has `|alpha[q]|` equal to its own traffic to the last digit —
 * a dot product with one term and no cancellation, which is the best
 * determined a number gets (D209).
 *
 * `PIVOT_MARGIN` is the NOISE floor D207 put on the column side, and this is
 * its mirror: below one ulp of the terms that produced it, a dot product has
 * no value to read. `scsd1` reaches a call at 0.35 ulps and pivots on it.
 *
 * The stability test runs first, so the traffic walk is skipped on every call
 * that is already rejected. `*min_alpha` receives whichever floor rejected
 * it, because the two mean different things and the caller says so. */
static bool alpha_unusable(sx *s, int64_t q, double *min_alpha)
{
    const double a = fabs(s->alpha[q]);
    *min_alpha = PIVOT_MIN;
    if (a < PIVOT_MIN)
        return true;
    const double rel = PIVOT_MARGIN * DBL_EPSILON * alpha_traffic(s, q);
    if (a < rel) {
        *min_alpha = rel;
        return true;
    }
    return false;
}

static bool can_move(const sx *s, int64_t v)
{
    double wrong_way;
    switch (s->status[v]) {
    case JM_AT_LOWER: wrong_way = s->d[v] < 0.0 ? -s->d[v] : 0.0; break;
    case JM_AT_UPPER: wrong_way = s->d[v] > 0.0 ? s->d[v] : 0.0; break;
    default:          return false;   /* free, or basic: nowhere to send it */
    }
    if (wrong_way == 0.0)
        return false;
    if (wrong_way <= NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v))
        return false;

    double other = s->status[v] == JM_AT_LOWER ? real_upper(s, v)
                                               : real_lower(s, v);
    if (!isfinite(other))
        return false;
    /* A rate against a rate, and in both spaces. `DUAL_TOL` bounds a
     * reduced cost at every other site that reads it; this tested a rate
     * times a distance against it until D214. `breached` gives up
     * neither space (D92) and is what `wants_a_pivot` applies to the
     * complementary case, a column with no other real bound (D27, D214). */
    return breached(s, v);
}

static bool anything_to_move(const sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++)
        if (can_move(s, v))
            return true;
    return false;
}

/* Makes the settled point dual feasible again. A column with a real bound
 * on the other side is sent to it. One with no other real bound has its
 * cost shifted instead. Only the first kind counts as movement, which is
 * why `anything_to_move` is asked first. */
static void arm_reentry(sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        if (can_move(s, v)) {
            s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                       : JM_AT_LOWER;
        } else if (dual_breach(s, v) != 0.0) {
            /* Its sign is still wrong and the ratio test must not meet a
             * reduced cost already past zero. The threshold stays DUAL_TOL
             * in the scaled space (D92). */
            shift_to_feasible(s, v);
        }
    }
}

/* --------------------------------------------------------------------- */
/* Primal clean-up, for a column with nowhere to rest                    */
/* --------------------------------------------------------------------- */

/* Is this column one that only a basis change can repair? A column with no
 * other real bound cannot be moved and its term in `P − D` is zero. The
 * filters that apply: past DUAL_TOL in either space (`breached`, D92), and
 * above the rounding of its dot product (D27). A nonbasic free variable
 * qualifies too, with `|d|` as its breach. */
static bool wants_a_pivot(const sx *s, int64_t v)
{
    if (!breached(s, v))
        return false;
    double wrong_way = fabs(s->d[v]);
    if (wrong_way <= NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v))
        return false;
    if (s->status[v] == JM_FREE)
        return true;   /* no bound in either direction; nothing left to ask */
    return !isfinite(s->status[v] == JM_AT_LOWER ? real_upper(s, v)
                                                : real_lower(s, v));
}

/* D207's floor, applied to the candidate list rather than to the scan: a
 * pivot must stand above PIVOT_MARGIN ulps of the column's largest entry.
 * Compacting the list is exact where D207's skipped second pass was not:
 * Harris's first pass takes a minimum over the candidates, so removing one
 * can change the winner, and every removal has to be applied. A floor that
 * would leave nothing keeps the list as it was: -1 means no declared bound
 * blocks, and both callers refuse on it (D207). */
static int64_t primal_apply_floor(sx *s, int64_t n, double cmax)
{
    const double rel = PIVOT_MARGIN * DBL_EPSILON * cmax;
    if (!(rel > PIVOT_MIN) || n == 0)
        return n;
    int64_t m = 0;
    for (int64_t k = 0; k < n; k++) {
        if (s->pden[k] < rel)
            continue;
        s->prow[m] = s->prow[k];
        s->pnum[m] = s->pnum[k];
        s->pden[m] = s->pden[k];
        m++;
    }
    jm_work_add(&s->work, n * JM_WORK_NONZERO);
    return m > 0 ? m : n;
}

/* Harris's two-pass ratio test in primal form (Gill, Murray, Saunders and
 * Wright 1989 section 3.2; `docs/research/harris-primal.md`; D212). `pnum`
 * is the exact distance to the blocking bound in the direction of travel,
 * never negative; `pden` the pivot magnitude. `jm_harris_pick` widens every
 * distance by `PRIMAL_HARRIS_DELTA * primal_tol`, takes the smallest quotient,
 * and returns the largest pivot whose exact quotient fits in it. The step
 * handed back is that exact quotient, so ONE relaxed step puts a basic at most
 * that width past its bound (D213). Consecutive relaxed steps are bounded by
 * nothing here, only by `refresh`; how far they reach is unmeasured and is
 * D213's second open question. Under Bland's rule the
 * exact minimum
 * with the lowest-index tie stays: the finiteness argument needs a fixed
 * rule and not a widened one (D26). Returns a candidate index, or -1. */
static int64_t primal_pick(sx *s, int64_t n, bool bland)
{
    if (n <= 0)
        return -1;
#ifndef NDEBUG
    for (int64_t k = 0; k < n; k++)
        assert(s->pden[k] > 0.0 && s->pnum[k] >= 0.0);
#endif
    if (!bland) {
        const double width = PRIMAL_HARRIS_DELTA * s->primal_tol;
        /* The phase-1 bound, asserted here and not beside the constant: a
         * comparison of floating constants is not an integer constant
         * expression, so `static_assert` cannot carry it (C23 6.7.11, and
         * `-Wpedantic -Werror` rejects it). This site is the stronger place
         * anyway, because it reads the per-model tolerance. The product
         * underflows to zero for a subnormal `primal_tol`, which
         * `jaos_set_primal_tolerance` accepts; zero is the no-relaxation width
         * and is admitted, so the ratio is asserted separately (D213). */
        assert(PRIMAL_HARRIS_DELTA > 0.0 && PRIMAL_HARRIS_DELTA <= 1.0);
        assert(width >= 0.0 && width <= s->primal_tol);
        const int64_t k = jm_harris_pick(n, s->pnum, s->pden, width);
        jm_work_add(&s->work, 2 * n * JM_WORK_NONZERO);
        return k;
    }
    int64_t best = -1;
    double best_step = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        const double t = s->pnum[k] / s->pden[k];
        if (jm_primal_row_wins(t, s->basis[s->prow[k]], best_step,
                               best >= 0 ? s->basis[s->prow[best]] : -1,
                               true)) {
            best_step = t;
            best = k;
        }
    }
    jm_work_add(&s->work, n * JM_WORK_NONZERO);
    return best;
}

/* Harris's zero step (GMSW 1989 section 3.3): when the chosen row already
 * stands past its bound, by at most the Harris width from an earlier relaxed
 * step, the step is zero and the blocking variable is kept. `pivot` derives
 * its step from `xb[r]`, so snapping `xb[r]` onto the bound here makes that
 * step exactly zero: the leaving variable goes nonbasic at its bound, the
 * entering one enters at its own, nothing else moves, and the residual in
 * `Ax = b` this leaves is the distance snapped away, one Harris width for
 * each relaxed step that put it there, until `refresh` recomputes the basics.
 * Without it the entering variable would land that width over `|alpha_q|`
 * past its own bound (D212). */
static void snap_if_past(sx *s, int64_t r, bool below)
{
    const int64_t v = s->basis[r];
    const double bound = below ? real_lower(s, v) : real_upper(s, v);
    if (below ? s->xb[r] < bound : s->xb[r] > bound)
        s->xb[r] = bound;
}

/* How far column q can travel before a basic variable reaches a bound.
 * Moving q by `dx` moves the basics by `-B^-1 M_q dx`. The direction is
 * read off the reduced cost, not the status. Only bounds the model
 * declared can stop it: a basic at rest on a lent bound would be published
 * at a value the model never allowed. If nothing real blocks, this returns
 * -1 and the column is left alone (D19). `*step` receives the distance to
 * the blocking position, `HUGE_VAL` when nothing blocks.
 *
 * It leaves `B^-1 M_q` in `s->col`, and a bound flip reads it there.
 * Anything writing `col` between this and that would be writing the flip's
 * input.
 *
 * `bland` is a parameter rather than a read of `s->bland` because one of
 * the two callers must not have it: `primal_cleanup` passes false; its
 * candidate set is a snapshot and each entry is pivoted at most once. */
static int64_t primal_ratio_test(sx *s, int64_t q, bool bland, bool *below,
                                 double *step)
{
    var_column(s, q, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t n = 0;
    double cmax = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];      /* per unit q travels */
        const double amove = fabs(move);
        if (amove > cmax)
            cmax = amove;
        if (!(amove >= PIVOT_MIN))
            continue;                              /* cannot be told from zero, or NaN */

        const int64_t b = s->basis[i];
        const double limit = move < 0.0 ? real_lower(s, b) : real_upper(s, b);
        if (!isfinite(limit))
            continue;

        double dist = move > 0.0 ? limit - s->xb[i] : s->xb[i] - limit;
        if (dist < 0.0)
            dist = 0.0;                            /* already there: degenerate */
        s->prow[n] = i;
        s->pnum[n] = dist;
        s->pden[n] = amove;
        n++;
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    n = primal_apply_floor(s, n, cmax);
    const int64_t k = primal_pick(s, n, bland);
    if (k < 0) {
        *step = HUGE_VAL;
        return -1;
    }
    const int64_t r = s->prow[k];
    *below = -dir * s->col[r] < 0.0;
    *step = s->pnum[k] / s->pden[k];
    if (*step == 0.0)
        snap_if_past(s, r, *below);
    return r;
}

/* Lets every column that wants a pivot have one, and reports how many.
 * `pivot()` needs `rho` and `alpha`, which `build_pricing_row` leaves
 * behind. The point stays primal feasible and the objective cannot rise.
 * Which columns want one is decided before any of them gets one (D30):
 * every pivot changes the basis the duals belong to. */
static jaos_status primal_cleanup(sx *s, int64_t *pivots)
{
    *pivots = 0;

    /* Borrowed: no dual iteration is in flight. */
    int64_t n = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        if (wants_a_pivot(s, v))
            s->cand[n++] = v;

    for (int64_t k = 0; k < n; k++) {
        int64_t q = s->cand[k];
        if (s->status[q] == JM_BASIC)
            continue;      /* an earlier pivot of this pass took it in */

        /* Call in this column's own loan before judging it. Shifting a
         * nonbasic's cost moves only its own reduced cost, which makes this
         * exact and local. Restored from cost0 (D121); `d` moves by the
         * amount the COST moved, not `shift[q]`. */
        if (s->cost[q] != s->cost0[q] || s->shift[q] != 0.0) {
            const double give_back = s->cost[q] - s->cost0[q];
            s->cost[q] = s->cost0[q];
            s->d[q] -= give_back;
            s->shift[q] = 0.0;
            s->duals_dirty = true;   /* q's cost may now breach its bound */
        }
        if (!breached(s, q))
            continue;      /* an earlier pivot of this pass really did fix it */

        bool below = false;
        double step = 0.0;
        /* `step` is unread here: `wants_a_pivot` admits only columns with no
         * declared bound in the improving direction. */
        int64_t r = primal_ratio_test(s, q, false, &below, &step);
        if (r < 0)
            continue;

        build_pricing_row(s, r);

        double min_alpha = 0.0;
        if (alpha_unusable(s, q, &min_alpha))
            continue;   /* the pricing row disagrees with the column: leave it */

        bool took = false;
        jaos_status st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took) {
            /* The factorization contradicted itself; let the caller's
             * refresh come round. */
            break;
        }

        /* Billed as the iterations they are (D16). */
        s->iters++;
        (*pivots)++;

        /* A failed basis update is reported by asking for a rebuild and
         * returning JAOS_OK. Leave now; the caller refreshes. */
        if (s->needs_refactor)
            break;
    }
    return JAOS_OK;
}

/* Hands a settled point back to the dual simplex. Anything other than a
 * second optimum is discarded and the settled point stands. A library
 * error propagates. Among optima the best point is kept (better_point,
 * D89). */
static jaos_status reenter_after_settling(sx *s)
{
    /* The point on entry is a candidate like any other (D89). */
    s->bst_valid = false;
    if (!save_best(s))
        return JAOS_ERR_OUT_OF_MEMORY;

    for (int64_t round = 0; round < SETTLE_ROUNDS; round++) {
        /* Asked before anything is saved: the saving is the whole cost of a
         * round with nothing to repair. */
        if (!anything_to_move(s)) {
            /* What can remain is a column with nowhere to move to. */
            if (!save_settled(s))
                return JAOS_ERR_OUT_OF_MEMORY;

            int64_t pivots = 0;
            jaos_status st = primal_cleanup(s, &pivots);
            if (st != JAOS_OK)
                return st;
            if (pivots == 0) {
                /* Out of work rather than out of rounds. */
                bool ok = false;
                st = take_best_if_better(s, &ok);
                if (st != JAOS_OK)
                    return st;
                return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
            }

            /* The basis has changed under the point; nothing may look at
             * the state before a refresh. */
            bool ok = false;
            set_verified(s, false);
            s->needs_refactor = true;
            st = refresh(s, &ok, true);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                st = restore_settled(s, &ok);
                if (st != JAOS_OK)
                    return st;
                if (!ok)
                    return JAOS_ERR_NUMERICAL;
                /* The refresh wrote a message on its way to `!ok` and the
                 * restore recovered from it; nothing failed. */
                s->m->err[0] = '\0';
                return JAOS_OK;
            }
            settle_shifts(s);
            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }
        if (!save_settled(s))
            return JAOS_ERR_OUT_OF_MEMORY;
        arm_reentry(s);

        /* The point changed, so any verification is spent. */
        set_verified(s, false);
        s->needs_refactor = true;

        jaos_solve_status again = JAOS_SOLVE_NOT_RUN;
        jaos_status st = run(s, &again);
        if (st != JAOS_OK)
            return st;

        if (again == JAOS_SOLVE_OPTIMAL) {
            settle_shifts(s);
            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }

        bool ok = false;
        st = restore_settled(s, &ok);
        if (st != JAOS_OK)
            return st;
        if (!ok)
            return JAOS_ERR_NUMERICAL;
        settle_shifts(s);
        st = take_best_if_better(s, &ok);
        if (st != JAOS_OK)
            return st;
        return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
    }

    /* The rounds ran out: the loop was oscillating. Publish the best (D89). */
    bool ok = false;
    jaos_status st = take_best_if_better(s, &ok);
    if (st != JAOS_OK)
        return st;
    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
}

/* --------------------------------------------------------------------- */
/* Reading the verdict                                                   */
/* --------------------------------------------------------------------- */

/* Is this column still held back by a bound JAOS invented? The evidence: a
 * nonbasic resting on its invented bound whose reduced cost still points
 * outwards past DUAL_TOL. This runs after settle_shifts. */
static bool held_by_an_invented_bound(const sx *s, int64_t j)
{
    if (s->fake[j] == FAKE_LO)
        return s->status[j] == JM_AT_LOWER && s->d[j] > s->dual_tol;
    if (s->fake[j] == FAKE_UP)
        return s->status[j] == JM_AT_UPPER && s->d[j] < -s->dual_tol;
    return false;
}

/* Would the objective run away if that bound were lifted? The line
 * dx_B = -B^-1 M_j dx_j is a ray of the original problem exactly when no
 * basic runs into a bound the model itself declared; lent bounds do not
 * count, which makes the verdict independent of ARTIFICIAL_BOUND. */
static bool improves_without_limit(sx *s, int64_t j)
{
    var_column(s, j, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    /* dx_j leaves a lower loan downwards and an upper loan upwards. */
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

/* The verdict on a point the bounded problem calls optimal. A ray off an
 * invented bound is unbounded; no column held by a loan is optimal. What
 * is left is a column stopped by a real constraint past the bound phase 1
 * lent: the solve refuses. Lifting the loan and re-solving is `TODO.md`
 * section 0 stage 7 (D188). */
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

/* Seconds since this solve started, and the only clock read in the solver.
 * A failed `clock_gettime` reads as zero elapsed: the budget becomes
 * infinite, not exhausted. */
static double elapsed_seconds(const sx *s)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0.0;
    return (double)(now.tv_sec - s->started.tv_sec) +
           1e-9 * (double)(now.tv_nsec - s->started.tv_nsec);
}

static bool out_of_time(const sx *s)
{
    if (s->m->cfg.time_limit <= 0.0)
        return false;
    return elapsed_seconds(s) >= s->m->cfg.time_limit;
}

/* --------------------------------------------------------------------- */
/* The primal method                                                     */
/* --------------------------------------------------------------------- */

/* How far the worst basic is outside a bound the model declared.
 * `real_lower`/`real_upper`, not `lo`/`up`: a basic outside a bound dual
 * phase 1 invented is not primal infeasible. */
static double primal_worst_violation(const sx *s)
{
    double worst = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        double viol = 0.0;
        if (isfinite(lo) && lo - s->xb[i] > viol)
            viol = lo - s->xb[i];
        if (isfinite(up) && s->xb[i] - up > viol)
            viol = s->xb[i] - up;
        if (viol > worst)
            worst = viol;
    }
    return worst;
}

/* The entering column, by Dantzig's rule; -1 when none is eligible (D81,
 * D82, D84). Eligibility is `dual_breach` and not `breached` (D92). A
 * fixed column never enters. Ties go to the lowest index, which the strict
 * `>` gives (D8). `*total` receives the sum of every breach. */
static int64_t primal_price(sx *s, double *total)
{
    /* Decided before the loop, so one iteration uses one rule throughout. */
    if (!s->bland &&
        s->iters - s->last_gain > STALL_FACTOR * (s->nrow + s->ncol + 1)) {
        s->bland = true;
        s->n_bland++;
        jm_log(s->m, JAOS_LOG_DETAIL,
               "iter %lld: no progress for %lld iterations, switching to "
               "Bland's rule", (long long)s->iters,
               (long long)(s->iters - s->last_gain));
    }

    int64_t best = -1;
    double best_breach = 0.0;
    double sum = 0.0;

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        if (s->lo[v] == s->up[v])
            continue;              /* fixed: nowhere to go */
        const double breach = dual_breach(s, v);
        if (breach == 0.0)
            continue;
        sum += breach;

        /* Under Bland's rule: the lowest-indexed eligible one (D26). */
        if (s->bland) {
            if (best < 0)
                best = v;          /* ascending scan: the first is the lowest */
            continue;
        }
        if (breach > best_breach) {
            best_breach = breach;
            best = v;
        }
    }
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    *total = sum;
    return best;
}

/* q crosses its own box to the other bound; no basis changes. Only the
 * point moves, by `-delta * B^-1 M_q`, read out of `s->col` where the
 * ratio test left it. The caller has established that no basic blocks
 * sooner. */
static void primal_bound_flip(sx *s, int64_t q, double delta)
{
#ifndef NDEBUG
    /* `s->col` must still hold `B^-1 M_q` from the ratio test, and `s->col`
     * has five other writers, two of which alias it as `rhs`. `memcmp` and
     * not `==`: a NaN compares unequal to itself, and `+0.0` written over
     * `-0.0` compares equal. `dbg_col` is its own buffer, never the shared
     * scratch. `s->work` is saved and restored so a debug build bills what
     * the release build bills. */
    {
        double *chk = s->dbg_col;
        const jm_work saved = s->work;
        var_column(s, q, chk);
        jm_lu_ftran(&s->lu, chk, &s->work);
        s->work = saved;
        assert(memcmp(chk, s->col, (size_t)s->nrow * sizeof *chk) == 0);
    }
#endif
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= delta * s->col[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    s->status[q] = s->status[q] == JM_AT_LOWER ? JM_AT_UPPER : JM_AT_LOWER;
}

/* --------------------------------------------------------------------- */
/* Primal phase 1                                                        */
/* --------------------------------------------------------------------- */

/* Builds the phase-1 cost vector and reports the total infeasibility it
 * measures: `-1` on a basic below a bound the model declared, `+1` on one
 * above, `0` elsewhere. `real_lower`/`real_upper`, never `lo`/`up`. The
 * tolerance is `primal_tol`, the same one `primal_worst_violation` uses. */
static double primal_phase1_costs(sx *s)
{
    /* Clear only what the last call set; see `c1_at`. */
    const int64_t cleared = s->n_c1_at;
    for (int64_t k = 0; k < cleared; k++)
        s->c1[s->c1_at[k]] = 0.0;
    s->n_c1_at = 0;

    double total = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        if (isfinite(lo) && s->xb[i] < lo - s->primal_tol) {
            s->c1[v] = -1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += lo - s->xb[i];
        } else if (isfinite(up) && s->xb[i] > up + s->primal_tol) {
            s->c1[v] = 1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += s->xb[i] - up;
        }
    }
    /* At most one append per row, because only a basic variable can be
     * infeasible (D199), and `c1_at` is allocated at `nrow`. A third branch
     * in the loop above, or a wider bound on it, writes past the heap (D232). */
    assert(s->n_c1_at <= s->nrow);
    /* What was cleared, plus the rows scanned (D198, D199). */
    jm_work_add(&s->work, (cleared + s->nrow) * JM_WORK_NONZERO);
    return total;
}

/* Phase-1 reduced costs into `d`, by lending `compute_duals` a different
 * objective for one call. The swap is a pointer and the restore is
 * unconditional, so `cost` is the same array on the way out. `refine` is
 * not offered: phase-1 duals are rebuilt every iteration (D29). */
static void primal_phase1_duals(sx *s)
{
    /* The restore below is unconditional, so `cost` is never the lent array
     * on the way in. A re-entrant call that left it lent would have phase 2
     * optimising the phase-1 objective (D232). */
    assert(s->cost != s->c1);
    double *real_cost = s->cost;
    s->cost = s->c1;
    compute_duals(s, false);
    s->cost = real_cost;
}

/* Which bound row i's basic lands on if it leaves: an infeasible one lands
 * on the bound it was travelling back to, a feasible one on the bound in
 * the direction of travel. Recomputed for the chosen row rather than kept
 * per candidate. */
static bool phase1_lands_low(const sx *s, int64_t i, double move)
{
    const int64_t v = s->basis[i];
    const double lo = real_lower(s, v), up = real_upper(s, v);
    if (isfinite(lo) && s->xb[i] < lo - s->primal_tol)
        return true;
    if (isfinite(up) && s->xb[i] > up + s->primal_tol)
        return false;
    return move < 0.0;
}

/* How far q may travel in phase 1 before a basic reaches a bound that
 * matters. A feasible basic must stay feasible, so it blocks at the
 * declared bound the way it travels. An infeasible one blocks at the bound
 * it travels back towards; travelling away, nothing blocks. Short-step
 * form (`docs/research/primal-simplex.md` section 4), with Harris's two
 * passes over the candidates (`primal_pick`). Relaxing an infeasible
 * basic's blocking bound by the Harris width lets it travel that far into
 * the feasible region, which is harmless (`docs/research/harris-primal.md`).
 * Returns the blocking position with `*below` saying which bound, or -1.
 * `*step` receives the distance. Leaves `B^-1 M_q` in `s->col`, as the
 * phase-2 test does. */
static int64_t primal_phase1_ratio(sx *s, int64_t q, bool bland, bool *below,
                                   double *step)
{
    var_column(s, q, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t n = 0;
    double cmax = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];      /* per unit q travels */
        const double amove = fabs(move);
        if (amove > cmax)
            cmax = amove;
        if (!(amove >= PIVOT_MIN))
            continue;                              /* cannot be told from zero, or NaN */

        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        const bool under = isfinite(lo) && s->xb[i] < lo - s->primal_tol;
        const bool over  = isfinite(up) && s->xb[i] > up + s->primal_tol;

        double limit;
        assert(phase1_lands_low(s, i, move) == (under || (!over && move < 0.0)));
        if (under) {
            if (move < 0.0)
                continue;                          /* going further under */
            limit = lo;
        } else if (over) {
            if (move > 0.0)
                continue;                          /* going further over */
            limit = up;
        } else {
            limit = move < 0.0 ? lo : up;
            if (!isfinite(limit))
                continue;
        }

        double dist = move > 0.0 ? limit - s->xb[i] : s->xb[i] - limit;
        if (dist < 0.0)
            dist = 0.0;                            /* already there: degenerate */
        s->prow[n] = i;
        s->pnum[n] = dist;
        s->pden[n] = amove;
        n++;
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    n = primal_apply_floor(s, n, cmax);
    const int64_t k = primal_pick(s, n, bland);
    if (k < 0) {
        *step = HUGE_VAL;
        return -1;
    }
    const int64_t r = s->prow[k];
    *step = s->pnum[k] / s->pden[k];
    *below = phase1_lands_low(s, r, -dir * s->col[r]);
    if (*step == 0.0)
        snap_if_past(s, r, *below);
    return r;
}

/* Drives the sum of bound violations to zero, from whatever basis it is
 * given, in place: no artificial variables, no second model. The costs are
 * rebuilt every iteration, because a pivot changes which basics are
 * infeasible. It refuses rather than declaring the model infeasible (D19). */
static jaos_status run_primal_phase1(sx *s, jaos_solve_status *out,
                                     bool *feasible)
{
    *feasible = false;
    /* Both pointers, because the body allocates both: a partial failure
     * must not skip the block on a later entry. `c1` is zeroed, because
     * `primal_phase1_costs` does not initialise it. */
    if (s->c1 == nullptr || s->c1_at == nullptr) {
        free(s->c1);    s->c1 = nullptr;
        free(s->c1_at); s->c1_at = nullptr;
        s->c1 = jm_calloc_array(s->nvar, sizeof *s->c1);
        s->c1_at = jm_alloc_array(s->nrow, sizeof *s->c1_at);
        s->n_c1_at = 0;
        if (s->c1 == nullptr || s->c1_at == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);
    const int64_t entered = s->iters;
    double best_total = HUGE_VAL;

    /* Phase 1's stall accounting is the SHARED `s->last_gain` and
     * `s->bland`; `run_primal` resets both before and after this call. */
    s->last_gain = s->iters;
    s->bland = false;

    for (;;) {
        /* The budgets end the solve here, and `*feasible` staying false says
         * so: phase 2 must not start from a point phase 1 did not finish. */
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        /* `infeas_best` carries the phase-1 total here; phase 2 sets it to
         * 0.0 at the hand-over. */
        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations in the primal phase 1 (%lld into the "
                             "solve, against a shared cap of %lld), the last "
                             "%lld without the total infeasibility improving; "
                             "this is a JAOS defect",
                       (long long)(s->iters - entered),
                       (long long)s->iters, (long long)iter_cap,
                       (long long)(s->iters - s->last_gain));
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            bool ok = false;
            jaos_status st = refresh(s, &ok, false);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                /* Soft, like every other copy of this gate. */
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }

        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock (D8). */
        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "phase 1, iter %lld: infeasibility %.6g, work %lld",
                   (long long)s->iters, total, (long long)s->work.units);
        if (total == 0.0) {
            *feasible = true;
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "phase 1 reached a feasible point in %lld iterations",
                   (long long)(s->iters - entered));
            return JAOS_OK;                        /* phase 2 next */
        }
        if (total < best_total) {
            best_total = total;
            s->infeas_best = total;   /* what a progress callback reads */
            s->last_gain = s->iters;
            s->bland = false;
        }

        /* `total` is a sum of bound violations and cannot rise under an
         * exact pivot. It rises when the basis has gone near singular and
         * `refresh` recomputes `xb` from it, which is what a run of pivots
         * on tiny elements leads to (D211). Past `PHASE1_RISE_MAX` no
         * further pivot on that basis has ever lowered it again (D218).
         * A non-finite total is the same failure at its extreme, and is
         * tested apart because `inf > inf` is false and the ratio below
         * would let it through for the rest of the solve.
         *
         * The message is the point of the branch as much as the exit: a
         * soft outcome with no sentence is what D205 removed.
         *
         * A ratio and not `best_total * (1 + max)`: `best_total` is
         * `HUGE_VAL` until the first total lands, and a product would
         * overflow to infinity and silently never fire. The `>` in front
         * short-circuits it on every descending iteration, which is nearly
         * all of them, so the division is not on the hot path. */
        if (!isfinite(total) ||
            (total > best_total &&
             total / best_total > 1.0 + PHASE1_RISE_MAX)) {
            /* Refuse on a recomputed point and never on a carried one, the
             * same D20 shape as the two exits below and as `alpha_unusable`.
             * `pivot` moves `xb` incrementally, so a rise read off carried
             * values can be the drift rather than the basis, and the
             * sentence this branch publishes is a claim about the basis. */
            if (!verified_fresh(s)) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "the primal phase 1's total infeasibility stands at "
                       "%.6g at iteration %lld, %.6g times its own best of "
                       "%.6g, on a freshly computed point; the basis it is "
                       "pivoting on is too ill-conditioned for another pivot "
                       "to repair the start",
                       total, (long long)s->iters, total / best_total,
                       best_total);
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        /* Decided once per iteration, before either choice is made. */
        if (!s->bland &&
            s->iters - s->last_gain > STALL_FACTOR * (s->nrow + s->ncol + 1)) {
            s->bland = true;
            s->n_bland++;
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "iter %lld: the primal phase 1 has not reduced its "
                   "infeasibility for %lld iterations, switching to Bland's "
                   "rule", (long long)s->iters,
                   (long long)(s->iters - s->last_gain));
        }

        primal_phase1_duals(s);

        /* The entering column on the phase-1 objective: Dantzig, with the
         * plain sign test rather than `dual_breach`. */
        int64_t q = -1;
        double best_d = s->dual_tol;
        for (int64_t v = 0; v < s->nvar; v++) {
            if (s->status[v] == JM_BASIC || s->lo[v] == s->up[v])
                continue;
            double gain;
            switch (s->status[v]) {
            case JM_AT_LOWER: gain = -s->d[v]; break;
            case JM_AT_UPPER: gain =  s->d[v]; break;
            case JM_FREE:     gain = fabs(s->d[v]); break;
            default:          continue;
            }
            /* Under Bland's rule: the lowest-indexed eligible one. */
            if (s->bland) {
                if (q < 0 && gain > s->dual_tol)
                    q = v;     /* ascending scan: the first is the lowest */
                continue;
            }
            if (gain > best_d) {
                best_d = gain;
                q = v;
            }
        }
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);

        if (q < 0) {
            /* Nothing improves, but read off carried numbers (D20). */
            if (!verified_fresh(s)) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv) {
                    /* The soft form: a solve outcome, not a library error. */
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "the primal phase 1 cannot reduce a total bound "
                       "violation of %.6g any further, on a freshly computed "
                       "point; reading that as an infeasible model needs the "
                       "proof D19 requires, and the columns may be held by "
                       "bounds dual phase 1 invented rather than by the "
                       "model", total);
            return JAOS_ERR_NUMERICAL;
        }

        bool below = false;
        double step = 0.0;
        int64_t r = primal_phase1_ratio(s, q, s->bland, &below, &step);

        /* q reaching its own opposite bound first is a flip, as in phase 2
         * (D189). */
        {
            const double other = s->d[q] < 0.0 ? real_upper(s, q)
                                               : real_lower(s, q);
            if (isfinite(other)) {
                const double delta = other - nonbasic_value(s, q);
                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);
                    set_verified(s, false);
                    s->iters++;
                    s->n_primal_iters++;
                    continue;
                }
            }
        }

        if (r < 0) {
            /* An impossibility, so the numbers had better be fresh (D20). */
            if (!verified_fresh(s)) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv) {
                    /* The soft form, as above. */
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "column %lld reduces the primal phase 1's objective "
                       "and no declared bound stops it, on a freshly computed "
                       "point, which cannot happen on an objective bounded "
                       "below by zero; this is a JAOS defect", (long long)q);
            return JAOS_ERR_NUMERICAL;
        }

        build_pricing_row(s, r);
        double min_alpha = 0.0;
        if (alpha_unusable(s, q, &min_alpha)) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            /* Which floor rejected it is the whole diagnosis: PIVOT_MIN means
             * the pivot is too small to divide by, the relative one means the
             * number is below the rounding of its own dot product. */
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization, against a "
                       "floor of %.6g; this is a JAOS defect",
                       (long long)q, s->alpha[q], (long long)r, min_alpha);
            return JAOS_ERR_NUMERICAL;
        }

        set_verified(s, false);
        bool took = false;
        /* `d` here holds phase-1 reduced costs, so `pivot()` maintains the
         * phase-1 pricing; the phase-2 costs are recomputed at hand-over. */
        jaos_status st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took)
            continue;

        s->iters++;
        s->n_primal_iters++;
    }
}

/* The primal simplex: phase 1 when the point it is given is not primal
 * feasible, then phase 2. Mirrors `run()` clause for clause. Every cold
 * start goes through phase 1 (D195). Phase 2 barely runs: after the first
 * phase-2 pivot the dual's re-entry solves the model (D194, D197); whether
 * to guard phase 2 too is open in `TODO.md` §0. A phase 1 that cannot
 * repair the start returns `NUMERICAL_ERROR`, never `INFEASIBLE` (D19). */
static jaos_status run_primal(sx *s, jaos_solve_status *out)
{
    s->dinfeas_best = HUGE_VAL;
    s->last_gain = s->iters;
    s->bland = false;
    /* `HUGE_VAL` until phase 1 has computed something; 0.0 only once the
     * point really is feasible. */
    s->infeas_best = HUGE_VAL;

    /* The warm start's cost sweep must not run for the primal: dual
     * infeasibility is what this method consumes. The per-iteration
     * `shift_to_feasible` inside `pivot()` still runs. */
    s->shift_pending = false;

    bool ok = false;
    jaos_status st = refresh(s, &ok, false);
    if (st != JAOS_OK)
        return st;
    if (!ok) {
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }

    if (primal_worst_violation(s) > s->primal_tol) {
        bool feasible = false;
        const int64_t phase1_entered = s->iters;
        s->in_phase1 = true;
        st = run_primal_phase1(s, out, &feasible);
        s->in_phase1 = false;
        /* Recorded here so it is written on EVERY exit from phase 1. */
        s->n_phase1_iters = s->iters - phase1_entered;
        if (st != JAOS_OK)
            return st;
        if (!feasible)
            return JAOS_OK;   /* a budget or an unrepairable basis ended it;
                               * `*out` says which */

        /* Phase 1 leaves `d` holding its own reduced costs, so they are
         * rebuilt here from a fresh factorization (D20). */
        s->needs_refactor = true;
        bool ok2 = false;
        st = refresh(s, &ok2, false);
        if (st != JAOS_OK)
            return st;
        if (!ok2) {
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        /* A phase 1 that returns `JAOS_OK` on a point still outside a bound
         * would hand phase 2 a start it has no invariant for. */
        const double left = primal_worst_violation(s);
        if (left > s->primal_tol) {
            jm_set_err(s->m,
                       "the primal phase 1 returned with a declared bound "
                       "still violated by %.6g; this is a JAOS defect", left);
            return JAOS_ERR_NUMERICAL;
        }

        /* Phase 2 starts its own stall accounting. */
        s->last_gain = s->iters;
        s->bland = false;
        s->dinfeas_best = HUGE_VAL;
    }

    /* Outside the branch: the point is feasible on both paths into here.
     * `include/jaos.h` licenses the infinity for the first call only. */
    s->infeas_best = 0.0;

    /* The cap is shared with phase 1 and the dual's re-entry, and the
     * CUMULATIVE `s->iters` is tested against it (D196); rebase the cap per
     * phase if `ITER_SANITY_FACTOR` ever drops below about 60. */
    const int64_t phase2_entered = s->iters;
    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "primal phase-2 iterations (%lld into the solve, "
                             "%lld of them phase 1, against a shared cap of "
                             "%lld), the last %lld without the total dual "
                             "infeasibility improving%s, %lld pivots declined "
                             "on factorization disagreement; this is a JAOS "
                             "defect",
                       (long long)(s->iters - phase2_entered),
                       (long long)s->iters, (long long)phase2_entered,
                       (long long)iter_cap,
                       (long long)(s->iters - s->last_gain),
                       s->bland ? ", under Bland's rule" : "",
                       (long long)s->n_stability);
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            st = refresh(s, &ok, false);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }

        double total = 0.0;
        int64_t q = primal_price(s, &total);

        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "iter %lld: best dual infeasibility %.6g, work %lld",
                   (long long)s->iters, s->dinfeas_best,
                   (long long)s->work.units);

        /* Improving turns Bland's rule back off (D26). */
        if (total < s->dinfeas_best) {
            s->dinfeas_best = total;
            s->last_gain = s->iters;
            s->bland = false;
        }

        if (q < 0) {
            /* Optimality for the costs in force, but read off carried
             * numbers (D20). */
            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        bool below = false;
        double step = 0.0;
        int64_t r = primal_ratio_test(s, q, s->bland, &below, &step);

        /* Does q reach its own opposite bound first? No basic can express
         * that limit. `real_upper`/`real_lower`, never `up`/`lo`: flipping
         * onto an invented bound would park a variable on a value the model
         * never declared. */
        {
            const double other = s->d[q] < 0.0 ? real_upper(s, q)
                                               : real_lower(s, q);
            if (isfinite(other)) {
                const double delta = other - nonbasic_value(s, q);
                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);
                    /* The point moved, so any verification is spent. The
                     * basis did not. */
                    set_verified(s, false);
                    s->iters++;
                    s->n_primal_iters++;
                    continue;
                }
            }
        }

        if (r < 0) {
            /* Nothing the model declared stops this column. Not declared
             * unbounded (D19): the column may be leaving a bound dual phase
             * 1 invented. That verdict is §0 stage 7. */
            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "column %lld improves and no declared bound stops it, "
                       "on a freshly computed point; reading that as an "
                       "unbounded ray needs the proof D19 requires, and the "
                       "column may be leaving a bound dual phase 1 invented "
                       "rather than one the model declared",
                       (long long)q);
            return JAOS_ERR_NUMERICAL;
        }

        build_pricing_row(s, r);
        double min_alpha = 0.0;
        if (alpha_unusable(s, q, &min_alpha)) {
            /* The pricing row disagrees with the column about the pivot. */
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld on a freshly "
                       "built factorization, against a floor of %.6g, which "
                       "no pivot can use; this is a JAOS defect",
                       (long long)q, s->alpha[q], (long long)r, min_alpha);
            return JAOS_ERR_NUMERICAL;
        }

        set_verified(s, false);
        bool took = false;
        st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took)
            continue;   /* declined and cost no iteration; see run() */

        s->iters++;
        s->n_primal_iters++;
    }
}

static jaos_status run(sx *s, jaos_solve_status *out)
{
    /* Each entry into the loop is its own solve: a re-entry (D25) must not
     * inherit a plateau counted against the pass before it. */
    s->infeas_best = HUGE_VAL;
    s->last_gain = s->iters;
    s->bland = false;

    bool ok = false;
    jaos_status st = refresh(s, &ok, false);
    if (st != JAOS_OK)
        return st;
    if (!ok) {
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        /* Asked on a fixed iteration count and never on a clock (D8). */
        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            /* A defect in JAOS, not a property of the model (D72). */
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations, the last %lld without the total "
                             "infeasibility improving%s, %lld pivots declined "
                             "on factorization disagreement; this is a JAOS "
                             "defect",
                       (long long)s->iters,
                       (long long)(s->iters - s->last_gain),
                       s->bland ? ", under Bland's rule" : "",
                       (long long)s->n_stability);
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            st = refresh(s, &ok, false);
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

        /* On a count and never on a clock (D8). */
        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "iter %lld: best infeasibility %.6g, work %lld",
                   (long long)s->iters, s->infeas_best,
                   (long long)s->work.units);
        if (r < 0) {
            /* Optimality is not accepted on carried numbers (D20): recompute
             * from a fresh factorization and price again. This refresh is
             * the one that refines (D29). */
            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            /* Optimal for the problem as bounded; classify_optimum decides
             * whether that is the original's answer. */
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        double theta_dual = 0.0;
        int64_t q = price_and_select(s, r, below, violation, &theta_dual);
        if (q < 0) {
            /* No entering column can repair row r: the dual is unbounded
             * and the primal has no feasible point. Read off carried
             * numbers (D20, D39). Take a second opinion first. */
            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;
        }

        /* The basis is about to change, so any verification is spent. */
        set_verified(s, false);
        bool took = false;
        st = pivot(s, r, q, below, theta_dual, &took);
        if (st != JAOS_OK)
            return st;

        /* A declined pivot changed nothing and costs no iteration. It
         * cannot spin: a rebuild leaves `n_updates` at zero, and the check
         * declines only above zero. */
        if (!took)
            continue;

        s->iters++;
    }
}

/* --------------------------------------------------------------------- */
/* Entry point                                                           */
/* --------------------------------------------------------------------- */

/* Solution buffers, kept across solves and resized only when the model
 * changes shape. Not static: jm_postsolve_expand and jm_postsolve_solved
 * call it on the caller's own model. */
jaos_status jm_model_ensure_solution_arrays(jaos_model *m)
{
    if (m->sol_col != nullptr && m->sol_row != nullptr &&
        m->sol_dual != nullptr && m->sol_redcost != nullptr &&
        m->sol_col_status != nullptr && m->sol_row_status != nullptr)
        return JAOS_OK;

    /* All six or none. A partial set must not read as "already there". */
    free(m->sol_col);        m->sol_col = nullptr;
    free(m->sol_row);        m->sol_row = nullptr;
    free(m->sol_dual);       m->sol_dual = nullptr;
    free(m->sol_redcost);    m->sol_redcost = nullptr;
    free(m->sol_col_status); m->sol_col_status = nullptr;
    free(m->sol_row_status); m->sol_row_status = nullptr;

    m->sol_col     = jm_alloc_array(m->num_col, sizeof(double));
    m->sol_row     = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_dual    = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_redcost = jm_alloc_array(m->num_col, sizeof(double));
    m->sol_col_status = jm_alloc_array(m->num_col, sizeof(jaos_basis_status));
    m->sol_row_status = jm_alloc_array(m->num_row, sizeof(jaos_basis_status));
    if (!m->sol_col || !m->sol_row || !m->sol_dual || !m->sol_redcost ||
        !m->sol_col_status || !m->sol_row_status) {
        free(m->sol_col);        m->sol_col = nullptr;
        free(m->sol_row);        m->sol_row = nullptr;
        free(m->sol_dual);       m->sol_dual = nullptr;
        free(m->sol_redcost);    m->sol_redcost = nullptr;
        free(m->sol_col_status); m->sol_col_status = nullptr;
        free(m->sol_row_status); m->sol_row_status = nullptr;
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    return JAOS_OK;
}

/* A published zero is a zero (D21). */
static double published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

/* Mapped rather than cast: a silent renumbering of either enum would
 * otherwise publish a wrong basis with no compile error. */
static jaos_basis_status published_status(jm_var_status st)
{
    switch (st) {
    case JM_BASIC:    return JAOS_BASIS_BASIC;
    case JM_AT_LOWER: return JAOS_BASIS_AT_LOWER;
    case JM_AT_UPPER: return JAOS_BASIS_AT_UPPER;
    case JM_FREE:     return JAOS_BASIS_FREE;
    }
    return JAOS_BASIS_BASIC;
}

/* `p` is the presolve workspace, always non-null: NONE when nothing was
 * reduced, in which case m == s->m already IS the caller's model. `(void)p`
 * covers the JAOS_NO_PRESOLVE build. */
static jaos_status publish(sx *s, jaos_solve_status status, jm_presolve *p)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    (void)p;

    m->solve_status = status;
    m->solve_iters = s->iters;
    /* The work snapshot is taken at the end of each path below, not here:
     * publishing itself runs a BTRAN (D16). */

    jaos_status st = jm_model_ensure_solution_arrays(m);
    if (st != JAOS_OK)
        return st;

    if (status != JAOS_SOLVE_OPTIMAL) {
        /* Zero rather than leave the previous solve's answer readable. */
        m->objective = 0.0;
        memset(m->sol_col, 0, (size_t)m->num_col * sizeof(double));
        memset(m->sol_row, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_dual, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_redcost, 0, (size_t)m->num_col * sizeof(double));

        /* The basis is written, kept, and only then cleared. Kept because a
         * budget stop, infeasible or unbounded all leave a good basis for
         * the next solve. Cleared because `jaos_basis` publishes the basis
         * behind an answer and there is no answer. A numerical failure is
         * the one outcome left out. */
        if (status == JAOS_SOLVE_WORK_LIMIT ||
            status == JAOS_SOLVE_TIME_LIMIT ||
            status == JAOS_SOLVE_INTERRUPTED ||
            status == JAOS_SOLVE_INFEASIBLE ||
            status == JAOS_SOLVE_UNBOUNDED) {
            for (int64_t j = 0; j < m->num_col; j++)
                m->sol_col_status[j] = published_status(s->status[j]);
            for (int64_t i = 0; i < m->num_row; i++)
                m->sol_row_status[i] =
                    published_status(s->status[m->num_col + i]);
            (void)jm_model_remember_basis(m);
        }
        memset(m->sol_col_status, 0,
               (size_t)m->num_col * sizeof *m->sol_col_status);
        memset(m->sol_row_status, 0,
               (size_t)m->num_row * sizeof *m->sol_row_status);
        m->solve_work = s->work.units;
        /* Seconds never enter a baseline (D17). */
        m->solve_time = elapsed_seconds(s);
#if !defined(JAOS_NO_PRESOLVE)
        if (p->outcome == JM_PRESOLVE_REDUCED) {
            jaos_status pst = jm_postsolve_expand(p);
            if (pst != JAOS_OK)
                return pst;
        }
#endif
        return JAOS_OK;
    }

#ifndef NDEBUG
    /* No loan may still be outstanding here: `sol_dual` is a BTRAN of
     * `s->cost` and `sol_redcost` is `s->d`. Only on this branch: a solve
     * that ends anywhere but OPTIMAL never calls `settle_shifts` and may
     * carry loans. The cost is compared as well as the record (D122). */
    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]);
#endif

    /* Back into the model's own units. A column carries its factor, a row
     * activity divides its own out; the duals go the other way. */
    const double *rho = m->row_scale, *gamma = m->col_scale;

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = published(gamma[j] * var_value(s, j));
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row[i] = published(var_value(s, m->num_col + i) / rho[i]);

    /* y = B^-T c_B, then undo the internal minimisation. */
    double *y = s->y;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_dual[i] = published(sigma * y[i] * rho[i]);
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_redcost[j] = published(sigma * s->d[j] / gamma[j]);

    /* The basis is published unscaled. */
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col_status[j] = published_status(s->status[j]);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row_status[i] = published_status(s->status[m->num_col + i]);

    /* From the values just written, compensated (D169). */
    jm_model_publish_objective(m);
    m->solve_work = s->work.units;
    m->solve_time = elapsed_seconds(s);

    /* Where the next solve will start. The failure is swallowed on purpose. */
    (void)jm_model_remember_basis(m);

#if !defined(JAOS_NO_PRESOLVE)
    /* m is the reduced model whenever p->outcome is REDUCED; here the
     * answer crosses back into the caller's own row and column space. */
    if (p->outcome == JM_PRESOLVE_REDUCED) {
        jaos_status pst = jm_postsolve_expand(p);
        if (pst != JAOS_OK)
            return pst;
    }
#endif
    return JAOS_OK;
}

jaos_status jm_dual_simplex(jaos_model *m)
{
    jm_presolve p;
    jm_presolve_init(&p);
    p.orig = m;

    /* A new solve owns these three, so no exit can publish the previous
     * one's. Three returns below run before any of them is written. */
    m->solve_iters = 0;
    m->solve_primal_iters = 0;
    m->solve_phase1_iters = 0;

    /* Presolve's own charge, continued on the same accumulator as the
     * solve's. Always {0}, even under JAOS_NO_PRESOLVE. */
    jm_work pre_work = {0};

#if !defined(JAOS_NO_PRESOLVE)
    /* A development switch, not an option (D64). */
    jaos_status pst = jm_presolve_run(m, &p, &pre_work);
    if (pst != JAOS_OK) {
        jm_presolve_free(&p);
        return pst;
    }

    /* Presolve ran first, so it reports first. */
    if (p.outcome == JM_PRESOLVE_NONE) {
        jm_log(m, JAOS_LOG_SUMMARY, "presolve: nothing fired");
    } else if (p.outcome == JM_PRESOLVE_INFEASIBLE ||
              p.outcome == JM_PRESOLVE_UNBOUNDED) {
        jm_log(m, JAOS_LOG_SUMMARY,
               "presolve: %s, no simplex run; "
               "empty_row=%lld empty_col=%lld singleton_row=%lld "
               "singleton_col=%lld free_col_singleton=%lld rounds=%lld",
               p.outcome == JM_PRESOLVE_INFEASIBLE ? "infeasible"
                                                    : "unbounded",
               (long long)p.counts.empty_row, (long long)p.counts.empty_col,
               (long long)p.counts.singleton_row,
               (long long)p.counts.singleton_col,
               (long long)p.counts.free_col_singleton,
               (long long)p.counts.rounds);
    } else {
        jm_log(m, JAOS_LOG_SUMMARY,
               "presolve: %lld rows, %lld columns, %lld nonzeros -> "
               "%lld rows, %lld columns, %lld nonzeros; "
               "fixed_col=%lld empty_row=%lld empty_col=%lld "
               "singleton_row=%lld singleton_col=%lld "
               "free_col_singleton=%lld rounds=%lld",
               (long long)m->num_row, (long long)m->num_col,
               (long long)m->num_nz, (long long)p.reduced.num_row,
               (long long)p.reduced.num_col, (long long)p.reduced.num_nz,
               (long long)p.counts.fixed_col, (long long)p.counts.empty_row,
               (long long)p.counts.empty_col,
               (long long)p.counts.singleton_row,
               (long long)p.counts.singleton_col,
               (long long)p.counts.free_col_singleton,
               (long long)p.counts.rounds);
    }
#endif

    if (p.outcome == JM_PRESOLVE_SOLVED) {
        /* Nothing is left for the simplex to run on. No sx is built. */
        jaos_status st = jm_postsolve_solved(&p);
        jm_presolve_free(&p);
        return st;
    }

    if (p.outcome == JM_PRESOLVE_INFEASIBLE ||
        p.outcome == JM_PRESOLVE_UNBOUNDED) {
        /* Proved by the reductions alone; no basis is ever built. */
        const jaos_solve_status status = (p.outcome == JM_PRESOLVE_INFEASIBLE)
            ? JAOS_SOLVE_INFEASIBLE : JAOS_SOLVE_UNBOUNDED;
        jaos_status st = jm_postsolve_infeasible_or_unbounded(&p, status);
        jm_presolve_free(&p);
        return st;
    }

    /* The only place the reduced/original distinction is made. */
    jaos_model *target = (p.outcome == JM_PRESOLVE_REDUCED) ? &p.reduced : m;
    m->presolve_num_row = target->num_row;
    m->presolve_num_col = target->num_col;
    m->presolve_num_nz  = target->num_nz;

    sx s;
    jaos_status st = sx_init(&s, target);
    if (st != JAOS_OK) {
        jm_presolve_free(&p);
        return st;
    }
    /* Seeded with presolve's charge, so the two are one accumulator. */
    s.work = pre_work;
    clock_gettime(CLOCK_MONOTONIC, &s.started);

    jm_log(m, JAOS_LOG_SUMMARY,
           "%s simplex: %lld rows, %lld columns, %lld nonzeros, "
           "primal tol %.3g, dual tol %.3g",
           m->cfg.force_primal ? "primal" : "dual",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, s.primal_tol, s.dual_tol);

    jaos_solve_status outcome;
    bool allow_warm = true;
    for (;;) {
        const bool warm = allow_warm && build_warm_basis(&s);
        if (!warm)
            build_initial_basis(&s);
        jm_log(m, JAOS_LOG_DETAIL, "starting from %s",
               warm ? "the basis on the model" : "the slack basis");
        /* Which method runs, and the only place that is decided.
         * `force_primal` is a development switch, not an option (D64).
         * Everything after this line is shared: `reenter_after_settling`
         * calls `run()`, so a forced-primal solve can still finish with
         * dual iterations. */
        st = m->cfg.force_primal ? run_primal(&s, &outcome)
                                 : run(&s, &outcome);
        if (st != JAOS_OK || outcome != JAOS_SOLVE_OPTIMAL)
            break;

        /* Settle first, then judge: the verdict reads the model's own
         * reduced costs, not the shifted ones. */
        settle_shifts(&s);
        st = reenter_after_settling(&s);
        if (st != JAOS_OK)
            break;

        /* The best point can carry a dual violation into an OPTIMAL verdict
         * (D146, D147), so it is read before publishing. Exact-zero on
         * purpose: settled_dual_violation counts only the excess beyond
         * dual_tol. An uncertified point from a WARM start is thrown away
         * whole and the solve restarts once, cold: the work stays on the
         * one accumulator (D16), the clock keeps its origin, the iteration
         * count restarts with the sx. An uncertified COLD start is
         * NUMERICAL_ERROR. The settle below is the guard's own contract: a
         * restore exit whose refresh fired repair_singular_basis has re-run
         * shift_to_feasible. */
        settle_shifts(&s);
        const double breach = settled_dual_violation(&s);
        if (breach != 0.0) {
            if (warm) {
                jm_log(m, JAOS_LOG_SUMMARY,
                       "the settled point from the supplied basis is not "
                       "dual feasible; restarting cold from the slack "
                       "basis after %lld iterations, %lld refactorizations, "
                       "%lld weight restarts, %lld stalls, %lld stability "
                       "rebuilds",
                       (long long)s.iters, (long long)s.n_refactor,
                       (long long)s.n_weight_restart, (long long)s.n_bland,
                       (long long)s.n_stability);
                const jm_work carried = s.work;
                const struct timespec t0 = s.started;
                sx_free(&s);
                st = sx_init(&s, target);
                if (st != JAOS_OK) {
                    jm_presolve_free(&p);
                    return st;
                }
                s.work = carried;
                s.started = t0;
                /* The warm attempt is thrown away, and so is anything it
                 * wrote to explain itself. */
                target->err[0] = '\0';
                allow_warm = false;
                continue;
            }
            jm_set_err(m, "the settled point is not dual feasible: a "
                          "reduced cost breaches its bound by %.6g after "
                          "settling, from a %s start; publishing that as "
                          "OPTIMAL would certify a point the reduced costs "
                          "do not support", breach, warm ? "warm" : "cold");
            outcome = JAOS_SOLVE_NUMERICAL_ERROR;
            break;
        }
        outcome = classify_optimum(&s);
        break;
    }

    /* The simplex writes its refusals with `jm_set_err(s->m, ...)`, and
     * `s->m` is `p.reduced` whenever presolve reduced one, which the caller
     * cannot see. Copied rather than redirected, because the indices in
     * these messages are the reduced model's; only when the solve failed.
     * `== NUMERICAL_ERROR` and not `!= OPTIMAL`: the buffer is not cleared
     * between a recovered failure and the verdict. `st != JAOS_OK` stays
     * FIRST: `outcome` is uninitialised on that branch. */
    if (target != m && target->err[0] != '\0' &&
        (st != JAOS_OK || outcome == JAOS_SOLVE_NUMERICAL_ERROR))
        memcpy(m->err, target->err, sizeof m->err);

    if (st == JAOS_OK)
        st = publish(&s, outcome, &p);

    /* Written on `m`, the caller's model, so no postsolve copy is needed.
     * The abandoned branch's total is tested AFTER `publish`: on the
     * presolve-reduced path `publish` can fail before `jm_postsolve_expand`
     * copies up. */
    if (st != JAOS_OK)
        m->solve_iters = s.iters;
    m->solve_primal_iters = s.n_primal_iters;
    m->solve_phase1_iters = s.n_phase1_iters;

    /* Both branches, and the failing one needs the counts more. */
    if (st == JAOS_OK)
        jm_log(m, JAOS_LOG_SUMMARY,
               "%s after %lld iterations, %lld work units; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds, %lld primal iterations, %lld of them "
               "phase 1",
               jaos_solve_status_str(outcome), (long long)s.iters,
               (long long)s.work.units, (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability, (long long)s.n_primal_iters,
               (long long)s.n_phase1_iters);
    else
        jm_log(m, JAOS_LOG_SUMMARY,
               "abandoned after %lld iterations, %lld work units: %s; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds, %lld primal iterations, %lld of them "
               "phase 1",
               (long long)s.iters, (long long)s.work.units,
               jaos_status_str(st), (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability, (long long)s.n_primal_iters,
               (long long)s.n_phase1_iters);

    sx_free(&s);
    jm_presolve_free(&p);
    return st;
}
