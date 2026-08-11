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

#include <float.h>
#include <math.h>
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

/* When the pricing row is read through its pattern instead of in full.
 *
 * `alpha = rho' M` is dense storage holding a sparse vector: over the
 * Kennington set its nonzeros are 0.1% of the variables on `ken-11` and
 * 83.6% on `osa-60`, and the consumers scan all of them either way. Below
 * this fraction the pattern is cheaper to walk than the array; above it the
 * indirection costs more than it saves, and the dense loop — sequential,
 * three arrays, no dependent loads — wins.
 *
 * A divisor rather than a fraction so that the crossover can be swept by
 * changing one integer, which is how it was chosen (D40). What the sweep
 * measures is work units, and those are blind to exactly the cache
 * behaviour that decides the other side of the trade, so this is set on the
 * conservative side of where the counter alone would put it. */
constexpr int64_t SPARSE_ALPHA_DEN = 4;

/* The same question for the pricing row itself, and it needed asking
 * separately: `rho` is 32.6% nonzero over the standard set and 1.0% over
 * Kennington, so a rule good for one is wrong for the other.
 *
 * Reading `rho` through a pattern costs marking it, emitting it in order,
 * and walking it — about three touches per nonzero — against one per row
 * for the scan it replaces. So it pays below roughly a third, and the
 * divisor is swept rather than argued (D43). Above it the pattern is
 * collected, found too large, and thrown away; the collection is a
 * comparison inside a pass the solve was making anyway. */
constexpr int64_t SPARSE_RHO_DEN = 4;

/* And the same question a third time, for the entering column's FTRAN.
 *
 * Cheaper to answer than the other two: nothing has to be ordered, because
 * every reader of this vector is elementwise, so the sparse form costs one
 * indirection per nonzero against one sequential read per row. It pays
 * further up than the other two for that reason (D44).
 *
 * **Set from seconds, not from work units, and they disagree** (D45). The
 * unit sweep was monotone all the way to "always" and would have put this at
 * 1; timed on the same machine, 8 beats 2 and 2 loses time on models where
 * the pricing row is dense. What the counter cannot see is that the loop
 * this replaces is a contiguous `a[i] -= b * c[i]` — about the cheapest unit
 * it ever charges — so removing those units buys much less than removing the
 * branchy per-variable ones D40 removed. It is the first constant in this
 * solver whose value contradicts its own work-unit measurement. */
constexpr int64_t SPARSE_COL_DEN = 8;

/* Refactorization interval, and the stability trigger PLAN 2.5.5 asked for
 * beside it. Until D86 only the interval and the reactive fallback on a
 * failed update existed, and an interval alone cannot notice that it has
 * become too long for a particular model. */
constexpr int64_t REFACTOR_EVERY = 64;

/* How far the two computations of the pivot element may disagree before the
 * factorization they both came through stops being trusted.
 *
 * `alpha_q` is row r of `B^-1` dotted with column q, which arrives by BTRAN;
 * `col[r]` is column q transformed by FTRAN. They are the same number in
 * exact arithmetic, so their relative difference is not a heuristic about
 * conditioning — it is the patched factorization contradicting itself, read
 * off two solves the iteration was already paying for.
 *
 * **Measured on both sides, and the interval between them is wide (D86).**
 * Over all 139 gate instances at the interval above, the worst disagreement
 * any pivot produces is 7.83e-08 and **not one reaches 1e-7**. On `pilot87`
 * at an interval of 128, where the solve grinds 1.38M iterations and the
 * guard then blames a defect that is not there, it reaches **1.99** — two
 * computations of one number differing by more than the number.
 *
 * The value inside that gap barely matters, which is the useful part: the
 * first pivot to cross 1e-7, 1e-6, 1e-5, 1e-4 *and* 1e-3 is the same one,
 * iteration 120880 of 1382801. The decay does not creep in, it arrives. So
 * this sits in the middle of a four-decade plateau, 128x above the worst
 * healthy pivot in the gate and 1e5 below the broken one. */
constexpr double LU_AGREE_TOL = 1e-5;

/* The clock is read once every this many iterations rather than every
 * iteration. Reading it cannot change which pivot is chosen (D8) — it only
 * decides whether to stop — and at this granularity the syscall cost
 * disappears while the cutoff stays responsive. */
constexpr int64_t TIME_CHECK_EVERY = 64;

/* How often a progress line is offered, in iterations. A count and not an
 * interval: output paced by a clock differs between two runs of the same
 * model, and the whole point of D8 is that nothing about a solve does. */
constexpr int64_t LOG_EVERY = 1000;

/* How often a watcher is asked whether to carry on, in iterations. A count
 * for LOG_EVERY's reason, and 64 rather than 1000 because this one decides
 * something: a caller who wants to interrupt wants it to take effect, and
 * TIME_CHECK_EVERY is already the granularity at which this solver has agreed
 * a stop is responsive enough. The call is skipped entirely when nobody is
 * watching, so a model with no callback pays one predictable branch. */
constexpr int64_t PROGRESS_EVERY = 64;

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

/* How long a solve may fail to improve before it is treated as cycling, as
 * a multiple of `nrow + ncol + 1` — the same normalisation the guard above
 * uses, and for the same reason: a plateau that is long for a small model
 * is nothing for a large one.
 *
 * The number has a measurement on both sides of it (D17). Across the
 * standard 94, the longest plateau on an instance that terminates is
 * `truss` at 1.67 of its size, and `dfl001` is next at 0.94; `grow15`,
 * which cycles, sits at 198. There are two orders of magnitude of daylight
 * between those, and 10 is six times clear of the worst healthy case and
 * twenty times inside the cycling one.
 *
 * What makes this a safe constant, unlike the perturbation size Q10 is
 * still waiting on, is that it cannot change an answer. It only decides
 * when to switch to a rule that is itself exact and terminating; too small
 * costs iterations, too large costs iterations, and neither costs
 * correctness. */
constexpr int64_t STALL_FACTOR = 10;

/* How far above the rounding of its own dot product a reduced cost has to
 * stand before the re-entry will act on it.
 *
 * `d_j = c_j − y' M_j` is a sum, and a sum is known no more finely than the
 * terms that went into it — the argument D23 makes for a row activity,
 * applied to a column. So `eps` times the traffic through the column is
 * where a reduced cost stops being a number and starts being what is left
 * of cancellation, and this is the margin above that.
 *
 * Measured over both feasible sets, on every column the re-entry would
 * consider moving. Five instances of the 110 have any: the ratio
 * `|d| / (eps · traffic)` is at least 5.055e8 on `etamacro`, `nesm`,
 * `pilot` and `pilot87`, and between 2.1 and 36 on `pds-20`, whose columns
 * are rounding noise carried by boxes a thousand wide. Seven orders of
 * daylight, and 1e5 is the geometric middle of it.
 *
 * The behaviour saturates: 1e5 and 1e7 give identical answers on all three
 * sets, so the number is not sitting on an edge. 1e3 also works and costs
 * `pds-20` 28% more work, which is what the margin is buying. */
constexpr double NOISE_MARGIN = 1e5;

/* How many times a settled point may be handed back to the dual simplex.
 *
 * Settling the shifts is what turns a solve that was optimal for a
 * convenient problem into an answer about the one that was asked, and it
 * can leave reduced costs pointing the wrong way (PLAN 2.8.1). Re-entry
 * repairs that by moving those columns and letting the method run again, so
 * each round is a whole solve.
 *
 * This is a backstop and not a limit meant to bind, in the same sense as
 * ITER_SANITY_FACTOR above. The loop has its own termination and it is the
 * real one: a round only begins if some column can still be moved or
 * pivoted, and rounds stop finding any.
 *
 * Measured over the standard 94, one round per solve is the flat cost of
 * asking: ninety instances open the loop once, find nothing to move and
 * leave. Four go further — `etamacro`, `greenbea` and `nesm` in two rounds,
 * `pilot` in twelve, `pilot87` in sixteen — and three of those need a
 * primal pivot: `greenbea` takes eight in a single call, `pilot` fifteen
 * across five, `pilot87` seventeen across three.
 *
 * So the worst case sits at half the cap and nothing binds. It has bound
 * twice, and both times the cap was capping a defect rather than a solve.
 * At 4, `pilot87` ran out of rounds with work still to do, and stopping it
 * there cost a factor of 6.8 on its dual violation and 5.6 on its gap for
 * 0.36% of its iterations. At 32, it ran the loop out again — because the
 * clean-up could only take one pivot per call (D30), so what should have
 * been three calls was thirty-two rounds of one. A cap tight enough to bind
 * is a cap deciding the answer, and both times the honest reading was that
 * something below it was broken.
 *
 * Non-termination is guarded elsewhere and not by this number: every round
 * that moves something makes at least one pivot, `iters` accumulates across
 * rounds, and the iteration cap in run() covers all of them together. */
constexpr int64_t SETTLE_ROUNDS = 32;

/* Bounds JAOS invented to get a dual feasible start. A column is caught by
 * exactly one branch of the cost-sign test, so one value says both whether
 * a bound was lent and which side it went on — two parallel flags would
 * encode an invariant the types do not. It is also what real_lower and
 * real_upper undo the loan from.
 *
 * Named rather than left anonymous inside sx because the re-entry keeps a
 * copy of it. */
typedef enum { NOT_FAKE = 0, FAKE_LO, FAKE_UP } jm_fake;

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    /* The matrix the solver actually works on: the model's values with the
     * scaling applied, sharing the model's sparsity pattern because
     * scaling never moves a nonzero. The model's own copy stays as loaded
     * — it is what the checker judges against, and a solver that could
     * rewrite it would be a solver marking its own homework. */
    double *av;              /* [num_nz] */

    /* The same scaled values again, ordered by row rather than by column,
     * over the model's CSR mirror. Pricing reads a row of B^-1 against
     * every column, and that row is usually mostly zeros: walking the
     * matrix by row lets the zeros be skipped, where walking it by column
     * cannot (D35). Memory for work, deliberately, like U in the
     * factorization. */
    double *arv;             /* [num_nz], parallel to m->ar_index */

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

    /* Which bound, if any, JAOS lent each variable; see jm_fake above for
     * why one value carries both halves of that. */
    jm_fake *fake;

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
    /* The duals, and the pricing row. These are two different quantities
     * and they get two different vectors, which is not a convenience: they
     * shared one for a long time, every reader had to know which producer
     * had run last, and the one function whose comment said so was called
     * from the one place its comment forbade (D30). Separate storage makes
     * that mistake impossible to write rather than possible to detect. */
    double *y;               /* [nrow] the duals, B^-T c_B */
    double *rho;             /* [nrow] row r of B^-1 */
    double *tau;             /* [nrow] B^-1 rho, for the weight update */
    double *alpha;           /* [nvar] pricing row */

    /* Where `alpha` can be nonzero, ascending and without repeats, or
     * `anpat < 0` when that is not known and the array has to be read in
     * full. Two things read it: the ratio test, which would otherwise scan
     * every variable to find the few it can use, and the clear at the top of
     * price_all, which would otherwise memset a quarter of a megabyte to
     * erase a hundred numbers.
     *
     * `amark` is the bitmap jm_pattern_order orders the pattern through. It
     * is zero everywhere between iterations, which is what lets it be
     * allocated once; nothing reads it outside that call. */
    int64_t *apat;           /* [nvar] */
    int64_t anpat;
    uint64_t *amark;         /* [(nvar + 63) / 64] */

    /* Where `rho` is nonzero, ascending, or `nrpat < 0` when nobody has
     * looked. The BTRAN reports it on the way out, from the pass that
     * permutes the answer back and therefore visits every slot regardless;
     * `rmark` is what puts it in ascending order, which is the order every
     * consumer needs and the permutation does not give. */
    int64_t *rpat;           /* [nrow] */
    int64_t nrpat;
    uint64_t *rmark;         /* [(nrow + 63) / 64] */

    /* Where the entering column's FTRAN is nonzero, or `ncpat < 0` when it
     * was too dense to be worth carrying. Unordered on purpose: the three
     * things that read it — the steepest-edge recurrence and two updates of
     * `x_B` — are all elementwise, so ordering would buy nothing. */
    int64_t *cpat;           /* [nrow] */
    int64_t ncpat;

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

    /* The settled point, kept so that a re-entry which ends worse than it
     * started can be undone. These five arrays are the whole of what a
     * re-entry may write: everything else about a basis is derived from
     * them, which is what makes restoring them enough (see save_settled).
     *
     * Allocated on the first re-entry rather than in sx_init, because most
     * solves never have one — and a solver that is re-solved thousands of
     * times by branch and bound should not pay for this per call. */
    jm_var_status *sav_status;
    int64_t *sav_basis;
    double *sav_lo, *sav_up;
    jm_fake *sav_fake;

    struct timespec started;
    int64_t iters;
    bool needs_refactor;

    /* Has optimality been re-checked against a freshly computed point since
     * the last basis change? See the r < 0 branch in run(). */
    bool verified;

    /* Does the next refresh owe a full sweep of shift_to_feasible?
     *
     * Set by a warm start and by nothing else. The cold start is dual
     * feasible by construction and its first refresh must therefore leave the
     * costs bit for bit alone — that is what makes every reference digest a
     * test of this feature changing nothing. A warm basis has no such
     * guarantee and the sweep is what buys it one, once. */
    bool shift_pending;

    /* May some nonbasic reduced cost be dual infeasible?
     *
     * The dual step only moves the costs the pricing row touches, so a step
     * driven by the row's pattern can skip the rest — but the same loop also
     * runs shift_to_feasible, which is real repair on anything left breached
     * by a computation that did not go through a pivot. Two do that:
     * compute_duals, which rebuilds every cost from scratch, and
     * primal_cleanup, which calls a column's loan back in. Either arms this,
     * and the next dual update pays for one full sweep to disarm it. */
    bool duals_dirty;

    /* Cycle detection. `infeas_best` is the smallest total primal
     * infeasibility this solve has reached and `last_gain` the iteration
     * that reached it; when the gap between that and now grows past
     * STALL_FACTOR times the model's size, `bland` goes on and the pricing
     * and ratio-test rules change under it. It goes off again the moment
     * the total improves — see price_row, which computes the total for
     * free in a loop it was already running. */
    double infeas_best;
    int64_t last_gain;
    bool bland;

    /* The caller's two tolerances, resolved once against the defaults so
     * that no site below has to remember which of the two to consult. They
     * are read here rather than from the model on purpose: a solve works on
     * one set of tolerances from start to finish, whatever anyone does to
     * the model while it runs. */
    double primal_tol;
    double dual_tol;

    /* What the solve did that a caller watching it would want to know, and
     * that four diagnostics this milestone had to instrument by hand to see.
     * Counted always — two increments against a solve that does millions of
     * things — and reported only if someone is listening. */
    int64_t n_refactor;
    int64_t n_weight_restart;
    int64_t n_bland;
    /* Pivots declined because the factorization contradicted itself (D86).
     * Worth its own count rather than folding into n_refactor: a rebuild on
     * a schedule and a rebuild because the numbers stopped agreeing say
     * different things about a model. */
    int64_t n_stability;
} sx;

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

static void sx_free(sx *s)
{
    free(s->av); free(s->arv);
    free(s->lo); free(s->up); free(s->cost); free(s->shift);
    free(s->status); free(s->basis); free(s->where);
    free(s->xb); free(s->d); free(s->dse);
    free(s->col); free(s->raw); free(s->y); free(s->rho);
    free(s->tau); free(s->alpha); free(s->apat); free(s->amark);
    free(s->rpat); free(s->rmark); free(s->cpat);
    free(s->cand); free(s->rnum); free(s->rden); free(s->rrange);
    free(s->bs); free(s->bi); free(s->bv);
    free(s->fake);
    free(s->sav_status); free(s->sav_basis);
    free(s->sav_lo); free(s->sav_up); free(s->sav_fake);
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

    /* Resolved once, here, so that a solve runs on one pair of tolerances
     * from beginning to end. Zero on the model means the caller never set
     * one, which is what makes an untouched model behave exactly as it did
     * before these existed. */
    s->primal_tol = m->cfg.primal_tol > 0.0 ? m->cfg.primal_tol : PRIMAL_TOL;
    s->dual_tol   = m->cfg.dual_tol   > 0.0 ? m->cfg.dual_tol   : DUAL_TOL;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }

    /* Pricing walks the matrix by row (D35), so the mirror has to exist.
     * It is built once per load and survives re-solves. */
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
    s->shift  = jm_calloc_array(s->nvar, sizeof(double));
    s->status = jm_alloc_array(s->nvar, sizeof(jm_var_status));
    s->basis  = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->where  = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->xb     = jm_calloc_array(s->nrow, sizeof(double));
    s->d      = jm_calloc_array(s->nvar, sizeof(double));
    s->dse    = jm_alloc_array(s->nrow, sizeof(double));
    s->col    = jm_calloc_array(s->nrow, sizeof(double));
    s->raw    = jm_calloc_array(s->nrow, sizeof(double));
    s->y      = jm_calloc_array(s->nrow, sizeof(double));
    s->rho    = jm_calloc_array(s->nrow, sizeof(double));
    s->tau    = jm_calloc_array(s->nrow, sizeof(double));
    s->alpha  = jm_calloc_array(s->nvar, sizeof(double));
    s->apat   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->amark  = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
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
    s->bs     = jm_alloc_array(s->nrow + 1, sizeof(int64_t));
    s->fake   = jm_calloc_array(s->nvar, sizeof *s->fake);

    if (!s->av || !s->arv || !s->lo || !s->up || !s->cost || !s->shift ||
        !s->status || !s->basis ||
        !s->where || !s->xb || !s->d || !s->dse || !s->col || !s->raw ||
        !s->y || !s->rho || !s->tau || !s->alpha || !s->apat || !s->amark ||
        !s->rpat || !s->rmark || !s->cpat || !s->cand || !s->rnum ||
        !s->rden || !s->rrange || !s->bs || !s->fake) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

    const double *rho = m->row_scale, *gamma = m->col_scale;

    /* ahat_ij = rho_i * a_ij * gamma_j. */
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];

    /* The same product in the same order of operations, laid out by row.
     * Same order matters: it is what lets the row-wise pricing pass produce
     * bit-identical sums to the column-wise one it replaces. */
    for (int64_t i = 0; i < s->nrow; i++)
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            s->arv[p] = rho[i] * m->ar_value[p] * gamma[m->ar_index[p]];

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

/* w' M_v for a dense row vector w, charging the nonzeros it touches (PLAN
 * 2.7 weights pricing the same as any other solve traffic).
 *
 * The vector is a parameter rather than read from the solver state, because
 * the two vectors it is called with mean entirely different things: with the
 * duals it produces a reduced cost, with row r of B^-1 it produces a pricing
 * row entry. Passing it makes each call site say which.
 *
 * src/check.c has a loop of similar shape. They stay apart because sharing
 * would make the checker link against solver internals — this function
 * takes solver state, handles logicals and bills work units, none of which
 * the checker has any business seeing. See the header of src/check.c for
 * what the checker's independence actually rests on; it is not this. */
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

/* The basis a previous solve or a caller left on the model, installed as the
 * point this solve starts from. Returns false when there is none, or when
 * what is there cannot be started from — the caller then builds the slack
 * basis above, which is always available and always correct.
 *
 * A status naming a bound that is no longer finite is repaired rather than
 * refused, because it is what a basis that was valid when it was stored looks
 * like after the model moved under it: jaos_set_col_bounds can retire the very
 * bound a variable was resting on, and a variable pinned to infinity has no
 * value. It is moved to its other bound.
 *
 * **When there is no other bound the whole warm start is abandoned.** A
 * nonbasic variable with no bounds rests at zero, and for the logical of a row
 * that means the row's activity is pinned at zero — a constraint the model
 * does not have.
 *
 * **The reason this refusal was written no longer holds, and it is kept for
 * now anyway (D85).** It went in because the method could not always price
 * such a variable back off zero: `wants_a_pivot` read a free nonbasic as
 * sitting at an upper bound, so it repaired a positive reduced cost and left a
 * negative one alone, and the point was then published as OPTIMAL when it was
 * not. That is repaired — both it and `primal_ratio_test` read the sign of the
 * reduced cost now — and the primal clean-up brings a free column back in.
 *
 * What is left is a question about warm starting rather than about
 * correctness, and it is open rather than settled: lifting this refusal moves
 * warm trajectories, so it cannot be judged by the gate's unmoved digests the
 * way D85 was, and it needs the warm campaigns. The prize is measured — D69
 * says `cycle`, one instance in 92, loses its entire warm start here. PLAN.md
 * carries it under the repair that closed the defect.
 *
 * The narrower case follows the same rule while it stands: a basis whose
 * stored status was already free — a variable that had no bounds when the
 * basis was recorded and still has none — is refused too, even though its
 * reduced cost was zero at the optimum it came from, because nothing here can
 * know whether a cost or a coefficient has moved since.
 *
 * A set of columns that no longer factors is repaired rather than refused, and
 * nothing here can see it — refresh's repair_singular_basis does, and puts
 * logicals back until it does.
 *
 * What it cannot do is establish dual feasibility. The slack basis gets that
 * by construction; a warm basis has reduced costs that are not known until it
 * is factored, so the repair happens where they are. The first refresh shifts
 * every breached cost to the feasible side and writes down the loan, exactly
 * as it already does after a singular repair, and settle_shifts calls all of
 * it back before any verdict is read.
 *
 * No artificial bounds are lent here, and that is a decision rather than an
 * omission. The cold start's loans are chosen by the sign of a cost, which is
 * the reduced cost only because B = -I makes the duals zero; from a warm
 * basis that reasoning does not hold, and sizing loans off the duals instead
 * would stand a second dual phase 1 beside the one already running. Shifting
 * is what this solver already uses to hold a basis it did not choose dual
 * feasible, and it is repaid through a path that four instances exercise.
 *
 * The weights start at one, which here is a prior rather than a fact. B = -I
 * makes every weight exactly one and that is the reason the cold start is the
 * slack basis at all; an arbitrary basis has weights that cost one solve per
 * row to know. One is the same neutral value repair_singular_basis restarts
 * to after it changes several columns of B at once, and for the same reason:
 * the exact weight injected each iteration rebuilds the estimates from there.
 * It costs pricing quality on the early iterations and no verdict depends on
 * a weight. */
static bool build_warm_basis(sx *s)
{
    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return false;

    /* Everything that would make this basis unusable, asked before a single
     * field is written. The count is one of the two: both writers of a
     * starting basis enforce it, so a mismatch means one of *them* is wrong
     * rather than that the caller is — but the answer is still to fall back,
     * because the cold start is always correct and corrupting `basis` is the
     * alternative. */
    int64_t nbasic = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        jaos_basis_status want =
            v < s->ncol ? m->start_col_status[v]
                        : m->start_row_status[v - s->ncol];
        if (want == JAOS_BASIS_BASIC) {
            nbasic++;
        } else if (!isfinite(s->lo[v]) && !isfinite(s->up[v])) {
            return false;   /* nowhere to rest but zero; see above */
        }
    }
    if (nbasic != s->nrow)
        return false;

    int64_t p = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        jaos_basis_status want =
            v < s->ncol ? m->start_col_status[v]
                        : m->start_row_status[v - s->ncol];
        if (want == JAOS_BASIS_BASIC) {
            s->basis[p] = v;
            s->status[v] = JM_BASIC;
            s->where[v] = p;
            p++;
            continue;
        }

        /* Every nonbasic has at least one finite bound; the pass above
         * refused the basis otherwise. The stored side is kept when it is
         * still there, and the other one taken when it is not. */
        s->where[v] = -1;
        if (want == JAOS_BASIS_AT_UPPER && isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else if (isfinite(s->lo[v]))
            s->status[v] = JM_AT_LOWER;
        else
            s->status[v] = JM_AT_UPPER;
    }

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
    /* At least one slot even when the basis has no entries at all, because
     * jm_lu_factor is entitled to non-null arrays whenever dim > 0 and
     * JM_GROW leaves a zero request unallocated. A basis of nothing but empty
     * columns is where that happens; it is singular, which the factorization
     * reports as a rank and the repair puts right, but it has to get as far
     * as being factored to say so. The slack basis cannot reach it — every
     * logical carries an entry — and a warm one can, which is how it was
     * found. */
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

/* r -= B z, for a dense vector over the rows. The basis is held as a list
 * of columns of M, so this walks the columns and scatters; there is no
 * assembled B to multiply by. */
static void subtract_basis_times(sx *s, double *r, const double *z)
{
    int64_t nz = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        double zi = z[i];
        if (zi == 0.0)
            continue;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                r[m->a_index[k]] -= s->av[k] * zi;
            nz += m->a_start[v + 1] - m->a_start[v];
        } else {
            r[v - s->ncol] += zi;      /* the column is -e_i */
            nz++;
        }
    }
    jm_work_add(&s->work, nz * JM_WORK_NONZERO);
}

/* x_B = -B^-1 (N x_N).
 *
 * `refine` asks for one step of iterative refinement on that solve: the
 * residual of the system is computed against the basis columns themselves
 * and solved for again, and the correction added. See refresh() for which
 * solves get it and why the rest do not. */
static void compute_primal(sx *s, bool refine)
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
 * transposed solve: the two travel together, because a point read off an
 * accurate x_B and an inaccurate y is not more consistent than one read off
 * neither — measured, and it is how this arrived (D29). */
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

    /* These costs owe nothing to the shifting the solve has been doing, so
     * any of them may now sit on the infeasible side of its bound. The next
     * dual update is the thing that repairs that, and it has to look at all
     * of them to find them. */
    s->duals_dirty = true;
}

/* Declared here rather than moved: the repair below needs it, and it belongs
 * with the settling-up code where its reason is written out. */
static void shift_to_feasible(sx *s, int64_t v);

/* How many times one refresh will repair and refactor before giving up.
 *
 * One is enough in exact arithmetic — the proof is in repair_singular_basis
 * — so this is a backstop against the repaired basis coming back deficient
 * for a reason the proof does not cover, which is threshold pivoting
 * declining a pivot the algebra says exists. It is not a budget to be tuned:
 * a solve that needs a third pass has something else wrong with it. */
constexpr int REPAIR_ATTEMPTS = 4;

/* Puts a basis back together after the factorization finds it singular.
 *
 * The LU's contract (jaos_internal.h) is that rank < dim is a fact rather
 * than an error, and that the caller acts on it by replacing basis columns.
 * This is that caller. Until this existed the solve gave up instead, and
 * `gran` of the infeasible set is what that cost: an INFEASIBLE verdict the
 * model was owed, replaced by a numerical error after 1728 iterations.
 *
 * **A singular basis is not something a model can cause.** Every basis the
 * dual simplex assembles is nonsingular in exact arithmetic — it never
 * pivots on an alpha below PIVOT_MIN — so one that will not factor is
 * always carried error, and `gran`'s 2658 rows are the scale at which that
 * finally accumulated. No unit test can construct one, which is why
 * `tests/test_simplex.c` covers the family instead: rank-deficient
 * constraint matrices, where the danger is a wrong verdict and not a crash.
 *
 * A rank-deficient factorization hands over exactly the two lists the
 * repair needs. Slots 0..rank-1 name the rows that were pivoted and the
 * basis positions that were used; whatever is missing from those lists is,
 * on the row side, a row nothing covers, and on the column side, a column
 * that turned out to depend on the others. There are equally many of each.
 *
 * The repair pairs them off and puts the logical of an uncovered row into
 * the dependent position. That the result is nonsingular is not a hope. Sort
 * the rows as (pivoted, uncovered) and the new basis reads
 *
 *     B' = [ P  0 ]
 *          [ Q -I ]
 *
 * because a logical is a unit column and the rows it touches are exactly
 * the ones P does not. P is nonsingular — triangularizing it is what the
 * factorization just did — so det B' = det P * det(-I), which is not zero.
 *
 * The logical of an uncovered row cannot already be in the basis, so the
 * pairing never installs the same variable twice. Its column is a singleton
 * on that row; a factorization holding it would have had a pivot candidate
 * of magnitude one whose threshold ratio is exactly one and whose Markowitz
 * count is the smallest there is, so it would have taken it and covered the
 * row. The check below is kept anyway: it costs one comparison on a path
 * that runs once in a solve, and the alternative to checking is corrupting
 * the basis if the reasoning is ever made wrong by a change elsewhere.
 *
 * The two mark arrays are allocated here and not kept in sx. Nearly every
 * solve never calls this at all, and carrying two arrays per solve for a
 * path that rare is the wrong trade — this is the opposite case from the
 * refactorization buffers, which every solve uses many times.
 *
 * Returns false when nothing was repaired, which the caller must treat as
 * the numerical failure it then is. */
static bool repair_singular_basis(sx *s)
{
    const int64_t n = s->nrow;
    const int64_t rank = s->lu.rank;

    /* rank < 0 marks a factorization wrecked by a failed update; its
     * permutations mean nothing and there is nothing here to read. */
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
            /* Fewer uncovered rows than dependent columns: the two lists
             * disagree about the same rank, so one of them is wrong and
             * neither can be acted on. */
            done = false;
            break;
        }

        int64_t leaving  = s->basis[p];
        int64_t entering = s->ncol + i;
        if (s->status[entering] == JM_BASIC) {
            done = false;
            break;
        }

        /* Where the evicted variable is parked. A basic's reduced cost is
         * zero, so both of its bounds are dual feasible and the choice is
         * free; the lower one is taken first, which is the tie-break
         * build_initial_basis already uses. A variable with neither bound
         * becomes nonbasic free, and the shift in refresh is what keeps
         * that dual feasible once its reduced cost exists. */
        if (isfinite(s->lo[leaving]))
            s->status[leaving] = JM_AT_LOWER;
        else if (isfinite(s->up[leaving]))
            s->status[leaving] = JM_AT_UPPER;
        else
            s->status[leaving] = JM_FREE;
        s->where[leaving] = -1;

        s->basis[p] = entering;
        s->status[entering] = JM_BASIC;
        s->where[entering] = p;
        i++;
    }

    free(row_covered);
    free(pos_used);
    if (!done)
        return false;

    /* Every weight is the squared norm of a row of B^-1, and B^-1 has just
     * changed in several columns at once. The recurrence has no way to
     * carry weights across that, so they are restarted from the value the
     * slack basis starts at rather than left describing a basis that no
     * longer exists. Pricing quality is all this costs; no verdict depends
     * on a weight. */
    for (int64_t k = 0; k < n; k++)
        s->dse[k] = 1.0;
    return true;
}

/* Rebuild the factorization and everything derived from it. Returns false
 * when the basis will not factor and the repair above cannot put it right,
 * which for a basis the algorithm itself assembled means the numerics have
 * failed rather than the model.
 *
 * `refine` asks the two solves for one step of iterative refinement, and the
 * callers that ask are the ones whose result can be published: the refresh
 * that verifies a declaration of optimality (D20), the one that rebuilds
 * after a primal clean-up, and the one that restores a settled point. The
 * line between those and the rest is not a cost-saving, it is what the
 * numbers are being used for (D29).
 *
 * Mid-solve, x_B and y are inputs to a choice of pivot, and a trajectory is
 * not more correct for being computed from more accurate numbers. Refining
 * every refresh was measured: `pilot-ja`, a model with a known finite
 * optimum, comes back INFEASIBLE, and `pilot87` pays 4.5x the work. At the
 * end, the same two vectors are the answer, and an answer *is* more correct
 * for being more accurate — `pilot`'s rejected row is a residual of this
 * solve and nothing else (PLAN 2.9).
 *
 * So this is the second half of D20's argument rather than a new mechanism.
 * D20 refuses to read a verdict off carried numbers; this refuses to read
 * one off an inaccurate solve of the fresh factorization those numbers were
 * rebuilt from. */
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

    /* The repair had to choose bounds for the variables it evicted before
     * their reduced costs existed, so some of those costs are now on the
     * wrong side. Shifting is the mechanism the method already uses for
     * exactly this — every iteration does it in pivot() — and the shifts
     * are called back before any verdict is read. Only after a repair, or
     * on the first refresh of a warm start, which is the same situation
     * arrived at from the other direction: a basis nobody proved dual
     * feasible before it was installed. A cold solve that never went
     * singular is left bit for bit as it was. */
    bool sweep = repaired || s->shift_pending;
    s->shift_pending = false;
    if (sweep)
        for (int64_t v = 0; v < s->nvar; v++)
            shift_to_feasible(s, v);

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
    /* Decided before the loop, off the previous call's accounting, so that
     * one iteration uses one rule throughout. */
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

        /* Under Bland's rule the leaving variable is the lowest-indexed one
         * that violates a bound, not the one that violates it most per unit
         * of movement. The steepest-edge score is what makes a real model
         * tractable and it is also a free choice among equals at a
         * degenerate vertex, which is where a cycle comes from; the index
         * rule has no freedom left to spend. */
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

    /* The measure of progress, and it is not the objective: the dual method
     * drives primal infeasibility out, so that is the quantity a stalled
     * solve stops moving. It is not monotone either, which is why this
     * tracks the best reached rather than the last seen — a solve that gets
     * worse and then better has made progress, and a cycle never improves
     * on its own best at all.
     *
     * Improving turns Bland's rule back off. Its cost is real (D26) and it
     * is worth paying only while the alternative is not working. */
    if (total < s->infeas_best) {
        s->infeas_best = total;
        s->last_gain = s->iters;
        s->bland = false;
    }
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
    /* Only a row where alpha is nonzero moves, which the dense form finds by
     * testing every row and the sparse one is told. Each row's new weight
     * depends on its own old one and on nothing else here, so the two visit
     * the same rows and compute the same numbers; what differs is how many
     * rows are looked at to find them. */
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

    /* Same borrowing as `col` above: `cpat` belongs to pivot, which fills it
     * from its own FTRAN before reading it, and this runs first. */
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
/* One variable's eligibility, and its place in the candidate arrays if it
 * has one. Split out because the scan around it comes in two forms — over
 * the pricing row's pattern where there is one, over every variable where
 * there is not — and a rule this delicate must not be written twice. */
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

static int64_t dual_ratio_test(sx *s, bool below, double violation,
                               double *theta_out)
{
    int64_t n = 0;

    /* A variable outside the pattern has alpha exactly zero, which the
     * PIVOT_MIN test rejects before anything else is read — so the two scans
     * admit the same candidates, and the pattern being ascending puts them
     * in the same array positions. That matters more than it looks:
     * bfrt_walk and jm_harris_pick both break an exact tie by whichever
     * candidate it meets first, and apply_flips adds up a column per flip in
     * the order they stand. Any other order is a different trajectory. */
    if (s->anpat >= 0) {
        for (int64_t t = 0; t < s->anpat; t++)
            admit_candidate(s, s->apat[t], below, &n);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    } else {
        for (int64_t v = 0; v < s->nvar; v++)
            admit_candidate(s, v, below, &n);
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    }

    if (n == 0)
        return -1;

    /* Bland's rule takes the exact minimum quotient, so there is no window
     * to trade and nothing to flip: bound flipping is part of the ratio
     * test's freedom, and the freedom is what cycled. The candidate set is
     * the same one; only the choice among it changes. */
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

    /* The step that lands the winner's reduced cost exactly on zero. Every
     * other candidate inside the window ends at worst DUAL_TOL past
     * feasible, which is the whole of what Harris trades away. */
    *theta_out = s->d[best] / s->alpha[best];
    return best;
}

/* Bland's rule over the same candidates. Documented in the header, beside
 * Harris', because the two are the same decision made two ways. */
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

/* A scatter's record of where it wrote, made ascending and distinct.
 * Documented in the header, beside the two picks, and reachable for the
 * same reason they are. */
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words)
{
    *words = 0;
    if (n <= 0 || limit <= 0)
        return 0;

    /* The touched range, so that a pattern living in one corner of a large
     * model does not pay for the whole bitmap on the way back out. */
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

    /* Reading back over the input is safe: every position is in the bitmap
     * by now, and the distinct count can only be smaller than what went in,
     * so the write index never overtakes anything still needed. */
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
    return k;
}

/* Harris' window and the best-conditioned pivot inside it. Documented in
 * the header, which is also where the reachable-from-outside rationale
 * lives. */
int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol)
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
        if (num[k] / den[k] <= window && den[k] > best_den) {
            best_den = den[k];
            best = k;
        }
    }
    return best;
}

/* Builds row r of B^-1 M and picks the entering variable from it. May also
 * swap nonbasic variables between their bounds on the way — see
 * dual_ratio_test — which moves the primal point but not the basis. */
/* alpha = rho' M, for every variable at once.
 *
 * The column-wise form this replaces asked each column in turn for its dot
 * product with rho, which costs the entire matrix every iteration no matter
 * how sparse rho is. And rho is a row of B^-1: over the standard set its
 * density is 0.24 at the median and 0.004 at the sparsest (D35). Walking by
 * row instead lets one zero of rho skip a whole row of the matrix, which is
 * the saving the column view structurally cannot express.
 *
 * The sums come out bit-identical to the column-wise ones, and that is by
 * construction rather than by luck. Each column of the CSC copy is sorted by
 * row index, and the rows here are visited in increasing order, so every
 * column accumulates its terms in exactly the order it did before. A skipped
 * row would have contributed `0.0 * a_ij`, which cannot change a sum it is
 * added to — only the sign of a zero, which no test in the method reads. */
static void price_all(sx *s)
{
    const jaos_model *m = s->m;

    /* Erasing the previous row. Where it was is known unless something
     * wrote alpha behind this function's back, and on a hyper-sparse model
     * the difference is the whole iteration: `ken-13` puts 143 numbers into
     * 71291 slots, and the memset is larger than everything else it does. */
    if (s->anpat < 0)
        memset(s->alpha, 0, (size_t)s->nvar * sizeof *s->alpha);
    else
        for (int64_t k = 0; k < s->anpat; k++)
            s->alpha[s->apat[k]] = 0.0;

    /* Recording the pattern while scattering costs a comparison against a
     * value the `+=` has already loaded, and a store on the slots that were
     * zero. That is the cheapest form this can take: a second array read per
     * matrix entry is what made the basic-column filter not worth having
     * (D35), and this loop is the same loop.
     *
     * A slot can be recorded twice — cancel it back to exactly zero and the
     * next write reads zero again — so what comes out is a list, not a set.
     * jm_pattern_order makes it one.
     *
     * `np` counts what the scatter found and `cap` bounds what is kept, so
     * a pattern too large to be worth walking is detected by having run out
     * of room rather than by a second pass to measure it. */
    const int64_t cap = s->nvar / SPARSE_ALPHA_DEN;
    int64_t np = 0, touched = 0;

    /* The rows to visit, ascending either way: `rpat` where price_and_select
     * judged it worth ordering, and every row otherwise. The zeros a scan
     * steps over would each contribute `0.0 * a_ij`, which is the same
     * nothing the pattern skips (D35). */
    const bool sparse_rows = s->nrpat >= 0;
    const int64_t nvisit = sparse_rows ? s->nrpat : s->nrow;
    int64_t nfound = 0;

    for (int64_t k = 0; k < nvisit; k++) {
        int64_t i = sparse_rows ? s->rpat[k] : k;
        double w = s->rho[i];
        if (w == 0.0)
            continue;
        /* A scan that was going to happen anyway can leave the pattern
         * behind it, ascending and complete, which is what pivot's exact
         * weight reads (D42). Only the dense branch needs to: the sparse one
         * is walking that very list. */
        if (!sparse_rows)
            s->rpat[nfound++] = i;
        /* A logical's column is -e_i, so row i writes slot ncol+i and no
         * other row can: it is new to the pattern without being asked. */
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

    /* A basic variable prices to zero by definition, and the sweep above has
     * no way to know which those are. They stay in the pattern: it is what
     * the next clear works from, and every consumer skips a basic variable
     * on its status long before it looks at the number.
     *
     * Only a slot the scatter wrote can be anything but zero, so where the
     * pattern is complete this is a walk over it rather than over every row.
     * The `status` read that costs is per pattern entry, not per matrix
     * entry — which is the whole difference between it and the filter D35
     * measured and refused.
     *
     * Whether that is cheaper is a comparison of two loop lengths and needs
     * no constant to decide: `np` slots written against `nrow` basis
     * positions. On a model with far more columns than rows the pattern is
     * the longer of the two and the basis walk wins. */
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

    /* The matrix entries read, and the rows walked to read them.
     *
     * That second term was `nrow` whatever `rho` looked like, because the
     * walk was over every row; now it is the length of the walk actually
     * made. On the Kennington set the old term was 27% of everything the
     * solver billed.
     *
     * The reset above is deliberately not charged, and neither is the clear
     * at the top. Both were unbilled before this change and both got
     * cheaper with it; billing them now would mix an accounting correction
     * into a measurement, and the baseline diff could not tell the two
     * apart. They belong with the rest of PLAN 2.11's unbilled sweeps until
     * something charges all of them at once. */
    jm_work_add(&s->work, (touched + nvisit) * JM_WORK_NONZERO);

    if (np > cap) {
        s->anpat = -1;   /* too dense to be worth walking, and incomplete */
        return;
    }
    int64_t words = 0;
    s->anpat = jm_pattern_order(np, s->apat, s->amark, s->nvar, &words);
    jm_work_add(&s->work, (np + words + s->anpat) * JM_WORK_NONZERO);
}

static int64_t price_and_select(sx *s, int64_t r, bool below,
                                double violation, double *theta_dual)
{
    memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
    s->rho[r] = 1.0;

    /* The solve says where its answer is; the ordering makes that ascending,
     * which is what price_all's sums need to come out the way the column-wise
     * pass produced them (D35). Both are paid in the size of the answer
     * rather than in the size of the basis — which is only worth having when
     * the answer is the smaller of the two. */
    int64_t nr = 0, words = 0;
    jm_lu_btran_sparse(&s->lu, s->rho, &s->work, s->rpat, &nr);
    if (nr * SPARSE_RHO_DEN <= s->nrow) {
        s->nrpat = jm_pattern_order(nr, s->rpat, s->rmark, s->nrow, &words);
        jm_work_add(&s->work, (nr + words + s->nrpat) * JM_WORK_NONZERO);
    } else {
        s->nrpat = -1;   /* too dense to be worth ordering; scan instead */
    }

    price_all(s);

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

/* One variable's share of the dual step, and the repair that follows it.
 * Split out for the reason admit_candidate is: the loop around it comes in
 * two forms and a rule this delicate must not be written twice. */
static void update_dual(sx *s, int64_t v, int64_t q, double theta_dual)
{
    if (s->status[v] == JM_BASIC || v == q)
        return;
    s->d[v] -= theta_dual * s->alpha[v];
    shift_to_feasible(s, v);
}

/* Applies the basis change: q enters at position r, the variable there
 * leaves to the bound it violated.
 *
 * `*took` says whether it happened. A pivot declined for the stability
 * reason below leaves every field exactly as it found them and asks for a
 * refactorization instead, so the caller must not bill an iteration for it —
 * nothing moved, and counting it would let a run of declines exhaust the
 * iteration guard while reporting progress that was never made. */
static jaos_status pivot(sx *s, int64_t r, int64_t q, bool below,
                         double theta_dual, bool *took)
{
    int64_t leaving = s->basis[r];
    double bound = below ? s->lo[leaving] : s->up[leaving];
    double alpha_q = s->alpha[q];

    /* The entering column, transformed. **This happens before anything is
     * mutated, and that ordering is the stability trigger rather than tidiness
     * (D86).** `col[r]` and `alpha_q` are the same number reached by two
     * different solves against the same factorization, so comparing them is
     * free evidence about whether that factorization still holds — but only
     * while the iteration can still be abandoned. It used to run after the
     * reduced costs had already been stepped, which left nothing to abandon.
     *
     * Moving it moves no arithmetic: the two share no buffers — `raw`, `col`
     * and `cpat` against `d`, `shift` and `cost` — and the work units they
     * bill are integers whose order of addition cannot change a total. The
     * raw column is kept because the LU update wants it untransformed. */
    var_column(s, q, s->raw);
    memcpy(s->col, s->raw, (size_t)s->nrow * sizeof *s->col);
    {
        int64_t nc = 0;
        jm_lu_ftran_sparse(&s->lu, s->col, &s->work, s->cpat, &nc);
        s->ncpat = nc * SPARSE_COL_DEN <= s->nrow ? nc : -1;
    }

    /* Do the two agree? If they do not, the updates since the last rebuild
     * have taken the factorization somewhere neither solve can be read off,
     * and pivoting on either number puts a fiction into the basis. Ask for a
     * rebuild and hand the iteration back unspent.
     *
     * **Only when a rebuild is a plausible cure**, which is what `n_updates`
     * says. On a factorization that was just built from scratch the same
     * disagreement means the basis itself is that badly conditioned, and
     * rebuilding it again would produce the same numbers and the same
     * refusal, forever. There the pivot is taken: it is the worse of two
     * options and the only one that terminates, and the outcome is a solve
     * that ends rather than a loop that does not. */
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

    /* Primal step: how far the entering variable moves so that row r lands
     * exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;

    /* Reduced costs shift by the dual step along the pricing row, and any
     * that the window pushed past feasible are bought back on the spot.
     *
     * A variable the pricing row does not touch takes no step — its share is
     * `theta_dual * 0.0`, which leaves the value alone — so the pattern is
     * enough to move every cost that moves. What the pattern is not enough
     * for is the repair: `shift_to_feasible` is a no-op only on a cost that
     * is already feasible, and that holds for everything this loop skips
     * exactly while `duals_dirty` is clear. It is set by the two places that
     * write a reduced cost without going through a pivot, and one full sweep
     * clears it. */
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
     * and lets the recurrence be caught when it has drifted away from one.
     *
     * Summed over rho's pattern where price_all left one. The zeros it skips
     * contribute `0.0 * 0.0` to a running total that is a sum of squares and
     * therefore never negative zero, so `x + 0.0` is exactly `x` and the
     * total comes out bit for bit the same. Ascending order is inherited
     * rather than arranged: the walk that recorded the pattern was one. */
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

    /* x_B moves by the entering column's step, and a row the column does not
     * reach does not move: `theta_primal * 0.0` is a zero this subtracts from
     * a value it leaves alone. Row r is written outright below either way. */
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
            if (x < s->lo[b] - s->primal_tol ||
                x > s->up[b] + s->primal_tol) {
                safe = false;
                break;
            }
            /* The bounds just tested include the invented ones, and a
             * basic left sitting *on* one would be published at a value
             * the model never allowed. The swap that would do that is
             * refused; no test constructs this, it takes a box ~1e10
             * wide, but an answer must not depend on nobody ever
             * building one. */
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

/* Calls in every cost the solve borrowed. Says whether anything was owed,
 * because a solve that borrowed nothing has duals that already price the
 * model's own costs and recomputing them would only add rounding. */
static bool repay_shifts(sx *s)
{
    bool any = false;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->shift[v] == 0.0)
            continue;
        s->cost[v] -= s->shift[v];
        s->shift[v] = 0.0;
        any = true;
    }
    return any;
}

/* Calls in the loans and recomputes the duals from the model's own costs so
 * that what is published belongs to the problem that was asked about rather
 * than to the one that was convenient. Then repairs what the true costs
 * turn out to leave infeasible. */
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

/* The solve loop, which a re-entry runs again from a point of its own
 * choosing. Declared rather than moved: it belongs with the driver, and
 * hoisting it above this section would put the method's main loop in the
 * middle of the code that cleans up after it. */
static jaos_status run(sx *s, jaos_solve_status *out);

/* How far this nonbasic's reduced cost points the wrong way, or zero.
 *
 * Measured against DUAL_TOL because that is what the rest of the solve
 * calls zero: a breach this solver's own arithmetic cannot distinguish from
 * zero is not evidence of anything, and treating it as work to be done
 * would have every solve re-entering forever. */
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

/* Everything a re-entry is allowed to write. Restoring these five and
 * rebuilding from them lands on exactly the point that was saved:
 * `where` is the inverse of `basis`, `xb` is what compute_primal derives
 * from the nonbasic values, `d` is what compute_duals derives from the
 * costs, and the factorization is of `basis`. None of the five is
 * derived from anything else, and nothing else is not derived from them. */
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

/* Puts back the saved point and rebuilds everything that hangs off it.
 *
 * The costs are returned to the model's own first: a re-entry that failed
 * may have borrowed on its way there, and those loans belong to a solve
 * that is being discarded. */
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

    s->needs_refactor = true;
    return refresh(s, ok, true);
}

/* Makes the settled point dual feasible again.
 *
 * There are two ways to put a wrong-signed reduced cost right, and which
 * one applies is a property of the column rather than a choice:
 *
 *   - A column with a real bound on the other side can be sent to it. Its
 *     reduced cost is then feasible for that bound instead, at no cost in
 *     accuracy, and the primal breaks — which is the point. Primal
 *     infeasibility is what the dual simplex exists to remove, so this is
 *     the move that gives it something to do.
 *   - A column with no other real bound has nowhere to go, so its cost is
 *     shifted instead, exactly as the ratio test does mid-solve. That
 *     restores the invariant without moving the point, which means it
 *     hands the method no work; it is here so that the ones that *can* be
 *     moved are not run past a ratio test whose candidates include costs
 *     already on the wrong side of zero.
 *
 * Only the first kind counts as movement, which is why `anything_to_move`
 * is asked before any of this runs rather than after. A round that managed
 * nothing but shifts would re-solve a point the method is already at and
 * settle back to precisely the residue it started from, having borrowed
 * costs on the way — and a verdict read off borrowed costs is the one thing
 * settling exists to prevent. So a round with nothing to move does not
 * begin. */

/* Everything that went into `d_j`: `|c_j|` plus the magnitudes of the terms
 * of `y' M_j`. It is the same quantity the checker calls a row's traffic,
 * read down a column instead of along a row.
 *
 * `y` is whatever compute_duals last left in `s->y`, which is inherited
 * state rather than derived. Every path into the re-entry passes through a
 * refresh, so it is the duals of a basis — but not necessarily of *this*
 * basis once a clean-up has pivoted, which is why the candidates are chosen
 * before any of them moves. It cannot be a pricing row: those live in
 * `s->rho`, and they have since D30. */
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

/* Worth a flip when the wrong sign carries objective behind it, and when
 * the reduced cost carrying it is a number rather than rounding.
 *
 * **The objective part.** `P − D = sum_v w_v (v − bound_v)`, and for a
 * nonbasic on a bound with a wrong-signed reduced cost the multiplier
 * points at the *other* bound, so its term is `|d|` times the width of the
 * box. That is what the checker publishes as `Q` (D24), and it is the
 * quantity to test because it does not have a space: `publish` divides `d`
 * by the same `gamma` it multiplies the value by, so the product is
 * identical in scaled space and in the model's own. The breach on its own
 * is not — `etamacro`'s reads 4.89e-8 scaled and 1.56e-6 published, inside
 * this solver's zero on one side of a change of variable and past the
 * checker's tolerance on the other. Choosing between those readings was
 * measured and costs `pilot87` its answer (PLAN 2.8.1).
 *
 * **The rounding part, and it is not a second test.** A product is only
 * as good as its factors: on `pds-20` the contribution alone flips columns
 * whose reduced costs are 1e-10 — three orders below what this solver calls
 * zero — because their boxes are a thousand wide, and 32 rounds of that
 * cost the instance 3.2x its work. So `|d|` counts only where it stands
 * above the rounding of the dot product that produced it. See NOISE_MARGIN
 * for the measurement that places the line.
 *
 * A column with no other real bound contributes nothing, by the same
 * identity that gives the term: there is no `w · bound` for an infinite
 * bound. That is why `greenbea`'s ten are untouched by any of this — they
 * are a dual violation with no objective behind it, and what they need is a
 * primal pivot (§2.1). */
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
    return wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol;
}

static bool anything_to_move(const sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++)
        if (can_move(s, v))
            return true;
    return false;
}

static void arm_reentry(sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        if (can_move(s, v)) {
            s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                       : JM_AT_LOWER;
        } else if (dual_breach(s, v) != 0.0) {
            /* Nowhere worth sending it, but its sign is still wrong and the
             * ratio test must not meet a reduced cost already past zero.
             * The shift threshold stays DUAL_TOL on the breach itself:
             * what the solver calls zero is a different question from what
             * is worth repairing. */
            shift_to_feasible(s, v);
        }
    }
}

/* --------------------------------------------------------------------- */
/* Primal clean-up, for a column with nowhere to rest                    */
/* --------------------------------------------------------------------- */

/* Is this column one that only a basis change can repair?
 *
 * The re-entry above moves a column to its other bound. A column that has
 * no other real bound cannot be moved, and its term in `P − D` is zero by
 * the same identity, so nothing the contribution test says applies to it.
 * `greenbea` finishes with ten of them: at a lower bound of 0, no upper
 * bound, reduced costs from −0.019 to −5.28 (PLAN 2.8.1). What they want is
 * to travel until something stops them, and that is a basis change.
 *
 * The two filters that do apply are the ones about whether there is
 * anything there: past DUAL_TOL, which is what the solver calls nonzero,
 * and above the rounding of the dot product that produced it (D27).
 *
 * **A nonbasic free variable is the other kind, and it used to be invisible
 * here.** It has no bound in either direction, so it qualifies for the same
 * reason `greenbea`'s ten do — only more so — and it is the one status whose
 * reduced cost may be wrong in *either* sign, because zero is the only
 * feasible value for it. The magnitude of the breach is therefore `|d|` and
 * not a signed expression: the old form read a free variable as sitting at an
 * upper bound, so it repaired a positive reduced cost and silently dropped a
 * negative one, and a point held at zero by a column nothing would move was
 * then published as OPTIMAL. `|d|` is what the two bounded cases already
 * computed — `dual_breach` above has already established the sign for them —
 * so this is the same number for everything except the case it repairs. */
static bool wants_a_pivot(const sx *s, int64_t v)
{
    if (dual_breach(s, v) == 0.0)
        return false;
    double wrong_way = fabs(s->d[v]);
    if (wrong_way <= NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v))
        return false;
    if (s->status[v] == JM_FREE)
        return true;   /* no bound in either direction; nothing left to ask */
    return !isfinite(s->status[v] == JM_AT_LOWER ? real_upper(s, v)
                                                : real_lower(s, v));
}

/* How far column q can travel before a basic variable reaches a bound.
 *
 * `x_B = -B^-1 N x_N`, so moving q by `dx` moves the basics by
 * `-B^-1 M_q dx`. q travels upwards off a lower bound and downwards off an
 * upper one — the direction its reduced cost points — and each basic is
 * stopped by whichever of its bounds lies that way.
 *
 * **The direction is read off the reduced cost rather than off the status**,
 * and that is the same rule stated once instead of twice. A column only
 * reaches here past `wants_a_pivot`, which is past `dual_breach`, so at a
 * lower bound `d` is already known negative and at an upper bound already
 * positive — `d < 0` picks out exactly the two cases the status test used to
 * name. What it also picks out, and the status test could not, is a *free*
 * column: zero is its only feasible reduced cost, so which way it improves is
 * a fact about `d` alone, and reading its status instead sent it downwards
 * whichever sign it had.
 *
 * **Only bounds the model declared can stop it.** A basic brought to rest on
 * a bound dual phase 1 lent would be published at a value the model never
 * allowed, which is the case `repair_dual_infeasibility` refuses for the
 * same reason. If nothing real blocks, this returns -1 and the column is
 * left alone: the honest reading of that is an unbounded ray, and declaring
 * one here — off a basis that has just been rebuilt twice — is exactly the
 * class of verdict D19 requires proof for. Leaving the residue is the
 * smaller error.
 *
 * Returns the blocking position, with *below saying which bound it lands
 * on, or -1. */
static int64_t primal_ratio_test(sx *s, int64_t q, bool *below)
{
    var_column(s, q, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < PIVOT_MIN)
            continue;                        /* cannot be told from zero */

        int64_t b = s->basis[i];
        double limit = move < 0.0 ? real_lower(s, b) : real_upper(s, b);
        if (!isfinite(limit))
            continue;

        double step = (limit - s->xb[i]) / move;
        if (step < 0.0)
            step = 0.0;                      /* already there: degenerate */
        if (step < best_step) {
            best_step = step;
            best = i;
            *below = move < 0.0;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return best;
}

/* Lets every column that wants a pivot have one, and reports how many.
 *
 * This is a primal ratio test and the basis change `pivot()` already
 * performs, which is the whole of what §2.1's exclusion of the primal
 * simplex leaves room for: no primal pricing, no phase 1 of its own, no
 * second set of weights. The entering column is chosen by the residue
 * rather than by a pricing rule, so there is nothing here to price with.
 *
 * `pivot()` needs row r of `B^-1` in `rho` and the pricing row in `alpha`,
 * which is what `price_and_select` normally leaves behind; the two lines
 * that build them are repeated here rather than factored out, because
 * pulling them into a helper would put the dual method's preamble in a
 * function the dual method does not call.
 *
 * The step it computes is `(x_B[r] - bound) / alpha[q]`, and `alpha[q]` is
 * row r of `B^-1 M` at column q — the same number the ratio test above
 * blocked on. So the dual pivot and the primal one are one basis change
 * differing only in which of `r` and `q` was chosen first.
 *
 * Two guarantees come free and are worth stating, because the re-entry
 * above has neither. The point stays primal feasible: the ratio test is
 * what makes it so. And the objective cannot rise: `d_q` points the way q
 * travels, so every step is a descent, and a degenerate step of zero
 * changes the basis without changing the point.
 *
 * **Which columns want one is decided before any of them gets one, and that
 * is not an optimisation (D30).** The candidate set is a snapshot of one
 * point: the duals it is judged against belong to the basis on entry, and
 * every pivot here changes that basis. Re-asking mid-loop would judge each
 * column against a different point from the one that chose it.
 *
 * It also used to be catastrophic rather than merely inconsistent, and that
 * is worth keeping written down: `column_traffic` read the duals out of the
 * same vector this function fills with a pricing row, so from the second
 * candidate on it computed a threshold from the wrong quantity and refused
 * every column. Measured on `pilot87`: 12 candidates on entry, one pivot,
 * zero on exit, every round. The loop could only ever take one pivot per
 * call, and `greenbea`'s eight were eight separate rounds of the re-entry,
 * which is why nothing looked wrong. The duals now have their own vector, so
 * that particular failure is no longer expressible.
 *
 * What is re-checked per candidate reads only `d` and the loan against it. */
static jaos_status primal_cleanup(sx *s, int64_t *pivots)
{
    *pivots = 0;

    /* Borrowed: `cand` is the dual ratio test's candidate set, and no dual
     * iteration is in flight here. */
    int64_t n = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        if (wants_a_pivot(s, v))
            s->cand[n++] = v;

    for (int64_t k = 0; k < n; k++) {
        int64_t q = s->cand[k];
        if (s->status[q] == JM_BASIC)
            continue;      /* an earlier pivot of this pass took it in */

        /* Call in this column's own loan before judging it. `pivot()` runs
         * shift_to_feasible over every variable, so the pivot before this
         * one did not repair the other candidates' sign conditions — it lent
         * them away, and a routine whose whole job is the residue settling
         * reveals must not read a cost that has just been papered over.
         * Shifting a nonbasic's cost moves only its own reduced cost, which
         * is what makes taking it back here exact and local. */
        if (s->shift[q] != 0.0) {
            s->cost[q] -= s->shift[q];
            s->d[q] -= s->shift[q];
            s->shift[q] = 0.0;
            s->duals_dirty = true;   /* q's cost may now breach its bound */
        }
        if (dual_breach(s, q) == 0.0)
            continue;      /* an earlier pivot of this pass really did fix it */

        bool below = false;
        int64_t r = primal_ratio_test(s, q, &below);
        if (r < 0)
            continue;

        memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
        s->rho[r] = 1.0;
        jm_lu_btran(&s->lu, s->rho, &s->work);
        for (int64_t v = 0; v < s->nvar; v++)
            s->alpha[v] = s->status[v] == JM_BASIC ? 0.0
                                              : price_entry(s, s->rho, v);
        s->anpat = -1;   /* written in full and behind price_all's back */
        s->nrpat = -1;   /* and so is rho, by the BTRAN just above */

        if (fabs(s->alpha[q]) < PIVOT_MIN)
            continue;   /* the pricing row disagrees with the column: leave it */

        bool took = false;
        jaos_status st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took) {
            /* The factorization contradicted itself and asked for a rebuild.
             * Everything below reads it, so leave now and let the caller's
             * refresh come round: this loop's candidates were chosen against
             * a point that a rebuild is about to recompute anyway. */
            break;
        }

        /* Billed as the iterations they are, so the guard in run() covers
         * them and the work counter does not under-report the solve (D16). */
        s->iters++;
        (*pivots)++;

        /* `pivot()` reports a basis update that failed by asking for a
         * rebuild and returning JAOS_OK, because the dual method's loop
         * reads that flag before every iteration. This loop is the one
         * place that pivots without going through it, and everything above
         * reads the factorization: both triangular solves return without
         * writing anything once it is wrecked, so the ratio test and the
         * pricing row would be computed from whatever the buffers last
         * held. Leaving now hands the remaining candidates to the next
         * round — the caller refreshes whenever this loop took a pivot —
         * rather than to a factorization that no longer exists. */
        if (s->needs_refactor)
            break;
    }
    return JAOS_OK;
}

/* Hands a settled point back to the dual simplex.
 *
 * The residue settling leaves is not noise to be tolerated: on `greenbea` a
 * perturbation of 7e-6 arrives as a violated sign condition of five
 * (PLAN 2.8.1). But the point it leaves is a genuine one — primal feasible,
 * and optimal for the costs the solve was working with — so the way to
 * improve on it is to make it dual feasible again and let the method run,
 * not to patch what it published.
 *
 * What makes this safe to attempt is the fallback rather than the attempt.
 * A model that has just been proved to have an optimum has not become
 * infeasible, and a re-entry that reports it has is reporting on itself:
 * the flips it made are a starting point of its own choosing, and the dual
 * simplex declaring no feasible point exists from there says the choice was
 * bad, not that the model is. So anything other than a second optimum is
 * discarded and the settled point stands. This is the failure both earlier
 * repairs produced — a feasible model returned INFEASIBLE — and it is the
 * reason the saved point exists at all rather than an afterthought about
 * robustness.
 *
 * A round that ends in a library error (out of memory, a basis that cannot
 * be factorized) is different in kind and propagates: those are not
 * verdicts about the model either, but nothing about them says the saved
 * point is still reachable.
 *
 * What this does *not* do, said plainly because the guard above invites the
 * assumption: a round's result is accepted for being a second optimum, not
 * for being a better one. Nothing here compares the two. The method that
 * produced the new point works on shifted costs exactly as the first pass
 * did, so it settles to a residue of its own and there is no argument from
 * construction that the residue is smaller. That the answers do improve is
 * a measurement across all three instance sets and not a property of this
 * loop — which is what the baselines under bench/ are for, and the reason
 * a criterion of "keep the smaller violation" was not invented here: the
 * solver has no oracle, and picking a scalar for "better" would be a guess
 * wearing the clothes of a guarantee. */
static jaos_status reenter_after_settling(sx *s)
{
    for (int64_t round = 0; round < SETTLE_ROUNDS; round++) {
        /* Asked before anything is saved, because the saving is what a solve
         * with nothing to repair would otherwise pay for: five arrays over
         * every variable, allocated on a path most solves never leave. On
         * `ken-18` that is seven megabytes to copy in order to discover
         * there was no work. */
        if (!anything_to_move(s)) {
            /* Nothing left that moving repairs. What can remain is a column
             * with nowhere to move to, and that needs a basis change. */
            if (!save_settled(s))
                return JAOS_ERR_OUT_OF_MEMORY;

            int64_t pivots = 0;
            jaos_status st = primal_cleanup(s, &pivots);
            if (st != JAOS_OK)
                return st;
            if (pivots == 0)
                return JAOS_OK;

            /* The basis has changed under the point, and `rho` now holds a
             * pricing row rather than the duals — which `column_traffic`
             * reads. So nothing may look at the state again before a
             * refresh rebuilds both from the factorization. */
            bool ok = false;
            s->verified = false;
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
                return JAOS_OK;
            }
            settle_shifts(s);
            continue;
        }
        if (!save_settled(s))
            return JAOS_ERR_OUT_OF_MEMORY;
        arm_reentry(s);

        /* The point has changed underneath the basis, so any verification
         * of the old one is spent and the factorization has to be re-read
         * before pricing believes anything. run() opens with a refresh,
         * which does both. */
        s->verified = false;
        s->needs_refactor = true;

        jaos_solve_status again = JAOS_SOLVE_NOT_RUN;
        jaos_status st = run(s, &again);
        if (st != JAOS_OK)
            return st;

        if (again == JAOS_SOLVE_OPTIMAL) {
            settle_shifts(s);
            continue;
        }

        bool ok = false;
        st = restore_settled(s, &ok);
        if (st != JAOS_OK)
            return st;
        if (!ok)
            return JAOS_ERR_NUMERICAL;
        settle_shifts(s);
        return JAOS_OK;
    }
    return JAOS_OK;
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
        return s->status[j] == JM_AT_LOWER && s->d[j] > s->dual_tol;
    if (s->fake[j] == FAKE_UP)
        return s->status[j] == JM_AT_UPPER && s->d[j] < -s->dual_tol;
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

/* Seconds since this solve started, and the only thing in the solver that
 * reads a clock. Two callers: the budget, which is the one place a clock is
 * allowed to end something a caller asked to be ended; and the figure
 * published at the end. They share this so the number a caller reads and the
 * number the limit was judged against cannot drift apart.
 *
 * A failed `clock_gettime` reads as zero elapsed. That makes the budget
 * infinite rather than instantly exhausted, which is the safe direction: a
 * solve that cannot read the clock should finish, not be cut off. */
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

static jaos_status run(sx *s, jaos_solve_status *out)
{
    /* Each entry into the loop is its own solve as far as progress goes: a
     * re-entry (D25) starts from a point of its own and must not inherit a
     * plateau counted against the pass before it. */
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
        /* Whoever is watching gets asked here, beside the budgets, because a
         * callback that stops a solve *is* a budget — one whose rule lives in
         * the caller instead of in a number. Asking on a fixed iteration
         * count and never on a clock is what keeps the question itself
         * reproducible: the same model puts it at the same iterations every
         * run, so a callback that answers the same way twice gets the same
         * solve twice, bit for bit. */
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
            /* A defect in JAOS, not a property of the model. Reporting it
             * as a solve outcome would put it in the same bucket as an
             * honestly hard problem.
             *
             * The two numbers beside the count are what the next person has
             * to know and used to have to instrument for. How long the total
             * infeasibility has stood still separates a solve that is
             * grinding from one that is stuck; whether Bland's rule was on
             * separates a cycle the anti-cycling rule never caught from one
             * it caught and could not finish off. Those are different
             * defects with different cures, and `pilot87` at a
             * refactorization interval of 128 is the second (D72). */
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

        /* Progress, on a count and never on a clock: a line that appeared
         * every so many milliseconds would make the output depend on the
         * machine, and output that differs between two runs of the same
         * model is the one thing D8 forbids.
         *
         * After the pricing rather than before it, so the first line carries
         * a number instead of the infinity `infeas_best` starts at. It is
         * the best total reached and says so — price_row computes the
         * current total for the stall test and keeps only the best, and
         * exposing the other one would mean carrying state for a log line. */
        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "iter %lld: best infeasibility %.6g, work %lld",
                   (long long)s->iters, s->infeas_best,
                   (long long)s->work.units);
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
             * work counter bills it (D16).
             *
             * An instance asked, and the answer was not what 2.5.5 predicted
             * (D29). `pilot` was refused on a row that no basic variable
             * violates: the disagreement is between the checker's dot
             * product and the solve x_B = -B^-1 (N x_N) that produced the
             * carried activity, on a factorization that is already fresh —
             * so refactorizing earlier could not have reached it. What does
             * is refining the solve, which is why this refresh is the one
             * that asks for it. */
            if (!s->verified) {
                st = refresh(s, &ok, true);
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
            /* No entering column can repair row r, which says the dual is
             * unbounded and the primal therefore has no feasible point.
             *
             * That verdict is read off `alpha`, and `alpha` is a BTRAN
             * against a factorization that has been patched by every update
             * since the last rebuild — the same carried numbers D20 refuses
             * to declare *optimality* on. The refusal belongs here too, and
             * more so. An optimum accepted too early is an answer with
             * error in it; an infeasibility accepted too early denies a
             * model that has an answer at all, which is the one outcome the
             * infeasible set exists to make impossible.
             *
             * The failure is not hypothetical and not rare. Every candidate
             * is rejected when no |alpha_v| clears PIVOT_MIN, and drift in
             * a patched factorization shrinks exactly those numbers. Sweep
             * the refactorization interval and it appears at every value
             * tried except the one in the tree: `pilot-ja`, `pilot-we`,
             * `pilot87`, `agg`, `greenbea` and `perold` all come back
             * INFEASIBLE with finite optima published for them (D39).
             *
             * So take a second opinion from a fresh factorization first. If
             * the columns really are unusable they still will be, and the
             * cost is one refactorization on a solve that is ending anyway. */
            if (!s->verified) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                s->verified = true;
                continue;
            }
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;
        }

        /* The basis is about to change, so any verification of the point
         * it implied is spent. */
        s->verified = false;
        bool took = false;
        st = pivot(s, r, q, below, theta_dual, &took);
        if (st != JAOS_OK)
            return st;

        /* A declined pivot changed nothing and costs no iteration. The top
         * of this loop refactorizes and the row is priced again from numbers
         * that agree with themselves. It cannot spin: a rebuild leaves
         * `n_updates` at zero, and the check declines only above zero. */
        if (!took)
            continue;

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
    if (m->sol_col != nullptr && m->sol_row != nullptr &&
        m->sol_dual != nullptr && m->sol_redcost != nullptr &&
        m->sol_col_status != nullptr && m->sol_row_status != nullptr)
        return JAOS_OK;

    /* All six or none. A partial set — left behind when one of these
     * allocations failed on an earlier solve — must not read as "already
     * there", or this publish writes through the missing ones. */
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

/* A published zero is a zero.
 *
 * IEEE keeps two of them, and which one a value lands on depends on the sign
 * of whatever produced it — so a solve can report -0.0 where an equivalent
 * one reports 0.0, for the same variable at the same optimum. The number is
 * the same and every test in the checker agrees they are the same; what
 * differs is the bytes.
 *
 * That matters because a byte-for-byte comparison of two published solutions
 * is this project's cheapest and strongest evidence that a change altered
 * nothing (D21). An instrument that reports a difference where there is no
 * difference in value is a worse instrument, and normalising here costs one
 * comparison per published number on a path that runs once per solve. */
static double published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

/* The internal status and the published one carry the same four cases, and
 * are mapped rather than cast: they are two enums that happen to agree
 * today, and a silent renumbering of either would otherwise publish a wrong
 * basis with no compile error anywhere. */
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

static jaos_status publish(sx *s, jaos_solve_status status)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;

    m->solve_status = status;
    m->solve_iters = s->iters;
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

        /* The basis is written, kept, and only then cleared, and the three
         * steps are three different statements.
         *
         * Written and kept because a solve that ran out of budget stopped at
         * a perfectly good basis, and starting the next one there is the
         * whole reason a budget is a thing a caller can set: `jaos_solve`
         * until the work limit, raise it, `jaos_solve` again, and the second
         * call continues instead of beginning. The same holds for a verdict
         * of infeasible or unbounded — the model is answered, but the next
         * model a branch-and-bound builds differs from it by one bound, and
         * that basis is the closest one there is.
         *
         * Cleared because `jaos_basis` publishes the basis *behind an answer*
         * and there is no answer. A stopping point is not a solution, and the
         * two must not be readable through the same call.
         *
         * A numerical failure is the one outcome left out. It cannot corrupt
         * anything — a warm start is never a claim — but it is the one state
         * this solver does not vouch for, and offering it as a starting point
         * would be recommending it. */
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
        /* Taken beside the work snapshot and for its reason: publishing runs
         * a kernel, so a figure taken before it would be short by one BTRAN.
         * Seconds are a development number here as everywhere -- reported,
         * never entering a baseline (D17). */
        m->solve_time = elapsed_seconds(s);
        return JAOS_OK;
    }

    /* Out of the scaled copy and back into the model's own units. A column
     * carries its factor, a row activity divides its own out; the duals go
     * the other way, because a dual is a rate per unit of the thing it
     * prices. Every entry is written, so no pre-zeroing is needed. */
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

    /* The basis is published unscaled in the only sense that applies to it:
     * scaling multiplies a column by a positive factor, which moves where a
     * bound is but never which bound a variable rests on. So the statuses
     * carry over from the scaled copy unchanged, and the logical of row i is
     * that row's activity — the same variable sol_row[i] was read from. */
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col_status[j] = published_status(s->status[j]);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row_status[i] = published_status(s->status[m->num_col + i]);

    double obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++)
        obj += m->col_cost[j] * m->sol_col[j];
    m->objective = obj;
    m->solve_work = s->work.units;
    m->solve_time = elapsed_seconds(s);

    /* Where the next solve will start, unless the caller says otherwise.
     *
     * The failure is swallowed on purpose, and it is the only place in this
     * file that swallows one. A starting basis is an optimisation and never a
     * claim: losing it costs the *next* solve some iterations and costs this
     * one nothing, while reporting it would abandon an answer that is already
     * computed, already correct and already written where the caller reads
     * it. The two are not the same kind of failure and must not get the same
     * answer. */
    (void)jm_model_remember_basis(m);
    return JAOS_OK;
}

jaos_status jm_dual_simplex(jaos_model *m)
{
    sx s;
    jaos_status st = sx_init(&s, m);
    if (st != JAOS_OK)
        return st;
    clock_gettime(CLOCK_MONOTONIC, &s.started);

    jm_log(m, JAOS_LOG_SUMMARY,
           "dual simplex: %lld rows, %lld columns, %lld nonzeros, "
           "primal tol %.3g, dual tol %.3g",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, s.primal_tol, s.dual_tol);

    jaos_solve_status outcome;
    bool warm = build_warm_basis(&s);
    if (!warm)
        build_initial_basis(&s);
    jm_log(m, JAOS_LOG_DETAIL, "starting from %s",
           warm ? "the basis on the model" : "the slack basis");
    st = run(&s, &outcome);
    if (st == JAOS_OK) {
        /* Settle first, then judge. The verdict turns on reduced costs, so
         * it has to read the model's own and not the shifted ones the
         * ratio test worked with. */
        if (outcome == JAOS_SOLVE_OPTIMAL) {
            settle_shifts(&s);
            st = reenter_after_settling(&s);
            if (st == JAOS_OK)
                outcome = classify_optimum(&s);
        }
        if (st == JAOS_OK)
            st = publish(&s, outcome);
    }

    /* What the solve did, in the terms a caller can act on. The three counts
     * are not decoration: four separate diagnoses this milestone had to
     * instrument the solver by hand to learn how often the weights were
     * being discarded, and a caller has no such option. */
    /* The three counts go on both branches, and the failing one needs them
     * more. A solve that ended is a solve nobody has to investigate; a solve
     * that was abandoned is the one where "how many times were the weights
     * discarded, and did it stall" is the first question anyone asks — and it
     * was the one branch that did not answer it. Found while diagnosing the
     * iteration guard, by wanting exactly these three numbers and being
     * handed a sentence instead. */
    if (st == JAOS_OK)
        jm_log(m, JAOS_LOG_SUMMARY,
               "%s after %lld iterations, %lld work units; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds",
               jaos_solve_status_str(outcome), (long long)s.iters,
               (long long)s.work.units, (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability);
    else
        jm_log(m, JAOS_LOG_SUMMARY,
               "abandoned after %lld iterations, %lld work units: %s; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds",
               (long long)s.iters, (long long)s.work.units,
               jaos_status_str(st), (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability);

    sx_free(&s);
    return st;
}
