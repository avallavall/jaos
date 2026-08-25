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

#include <assert.h>
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
 * better pivot. This is the width of the Harris window and **it is also what
 * the solve calls zero for a reduced cost at all** — `dual_breach`,
 * `published_breach`, `settled_dual_violation` and `points_outwards` all read
 * it, so it decides when there is nothing left to price and the solve stops.
 * Outside the ratio test the dual simplex keeps dual feasibility by
 * construction.
 *
 * **1e-7 published a point that was not the optimum on four instances and
 * this is 1e-9** (D184, on D174's sweep in `bench/measurements/02-84/`). A
 * reduced cost is a rate. What a column is still worth is that rate times the
 * distance it would travel, and this bounds the rate alone — on `pilot` a rate
 * under 1e-7 in scaled space was worth 2.31e-05 of objective, which is
 * 1.87e+08 times the floor arithmetic sets for that model, and `pilot` was the
 * only netlib instance HiGHS, SoPlex and Clp all beat.
 *
 *      dual_tol   1e-6      1e-7      1e-8    1e-9     1e-10   1e-11
 *      pilot      numerical 2.312e-05 2.312e-05 -5.266e-09 0   —
 *
 * At 1e-9 all four instances improve: `pilot` 2.312e-05 -> -5.266e-09 at
 * 0.9134x work, `pilot87` 1.044e-07 -> **exactly 0** at 0.9202x, `scsd6`
 * 1.118e-09 -> **exactly 0** at 1.0807x, `etamacro` 1.315e-08 -> -1.137e-13 at
 * 0.9934x. **Three of the four cost less work.**
 *
 * **1e-8 does not reach `pilot` at all**, so 1e-9 is the first setting that
 * does, and 1e-10 fails `dfl001` while 1e-11 fails `wood1p` too. The value is
 * bounded on both sides by measurement.
 *
 * The price is a netlib work geometric mean of 1.0339x and six instances past
 * `bench/run.c`'s 2.0x per-instance bar. **Those crossings are not monotone in
 * the tolerance and are trajectory scatter rather than cost**: `grow22` reads
 * 2.14x, 3.00x, 1.49x, 0.22x, 0.22x across 1e-6 to 1e-11, and `greenbea`
 * swings by a factor of three between adjacent settings (D174).
 *
 * A caller can still set this per model through `jaos_set_dual_tolerance`,
 * which reaches the same number and needs no rebuild (D64). */
constexpr double DUAL_TOL      = 1e-9;
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
 * become too long for a particular model.
 *
 * **Swept 2026-08-24, and it had never been** (D180,
 * `bench/measurements/02-92/`). Six settings, each its own tree and its own
 * binary, all three gate sets at every one:
 *
 *      interval     8      16      32     64     128     256
 *     work gm   1.0318  0.9484  0.9143  1.000  1.1873  1.5663
 *     worst      2.267   4.430   2.819      -   5.881   9.125
 *
 * The work column is a geometric mean of per-instance ratios against 64 and
 * the worst is the largest single ratio at that setting (D46: a set total is
 * a statement about two instances). **64 is not the work minimum** — 32 reads
 * 8.6% better on the mean — and moving there costs `grow15` 2.819x and
 * `pilot87` three orders of accuracy, 1.044e-07 to 5.329e-05. So the value
 * stays, chosen for the worst case rather than the mean, which is the shape
 * D151 chose its cap by.
 *
 * **No answer changes verdict at any setting**: 94 netlib and 29 infeasible
 * instances at six intervals, `objective=ok checker=ok det=ok` throughout.
 * The interval hides no defect, and `TODO.md` carried that sweep as a manual
 * debt because three of M1's four defect closures came out of running it by
 * hand.
 *
 * **What it did find is about `pilot`, and it belongs to that item rather
 * than to this constant.** Its distance from Koch reads 0 at 8, 32 and 128,
 * 2.312e-05 at 16 and at 64, and 5.266e-09 at 256 — not monotone, and 5.266e-09
 * is the same value D174's `dual_tol = 1e-9` reached. Two independent knobs
 * select among one small set of neighbouring vertices, so the tolerance is
 * what lets the solve stop and the trajectory is what decides where. */
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

/* How short a mapped starting basis may be and still be repaired rather than
 * refused (D151). build_warm_basis promotes logicals to close a shortfall;
 * this is the largest shortfall worth closing.
 *
 * A cap exists because the blanket repair was measured and refused (D149):
 * it is correct behind D148's certificate guard, and `dfl001` pays 172x work
 * for a 596-short repair whose trajectory the guard then throws away. The
 * value is where the repair pays and the guard rarely fires.
 *
 * The sweep, netlib work geometric mean against the cold solve, every
 * distinct shortfall in the set from "never repair" to "always"
 * (bench/measurements/02-60/cap-sweep.txt):
 *
 *      cap    0      1      2      4      5      6      7      8     596
 *     work  .2553  .2089  .2047  .1916  .1886  .1895  .1874  .1938  .2605
 *    worst   1.00   4.65   4.65   4.65   4.70   4.70  15.48  15.48  172.03
 *
 * Read the two rows together. The mean is flat across 1..7 and the worst
 * case is not: it holds at 4.65 through 4, and at 7 `greenbea` steps to
 * 15.48 and at 345 `dfl001` to 172. 4 takes the whole of the cheap gain —
 * 8.3% better than 1 — without moving the worst case at all. 5 buys a
 * further 1.6% and costs `brandy` 4.70 and `bnl1` 2.87; 7 buys 0.6% and
 * costs `greenbea` 15.48. So the number is chosen at the end of a plateau
 * rather than at a minimum, which is why it is 4 and not 7.
 *
 * **The obvious alternative shape was swept too and is worse.** A cap
 * relative to the model's rows (S <= r*nrow) reaches a best mean of only
 * .2081 and meets `greenbea`'s 15.48 at r = 0.0036, the eighth instance it
 * admits — `greenbea` is 7 short of 1954 rows, the smallest relative
 * shortfall in the set and one of the two worst outcomes. The shortfall's
 * absolute size separates these cases and its size relative to the model
 * does not.
 *
 * Kennington does not vote on the value. All five of its short solves are
 * short by exactly 1, so every cap at or above 1 gives that set the whole of
 * its gain (0.0572 to 0.0070); the number only trades netlib.
 *
 * **The two instances that lose most do NOT lose the same way, and the first
 * version of this paragraph said they did** (D178,
 * bench/measurements/02-90/). Only `degen2` is D148's guard: its settled warm
 * point carries a dual violation of 12.91, the trajectory is thrown away, and
 * the cold restart is why its warm and cold iteration counts are equal.
 * `scsd1` reaches a dual feasible point and the guard never fires — it
 * genuinely takes 314 iterations where cold takes 89. So there is one doomed
 * trajectory in the twenty that repair, not two, and a rule read off one
 * instance is not a rule.
 *
 * Eleven quantities this function knows BEFORE the solve were measured
 * against that split — the shortfall, how many rows the wanted basis leaves
 * uncovered, how many promotions each of the two loops below made, the
 * dimensions, and four ratios of them. Every one has winners inside the
 * losers' range, so none is a predictor.
 *
 * The ratios in the sweep above are D151's, at D151's tree, and the set has
 * moved since. The campaign at D177 reads a netlib work geometric mean of
 * 0.1910 against the sweep's predicted 0.1916, a worst case of 3.7165x on
 * `scsd1` rather than 4.65x, and `25fv47` now wins at 0.9854. Three
 * instances cost more warm than cold: `scsd1`, `degen2` and `lotfi`. Bounded,
 * and the price of the 17 that win. */
constexpr int64_t WARM_REPAIR_MAX_SHORT = 4;

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
     * called in at the end.
     *
     * `cost0` is the model's own scaled cost, written once and never again,
     * and calling a loan in RESTORES from it rather than subtracting the
     * record back out. Subtracting is what a repayment used to do and it is
     * not bit-exact: `x += d` then `x -= d` does not return `x`. On
     * `pilotnov`, **under the presolve reordering D118 refused**, the loans on
     * one column reached 1.6e+32 against a cost of magnitude one, which left
     * 67 costs permanently wrong, the worst of them off by 55.11, and the
     * solve published an objective 29% off as optimal (D121).
     *
     * No instance of the three gate sets reaches that, and `pilotnov` at HEAD
     * is bit-identical either way. What the sets do reach is the same
     * inexactness at a harmless size: 34 of their solution digests move when
     * the repayment becomes exact, and the residuals mostly improve with them
     * (D122). The defect is reachable and small everywhere it was looked for
     * — which is the argument for repairing it rather than bounding it.
     *
     * What makes the restore exact is only that `cost0` is the model's own
     * by construction. **There is no `cost[v] == cost0[v] + shift[v]`
     * invariant** and this comment used to claim one: `shift_to_feasible`
     * accumulates into the two arrays separately and they round apart at the
     * first lend that is large against the cost — `cost0 = 1`, one lend of
     * 1e17, and `cost` is 1e17 while `cost0 + shift` is 1e17 + 1. Every
     * reader who needs to know how much a cost moved must therefore compute
     * `cost[v] - cost0[v]` and never read `shift[v]` for it. Found by
     * `numerics-reviewer`.
     *
     * What `shift` *is* exact about: it holds the sum of every loan lent into
     * it since the last repayment, and nothing is lost from it. The array is
     * written at three sites and nowhere else — the lend in
     * `shift_to_feasible`, and the two repayments — measured at runtime
     * rather than read off the source, and validated against a dropped loan
     * (D124). */
    double *lo, *up, *cost;
    double *cost0;           /* [nvar] the model's own, never written twice */
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
    /* Two compensation terms, one per row each. `rhsc` is for the sum
     * compute_primal builds in `col`; `resc` is for the residual
     * subtract_basis_times forms in the refinement step, `b - B x_B` (D171).
     *
     * **They are separate arrays and that is not a spare-buffer decision.**
     * `rhsc` is still holding compute_primal's compensation while
     * subtract_basis_times runs, inside the same call.
     *
     * **The contract each of them offers a would-be borrower, stated exactly**
     * (`numerics-reviewer`). Each is `memset` at the entry of its single
     * reader and dead at its exit, so **neither carries a producer contract**:
     * nothing computes a value in one of them that anything else later reads.
     * A borrower keeping the same discipline — write it, read it, finish
     * inside one function body — is therefore safe. What is not safe is a
     * borrower whose value has to survive a call to `compute_primal(s, true)`,
     * because that call reaches both.
     *
     * An earlier version of this comment said `apply_flips` would need a third
     * array because both of these are live where it runs. **That was false**:
     * `apply_flips` is called from `dual_ratio_test` mid-iteration, with
     * `compute_primal` nowhere on the stack, and both are dead there. */
    double *rhsc;
    double *resc;
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

    /* Which variables are not basic, one bit each: bit v is set exactly when
     * `status[v] != JM_BASIC`. Membership, and never "has a finite bound" —
     * a nonbasic free variable belongs here like any other. It is what the
     * ratio test's dense branch walks instead of every variable in the
     * model, which is nearly all of what that branch used to do.
     *
     * `amark` above is zero between iterations and clears the words it sets;
     * this one is the opposite and deliberately so. It is persistent, it
     * survives every iteration, and clearing it would destroy the thing it
     * is for. jm_nonbasic_build is the only routine that writes it
     * wholesale; the eight sites that move a variable into or out of the
     * basis each maintain it by hand, and those eight are the only places it
     * can drift. */
    uint64_t *nbmark;        /* [(nvar + 63) / 64] */

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

#ifndef NDEBUG
    /* Where the candidate set the bitmap walk produced is parked, just long
     * enough for the scan it replaced to be run over the same state and
     * compared against it entry for entry (D-08). Present in every dev and
     * sanitizer build and in no shipped one, so no gate binary carries
     * them. */
    int64_t *dbg_cand;               /* [nvar] */
    double *dbg_rnum, *dbg_rden;     /* [nvar] */
    double *dbg_rrange;              /* [nvar] */
#endif

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

    /* And the best point any round reached, which is a different question
     * from the one above (D89). `sav_*` is where this round started, so a
     * round that fails can be undone; these are the best seen anywhere in the
     * loop, so a loop that oscillates does not publish whichever round the
     * cap happened to land on.
     *
     * `bst_obj` is that point's objective on the model's own costs. Every
     * round leaves a primal feasible point, so its objective is an upper
     * bound on the optimum and the lowest one is the best answer — which is
     * a guarantee rather than a scalar chosen for the occasion (D25). */
    jm_var_status *bst_status;
    int64_t *bst_basis;
    double *bst_lo, *bst_up;
    jm_fake *bst_fake;
    double bst_obj;
    /* And its worst dual sign violation in the model's own space, because
     * being defensible outranks being close — see better_point. */
    double bst_dviol;
    bool bst_valid;

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

    /* The same detector's measure for the primal method, and it is a
     * different quantity — which is why it is a different field.
     *
     * The dual keeps the reduced costs feasible and drives primal
     * infeasibility out, so `infeas_best` above is what a stalled dual solve
     * stops moving. **The primal is the mirror image**: it keeps the point
     * primal feasible and drives *dual* infeasibility out, so the total that
     * stops moving is the sum of the wrong-signed reduced costs.
     *
     * Storing it in `infeas_best` would have been one field carrying two
     * quantities, which is the defect class this file has already been caught
     * by once — see `can_move`, and D184. It also has a visible consequence:
     * `jaos_progress.primal_infeasibility` reads `infeas_best`, and a primal
     * solve genuinely does have zero primal infeasibility, so reporting that
     * is correct rather than a placeholder. */
    double dinfeas_best;

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

    /* Iterations `run_primal` took, as opposed to `iters`, which counts every
     * basis change the solve made whichever method made it.
     *
     * **It exists because nothing else could tell the two apart, and a test
     * proved that the hard way.** Four tests written for the primal method
     * all passed with `run_primal` doctored to declare optimality
     * immediately and pivot not once: the settling re-entry calls `run()`,
     * the dual repaired the point, and every assertion about the answer and
     * about `jaos_iterations` was satisfied by the wrong algorithm. A count
     * only one of them can raise is the observation that separates them, and
     * the closing summary line carries it so a caller can see which method
     * did the work as well. */
    int64_t n_primal_iters;

    /* The primal phase-1 cost vector: `-1` on a basic below a bound the model
     * declared, `+1` on one above, `0` everywhere else. Minimising it is
     * minimising the sum of bound violations, which is what phase 1 is.
     *
     * It is swapped into `s->cost` for the duration of one `compute_duals`
     * call and swapped straight back, so `d` comes back holding phase-1
     * reduced costs. That reuse is exact rather than convenient:
     * `compute_duals` reads `cost` and writes `y` and `d` and does nothing
     * else — no shifting, no repair — so pointing it at a different cost
     * vector gives the duals of a different objective and nothing more.
     *
     * Allocated on the first phase 1 rather than in `sx_init`, because a dual
     * solve never has one and a model re-solved thousands of times by branch
     * and bound should not pay for it per call. */
    double *c1;

    /* Is a primal method in flight? Read only by the two places a pivot lends
     * a cost — `update_dual` and the tail of `pivot()`.
     *
     * `shift_to_feasible` puts a reduced cost back on the feasible side by
     * moving the cost behind it. **The dual requires that**: dual feasibility
     * is its invariant, and a ratio test meeting a cost already past zero
     * computes a step with the wrong sign.
     *
     * **In the primal phase 1 it corrupts the model's objective, and the
     * measurement is exact.** Phase 1 puts *its own* reduced costs in `d` —
     * gradients of the sum of bound violations, so of magnitude one — and
     * `pivot()` then lends `cost[v]` whatever those say is needed. On `sc50a`
     * the primal stopped after two pivots with every reduced cost feasible
     * and **one variable carrying a loan of exactly 1.0**, the size of a
     * phase-1 cost and not of any repair. `settle_shifts` called it in, the
     * point was left 0.18 dual infeasible, and the D146 guard refused the
     * answer — on a model the dual solves in 47 iterations.
     *
     * Phase 2 is guarded for a second and weaker reason: its optimality test
     * is the absence of dual infeasibility, so a loan taken mid-solve makes
     * that test read as satisfied on costs the model does not own.
     *
     * **Guarding only `pivot()`'s own call is not enough and that was tried.**
     * `update_dual` lends against every variable the pricing row touches, once
     * per iteration, and it is the site that did the damage. */
    bool in_primal;
} sx;


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
#ifndef NDEBUG
    free(s->dbg_cand); free(s->dbg_rnum); free(s->dbg_rden);
    free(s->dbg_rrange);
#endif
    free(s->bs); free(s->bi); free(s->bv);
    free(s->fake); free(s->c1);
    free(s->sav_status); free(s->sav_basis);
    free(s->sav_lo); free(s->sav_up); free(s->sav_fake);
    free(s->bst_status); free(s->bst_basis);
    free(s->bst_lo); free(s->bst_up); free(s->bst_fake);
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
        !s->rden || !s->rrange || !s->bs || !s->fake) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

#ifndef NDEBUG
    /* The scratch the D-08 cross-check compares through. Allocated in its own
     * block rather than folded into the chain above so that the release
     * build's allocation list is the release build's, unchanged. */
    s->dbg_cand   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->dbg_rnum   = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_rden   = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_rrange = jm_alloc_array(s->nvar, sizeof(double));
    if (!s->dbg_cand || !s->dbg_rnum || !s->dbg_rden || !s->dbg_rrange) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
#endif

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
    /* The one write to cost0, and the only reason a repayment can be exact.
     * Everything after this line borrows from `cost` and gives back from
     * here (D121). */
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

    /* Every logical is basic and every structural is not, which is the whole
     * of what the bitmap has to say. Built rather than patched: this pair of
     * loops is the membership state, not a change to one. */
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);
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
 * **A nonbasic with no bounds is accepted, and rests free at zero (D90).**
 * That used to abandon the whole warm start, and the reason was real: the
 * method could not always price such a variable back off zero. `wants_a_pivot`
 * read a free nonbasic as sitting at an upper bound, so it repaired a positive
 * reduced cost and left a negative one alone, and the point was then published
 * as OPTIMAL when it was not. D85 repaired that — both it and
 * `primal_ratio_test` read the sign of the reduced cost now — so the premise
 * of the refusal is gone and the refusal went with it.
 *
 * What it costs is one row's worth of care: a free nonbasic pins its row's
 * activity at zero, which is a constraint the model does not have, and the
 * primal clean-up is what unpins it. What it buys was measured before it was
 * taken: D69 found `cycle` losing its entire warm start to this refusal, one
 * instance in 92.
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
     * field is written — but a SHORT count is repaired rather than refused
     * (D144). On a presolved re-solve this function judges the MAPPED basis,
     * and jm_presolve_run's mapping drops every stored-basic member presolve
     * removes again, so since D139 an exactly-published basis maps short on
     * a third of the warm solves (54 of 88 netlib, 5 of 11 Kennington,
     * worst shortfall 596 — bench/measurements/02-51 and 02-52). The
     * shortfall is a difference of five families' terms, which is why no
     * mapping-side pass repairs it and this consumer does.
     *
     * While short BY AT MOST WARM_REPAIR_MAX_SHORT, promote the logical of an
     * UNCOVERED row first — a row no wanted-basic member touches makes B
     * structurally singular without its own e_i — then logicals in fixed row
     * order (D8; the order is index order, never an address or a value). A
     * shortfall past the cap falls back to cold, because the repair stops
     * paying there and D148's guard throws the trajectory away anyway; the
     * constant carries its own sweep. A LONG count is still refused:
     * no long map has been measured, and a demotion rule for an unmeasured
     * case would be a constant fitted to nothing. Rank stays where it
     * already lives (repair_singular_basis), and the weights below restart
     * at one regardless. The stored arrays are the model's and are never
     * written; the repair happens on a copy that lives to the end of this
     * function. */
    /* OOM below falls back to cold and reports JAOS_OK, where the rest of
     * the solve surfaces OOM as an error. That is a stated rule, not an
     * accident: a warm start is an optimisation and never a claim, and the
     * cold path is always correct. The cost of the rule is that a machine
     * failing either the 4·nvar-byte status copy or the nrow-byte coverage
     * array can publish a different optimal vertex of a degenerate model
     * than one that does not — both OPTIMAL, both checked — which is
     * accepted because such a machine fails loudly a few lines later
     * anyway. Two allocations can fail here, not one, and the status is 4
     * bytes rather than 8 because jaos_basis_status is an unfixed enum
     * over 0..3 (measured, D151 review). */
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
        /* Billed like every other per-nonzero walk in this file: the warm
         * campaign this repair is judged by reads work units, and an
         * unbilled pass on exactly the solves it enables would flatter the
         * change being judged. */
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
        /* Said out loud so a later restart of this same solve names the
         * basis it threw away as a repaired one — the first question a
         * diagnosis of such a restart asks, found in review. */
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

        /* The stored side is kept when it is still there, the other one taken
         * when it is not, and **free when there is neither** — which is a
         * point the method can now price off zero (D90). */
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

    /* Same reason as in build_initial_basis: the loop above is the whole
     * membership state, so the bitmap is built from it rather than patched.
     * Every return before this point is taken before any status is written. */
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
 * assembled B to multiply by.
 *
 * **Compensated, like the sum it subtracts from (D171).** `r` arrives holding
 * `b`, which compute_primal now sums with compensation, and this is the other
 * half of the refinement residual. Leaving it naive was refused on an argument
 * — that these terms are products of `x_B`, an FTRAN output already carrying
 * the factorization's error, so an accumulator cannot reach an error that is
 * already inside a term. **The argument is sound and the conclusion was
 * wrong**: the residual is what the correction is computed from, and losing a
 * term there leaves a correction that is short. `pds-20` published a column
 * 8.81e-13 outside its own bound and now publishes one 5.05e-28 outside it;
 * `pds-06` and `pds-10` are the same shape. `bench/measurements/02-81/`
 * carries it, with the two halves of D29's symmetry measured separately —
 * compensating `compute_duals`' refinement dot as well changes not one count,
 * and is refused there.
 *
 * The cost is nothing the work counter can see and nothing it can charge:
 * geometric mean 1.0000x on netlib and on Kennington, 0 verdicts moved on any
 * of the 139. */
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
    /* Same guard and same reason as compute_primal's: a partial sum at inf or
     * NaN can never come back to finite, so catching it once at the end is
     * enough. */
    for (int64_t i = 0; i < s->nrow; i++)
        if (isfinite(r[i]) && isfinite(comp[i]))
            r[i] += comp[i];
    jm_work_add(&s->work, nz * JM_WORK_NONZERO);
}

/* x_B = -B^-1 (N x_N).
 *
 * `refine` asks for one step of iterative refinement on that solve: the
 * residual of the system is computed against the basis columns themselves
 * and solved for again, and the correction added. See refresh() for which
 * solves get it and why the rest do not.
 *
 * **The right-hand side is accumulated with Neumaier compensation (D168).**
 * `-N x_N` is a sum over every nonbasic variable's column, taken in column
 * order, and a row is a slot that many of those columns write into. A row
 * that meets a large term before many small ones loses the small ones
 * outright: each of them is below half an ulp of the running total, so each
 * addition returns the total unchanged and the whole tail is dropped.
 *
 * That is the same defect D165 removed from presolve's `cur_rl`/`cur_ru`, one
 * layer out, and it was a wrong answer here too. On D162's model — 256 columns
 * fixed at a quarter of an ulp of 1e9, and a feasible point every value of
 * which is a dyadic rational a double holds exactly — the solve reads
 * INFEASIBLE, because the activity it computes is short by 2^-17 and no
 * variable left in the model can make that up. `bench/measurements/02-78/`
 * carries the reading; the model is
 * `test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total`.
 *
 * The accumulator is the one presolve uses (`ps_acc_add` there), for the
 * reason written beside it: `long double` would buy the same accuracy and
 * break the cross-machine determinism claim (D34), while Neumaier is portable
 * and its two-term error recovery is exact under `-ffp-contract=off`.
 *
 * The cost is paid once per refactorization rather than once per iteration,
 * since this is called from refresh() and nowhere else.
 *
 * **`subtract_basis_times` was left alone here and is compensated now
 * (D171).** The argument for leaving it was that it sums products of `x_B`, an
 * FTRAN output already carrying the factorization's error, so an accumulator
 * cannot reach an error already inside a term. True, and it does not follow:
 * the residual is what the refinement correction is computed FROM. Three
 * Kennington instances published a column outside its own declared bound and
 * stopped.
 *
 * **`apply_flips` is still uncompensated and that is deliberate.** It
 * accumulates over a bound-flip batch, so mid-solve `x_B` loses terms again
 * after every batch; the final `refine = true` refresh rebuilds `x_B` from
 * scratch, so what it loses does not reach the answer. */
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

    /* An infinite or NaN partial sum carries no residue a correction could
     * hold, and `inf + (inf - inf)` is a NaN — the failure mode where a row
     * bound comparison silently answers false. The reason is D165's; the
     * PLACEMENT is not, and the difference is load-bearing. Presolve guards
     * every step and zeroes its compensation there; this guards once, after
     * the loop. That is sound only because a partial sum cannot come back:
     * `inf + finite` is `inf`, `inf + (-inf)` is NaN, and `NaN + x` is NaN,
     * so a row that ever reaches either stays there and is caught here.
     *
     * This pass bills no work units, and neither does the extra arithmetic in
     * the loop above — the same JM_WORK_NONZERO per nonzero covers about four
     * times the operations now. **So the counter cannot see this change**, and
     * any work-unit movement it reports on a campaign is trajectory rather
     * than arithmetic. `bench/measurements/02-78/timing.txt` is what measures
     * the arithmetic. */
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
     * the order they stand. Any other order is a different trajectory.
     *
     * The same argument carries the branch below it. `nbmark` holds every
     * variable that is not basic, and a basic one is what admit_candidate's
     * first test rejects — so walking the bitmap admits the candidates the
     * walk over [0, nvar) admitted, and walking it in bit order puts them
     * where that walk put them. Ascending by construction, because the bit
     * position *is* the variable index: there is no order to restore. */
    if (s->anpat >= 0) {
        for (int64_t t = 0; t < s->anpat; t++)
            admit_candidate(s, s->apat[t], below, &n);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    } else {
        /* How many variables were actually handed to admit_candidate, which
         * is what this branch is charged for (D93). Both branches now bill
         * the same rule — one per variable looked at — where this one used
         * to bill s->nvar for a walk that stopped visiting every variable
         * the moment it became a walk over nbmark. Charging the dimension
         * for a scan that no longer reads it would have hidden the whole of
         * this phase's saving in the one currency the project measures in
         * (D16).
         *
         * The bitmap words themselves are not charged. A word is not a
         * variable, and the rate at which one would be worth counting has no
         * measurement on either side of it; the skipping is the mechanism
         * that produces the saving rather than work done on a variable. It
         * joins the pricing form's own sweep over the variables, which
         * docs/work-units.md records as real work that no unit counts. */
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

        /* The bitmap's popcount is invariant wherever this branch runs:
         * exactly nrow variables carry JM_BASIC, so exactly nvar - nrow do
         * not. build_warm_basis enforces the count, repair_singular_basis
         * refuses an already-basic entering, and pivot swaps one for one.
         *
         * This is not redundant with the dn == n cross-check below, which
         * cannot see the fault this catches. That check compares candidate
         * SETS, and admit_candidate rejects JM_BASIC on its first line — so
         * a superset bitmap (a missed jm_nonbasic_remove, or a ninth
         * membership site added without its hook) yields an identical
         * candidate set and the cross-check stays silent. D93 recorded that
         * silence as correct because a superset was then only a performance
         * fault. It stopped being one the moment the line above started
         * billing `visited`: an inflated count moves s->work.units, which is
         * the gate's currency and is compared against cfg.work_limit, so the
         * same model can stop at a different point and publish a different
         * answer. A count check is O(1) and closes both directions. */
        assert(visited == s->nvar - s->nrow);
    }

#ifndef NDEBUG
    /* D-08: both scans, over the state that produced them.
     *
     * The bitmap is maintained by hand at eight sites, and a drift at any of
     * them is invisible from the solve — a variable dropped from the set is
     * left out of a ratio test that would have been correct with it, so the
     * solve carries on and publishes an answer that is merely different.
     * Only a solution digest says so, and by then the run is over. So every
     * iteration of every dev and sanitizer build runs the scan the branch
     * above replaced and requires the two candidate sets to agree in count
     * and position for position. D30 is why this is an assertion and not a
     * comment: that contract was documented correctly, prominently, in the
     * function it protected, and was violated anyway.
     *
     * It charges no work, deliberately. A second jm_work_add here would give
     * a dev build a different accounting from the release build that
     * produces every gate number, and the pinned work test would then be
     * pinned to a figure no gate ever sees. */
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

/* The nonbasic set as a bitmap. Documented in the header, beside the two
 * picks and the pattern order, and reachable for the same reason they are.
 *
 * Note the one way these differ from jm_pattern_order: the bitmap they carry
 * is persistent and nothing here clears it. See the header, and the comment
 * on `nbmark` in sx. */
int64_t jm_nonbasic_build(int64_t nvar, const jm_var_status *status,
                          uint64_t *mark)
{
    int64_t nwords = (nvar + 63) / 64;
    for (int64_t w = 0; w < nwords; w++)
        mark[w] = 0;

    /* Membership, not bounds: everything that is not JM_BASIC belongs here,
     * JM_FREE included. */
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

/* The testable mirror of the walk in dual_ratio_test — the same words in the
 * same order, materialised. The ratio test does not call it: an index array
 * is exactly the traffic the bitmap exists to remove. */
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

/* Row r of `B^-1` into `rho`, and row r of `B^-1 M` into `alpha`.
 *
 * Both methods need exactly this and neither can proceed without it. The dual
 * picks its entering column out of `alpha`; the primal has already chosen one
 * and needs the row to step every other reduced cost by, which is the update
 * `pivot()` performs. So this is genuinely common ground and not a
 * convenience — `docs/research/primal-simplex.md` §5 lists the pivot row
 * first among the things one factorization gives both algorithms.
 *
 * It used to live inside `price_and_select`, with `primal_cleanup` repeating
 * the two lines on the stated grounds that "pulling them into a helper would
 * put the dual method's preamble in a function the dual method does not
 * call". That was right while the dual was the only caller with a claim on
 * it. There are three now, and repeating a BTRAN plus a pricing pass three
 * times is how the three drift apart. **What the old copy in
 * `primal_cleanup` also lost is real: it built `alpha` column by column
 * through `price_entry`, which costs the whole matrix however sparse `rho`
 * is, where `price_all` walks the row-wise mirror and skips a whole row of
 * the matrix per zero of `rho` (D35).**
 *
 * Leaves `nrpat` and `anpat` describing the two patterns, or negative where
 * one was too dense to be worth carrying. */
static void build_pricing_row(sx *s, int64_t r)
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
}

static int64_t price_and_select(sx *s, int64_t r, bool below,
                                double violation, double *theta_dual)
{
    build_pricing_row(s, r);
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
 * repaid in settle_shifts.
 *
 * **The record is what the cost actually moved by, not what was asked for.**
 * The two differ whenever `need` is below half an ulp of the cost: the
 * addition leaves the cost where it was, and `shift[v] += need` then wrote
 * down a loan that was never made. D125 measured that on **167816 of
 * netlib's 1006960 lends, 16.7%**, and on 14.3% of Kennington's.
 *
 * `d[v] = 0.0` stays, and it is not the same question. It rounds a reduced
 * cost to zero when the cost that produced it could not move, which is what a
 * sum below the noise of its terms deserves. **Removing it was measured and
 * refused**: the breach then compounds across iterations, because
 * `update_dual` pushes the same variable further every iteration and nothing
 * resets it (D126).
 *
 * The blast radius of the record alone was measured before the change, not
 * after. `settle_shifts` skips `compute_duals` and `repair_dual_infeasibility`
 * when `repay_shifts` reports nothing outstanding, so a phantom loan forces a
 * re-pricing. Over all three sets that is **2 of 290 calls**, on 2 of netlib's
 * 188 solves and none of Kennington's or the infeasible set's
 * (`bench/measurements/02-37/`). */
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

/* One variable's share of the dual step, and the repair that follows it.
 * Split out for the reason admit_candidate is: the loop around it comes in
 * two forms and a rule this delicate must not be written twice. */
static void update_dual(sx *s, int64_t v, int64_t q, double theta_dual)
{
    if (s->status[v] == JM_BASIC || v == q)
        return;
    s->d[v] -= theta_dual * s->alpha[v];
    if (!s->in_primal)
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

    /* The bitmap moves with the status, on the same lines, so that the pair
     * cannot drift apart: one variable leaves the basis and joins the set,
     * one enters it and leaves the set. This is the site that runs every
     * iteration and the one the whole scheme rests on. */
    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
    jm_nonbasic_insert(s->nbmark, leaving);
    s->where[leaving] = -1;
    s->basis[r] = q;
    s->status[q] = JM_BASIC;
    jm_nonbasic_remove(s->nbmark, q);
    s->where[q] = r;

    /* The leaving variable's reduced cost is minus the dual step, which is
     * feasible for the bound it left to — unless the step itself came out
     * of a cost that was already past zero, which is the case the shifting
     * exists to make impossible. Checked rather than assumed.
 */
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
 * model's own costs and recomputing them would only add rounding.
 *
 * The cost is RESTORED from `cost0` rather than having the loan subtracted
 * back out, and the two are not the same thing. Subtracting is `x += d`
 * followed by `x -= d`, which does not return `x`: on `pilotnov` under the
 * presolve reordering D118 refused, the loans on one column summed to 1.6e+32
 * against a cost of magnitude one, and the round trip left 67 costs
 * permanently wrong, the worst by 55.11, with every shift record correctly at
 * zero. The solve then priced an objective nobody asked for, called it dual
 * feasible, and published a result 29% off as optimal (D121). Restoring is
 * exact and needs no argument about magnitudes.
 *
 * The test is on the COST and not on the record alone. A column whose cost
 * moved while its record came back to exactly zero is the case D121 measured
 * **67** of on `pilotnov` (`bench/measurements/02-29/cost-drift.txt`,
 * `moved=67 shift_still_pending=0`). D122 cited 186 here, which is a
 * different measurement in the same file — the loans whose lent and repaid
 * totals differ — and D124 showed that one is the tally re-associating rather
 * than anything the solver does. It is reachable by plain cancellation too:
 * lend +1e17 against a cost of 1, later lend -1e17, and `cost` is 0 with
 * `shift` at 0 and the model's own 1 gone. Gating on the record alone would
 * skip it, and would also return `false` here, so `settle_shifts` would not
 * even re-price it. With the cost compared, no column can leave a settle
 * different from `cost0`. Found by `numerics-reviewer`. */
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

/* The same condition, read in the space the answer is **published** in.
 *
 * D27 stated this difficulty and resolved half of it. A reduced cost seen
 * through the scaling is the same number under a change of variable chosen for
 * the solver's convenience, and the two readings disagree about whether there
 * is anything there at all: the term this exists for reads `6.53e-09` scaled
 * on `pilot87` — a hundredth of what the solver calls zero — and `1.67e-06`
 * once `publish` multiplies by a row scale of 256, which is past the tolerance
 * the independent checker judges it at. D27's answer was that any rule reading
 * the breach must pick one space, and it then arranged for `can_move` not to
 * read the breach at all: it tests `|d|` times the width of the box, a product
 * `publish` leaves invariant, so there is nothing to choose.
 *
 * **Neither reading may replace the other, and that is D92's whole result.**
 * Substituting this for `dual_breach` in the predicates that select a
 * clean-up pivot repairs the defect and costs `pilot87` its suboptimality
 * bound — `Q` from 0.00682 to 26.8, for 2.9x the work. The reason is not the
 * one it looks like. Counted over `pilot87`'s three solves, the published
 * reading **adds two** candidates, both on the perturbed model and both the
 * offender's kind, and **drops twenty-six**: three on the unperturbed solve,
 * eleven on the warm one, twelve on the cold. Every dropped one is a column
 * whose scale factor is above one, so a breach that is real in the arithmetic
 * falls under DUAL_TOL once published. The regression was the solver ceasing
 * to repair residue it repairs today, not chasing residue it should not.
 *
 * So the selection asks for **either** — see `breached`. A scale factor above
 * one hides a breach from the caller's view, one below it hides a breach from
 * the solver's, and neither view dominates. Repairing a residue the caller
 * cannot see costs iterations; ignoring one the caller can see is a wrong
 * answer, so the union is conservative in the only direction that matters.
 *
 * `settled_dual_violation` reads this one alone rather than the union,
 * because it answers a different question — how defensible the point is to
 * whoever receives it — and that question has only one space.
 *
 * The scale factors are per column and per row, always populated, and 1.0 when
 * nothing was scaled, so this is one multiply and never a branch. */
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

/* Is there a sign-condition breach here at all — in either space?
 *
 * The question the clean-up's selection asks, and the only one of the three
 * questions about a breach that takes the union. What is worth *moving* is
 * D27's contribution test, which has no space; what makes an answer
 * *defensible* is the published breach alone; what is *there to repair* is
 * this, because the scaling is a change of variable and a residue does not
 * stop existing by being looked at through one. See `published_breach` for
 * the measurement that settled it (D92). */
static bool breached(const sx *s, int64_t v)
{
    return dual_breach(s, v) != 0.0 || published_breach(s, v) != 0.0;
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
/* The objective of the point as it stands, on the model's own costs.
 *
 * `cost0` is read rather than `cost - shift`, and that is not a tidy-up. The
 * subtraction used to be described here as belt and braces; with a loan large
 * against the cost it lands on, it is the perturbed objective wearing the
 * model's name (D121). cost0 is the model's own by construction.
 *
 * Scaling cancels: a column's scaled cost is `c_j * gamma_j` and its scaled
 * value `x_j / gamma_j`, so the product is the model's own — which is what
 * makes this comparable across rounds without leaving the solver's space, and
 * is the same cancellation D87 relies on. */
static double settled_objective(const sx *s)
{
#ifndef NDEBUG
    /* The precondition, enforced rather than commented. Every caller today
     * reaches this with the loans settled; reading `cost0` unconditionally is
     * only right while that holds, and a future caller must not be able to
     * break it in silence. Costs the release build nothing. */
    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]);
#endif
    /* Compensated, and with each product's own rounding recovered, because
     * this number does not describe a trajectory — it RANKS two points, and
     * `take_best_if_better` publishes the winner (D175). The two steps are
     * `jm_model_publish_objective`'s own, shared rather than copied.
     *
     * The naive sum's failure is a tie rather than a small error: with a cost
     * of 1e16 met before 256 unit terms, one ulp at 1e16 is 2, so every one
     * of them is lost and a -1e16 column brings the total to exactly 0.0 for
     * a whole family of points. `better_point` then reads `0 < 0`, answers
     * no, and the loop keeps whichever round it stopped on
     * (`bench/measurements/02-85/two-points.txt`).
     *
     * On the three gate sets it changes nothing and that is measured, not
     * assumed: 276 comparisons, 0 verdicts moved, and of the 190 that tie
     * exactly every one is a point compared with itself. Only 4 comparisons
     * on the whole population are decided by the objective between two
     * distinct points, and the worst margin there is 1.53e-06 of the
     * separation. */
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
    /* An infinite or NaN partial sum carries no residue a correction could
     * hold, and `inf + (inf - inf)` is a NaN. The guard is D165's, and the
     * same one `jm_model_publish_objective` carries. */
    return (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

/* The worst dual sign violation the point carries, **in the model's own
 * space** rather than the solver's.
 *
 * The space is the whole difficulty and getting it wrong is D27's fault class.
 * `dual_breach` is a shifted cost in the scaled problem, and publishing
 * divides a structural's reduced cost by `gamma_j` and multiplies a logical's
 * by `rho_i` — per column and per row, not one global factor. So the scaled
 * numbers of two different rounds are not comparable when the columns that
 * breach differ between them, which D50 recorded that they do. Undoing the
 * scaling here is what makes the comparison mean anything, and it is also
 * exactly the quantity the independent checker will judge.
 *
 * **It used to unscale `dual_breach`'s output, which is not the same thing and
 * is why this function did not do what the paragraph above says it did.** The
 * tolerance was applied in the scaled space and only the survivors converted,
 * so a breach inside DUAL_TOL scaled and outside it published arrived here as
 * an exact zero: the worst violation the point actually carried was invisible
 * to the one number that ranks the rounds. `published_breach` applies the
 * tolerance after the conversion, which is what "in the model's own space"
 * meant all along (D92). */
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

/* Keeps the best point the loop has reached, and it is a different job from
 * save_settled (D89).
 *
 * D25's loop accepts a round's result for being a second optimum, not a
 * better one, and says plainly that nothing compares the two. That was
 * measured on trajectories that converge. On one that oscillates — `pilot87`
 * at a refactorization interval of 24 alternates between two levels of
 * residue with period four, five times over — not comparing means **the
 * answer published is decided by where the round cap falls**, and a constant
 * chosen for being generous is not a tie-break rule.
 *
 * **What "better" means, and the first answer was wrong.** Every round leaves
 * a primal feasible point, so its objective is an upper bound on the optimum
 * and the lowest one looks like the obvious winner. Measured, that publishes
 * a point the independent checker *rejects*: on `pilot87` at interval 24 the
 * best objective belongs to a round carrying a dual violation of 5.12e-06,
 * five times what the checker calls zero. Trading a defensible answer for a
 * closer one is not an improvement in a solver whose verdict is OPTIMAL —
 * that verdict rests on dual feasibility, and publishing it over something an
 * independent check refuses is the thing the gate exists to prevent.
 *
 * So the order is lexicographic: **be defensible first, be close second.** A
 * round whose dual violation is inside tolerance beats one that is not,
 * whatever their objectives; between two that are, the lower objective wins;
 * between two that are not, the smaller violation. Both quantities are in the
 * model's own space, so neither comparison depends on the scaling.
 *
 * D50 asked for keep-the-best and left it untried because the quantity to
 * compare was unsettled. It had `dual_breach` in mind, which would have been
 * wrong twice over: the wrong space, and the wrong question. */
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

/* Offers the best point to the loop, and takes it only if it beats where the
 * loop actually stopped. Silent when it does not, which is the common case: a
 * converging trajectory ends on its own best round. */
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
    /* The memcpy above replaced every status at once, so the bitmap is
     * rebuilt here for exactly the reason `where` is: what it described no
     * longer exists, and a restore carries no assignment for anything to
     * hook. This site and the one in restore_settled are the two the
     * research's table of assignment forms cannot see. */
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    s->needs_refactor = true;
    s->verified = false;
    return refresh(s, ok, true);
}

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
    /* And here, for the reason given in take_best_if_better. */
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

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
             * is worth repairing.
             *
             * And it stays in the *scaled* space, because it is a statement
             * about the arithmetic in flight rather than about the answer:
             * the ratio test it protects works on scaled costs. Reading the
             * published space here was measured — it shifts costs the scaling
             * had hidden, which by this function's own argument hands the
             * method no work, and it costs `pilot87` its certificate (D92). */
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
 * anything there: past DUAL_TOL in **either** space, which is `breached`, and
 * above the rounding of the dot product that produced it (D27).
 *
 * The first used to read the scaled space alone, and that is the defect D92
 * repairs: a logical of `pilot87` rested at its upper bound with a reduced
 * cost of 6.53e-09 scaled and 1.67e-06 published, because the row's scale
 * factor is 256. It has no other bound, so nothing could move it and its term
 * in `P − D` is zero — a pivot was the only repair available and this
 * predicate never offered one. Reading the published space *instead* was
 * measured and refused: it drops twenty-six candidates across the same
 * instance to gain two.
 *
 * The second filter stays scaled, and deliberately. It asks whether `|d|`
 * stands above the rounding of `c_j − y' M_j`, which is a question about the
 * arithmetic and belongs where the arithmetic happened; `column_traffic` is
 * the scaled sum for the same reason, and a ratio of two scaled quantities has
 * no space to get wrong.
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
 * computed — `breached` above has already established the sign for them — so
 * this is the same number for everything except the case it repairs. */
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

/* How far column q can travel before a basic variable reaches a bound.
 *
 * `x_B = -B^-1 N x_N`, so moving q by `dx` moves the basics by
 * `-B^-1 M_q dx`. q travels upwards off a lower bound and downwards off an
 * upper one — the direction its reduced cost points — and each basic is
 * stopped by whichever of its bounds lies that way.
 *
 * **The direction is read off the reduced cost rather than off the status**,
 * and that is the same rule stated once instead of twice. A column only
 * reaches here past `wants_a_pivot`, which is past `breached`, so at a
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
 * on, or -1. `*step` receives how far q may travel before that position
 * blocks, and `HUGE_VAL` when nothing does — `run_primal` compares it against
 * the distance to q's *own* opposite bound, which is the one limit no basic
 * variable can express.
 *
 * **It leaves `B^-1 M_q` in `s->col`, and a bound flip reads it there.**
 * Moving q by `delta` moves the basics by `-delta * col`, so the flip needs no
 * solve of its own. Anything writing `col` between this and that would be
 * writing the flip's input. */
static int64_t primal_ratio_test(sx *s, int64_t q, bool *below, double *step)
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
    *step = best_step;
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
 * which is what `price_and_select` normally leaves behind. Both now come
 * from `build_pricing_row`, which is the ground the two methods genuinely
 * share. **This loop used to build them itself and paid for it**: a dense
 * `price_entry` pass over every column, which costs the whole matrix however
 * sparse `rho` is, where the shared helper walks the row-wise mirror and
 * skips a row of the matrix per zero of `rho` (D35).
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
        if (s->cost[q] != s->cost0[q] || s->shift[q] != 0.0) {
            /* Restored from cost0, not subtracted back — repay_shifts
             * carries both arguments (D121, and the test on the cost rather
             * than on the record alone).
             *
             * `d` moves by the amount the COST actually moved, which is not
             * `shift[q]`: `d` is `cost[q] - y·M_q` by definition, so any
             * other step leaves the two disagreeing by exactly the drift
             * this repair exists to remove — on the one quantity that then
             * decides the pivot, since `breached` reads `d[q]` four lines
             * down and `theta_dual` carries it into every other reduced cost
             * through `update_dual`. Found by `numerics-reviewer`. */
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
         * declared bound in the improving direction, so the flip `run_primal`
         * uses it for can never arise on this path. */
        int64_t r = primal_ratio_test(s, q, &below, &step);
        if (r < 0)
            continue;

        build_pricing_row(s, r);

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
    /* The point on entry is a candidate like any other, and on a converging
     * trajectory it is often beaten immediately. Recorded before the first
     * round so that a loop which only ever makes things worse still publishes
     * what it was given (D89). */
    s->bst_valid = false;
    if (!save_best(s))
        return JAOS_ERR_OUT_OF_MEMORY;

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
            if (pivots == 0) {
                /* The loop has run out of work rather than out of rounds,
                 * which is the ending it is built for. Even so the best point
                 * may be behind it, so it is offered here too. */
                bool ok = false;
                st = take_best_if_better(s, &ok);
                if (st != JAOS_OK)
                    return st;
                return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
            }

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
            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;
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

    /* The rounds ran out, which is the case this exists for: the loop was
     * oscillating and stopped on whichever round the cap landed on. Publish
     * the best one instead (D89). */
    bool ok = false;
    jaos_status st = take_best_if_better(s, &ok);
    if (st != JAOS_OK)
        return st;
    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
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

/* --------------------------------------------------------------------- */
/* The primal method                                                     */
/* --------------------------------------------------------------------- */

/* How far the worst basic variable is outside a bound the model declared.
 *
 * The primal method's whole invariant is that this stays at zero, so it is
 * asked once before the loop starts and never again: every step the ratio
 * test allows keeps it there by construction. `price_row` answers the same
 * question for the dual, but it also writes the stall counters and the Bland
 * flag on its way through, and a feasibility check that moves state is a
 * check nobody can call twice.
 *
 * **Only bounds the model declared count**, `real_lower`/`real_upper` rather
 * than `lo`/`up`, for the reason `primal_ratio_test` gives at length: a basic
 * resting outside a bound dual phase 1 *invented* is not primal infeasible in
 * any sense the caller would recognise, and refusing to start on one would
 * refuse a point that is perfectly good. */
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

/* The entering column, by Dantzig's rule: the eligible nonbasic whose reduced
 * cost is furthest on the wrong side. Returns -1 when none is, which is
 * optimality for the costs in force.
 *
 * **Dantzig and not Devex, deliberately, and `TODO.md` §0 owns the order.**
 * Devex needs a weight recurrence that appears in no source this project has
 * been able to read — Harris (1973) is paywalled — and a sign or a `max()`
 * misplaced there gives a solver that works and is slow, which is the hardest
 * kind of defect to find. Dantzig needs no paper. It is a worse rule and the
 * primal was never a speed argument here (D81), so correctness first.
 *
 * **Full pricing, and it stays full.** D82 and D84 refused partial and
 * multiple pricing on *wrong answers* rather than on a trade — `pilot`
 * published OPTIMAL on an objective outside tolerance and the checker passed
 * it. Maros's pricing report says the two normalized rules a primal would
 * actually want are unsuitable for multiple pricing anyway, so the refusal
 * costs this nothing (`docs/research/primal-simplex.md` §3).
 *
 * **Eligibility is `dual_breach` and not `breached`.** The union of the two
 * spaces exists for the clean-up's *repair* question — is there a residue to
 * remove — and that is not this question. Pricing asks how fast the objective
 * improves per unit of movement, which is a rate in the space the arithmetic
 * happens in, and the ratio test and the step both work in that same space. A
 * column breached only in the published space is below what this solver calls
 * zero in its own, and the clean-up is what still attends to it (D92).
 *
 * **A fixed column never enters.** `lo == up` leaves it nothing to move
 * through, so every step it could take is zero, and choosing it repeatedly is
 * a cycle with no tolerance involved.
 *
 * Ties go to the lowest index, which the strict `>` gives for free and D8
 * requires: two columns with the same breach must be separated by something
 * that is the same on every machine and in every run.
 *
 * `*total` receives the sum of every breach seen, which is the primal's
 * progress measure and comes out of the loop this function was already
 * running — the same bargain `price_row` strikes for the dual. */
static int64_t primal_price(sx *s, double *total)
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

        /* Under Bland's rule the entering column is the lowest-indexed
         * eligible one and not the most attractive. That promise is the only
         * thing between a degenerate solve and a cycle (D26), and Hall &
         * McKinnon (2004) is why it is here from the first version rather
         * than added when the pricing gets clever: they exhibit LPs that
         * cycle under *both* the most negative reduced cost and steepest
         * edge, so no pricing rule buys immunity. */
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

/* q crosses its own box to the other bound, and no basis changes.
 *
 * The cheapest iteration the primal has: `B`, the factorization, the duals and
 * every reduced cost are untouched, because nothing entered or left the basis.
 * Only the point moves — `x_B` by `-delta * B^-1 M_q`, read out of `s->col`
 * where `primal_ratio_test` left it, so there is no solve to pay for either.
 *
 * **It terminates, and the argument is short enough to keep.** The duals do
 * not move, so `d[q]` does not move; q was eligible because its reduced cost
 * pointed off the bound it was resting on, and at the opposite bound that same
 * sign is feasible. So q is not eligible again, and each flip strictly reduces
 * the number of dual infeasible columns. The objective falls by `d_q * delta`,
 * which is negative by the same sign argument.
 *
 * The caller has already established that no basic variable blocks sooner, so
 * the point stays primal feasible. */
static void primal_bound_flip(sx *s, int64_t q, double delta)
{
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= delta * s->col[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    s->status[q] = s->status[q] == JM_AT_LOWER ? JM_AT_UPPER : JM_AT_LOWER;
}

/* --------------------------------------------------------------------- */
/* Primal phase 1                                                        */
/* --------------------------------------------------------------------- */

/* Builds the phase-1 cost vector and reports the total infeasibility it
 * measures. `-1` on a basic below a bound the model declared, `+1` on one
 * above, `0` everywhere else — so `c1' x` is the sum of the violations, up to
 * the constant the bounds contribute, and minimising it is phase 1.
 *
 * **Only bounds the model declared count**, `real_lower`/`real_upper` and
 * never `lo`/`up`, for the same reason every other primal predicate says so:
 * a basic resting outside a bound dual phase 1 *invented* is not infeasible in
 * any sense the caller would recognise, and driving it back would be phase 1
 * repairing the solver's own convenience.
 *
 * The tolerance is `primal_tol`, the same one `primal_worst_violation` uses to
 * decide whether phase 1 is needed at all. Two different answers to "is this
 * basic infeasible" inside one method is how a phase 1 terminates without
 * satisfying the test that sent it there. */
static double primal_phase1_costs(sx *s)
{
    memset(s->c1, 0, (size_t)s->nvar * sizeof *s->c1);
    double total = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        if (isfinite(lo) && s->xb[i] < lo - s->primal_tol) {
            s->c1[v] = -1.0;
            total += lo - s->xb[i];
        } else if (isfinite(up) && s->xb[i] > up + s->primal_tol) {
            s->c1[v] = 1.0;
            total += s->xb[i] - up;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return total;
}

/* Phase-1 reduced costs into `d`, by lending `compute_duals` a different
 * objective for the length of one call.
 *
 * The swap is a pointer and the restore is unconditional, so `cost` is the
 * same array on the way out as on the way in — this is not the inexact
 * add-then-subtract D121 refused, because nothing is arithmetically applied to
 * `cost` at all.
 *
 * `refine` is not offered. The refinement step exists for numbers that are the
 * answer (D29); phase-1 duals are an input to a pivot choice and are rebuilt
 * from scratch every iteration anyway, because which basics are infeasible
 * changes underneath them. */
static void primal_phase1_duals(sx *s)
{
    double *real_cost = s->cost;
    s->cost = s->c1;
    compute_duals(s, false);
    s->cost = real_cost;
}

/* How far q may travel in phase 1 before a basic variable reaches a bound that
 * matters.
 *
 * **A feasible basic and an infeasible one block on opposite questions, and
 * that is the whole difference from the phase-2 test.** A feasible basic must
 * stay feasible, so it blocks at whichever declared bound lies the way it is
 * travelling. An infeasible one is already outside a bound: travelling back
 * towards it, the bound is where it becomes feasible and the step must stop
 * there, because past it the variable would cross to the other side and phase
 * 1 would have traded one violation for another. Travelling away from it,
 * nothing blocks — the violation grows, which the entering column was only
 * chosen if the total still falls.
 *
 * This is the short-step form. The long step Maros (1986) describes walks
 * several breakpoints at once, keeping a running slope, and can make several
 * basics feasible in one iteration
 * (`docs/research/primal-simplex.md` §4). It is a speed technique on top of a
 * correct short step and it is not here yet.
 *
 * Returns the blocking position with `*below` saying which bound it lands on,
 * or -1 when nothing blocks. `*step` receives the distance. Leaves `B^-1 M_q`
 * in `s->col`, as the phase-2 test does and for the same reason. */
static int64_t primal_phase1_ratio(sx *s, int64_t q, bool *below, double *step)
{
    var_column(s, q, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t best = -1;
    double best_step = HUGE_VAL;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];      /* per unit q travels */
        if (fabs(move) < PIVOT_MIN)
            continue;                              /* cannot be told from zero */

        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        const bool under = isfinite(lo) && s->xb[i] < lo - s->primal_tol;
        const bool over  = isfinite(up) && s->xb[i] > up + s->primal_tol;

        double limit;
        bool lands_low;
        if (under) {
            if (move < 0.0)
                continue;                          /* going further under */
            limit = lo;
            lands_low = true;
        } else if (over) {
            if (move > 0.0)
                continue;                          /* going further over */
            limit = up;
            lands_low = false;
        } else {
            limit = move < 0.0 ? lo : up;
            lands_low = move < 0.0;
            if (!isfinite(limit))
                continue;
        }

        double t = (limit - s->xb[i]) / move;
        if (t < 0.0)
            t = 0.0;                               /* already there: degenerate */
        if (t < best_step) {
            best_step = t;
            best = i;
            *below = lands_low;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    *step = best_step;
    return best;
}

/* Drives the sum of bound violations to zero, from whatever basis it is given.
 *
 * No artificial variables and no second model: the basis is the one the caller
 * supplied and phase 1 works on it in place, which is the property crossover
 * needs and the textbook artificial-variable method cannot offer
 * (`docs/research/primal-simplex.md` §4). What changes between the phases is
 * the objective the duals are taken against, and nothing else — the same
 * `pivot()`, the same pricing row, the same ratio-test shape.
 *
 * **The costs are rebuilt every iteration and that is not laziness.** Which
 * basics are infeasible is what the phase-1 objective is made of, and a pivot
 * changes it: the leaving variable lands exactly on a bound, and other basics
 * may cross one on the way. Duals carried across that change would price
 * against an objective that no longer exists.
 *
 * **It refuses rather than declaring the model infeasible.** No improving
 * direction with infeasibility left is the textbook proof of primal
 * infeasibility, and this method is not entitled to it here: `real_*` measures
 * against declared bounds while the columns may be pinned by bounds dual phase
 * 1 invented, so "nothing improves" can be a statement about the loans rather
 * than about the model. D19 refuses exactly this kind of inference, and the
 * infeasible instance set is the dual method's to answer. */
static jaos_status run_primal_phase1(sx *s, jaos_solve_status *out,
                                     bool *feasible)
{
    *feasible = false;
    if (s->c1 == nullptr) {
        s->c1 = jm_alloc_array(s->nvar, sizeof *s->c1);
        if (s->c1 == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);
    const int64_t entered = s->iters;
    double best_total = HUGE_VAL;
    int64_t last_gain = s->iters;

    for (;;) {
        /* The budgets end the solve here rather than being left to the caller,
         * and `*feasible` staying false is what says so. A phase 1 that
         * stopped on a budget has not reached a feasible point, and letting
         * phase 2 start from one would be running a method outside the
         * invariant it is built on. */
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations in the primal phase 1, the last %lld "
                             "without the total infeasibility improving; this "
                             "is a JAOS defect",
                       (long long)s->iters,
                       (long long)(s->iters - last_gain));
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            bool ok = false;
            jaos_status st = refresh(s, &ok, false);
            if (st != JAOS_OK)
                return st;
            if (!ok)
                return JAOS_ERR_NUMERICAL;
        }

        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock, as `run()` logs for the same
         * reason: output that differs between two runs of the same model is
         * the one thing D8 forbids. */
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
            last_gain = s->iters;
        }

        primal_phase1_duals(s);

        /* The entering column, on the phase-1 objective. Dantzig again, and
         * the eligibility is the plain sign test rather than `dual_breach`:
         * these reduced costs belong to an objective that exists only inside
         * this loop, so the published-space reading `breached` unions in has
         * nothing to say about them. */
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
            if (gain > best_d) {
                best_d = gain;
                q = v;
            }
        }
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);

        if (q < 0) {
            jm_set_err(s->m,
                       "the primal phase 1 cannot reduce a total bound "
                       "violation of %.6g any further; reading that as an "
                       "infeasible model needs the proof D19 requires, and "
                       "the columns may be held by bounds dual phase 1 "
                       "invented rather than by the model", total);
            return JAOS_ERR_NUMERICAL;
        }

        bool below = false;
        double step = 0.0;
        int64_t r = primal_phase1_ratio(s, q, &below, &step);

        /* q reaching its own opposite bound first is a flip here exactly as it
         * is in phase 2, and for the same reason: no basic variable can
         * express that limit (D189). */
        {
            const double other = s->d[q] < 0.0 ? real_upper(s, q)
                                               : real_lower(s, q);
            if (isfinite(other)) {
                const double delta = other - nonbasic_value(s, q);
                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);
                    s->verified = false;
                    s->iters++;
                    s->n_primal_iters++;
                    continue;
                }
            }
        }

        if (r < 0) {
            jm_set_err(s->m,
                       "column %lld reduces the primal phase 1's objective "
                       "and no declared bound stops it, which cannot happen "
                       "on an objective bounded below by zero; this is a JAOS "
                       "defect", (long long)q);
            return JAOS_ERR_NUMERICAL;
        }

        build_pricing_row(s, r);
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization; this is a "
                       "JAOS defect", (long long)q, s->alpha[q],
                       (long long)r);
            return JAOS_ERR_NUMERICAL;
        }

        s->verified = false;
        bool took = false;
        /* `pivot()` steps every reduced cost by the dual step along the
         * pricing row, and `d` here holds phase-1 reduced costs — so what it
         * maintains is the phase-1 pricing, which is what the next iteration
         * would rebuild anyway. The phase-2 costs are recomputed at the
         * hand-over and owe nothing to any of this. */
        jaos_status st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took)
            continue;

        s->iters++;
        s->n_primal_iters++;
    }
}

/* The primal simplex, phase 2 only.
 *
 * Mirrors `run()` clause for clause where the question is the same — the
 * budgets, the callback, the iteration guard, the refusal to declare anything
 * on carried numbers — and differs only where the two methods genuinely
 * differ: it prices a column and then finds the row, where the dual prices a
 * row and then finds the column.
 *
 * **It refuses to start from a primal infeasible point, and that refusal is
 * the whole of stage 1's honesty.** There is no primal phase 1 yet
 * (`TODO.md` §0 stage 4), and the two wrong things to do here are both
 * available and both worse. Reporting `INFEASIBLE` would be a wrong answer on
 * a model that has an optimum, which is the one outcome the infeasible set
 * exists to make impossible. Running anyway would drive an objective across a
 * region the point is not in and publish whatever it reached. So it returns
 * `NUMERICAL_ERROR` and says why, which is the shape `classify_optimum`
 * already uses for the answer this method cannot reach.
 *
 * Consequence, stated so nobody has to discover it from a campaign: **a cold
 * start never gets here.** `build_initial_basis` is dual feasible by
 * construction and primal feasible by accident at best, so on the reference
 * instances this refuses almost everywhere. Its reach today is a warm basis
 * that happens to be feasible, a crossover, or a test fixture. */
static jaos_status run_primal(sx *s, jaos_solve_status *out)
{
    s->dinfeas_best = HUGE_VAL;
    s->last_gain = s->iters;
    s->bland = false;
    /* The primal holds the point feasible from end to end, so the figure a
     * progress callback reads is zero and is not a placeholder. */
    s->infeas_best = 0.0;

    /* **The warm start's cost sweep must not run for the primal, and this one
     * line is the difference between a primal method and a no-op.**
     *
     * `build_warm_basis` arms `shift_pending`, and the next `refresh` then
     * pushes every breached reduced cost onto the feasible side by shifting
     * the cost behind it. The dual needs that: it requires dual feasibility to
     * start at all, and a warm basis has no such guarantee.
     *
     * For the primal it removes the work. Dual infeasibility is precisely
     * what this method consumes — `primal_price` prices on `dual_breach` —
     * so a sweep that sets every breach to zero hands the loop an optimal
     * point on arrival. **Measured before it was fixed**: over all 24 bases a
     * two-row model admits, every accepted one gave `0 primal iterations` and
     * the right answer, because the settling re-entry's dual solve did the
     * whole job. Not one of them was optimal to begin with.
     *
     * Nothing else is lost by clearing it. The sweep's stated purpose is to
     * stop the *dual* ratio test meeting a cost already past zero; the primal
     * ratio test reads `d[q]` for a direction and nothing else. The
     * per-iteration `shift_to_feasible` inside `pivot()` is a different and
     * local repair, and it still runs. */
    s->shift_pending = false;

    bool ok = false;
    jaos_status st = refresh(s, &ok, false);
    if (st != JAOS_OK)
        return st;
    if (!ok) {
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }

    /* Phase 1, when the point it was handed is not primal feasible. It works
     * on the basis in place and hands back a feasible one, or refuses. */
    if (primal_worst_violation(s) > s->primal_tol) {
        bool feasible = false;
        st = run_primal_phase1(s, out, &feasible);
        if (st != JAOS_OK)
            return st;
        if (!feasible)
            return JAOS_OK;   /* a budget ended it; `*out` says which */

        /* Phase 1 leaves `d` holding *its* reduced costs, against an
         * objective that no longer exists. Everything below prices on the
         * model's own, so they are rebuilt here and not inherited. The
         * refactorization is fresh as well, because the point phase 1 reached
         * is the one phase 2's first optimality test will be read off, and
         * D20 refuses that on carried numbers. */
        s->needs_refactor = true;
        bool ok2 = false;
        st = refresh(s, &ok2, false);
        if (st != JAOS_OK)
            return st;
        if (!ok2) {
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        /* And it must actually have worked. A phase 1 that returns `JAOS_OK`
         * on a point still outside a bound would hand phase 2 a start it has
         * no invariant for, and phase 2 would drive an objective across a
         * region the point is not in — which is the whole failure this pair of
         * checks exists to make impossible. Checked against the same
         * tolerance that sent it there. */
        const double left = primal_worst_violation(s);
        if (left > s->primal_tol) {
            jm_set_err(s->m,
                       "the primal phase 1 returned with a declared bound "
                       "still violated by %.6g; this is a JAOS defect", left);
            return JAOS_ERR_NUMERICAL;
        }

        /* Phase 2 starts its own stall accounting. Phase 1 drove a different
         * objective, so the iteration it last improved on says nothing about
         * whether phase 2 is making progress, and inheriting it would arm
         * Bland's rule against a plateau that belongs to another problem. */
        s->last_gain = s->iters;
        s->bland = false;
        s->dinfeas_best = HUGE_VAL;
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
                             "primal iterations, the last %lld without the "
                             "total dual infeasibility improving%s, %lld "
                             "pivots declined on factorization disagreement; "
                             "this is a JAOS defect",
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

        double total = 0.0;
        int64_t q = primal_price(s, &total);

        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "iter %lld: best dual infeasibility %.6g, work %lld",
                   (long long)s->iters, s->dinfeas_best,
                   (long long)s->work.units);

        /* Improving turns Bland's rule back off, exactly as it does for the
         * dual: its cost is real (D26) and it is worth paying only while the
         * alternative is not working. */
        if (total < s->dinfeas_best) {
            s->dinfeas_best = total;
            s->last_gain = s->iters;
            s->bland = false;
        }

        if (q < 0) {
            /* Nothing wants to move, which is optimality for the costs in
             * force — but read off carried numbers. D20's refusal applies
             * here for the same reason it applies to the dual: `d` is
             * stepped in place every iteration and the factorization is
             * patched rather than rebuilt, so the test that would notice the
             * drift is the one being run. Recompute from a fresh
             * factorization and price again. */
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
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        bool below = false;
        double step = 0.0;
        int64_t r = primal_ratio_test(s, q, &below, &step);

        /* **Does q reach its own opposite bound first?** No basic variable
         * can express that limit, so a ratio test that only scans rows walks
         * straight past it — and this is not a theoretical worry. Before this
         * block existed the method published `x = 10` on a column bounded at
         * 1, as OPTIMAL, at an objective of -10 against a true -3.75. Only the
         * independent checker refused it
         * (`bench/measurements/02-102/`). Stage 1's pricing rule is what made
         * the case reachable, exactly as `TODO.md` §0 said it would.
         *
         * **Read from `real_upper`/`real_lower` and never from `up`/`lo`.**
         * Those strip the bounds dual phase 1 invented; the raw arrays do not.
         * Flipping onto an invented bound would park a variable on a value the
         * model never declared, which is the case `repair_dual_infeasibility`
         * refuses in as many words and the evidence `classify_optimum` reads
         * straight afterwards. A column whose other side is only an invented
         * bound therefore has no flip available, and if nothing blocks it
         * either it falls through to the refusal below — which is the honest
         * answer, not a ray.
         *
         * The direction is read off the reduced cost, the same rule
         * `primal_ratio_test` states at length: at a lower bound `d` is
         * negative and q travels up, at an upper bound positive and it travels
         * down, and a free column has no other bound at all. */
        {
            const double other = s->d[q] < 0.0 ? real_upper(s, q)
                                               : real_lower(s, q);
            if (isfinite(other)) {
                const double delta = other - nonbasic_value(s, q);
                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);
                    /* The point moved, so any verification of it is spent.
                     * The basis did not, so nothing needs refactorizing. */
                    s->verified = false;
                    s->iters++;
                    s->n_primal_iters++;
                    continue;
                }
            }
        }

        if (r < 0) {
            /* Nothing the model declared stops this column. That reads as an
             * unbounded ray and it is **not declared as one here**, for the
             * reason `primal_cleanup` gives and D19 requires: the honest
             * reading needs proof, and the column may be leaving a bound dual
             * phase 1 invented, in which case the ray is the solver's and not
             * the model's. Declaring `UNBOUNDED` on it is precisely the wrong
             * answer D19 was written to remove. That verdict is §0 stage 7
             * and it arrives with the phase 1 that makes the question
             * well-posed. */
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
            jm_set_err(s->m,
                       "column %lld improves and no declared bound stops it; "
                       "reading that as an unbounded ray needs the proof D19 "
                       "requires and the primal phase 1 that makes it "
                       "well-posed, and JAOS has neither yet",
                       (long long)q);
            return JAOS_ERR_NUMERICAL;
        }

        build_pricing_row(s, r);
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            /* The pricing row disagrees with the column about the pivot the
             * ratio test just chose. `pivot()` would divide by it. Rebuild
             * and price again rather than step on a number neither solve
             * supports — the same refusal `pivot()` makes for itself when the
             * two disagree by more than a tolerance. */
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld on a freshly "
                       "built factorization, which no pivot can use; this is "
                       "a JAOS defect", (long long)q, s->alpha[q],
                       (long long)r);
            return JAOS_ERR_NUMERICAL;
        }

        s->verified = false;
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
 * times, and an allocation per solve would be pure churn.
 *
 * Not static: jm_postsolve_expand and jm_postsolve_solved (src/presolve.c)
 * call it on the caller's own model exactly as publish() calls it here on
 * whichever model this solve actually ran on — a presolve-reduced solve has
 * two models to ensure buffers on, not one. */
jaos_status jm_model_ensure_solution_arrays(jaos_model *m)
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

/* `p` is the presolve workspace for this solve, always non-null: NONE when
 * nothing was reduced, in which case m == s->m already IS the caller's
 * model and every loop below writes straight into it exactly as before
 * presolve existed. `(void)p` covers the JAOS_NO_PRESOLVE build, where the
 * postsolve-expand call below is compiled out and p would otherwise go
 * unread (D-03). */
static jaos_status publish(sx *s, jaos_solve_status status, jm_presolve *p)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    (void)p;

    m->solve_status = status;
    m->solve_iters = s->iters;
    /* The work snapshot is taken at the *end* of each path below, not
     * here: publishing itself runs a kernel (the BTRAN for the duals),
     * and a counter that reported everything except the last thing it
     * did would be lying by one solve (D16). */

    jaos_status st = jm_model_ensure_solution_arrays(m);
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
    /* No loan may still be outstanding here, because the two blocks below
     * read the borrowed arrays directly: `sol_dual` is a BTRAN of `s->cost`
     * and `sol_redcost` is `s->d`. The objective is safe whatever this says —
     * it is built from `m->col_cost` — so this assert is about the duals and
     * the reduced costs alone, and only on this branch: a solve that ends
     * anywhere but OPTIMAL never calls `settle_shifts` and is entitled to
     * carry loans, and it publishes four arrays of zeros instead.
     *
     * The suspicion it answers: `refresh` re-runs `shift_to_feasible` over
     * every variable when `repair_singular_basis` fired, and both
     * `take_best_if_better` and `restore_settled` call it AFTER their own
     * `repay_shifts`. On that path `reenter_after_settling` returns with
     * loans back in the costs and nothing settles them again.
     *
     * The cost is compared as well as the record, for D122's reason: a
     * column whose cost moved while its record cancelled back to zero is the
     * case D121 measured 67 of on `pilotnov`, and the record alone would not
     * see it. Same pair as `settled_objective` asserts. `repay_shifts`
     * carries the number and where D122 took the wrong one from.
     *
     * Found by `numerics-reviewer` while reviewing D122, and kept out of that
     * change so one change did one thing. */
    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]);
#endif

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

    /* From the values just written and with a compensated sum (D169).
     * `sol_col` above is what this reads, so the objective and the point
     * cannot disagree. */
    jm_model_publish_objective(m);
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

#if !defined(JAOS_NO_PRESOLVE)
    /* m is the reduced model here whenever p->outcome is REDUCED (s->m was
     * set to &p->reduced in jm_dual_simplex); everything above just wrote a
     * complete, correct answer in REDUCED indices onto it. This is where
     * that answer crosses back into the caller's own row and column space
     * (D-08) — the same kind of boundary the scaled-to-original unit
     * conversion above already crosses, just for indices instead of units. */
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

    /* D-14: presolve's own charge, on the accumulator sx_init's own s.work
     * continues below rather than a second, separate one -- one solve, one
     * total, the same way jaos_work_units already reads a single figure
     * regardless of how many kernels contributed to it. Declared here,
     * outside the switch below, so it is always {0} rather than
     * conditionally uninitialized: under JAOS_NO_PRESOLVE it simply never
     * moves, and seeding s.work with a zero is the same as not seeding it. */
    jm_work pre_work = {0};

#if !defined(JAOS_NO_PRESOLVE)
    /* A development switch, not an option (D64): which reductions fire is
     * the method, and the method is not the caller's to choose. Sweeping a
     * constant that must not change a verdict is how three defects were
     * found that 139 instances at one setting did not (D39, D47, D72); this
     * switch is what makes the equivalent sweep possible for presolve
     * itself — `make EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`. With it defined,
     * src/presolve.c compiles to nothing that runs and the solver takes
     * exactly the path it took before this existed (D-03, D-09). */
    jaos_status pst = jm_presolve_run(m, &p, &pre_work);
    if (pst != JAOS_OK) {
        jm_presolve_free(&p);
        return pst;
    }

    /* Presolve ran first, so it reports first — beside the "dual simplex:"
     * summary line below and before it, and printed even on the SOLVED
     * path, which never reaches that line at all. The reduced dimensions
     * and non-zero counters only (D-13): a model nothing fired on says so
     * in a few words rather than fifteen zeros. INFEASIBLE/UNBOUNDED never
     * reach a compacted `reduced`, so they get their own, dimension-free
     * line rather than print zeros that would read as "reduced to
     * nothing" when the real story is "refused outright". */
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
        /* Every column presolve fixed, emptied or eliminated: nothing is
         * left for the simplex to run on. No sx is built and run() never
         * executes. */
        jaos_status st = jm_postsolve_solved(&p);
        jm_presolve_free(&p);
        return st;
    }

    if (p.outcome == JM_PRESOLVE_INFEASIBLE ||
        p.outcome == JM_PRESOLVE_UNBOUNDED) {
        /* Proved by the reductions alone (an empty row or a singleton
         * row's collapsed bound for infeasibility; an empty column's
         * favourable-bound-at-infinity for unboundedness, D19's one
         * exception) — no basis is ever built either. This is the path
         * D-12 asks this plan to confirm bench/run.c's double-solve
         * determinism check already reaches. */
        const jaos_solve_status status = (p.outcome == JM_PRESOLVE_INFEASIBLE)
            ? JAOS_SOLVE_INFEASIBLE : JAOS_SOLVE_UNBOUNDED;
        jaos_status st = jm_postsolve_infeasible_or_unbounded(&p, status);
        jm_presolve_free(&p);
        return st;
    }

    /* The scaled working copy is built from the reduced model when presolve
     * reduced one, and from the caller's own model otherwise — this is the
     * only place that distinction is made; everything below reads `target`
     * and does not know or care which case it is in. */
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
    /* sx_init's own memset just zeroed s.work; seed it with what presolve
     * already charged (D-14) so the two are one accumulator and not two
     * that happen to get added together later -- publish()'s own
     * m->solve_work = s->work.units is then presolve's units plus the
     * solve's, in the same total a work limit is compared against. */
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
         *
         * `force_primal` is a development switch and not an option, the same
         * kind of thing `JAOS_NO_PRESOLVE` is (D64): which algorithm solves a
         * model is the method, and the method is not the caller's to choose.
         * It exists so `bench/primal.c` can measure a primal path the gate
         * structurally cannot reach — a cold start is dual feasible and not
         * primal feasible, so left to itself the solver always picks the dual
         * and a primal simplex would pass every campaign here while doing
         * nothing (`TODO.md` §0).
         *
         * Everything after this line is shared. The settling, the re-entry
         * and the verdict are about the residue a solve leaves rather than
         * about how it got there. **One consequence is worth stating rather
         * than discovering: `reenter_after_settling` calls `run()`, so a
         * forced-primal solve can still finish with dual iterations.** That
         * is stage 1's shape and not an oversight; making the re-entry follow
         * the method is a later question, and the harness compares final
         * answers, which is what a caller receives either way. */
        /* Set and cleared here rather than inside `run_primal`, which returns
         * from a dozen places and would leak the flag out of any of them.
         * Everything after this line — the settling, the re-entry, the verdict
         * — is the dual's world and must behave exactly as it always has. */
        s.in_primal = m->cfg.force_primal;
        st = m->cfg.force_primal ? run_primal(&s, &outcome)
                                 : run(&s, &outcome);
        s.in_primal = false;
        if (st != JAOS_OK || outcome != JAOS_SOLVE_OPTIMAL)
            break;

        /* Settle first, then judge. The verdict turns on reduced costs, so
         * it has to read the model's own and not the shifted ones the
         * ratio test worked with. */
        settle_shifts(&s);
        st = reenter_after_settling(&s);
        if (st != JAOS_OK)
            break;

        /* The settling loop measured this number on every round and D89
         * taught it to publish the best point instead of failing when the
         * rounds oscillate. What it never did was read that number before
         * publishing, and on a hostile caller basis the best point carried
         * a violation of 35.34 into an OPTIMAL verdict — a wrong objective
         * with no signal anywhere (D146, D147). The read is exact-zero on
         * purpose: settled_dual_violation counts only the excess beyond
         * dual_tol per variable, so zero means every variable is within
         * tolerance, and every one of the 220 settling exits across the
         * three gate sets reads exactly that (02-56) — the guard cannot
         * fire on a legitimate solve, which is what keeps the gate
         * bit-identical under it. The call is unbilled like the settling
         * loop's own uses of the same function, deliberately: billing it
         * would move every gate solve's work units for a read that changes
         * nothing there.
         *
         * An uncertified point from a WARM start is thrown away whole and
         * the solve restarts once, cold, from the slack basis — jaos.h's
         * own contract, time and never correctness. The work stays on the
         * one accumulator (the wasted warm attempt was real work, D16);
         * the clock keeps its origin so the time budget covers both
         * attempts; the iteration count restarts with the sx and reports
         * the solve that produced the answer. An uncertified COLD start
         * has no better start to fall back to and is published as
         * NUMERICAL_ERROR — the one outcome no warm memory is offered
         * for, on the un-reduced path by publish()'s own whitelist and on
         * the reduced one by jm_postsolve_expand's matching test.
         *
         * The settle below is the guard's own contract, found in review:
         * a restore exit whose refresh fired repair_singular_basis has
         * re-run shift_to_feasible, and the guard would then read the
         * evidence the lend arranged — d[v] = 0.0 on exactly the breached
         * columns — and certify the D146 defect through a rarer door. On
         * an already-settled state repay_shifts finds nothing, returns
         * false and settle_shifts is a pure scan: no state moves, no work
         * is billed, and the gate stays bit-identical. */
        settle_shifts(&s);
        if (settled_dual_violation(&s) != 0.0) {
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
                allow_warm = false;
                continue;
            }
            outcome = JAOS_SOLVE_NUMERICAL_ERROR;
            break;
        }
        outcome = classify_optimum(&s);
        break;
    }

    /* The simplex writes its refusals with `jm_set_err(s->m, ...)`, and
     * `s->m` is the model the simplex RAN on — which is `p.reduced` whenever
     * presolve reduced one. The caller holds `m`. So every message the solve
     * produced reached a model the caller cannot see, and what a caller
     * actually received was `JAOS_ERR_NUMERICAL` and an empty string from
     * `jaos_model_error`.
     *
     * **This was not introduced by the primal method; it was found by it.**
     * The messages it loses are the ones a caller most needs: the iteration
     * guard's "this is a JAOS defect" in `run()`, `classify_optimum`'s
     * refusal to answer past a bound phase 1 lent, and now the primal's
     * refusal to start without a phase 1. Measured on the standard set:
     * exactly **8 of 94** instances carried the message through, and those
     * eight are precisely the ones whose `presolve=` columns are unchanged —
     * `degen2`, `degen3`, `fit1d`, `fit2d`, `scsd1`, `scsd6`, `scsd8`,
     * `truss`. Every model presolve touched lost it.
     *
     * Copied rather than redirected, because `s->m` is genuinely the right
     * model to blame *inside* the solve — the row and column indices in these
     * messages are the reduced model's. Only on the way out does the caller's
     * copy need to exist at all, and only when the solve failed: a successful
     * one leaves nothing to report and must not disturb whatever `m->err`
     * already holds. */
    if (st != JAOS_OK && target != m && target->err[0] != '\0')
        memcpy(m->err, target->err, sizeof m->err);

    if (st == JAOS_OK)
        st = publish(&s, outcome, &p);

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
               "%lld stability rebuilds, %lld primal iterations",
               jaos_solve_status_str(outcome), (long long)s.iters,
               (long long)s.work.units, (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability, (long long)s.n_primal_iters);
    else
        jm_log(m, JAOS_LOG_SUMMARY,
               "abandoned after %lld iterations, %lld work units: %s; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds, %lld primal iterations",
               (long long)s.iters, (long long)s.work.units,
               jaos_status_str(st), (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability, (long long)s.n_primal_iters);

    sx_free(&s);
    jm_presolve_free(&p);
    return st;
}
