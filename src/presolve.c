/* Presolve and postsolve (D-01).
 *
 * A reduced problem is built alongside the caller's model, never mutating
 * it (D-06) — the same move sx_init already makes one layer up when it
 * builds a scaled working copy, applied here before scaling exists (D-04).
 * Every reduction that fires pushes a tagged record onto an append-only
 * arena; postsolve replays the arena strictly LIFO (D-07) to recover the
 * caller's sol_col, sol_row, sol_dual, sol_redcost, sol_col_status and
 * sol_row_status in the model's own, original indices. The reduced index
 * space is an internal detail of this file and of sx; it never escapes
 * either (D-11).
 *
 * 02-01 shipped one reduction (a column whose bounds already arrived
 * equal) and the whole arena/postsolve machinery, proved end to end. This
 * plan (02-03) adds the four structural families a fixed-point loop
 * cascades through for the first time: empty rows, empty columns,
 * singleton rows and singleton columns (the free-column-singleton case
 * named separately, since it removes a row and a column for one record).
 * Three of the five can publish a verdict with no simplex run at all
 * (JM_PRESOLVE_INFEASIBLE, JM_PRESOLVE_UNBOUNDED, JM_PRESOLVE_SOLVED) — the
 * path D-12 asks this plan to confirm bench/run.c's double-solve
 * determinism check already reaches.
 *
 * Scope this plan deliberately narrows, and why:
 *
 *   - Singleton row's bound-fold is pure arithmetic — no choice, no cost
 *     involved — so it is unrestricted: any coefficient, any cost.
 *   - Singleton column (both the bounded case and the free-column-singleton
 *     case) fires only when the column's own cost is exactly zero. A
 *     nonzero-cost singleton column's elimination needs to know which of
 *     its bounds is optimal, and that is a choice the row's *dual* decides
 *     — information a pure presolve pass does not have until the reduced
 *     problem is solved. (A worked counterexample lives in 02-03-SUMMARY.md
 *     under "Deviations": pushing a nonzero-cost singleton column to its
 *     own favourable bound, ignoring the row, can manufacture infeasibility
 *     in a problem that was feasible without presolve.) Cost zero sidesteps
 *     the choice entirely — any feasible value is equally optimal, so the
 *     column becomes a genuine slack and presolve only has to find *a*
 *     feasible one, not *the* optimal one.
 *   - The free-column-singleton case is further restricted to a *mutual*
 *     singleton: the row it lives in must, at the moment it fires, have no
 *     other live entry either. This is what lets postsolve recover the
 *     column's value from the row's own (already correctly shifted)
 *     current bounds alone, with no dependency on any other column's
 *     final value and therefore no arena-replay-ordering hazard.
 *   - Once a row has had a bounded singleton column relaxed out of it, it
 *     is frozen against every other row-removing family for the rest of
 *     this presolve run (row_frozen[]). A relaxed row's bounds represent a
 *     range the removed column *might* still need, not a determined value,
 *     and removing that row outright before the range is resolved would
 *     make its later recovery ill-posed.
 *
 * None of this is a claim that the excluded cases are unsound in general —
 * only that this plan does not attempt them, because attempting them
 * without a review-worthy derivation is exactly the failure shape the
 * phase boundary names as the real risk (postsolve, not the reductions).
 *
 * 02-04 adds the activity-range machinery: one routine (ps_row_range)
 * computing what a row can reach given the current column boxes, read four
 * ways — the model is infeasible, the row is forced to an extreme, the row
 * can never bind, or the row bounds a variable. It is the same algorithm
 * src/check.c's implied_bounds already runs as a read-only diagnostic, with
 * two differences that both come from this one changing the answer rather
 * than reporting on it.
 *
 *   - The sum is accumulated with Neumaier compensation in `double`, not in
 *     `long double`. See ps_acc below: `long double` is architecture-
 *     dependent and D34 lists avoiding it among the construction rules the
 *     cross-machine determinism claim rests on. The checker may use it
 *     because its arithmetic never reaches the answer; presolve's does.
 *
 *   - **A tightened bound is published, and it has to be.** 02-04 first
 *     shipped the opposite: two column boxes, one presolve reasoned over and
 *     a looser one the reduced model handed the simplex, on the argument
 *     that an implied bound adds nothing to a feasible region its own row
 *     still constrains. The standard set refused it. Removing a row because
 *     its activity range lies inside its bounds is a statement about the
 *     range, and the range is computed over the TIGHTENED box; hand the
 *     simplex a looser box with that row gone and the reduced problem is a
 *     relaxation, whose optimum can sit at a point the original forbids.
 *     Postsolve then publishes it and the checker reports a primal
 *     violation, which is what `capri` did at col=58.3, row=42.4. Forcing
 *     fails the other way: a row that pins its columns over the tightened
 *     box does not pin them over the looser one, so the reduction
 *     over-restricts and a feasible model comes back INFEASIBLE — `pilot`
 *     and `pilot87` both did, at every epsilon from 1e-12 to 1e-6.
 *
 *     So the plan's own design was right and the refinement was wrong: a
 *     reduction justified by an implied bound is only sound if that bound
 *     is imposed. What that leaves is T-02-11, which is the risk the plan
 *     names and mitigates — the bound is rounded AWAY from tightening,
 *     never toward it, and PRESOLVE_TIGHTEN_EPS's sweep records what
 *     happens on both sides of every setting.
 *
 *   - Forcing rows fire only when every column they pin lands on a bound
 *     the ORIGINAL column had, and only when none of those columns still
 *     has an entry in an already-removed row that carries a multiplier.
 *     Both restrictions exist so the row's own dual (ps_replay_one,
 *     JM_PS_FORCING_ROW) is a derivation with nothing stale in it.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A published zero is a zero (D21) — the same rule and the same reason as
 * simplex.c's own `published`: a byte-for-byte comparison of two published
 * solutions is this project's cheapest and strongest evidence that a change
 * altered nothing, and an instrument that reports -0.0 where an equivalent
 * run reports 0.0 is a worse instrument. Kept as presolve's own copy rather
 * than shared, so this file gives simplex.c no reason to be included. */
static double ps_published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

/* The instrument-validation hook (D-10, jaos-testing's "validate the
 * instrument before believing it"): under this build-time guard, every
 * postsolve record's restore index reads one past where it actually
 * belongs. Compiled to the identity in every build that is not this one.
 * Reached from every postsolve replay site, since a record's index means
 * the same thing to all of them — see tests/test_presolve.c for where this
 * is caught and by which report field, one fault per family.
 *
 * `dim` (the caller's own row or column count, whichever `index` is an
 * index into) is what the offset wraps against — every reducing solve in
 * the whole binary takes this path under this build, including every
 * pre-existing model in this tree small enough that a plain +1 would run
 * past the end of the array. Wrapping keeps the fault an index confusion
 * (still wrong, still detectable where a family's test means to detect
 * it) rather than an out-of-bounds write, on any model shaped this
 * plan's own tests were never sized around. */
static int64_t ps_restore_index(int64_t index, int64_t dim)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    return (index + 1) % dim;
#else
    (void)dim;
    return index;
#endif
}

/* --------------------------------------------------------------------- */
/* Presolve's own tolerance space, and the two constants in it           */
/* --------------------------------------------------------------------- */

/* Presolve runs on the model as loaded, before scaling exists (D-04). That
 * is a THIRD tolerance space, and nothing converts into it: the solver's
 * PRIMAL_TOL/DUAL_TOL are magnitudes in scaled space and sx does not exist
 * yet when this file runs, and the checker's tol is a caller's diagnostic
 * choice for judging a finished answer. Neither was ever measured for
 * deciding whether to fold a bound, so neither is borrowed here — see
 * docs/tolerances.md's presolve section.
 *
 * Both constants below are reachable through the Makefile's EXTRA_CFLAGS
 * hook, the same way PRICE_PARTITIONS_VALUE is, so a sweep varies them
 * without editing this file between runs.
 *
 * PRESOLVE_TIGHTEN_EPS decides two things and only two: whether a
 * singleton row's fold has genuinely emptied a column's interval or merely
 * closed it to within rounding, and whether an emptied row's bounds still
 * admit zero after every column removed from it shifted them. Both are
 * questions about a residue left by a running difference of terms, so the
 * window is this constant times the traffic that produced the residue,
 * never this constant on its own.
 *
 * Swept over the standard set, `make clean` between every setting, with a
 * canary built to flip inside the grid and confirmed to flip:
 *
 *   eps        1e-12 1e-11 1e-10 1e-9  1e-8  1e-7  1e-6  1e-5  1e-4
 *   solved        94    94    94    94    94    94    94    94    94
 *   objective ok  94    94    94    94    94    94    94    94    94
 *   checker ok    79    79    79    79    79    79    79    79    79
 *   rows removed 7596  7596  7596  7596  7596  7596  7596  7596  7596
 *   cols removed 24693 24693 24693 24693 24693 24693 24693 24693 24693
 *   canary       INF   INF   INF   INF   OPT   OPT   OPT   OPT   OPT
 *
 * Nothing moves, over nine decades, and the canary row is what makes that
 * a reading rather than a broken instrument: a model whose singleton fold
 * conflicts with its column's own bound by 1e-8 is refused below 2e-9 and
 * solved above it, so the constant demonstrably reaches the binary at every
 * setting in the table. The cost is flat too, 96.7 s to 106.2 s at J=12
 * against a set that takes about 99 s.
 *
 * So the standard set contains no instance whose fold or whose emptied row
 * lands anywhere near any of these windows, and the plateau is at least
 * nine decades wide with neither edge found. 1e-9 is taken because it is
 * interior to the grid the plan asked for (1e-12 through 1e-6) and because
 * it is the setting the canary flips at, so a future edit that stops the
 * constant reaching the binary changes the canary rather than nothing. */
#ifndef JAOS_PRESOLVE_TIGHTEN_EPS_VALUE
#define JAOS_PRESOLVE_TIGHTEN_EPS_VALUE 1e-9
#endif
constexpr double PRESOLVE_TIGHTEN_EPS = JAOS_PRESOLVE_TIGHTEN_EPS_VALUE;

/* The cap on presolve's fixed-point rounds (D-02), following the precedent
 * IMPLIED_ROUNDS set in src/check.c:264: a safety stop rather than a quality
 * knob, since the loop already exits as soon as a round changes nothing. It
 * lands on top of 02-03's structural backstop (num_row + num_col + 1),
 * never above it.
 *
 * Swept over the standard set, `make clean` between every setting, counting
 * what the reductions actually removed — the canary that had to move, and
 * did, on a chain of 200 singleton rows built to resolve exactly one link
 * per round:
 *
 *   rounds         1     2     4     8    16    32    64   128
 *   canary links   1     2     4     8    16    32    64   128
 *   rows removed  6060  7178  7549  7596  7598  7598  7598  7598
 *   cols removed  22671 24300 24629 24693 24695 24695 24695 24695
 *   checker ok      82    79    79    79    79    79    79    79
 *
 * 16 is where the propagation stops changing. The cost is flat across the
 * whole sweep, 97.2 s to 103.6 s at J=12 against a set that takes about
 * 99 s, so there is nothing to trade against and the cap is set where the
 * fixed point is rather than where the budget runs out.
 *
 * The checker column is worth reading and worth not acting on. One round
 * certifies three answers more than sixteen do, because a reduction that
 * does not fire cannot get its dual wrong — that is a statement about the
 * postsolve dual recovery this phase already owes an entry for, not an
 * argument for a smaller cap. Capping the propagation to hide a postsolve
 * defect would leave the defect and lose the reductions. */
#ifndef JAOS_PRESOLVE_ROUNDS_VALUE
#define JAOS_PRESOLVE_ROUNDS_VALUE 16
#endif
constexpr int64_t JM_PRESOLVE_ROUNDS = JAOS_PRESOLVE_ROUNDS_VALUE;

/* --------------------------------------------------------------------- */
/* A compensated accumulator for the row activity range                  */
/* --------------------------------------------------------------------- */

/* An activity range is a sum of many terms of differing magnitude, so the
 * cancellation is real and a naive `double` accumulation loses digits
 * exactly where the range decides something.
 *
 * src/check.c accumulates its own activities in `long double` for that
 * reason and is right to: it is a diagnostic, and its arithmetic never
 * reaches the answer. **Presolve's does.** The range computed here decides
 * which rows are removed and which bounds are folded, so it is part of the
 * reproducible path, and `long double` is 80-bit on x86-64 and 128-bit or
 * plain `double` elsewhere — the same source would build a different reduced
 * model on a different architecture. D34 lists "no `long double`" among the
 * four construction rules the cross-machine determinism claim rests on,
 * alongside no FMA contraction and no address-ordered iteration, and
 * `long double` appears nowhere in this tree outside src/check.c.
 *
 * Neumaier compensation buys the accuracy without the type: it is portable,
 * deterministic, costs about a factor of two, and removes the dominant error
 * term of a long accumulation. `-ffp-contract=off` in the Makefile is what
 * makes the two-term error recovery exact rather than approximate. */
typedef struct {
    double sum, comp;
} ps_acc;

static void ps_acc_add(ps_acc *a, double t)
{
    const double s = a->sum + t;
    a->comp += (fabs(a->sum) >= fabs(t)) ? ((a->sum - s) + t)
                                         : ((t - s) + a->sum);
    a->sum = s;
}

static double ps_acc_value(const ps_acc *a)
{
    return a->sum + a->comp;
}

/* --------------------------------------------------------------------- */
/* A local, presolve-owned row-wise mirror of m's CSC matrix.            */
/* --------------------------------------------------------------------- */

/* m stays const throughout this file (D-06); jm_model_ensure_rowwise wants
 * a non-const model to cache onto, so presolve builds its own scratch
 * mirror instead of calling it — the same "read-only through a const
 * pointer" invariant jaos_internal.h documents on jm_presolve_run's
 * signature, kept at the type level rather than only in a comment. Freed
 * before jm_presolve_run returns; nothing outside the round loop below
 * needs it. */
typedef struct {
    int64_t *rs;    /* [num_row + 1] */
    int64_t *ridx;  /* [num_nz], column index per row-entry */
    double  *rval;  /* [num_nz] */
} ps_rowwise;

static bool ps_build_rowwise(const jaos_model *m, ps_rowwise *rw)
{
    rw->rs = jm_calloc_array(m->num_row + 1, sizeof *rw->rs);
    rw->ridx = jm_alloc_array(m->num_nz, sizeof *rw->ridx);
    rw->rval = jm_alloc_array(m->num_nz, sizeof *rw->rval);
    int64_t *cursor = jm_alloc_array(m->num_row, sizeof *cursor);
    if (!rw->rs || !rw->ridx || !rw->rval || !cursor) {
        free(rw->rs); free(rw->ridx); free(rw->rval); free(cursor);
        rw->rs = nullptr; rw->ridx = nullptr; rw->rval = nullptr;
        return false;
    }
    for (int64_t k = 0; k < m->num_nz; k++)
        rw->rs[m->a_index[k] + 1]++;
    for (int64_t i = 0; i < m->num_row; i++)
        rw->rs[i + 1] += rw->rs[i];
    memcpy(cursor, rw->rs, (size_t)m->num_row * sizeof *cursor);
    for (int64_t j = 0; j < m->num_col; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const int64_t dst = cursor[i]++;
            rw->ridx[dst] = j;
            rw->rval[dst] = m->a_value[k];
        }
    free(cursor);
    return true;
}

static void ps_free_rowwise(ps_rowwise *rw)
{
    free(rw->rs); free(rw->ridx); free(rw->rval);
}

/* --------------------------------------------------------------------- */
/* The row activity range — one computation, read three ways             */
/* --------------------------------------------------------------------- */

/* What a row can reach given the current column boxes. `lo_sum`/`hi_sum`
 * are the FINITE part of each end and `lo_inf`/`hi_inf` count the terms that
 * made that end infinite, kept apart rather than folded together for the
 * reason src/check.c's implied_bounds keeps them apart: one unbounded column
 * must not poison the range for the others, because the row can still bound
 * a column when that column is the only unbounded term in it.
 *
 * `traffic` is the sum of the magnitudes of the terms that formed the two
 * ends. It is the scale a residue of this row has to be judged against —
 * D23's argument, and fp-numerics': a computed sum carries an uncertainty on
 * the order of eps times the traffic through it, so an absolute test on a
 * row whose terms total 4e10 is asking for seventeen correct decimal digits
 * of a quantity that has fifteen. Every comparison this file makes against a
 * row activity is therefore PRESOLVE_TIGHTEN_EPS times max(1, traffic), and
 * never PRESOLVE_TIGHTEN_EPS on its own. */
typedef struct {
    double lo_sum, hi_sum;
    int64_t lo_inf, hi_inf;
    double traffic;
} ps_range;

static ps_range ps_row_range(const ps_rowwise *rw, int64_t i,
                             const double *cl, const double *cu,
                             const bool *col_dead)
{
    ps_acc lo = {0.0, 0.0}, hi = {0.0, 0.0}, tr = {0.0, 0.0};
    ps_range r = {0.0, 0.0, 0, 0, 0.0};

    for (int64_t k = rw->rs[i]; k < rw->rs[i + 1]; k++) {
        const int64_t j = rw->ridx[k];
        if (col_dead[j])
            continue;
        const double a = rw->rval[k];
        if (a == 0.0)
            continue;   /* the term is exactly zero, and 0 * inf is a NaN */

        const double t_lo = a > 0.0 ? cl[j] : cu[j];
        const double t_hi = a > 0.0 ? cu[j] : cl[j];
        if (isfinite(t_lo)) {
            ps_acc_add(&lo, a * t_lo);
            ps_acc_add(&tr, fabs(a * t_lo));
        } else {
            r.lo_inf++;
        }
        if (isfinite(t_hi)) {
            ps_acc_add(&hi, a * t_hi);
            ps_acc_add(&tr, fabs(a * t_hi));
        } else {
            r.hi_inf++;
        }
    }

    r.lo_sum = ps_acc_value(&lo);
    r.hi_sum = ps_acc_value(&hi);
    r.traffic = ps_acc_value(&tr);
    return r;
}

/* The two ends as numbers, infinite where a term made them so. */
static double ps_min_act(const ps_range *r)
{
    return r->lo_inf > 0 ? -HUGE_VAL : r->lo_sum;
}

static double ps_max_act(const ps_range *r)
{
    return r->hi_inf > 0 ? HUGE_VAL : r->hi_sum;
}

/* The window every comparison against this row's activity uses, and it is
 * NOT PRESOLVE_TIGHTEN_EPS.
 *
 * The three readings that compare an activity against a row bound —
 * infeasible, forcing, redundant — each ask whether two numbers are EQUAL.
 * There is no judgement in that and nothing to tune: the only thing that
 * can make them differ when they should not is the rounding in the sum that
 * produced one of them, which is DBL_EPSILON times the traffic through it.
 * A handful of those covers the compensated accumulation and the comparison.
 *
 * Using a tunable window here was 02-04's third and worst mistake. At
 * PRESOLVE_TIGHTEN_EPS = 1e-9 on a row whose terms total 1e6, it declared
 * every row whose minimum activity came within 1e-3 of its upper bound to
 * be forcing, and a forcing row pins EVERY column in it. On `pilot` that
 * pinned column 3554 at 1.15 while row 1095 — an equality row on that same
 * column — needed 0, and the model came back INFEASIBLE. Found by tracing
 * the fixes that shifted that row's bounds, not by reading the code: the
 * first two guesses (the epsilon, then the outward rounding) were both
 * wrong and both cost a campaign.
 *
 * The tunable constant governs one thing only: whether a bound tightening
 * is worth taking. That is a judgement, because a small improvement really
 * may be noise. Whether a row is forced is not. */
static double ps_row_tol(const ps_range *r)
{
    return 8.0 * DBL_EPSILON * (r->traffic > 1.0 ? r->traffic : 1.0);
}

/* The window a comparison between two BOUNDS uses. Infinities are skipped
 * rather than propagated: a scale of infinity would make every subsequent
 * comparison pass, which is the failure mode where an infeasible model is
 * quietly accepted. */
static double ps_bound_scale(double a, double b)
{
    double s = 1.0;
    if (isfinite(a) && fabs(a) > s)
        s = fabs(a);
    if (isfinite(b) && fabs(b) > s)
        s = fabs(b);
    return s;
}

/* --------------------------------------------------------------------- */
/* jm_presolve_init / _free                                              */
/* --------------------------------------------------------------------- */

void jm_presolve_init(jm_presolve *p)
{
    memset(p, 0, sizeof *p);
}

void jm_presolve_free(jm_presolve *p)
{
    /* Every one of these is either presolve's own allocation or still
     * null (never touched, when outcome stayed JM_PRESOLVE_NONE) — free on
     * null is always safe, so no outcome check is needed here. None of them
     * ever aliases the caller's model (D-06), which is what makes this safe
     * without ever calling jaos_model_free on `reduced`. */
    free(p->reduced.col_cost);
    free(p->reduced.col_lower);
    free(p->reduced.col_upper);
    free(p->reduced.row_lower);
    free(p->reduced.row_upper);
    free(p->reduced.a_start);
    free(p->reduced.a_index);
    free(p->reduced.a_value);
    free(p->reduced.ar_start);
    free(p->reduced.ar_index);
    free(p->reduced.ar_value);
    free(p->reduced.row_scale);
    free(p->reduced.col_scale);
    free(p->reduced.sol_col);
    free(p->reduced.sol_row);
    free(p->reduced.sol_dual);
    free(p->reduced.sol_redcost);
    free(p->reduced.sol_col_status);
    free(p->reduced.sol_row_status);
    free(p->reduced.start_col_status);
    free(p->reduced.start_row_status);

    free(p->orig_col);
    free(p->orig_row);
    free(p->col_map);
    free(p->row_map);
    free(p->arena);
    memset(p, 0, sizeof *p);
}

/* --------------------------------------------------------------------- */
/* jm_presolve_run                                                       */
/* --------------------------------------------------------------------- */

/* Pushes one record. Returns false on allocation failure, matching every
 * other allocation site in this file's own convention. */
static bool ps_push(jm_presolve *p, jm_presolve_rec rec)
{
    if (!JM_GROW(p->arena, p->arena_cap, p->arena_len + 1))
        return false;
    p->arena[p->arena_len++] = rec;
    return true;
}

/* Empty column's favourable-bound rule (D19's one exception: this is the
 * only family permitted to report unboundedness, and only here). Returns
 * false when the favourable side is infinite AND the cost is genuinely
 * directional (nonzero) — a sound proof, no ray needed. A cost of exactly
 * zero has no favourable side to be infinite about: any finite point is
 * equally optimal, and if both bounds are infinite too the column is
 * truly free with no objective consequence, published at 0. */
static bool ps_empty_col_value(double cl, double cu, double cost,
                               double *out_v)
{
    if (cost > 0.0) {
        if (isfinite(cl)) { *out_v = cl; return true; }
        return false;
    }
    if (cost < 0.0) {
        if (isfinite(cu)) { *out_v = cu; return true; }
        return false;
    }
    if (isfinite(cl)) { *out_v = cl; return true; }
    if (isfinite(cu)) { *out_v = cu; return true; }
    *out_v = 0.0;
    return true;
}

JAOS_NODISCARD jaos_status jm_presolve_run(const jaos_model *m, jm_presolve *p,
                                           jm_work *w)
{
    const int64_t nr = m->num_row, nc = m->num_col;

    /* Working, per-round state. All sized by the ORIGINAL dimensions and
     * indexed by original row/column, so a record's `index`/`index2` never
     * has to carry a second, reduced-space meaning. */
    bool *col_dead = jm_calloc_array(nc, sizeof *col_dead);
    bool *row_dead = jm_calloc_array(nr, sizeof *row_dead);
    bool *row_frozen = jm_calloc_array(nr, sizeof *row_frozen);
    /* Set for a column that still has an entry in a row already removed by
     * a family whose multiplier is not zero by construction. The forcing
     * row's own dual derivation reads every other row's multiplier, and a
     * row removed earlier in forward time replays LATER, so its slot still
     * reads zero at that moment — this is what keeps that derivation off
     * such a row rather than nearly right on it. */
    bool *col_pending_dual = jm_calloc_array(nc, sizeof *col_pending_dual);
    /* What has been subtracted from each row's bounds, in magnitude. Every
     * removal of a value-determined column shifts cur_rl/cur_ru by that
     * column's contribution, so a row's bounds are a running difference and
     * the residue left in them is worth nothing below eps times this. The
     * empty-row feasibility test is the one place it is read. */
    double *row_traffic = jm_calloc_array(nr, sizeof *row_traffic);
    double *cur_cl = jm_alloc_array(nc, sizeof *cur_cl);
    double *cur_cu = jm_alloc_array(nc, sizeof *cur_cu);
    double *cur_rl = jm_alloc_array(nr, sizeof *cur_rl);
    double *cur_ru = jm_alloc_array(nr, sizeof *cur_ru);
    int64_t *col_deg = jm_alloc_array(nc, sizeof *col_deg);
    int64_t *row_deg = jm_alloc_array(nr, sizeof *row_deg);
    ps_rowwise rw = {0};

    bool ok = col_dead && row_dead && row_frozen && col_pending_dual &&
              row_traffic && cur_cl && cur_cu && cur_rl && cur_ru &&
              col_deg && row_deg && ps_build_rowwise(m, &rw);

    jaos_status ret = JAOS_OK;
    if (!ok) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    for (int64_t j = 0; j < nc; j++) {
        cur_cl[j] = m->col_lower[j];
        cur_cu[j] = m->col_upper[j];
        col_deg[j] = m->a_start[j + 1] - m->a_start[j];
    }
    for (int64_t i = 0; i < nr; i++) {
        cur_rl[i] = m->row_lower[i];
        cur_ru[i] = m->row_upper[i];
        row_deg[i] = rw.rs[i + 1] - rw.rs[i];
    }

    p->outcome = JM_PRESOLVE_NONE;

    {
    /* Two caps, and the tighter one wins. The structural one — every round
     * that changes anything removes at least one row or column, so no round
     * can be productive more than num_row + num_col times (T-02-10) — is
     * the correctness backstop 02-03 built. JM_PRESOLVE_ROUNDS is D-02's
     * measured stop on top of it: the propagation reaches a fixed point long
     * before the structural bound on any real model, and the sweep that set
     * it is in the comment above the constant. */
    int64_t cap = nr + nc + 1;
    if (JM_PRESOLVE_ROUNDS < cap)
        cap = JM_PRESOLVE_ROUNDS;
    int64_t rounds_done = 0;

    for (int64_t round = 0; round < cap; round++) {
        bool changed = false;

        /* --- Row pass: empty and singleton rows. --------------------- */
        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || row_frozen[i])
                continue;

            if (row_deg[i] == 0) {
                /* The activity of an empty row is exactly zero. Its BOUNDS
                 * are not exact, and that is the whole of this test.
                 *
                 * A row arrives here one of two ways. It can have had no
                 * entries in the first place, in which case its bounds are
                 * the caller's own numbers and an exact comparison is
                 * right. Or every column that touched it was removed, and
                 * each removal subtracted that column's contribution from
                 * both bounds — so what is being compared against zero is a
                 * running difference of terms that may be many orders of
                 * magnitude larger than what is left of it. On an equality
                 * row emptied that way the exact test asks for the sum to
                 * cancel to the last bit, and it does not.
                 *
                 * 02-03 compared exactly here and was not wrong to at the
                 * time: its families rarely emptied a row completely.
                 * 02-04's do, and the exact test refused `pilot` and
                 * `pilot87` — two feasible models reported INFEASIBLE,
                 * which is the mirror-image catastrophe T-02-12 names, at
                 * every epsilon from 1e-12 to 1e-4 because the epsilon was
                 * not what decided it. Found by tracing the four sites that
                 * can set this outcome rather than by reading the code.
                 *
                 * row_traffic[i] is what went through those subtractions,
                 * so the window is D23's argument in presolve's own space:
                 * a residue below eps times the traffic that produced it is
                 * not a number. A row nothing was ever removed from carries
                 * zero traffic and gets the exact test it deserves. */
                const double etol = row_traffic[i] > 0.0
                    ? PRESOLVE_TIGHTEN_EPS *
                      (row_traffic[i] > 1.0 ? row_traffic[i] : 1.0)
                    : 0.0;
                if (cur_rl[i] > etol || cur_ru[i] < -etol) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;
                    goto done;
                }
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_EMPTY_ROW, .index = i })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                row_dead[i] = true;
                p->counts.empty_row++;
                changed = true;
                continue;
            }

            if (row_deg[i] == 1) {
                int64_t j = -1;
                double a = 0.0;
                for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++) {
                    if (!col_dead[rw.ridx[k]]) {
                        j = rw.ridx[k];
                        a = rw.rval[k];
                        break;
                    }
                }
                assert(j >= 0);

                /* Checked here, before the generic singleton-row fold
                 * below, and not left for the column pass to find: this
                 * row-pass runs first every round (D-07's ascending order,
                 * generalized), so any row that reaches degree 1 is
                 * consumed here regardless of what its one column looks
                 * like, and a mutual singleton's row would never survive
                 * to be seen degree-1 from the column side at all — the
                 * column pass's own JM_PS_FREE_COL_SINGLETON branch is
                 * otherwise unreachable, confirmed the hard way before
                 * this comment existed. */
                if (col_deg[j] == 1 && m->col_cost[j] == 0.0 &&
                    !isfinite(cur_cl[j]) && !isfinite(cur_cu[j])) {
                    jm_work_add(w, JM_WORK_NONZERO);
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_FREE_COL_SINGLETON,
                            .index = i, .index2 = j, .coef = a,
                            .lo = cur_rl[i], .hi = cur_ru[i] })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    col_dead[j] = true;
                    row_dead[i] = true;
                    p->counts.free_col_singleton++;
                    changed = true;
                    continue;
                }

                double implied_lo, implied_hi;
                if (a > 0.0) {
                    implied_lo = isfinite(cur_rl[i]) ? cur_rl[i] / a : -HUGE_VAL;
                    implied_hi = isfinite(cur_ru[i]) ? cur_ru[i] / a : HUGE_VAL;
                } else {
                    implied_lo = isfinite(cur_ru[i]) ? cur_ru[i] / a : -HUGE_VAL;
                    implied_hi = isfinite(cur_rl[i]) ? cur_rl[i] / a : HUGE_VAL;
                }

                const bool tightens_lo = implied_lo > cur_cl[j];
                const bool tightens_hi = implied_hi < cur_cu[j];
                const double new_lo = implied_lo > cur_cl[j] ? implied_lo
                                                             : cur_cl[j];
                const double new_hi = implied_hi < cur_cu[j] ? implied_hi
                                                             : cur_cu[j];

                jm_work_add(w, JM_WORK_NONZERO);

                /* 02-03 left an exact comparison here as a placeholder,
                 * because presolve's own tolerance space did not exist
                 * yet and no constant was going to be borrowed from the
                 * two that did. PRESOLVE_TIGHTEN_EPS is that constant;
                 * this is the one place in the file that used to compare
                 * exactly and no longer does.
                 *
                 * The two branches below are one step apart and each says
                 * which verdict it produces, because a single comparison
                 * deciding both is where an off-by-one epsilon turns a
                 * solvable model into a refused one. */
                double fold_lo = new_lo, fold_hi = new_hi;
                const double btol =
                    PRESOLVE_TIGHTEN_EPS * ps_bound_scale(new_lo, new_hi);

                if (new_lo > new_hi + btol) {
                    /* PAST the opposite bound. The intersection is empty by
                     * more than rounding in the row-bound shifts can
                     * explain, so the model has no feasible point. */
                    p->outcome = JM_PRESOLVE_INFEASIBLE;
                    goto done;
                }
                if (new_lo > new_hi) {
                    /* ON the opposite bound, within the epsilon. The
                     * interval collapsed to a point — a legitimate
                     * reduction, not a refusal: the column is fixed and the
                     * fixed-column rule below takes it on this same round's
                     * column pass. The midpoint is what a collapsed
                     * interval is worth and is symmetric in the two ends,
                     * so it does not depend on which side was tightened. */
                    fold_lo = fold_hi = 0.5 * (new_lo + new_hi);
                }

                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_SINGLETON_ROW,
                        .index = i, .index2 = j, .coef = a,
                        .row_tightens_lo = tightens_lo,
                        .row_tightens_hi = tightens_hi })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                cur_cl[j] = fold_lo;
                cur_cu[j] = fold_hi;
                row_dead[i] = true;
                col_deg[j]--;
                col_pending_dual[j] = true;
                p->counts.singleton_row++;
                changed = true;
            }
        }

        /* --- Column pass: fixed, empty, singleton (cost 0). ----------- */
        for (int64_t j = 0; j < nc; j++) {
            if (col_dead[j])
                continue;

            if (cur_cl[j] == cur_cu[j]) {
                const double v = cur_cl[j];
                p->reduced.obj_offset += m->col_cost[j] * v;
                jm_work_add(w, (m->a_start[j + 1] - m->a_start[j]) *
                               JM_WORK_NONZERO);
                for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                    const int64_t i = m->a_index[k];
                    if (row_dead[i])
                        continue;
                    cur_rl[i] -= m->a_value[k] * v;
                    cur_ru[i] -= m->a_value[k] * v;
                    row_traffic[i] += fabs(m->a_value[k] * v);
                    row_deg[i]--;
                }
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_FIXED_COL, .index = j,
                        .value = v, .cost = m->col_cost[j] })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                col_dead[j] = true;
                p->counts.fixed_col++;
                changed = true;
                continue;
            }

            if (col_deg[j] == 0) {
                double v;
                if (!ps_empty_col_value(cur_cl[j], cur_cu[j],
                                        m->col_cost[j], &v)) {
                    p->outcome = JM_PRESOLVE_UNBOUNDED;
                    goto done;
                }
                p->reduced.obj_offset += m->col_cost[j] * v;
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_EMPTY_COL, .index = j,
                        .value = v, .cost = m->col_cost[j] })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                col_dead[j] = true;
                p->counts.empty_col++;
                changed = true;
                continue;
            }

            if (col_deg[j] == 1 && m->col_cost[j] == 0.0) {
                int64_t i = -1;
                double a = 0.0;
                for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                    if (!row_dead[m->a_index[k]]) {
                        i = m->a_index[k];
                        a = m->a_value[k];
                        break;
                    }
                }
                assert(i >= 0);

                const bool free_col = !isfinite(cur_cl[j]) &&
                                      !isfinite(cur_cu[j]);

                if (free_col && !row_frozen[i] && row_deg[i] == 1) {
                    /* Mutual singleton: row i's only remaining live entry
                     * is this one. Recorded with the row's CURRENT bounds
                     * (already net of every value-determined column
                     * removed from it so far), since nothing else touching
                     * this row remains live to contribute anything more. */
                    jm_work_add(w, JM_WORK_NONZERO);
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_FREE_COL_SINGLETON,
                            .index = i, .index2 = j, .coef = a,
                            .lo = cur_rl[i], .hi = cur_ru[i] })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    col_dead[j] = true;
                    row_dead[i] = true;
                    p->counts.free_col_singleton++;
                    changed = true;
                    continue;
                }
                if (!free_col) {
                    /* Bounded, cost-0 singleton column: relax row i to
                     * absorb whatever this column could have contributed,
                     * drop the column, freeze the row against every other
                     * row-removing family for the rest of this run. */
                    jm_work_add(w, JM_WORK_NONZERO);
                    const double c1 = a * cur_cl[j], c2 = a * cur_cu[j];
                    const double cmin = c1 < c2 ? c1 : c2;
                    const double cmax = c1 > c2 ? c1 : c2;
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_SINGLETON_COL,
                            .index = i, .index2 = j, .coef = a,
                            .lo = cur_cl[j], .hi = cur_cu[j] })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    if (isfinite(cur_rl[i]))
                        cur_rl[i] -= cmax;
                    if (isfinite(cur_ru[i]))
                        cur_ru[i] -= cmin;
                    row_traffic[i] += fabs(cmax) > fabs(cmin) ? fabs(cmax)
                                                              : fabs(cmin);
                    col_dead[j] = true;
                    row_deg[i]--;
                    row_frozen[i] = true;
                    p->counts.singleton_col++;
                    changed = true;
                    continue;
                }
                /* Free column, but its row still has other live entries:
                 * out of this plan's scope (see file header). Leave both
                 * alone; a future plan or the simplex itself resolves it. */
            }
        }

        /* --- Activity pass: forcing, redundant, bound tightening. ---- */
        /* One range per row, read four ways, in this order because the
         * first three are cheaper and two of them remove the row outright.
         * Rows of degree 0 and 1 are already consumed above; a frozen row
         * is skipped for 02-03's reason (its bounds stand for a range a
         * removed column may still need, not a determined value). */
        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || row_frozen[i] || row_deg[i] < 2)
                continue;

            const ps_range rg =
                ps_row_range(&rw, i, cur_cl, cur_cu, col_dead);
            jm_work_add(w, row_deg[i] * JM_WORK_NONZERO);

            const double rtol = ps_row_tol(&rg);
            const double min_act = ps_min_act(&rg);
            const double max_act = ps_max_act(&rg);
            const double rl = cur_rl[i], ru = cur_ru[i];

            /* 1. INFEASIBLE. The row cannot be satisfied by any point of
             *    the current boxes, so the model has no feasible point. No
             *    column is fixed on this branch. */
            if ((isfinite(ru) && min_act > ru + rtol) ||
                (isfinite(rl) && max_act < rl - rtol)) {
                p->outcome = JM_PRESOLVE_INFEASIBLE;
                goto done;
            }

            /* 2. FORCING. The range touches one of the row's own bounds, so
             *    every live column in it is pinned at the bound that attains
             *    that extreme. `force_hi` is the minimum activity reaching
             *    the row's UPPER bound; `force_lo` the maximum reaching its
             *    LOWER one. */
            const bool force_hi = isfinite(ru) && min_act >= ru - rtol;
            const bool force_lo = isfinite(rl) && max_act <= rl + rtol;
            if (force_hi || force_lo) {
                /* Fired only when every column's attaining bound is one the
                 * CALLER's own model carried, which is what the checker
                 * judges the answer against. A column pinned at a bound
                 * presolve derived is interior in the caller's box, where a
                 * nonzero reduced cost is a violation and this row's own
                 * multiplier — the only thing postsolve has to pay with —
                 * can drive it to a sign but not to zero. Cheap to check
                 * and it keeps the family's dual recovery a derivation
                 * rather than a hope. */
                bool at_own_bounds = true;
                for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++) {
                    const int64_t j = rw.ridx[k];
                    if (col_dead[j] || rw.rval[k] == 0.0)
                        continue;
                    if (col_pending_dual[j]) {
                        /* This column's reduced cost cannot be read off yet
                         * at postsolve time — see col_pending_dual. */
                        at_own_bounds = false;
                        break;
                    }
                    const bool want_lo =
                        force_hi ? (rw.rval[k] > 0.0) : (rw.rval[k] < 0.0);
                    if (want_lo ? (cur_cl[j] != m->col_lower[j])
                                : (cur_cu[j] != m->col_upper[j])) {
                        at_own_bounds = false;
                        break;
                    }
                }

                if (at_own_bounds) {
                    int64_t nfix = 0;
                    bool oom = false;
                    for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++) {
                        const int64_t j = rw.ridx[k];
                        if (col_dead[j])
                            continue;
                        if (rw.rval[k] == 0.0) {
                            /* Contributes nothing to the range and is not
                             * pinned by it, but its entry in this row dies
                             * with the row like any other — and this row
                             * does carry a multiplier, so the column it
                             * leaves behind is one no later forcing row may
                             * derive a dual through. */
                            col_deg[j]--;
                            col_pending_dual[j] = true;
                            continue;
                        }
                        const bool want_lo = force_hi ? (rw.rval[k] > 0.0)
                                                      : (rw.rval[k] < 0.0);
                        const double v = want_lo ? cur_cl[j] : cur_cu[j];
                        assert(isfinite(v));

                        p->reduced.obj_offset += m->col_cost[j] * v;
                        jm_work_add(w, (m->a_start[j + 1] - m->a_start[j]) *
                                       JM_WORK_NONZERO);
                        for (int64_t kk = m->a_start[j];
                             kk < m->a_start[j + 1]; kk++) {
                            const int64_t ii = m->a_index[kk];
                            if (row_dead[ii])
                                continue;
                            cur_rl[ii] -= m->a_value[kk] * v;
                            cur_ru[ii] -= m->a_value[kk] * v;
                            row_traffic[ii] += fabs(m->a_value[kk] * v);
                            row_deg[ii]--;
                        }
                        /* JM_PS_FIXED_COL, not a tag of its own: a column
                         * this pins is a fixed column and its replay is
                         * already written and already tested. `coef` is the
                         * one extra thing the row's own record needs back. */
                        if (!ps_push(p, (jm_presolve_rec){
                                .tag = JM_PS_FIXED_COL, .index = j,
                                .value = v, .cost = m->col_cost[j],
                                .coef = rw.rval[k] })) {
                            oom = true;
                            break;
                        }
                        col_dead[j] = true;
                        nfix++;
                    }
                    if (oom) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    /* Pushed AFTER its columns, so the LIFO walk replays it
                     * BEFORE them: the row's multiplier has to be in place
                     * when each column's reduced cost is derived from it. */
                    if (!ps_push(p, (jm_presolve_rec){
                            .tag = JM_PS_FORCING_ROW, .index = i,
                            .index2 = nfix,
                            .row_tightens_hi = force_hi })) {
                        ret = JAOS_ERR_OUT_OF_MEMORY;
                        goto cleanup_scratch;
                    }
                    row_dead[i] = true;
                    p->counts.forcing_row++;
                    changed = true;
                    continue;
                }
            }

            /* 3. REDUNDANT. The whole range lies inside the row's bounds,
             *    so the row can never bind and is dropped with no column
             *    fixed. Its multiplier is zero, which satisfies its own
             *    sign condition unconditionally — the one family here whose
             *    dual recovery needs no derivation at all. */
            if ((!isfinite(rl) || min_act >= rl - rtol) &&
                (!isfinite(ru) || max_act <= ru + rtol)) {
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_REDUNDANT_ROW, .index = i })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                for (int64_t k = rw.rs[i]; k < rw.rs[i + 1]; k++)
                    if (!col_dead[rw.ridx[k]])
                        col_deg[rw.ridx[k]]--;
                row_dead[i] = true;
                p->counts.redundant_row++;
                changed = true;
                continue;
            }

            /* 4. BOUND TIGHTENING — measured, refused, and not shipped.
             *
             * The fourth reading of the same range is the one the plan asked
             * for and the one the standard set will not accept. What the
             * rest of the row leaves for a column implies a bound on it, and
             * imposing that bound is what lets the other three reductions
             * cascade. Every version of it built here refused feasible
             * models:
             *
             *   design                                   solved  obj  checker
             *   ------------------------------------------------------------
             *   tightening off (what ships)               94/94   94    79
             *   bound reasoned with, never published      92/94   92    75
             *   bound published                           90/94   90    52
             *   published, outward rounding at DBL_EPS    89/94   89    48
             *   published, no collapse-to-fixed           91/94   91    48
             *   published, row window at DBL_EPSILON      89/94   89    48
             *
             * and nine epsilon settings from 1e-12 to 1e-4 moved none of it
             * (see 02-04-MEASUREMENT/). The last row is the one that settles
             * it: with every window reduced to the arithmetic's own error,
             * `pilot`, `pilot87`, `agg` and `maros` still come back
             * INFEASIBLE, so the implied bounds themselves are too tight on
             * those models rather than the comparisons around them being too
             * loose. `pilot` row 1095 is the worked case — an equality row
             * on one column, pinned at 1.15 by a forcing row that only
             * became forcing because the boxes had been narrowed.
             *
             * The three readings above ship because they measure clean and
             * better than the tree they came from. This one does not,
             * because a feasible model reported INFEASIBLE is the mirror of
             * the catastrophe the infeasible set exists to catch (T-02-12)
             * and no amount of reduction is worth it. What a later plan
             * needs before trying again is in 02-04-SUMMARY.md. */
        }

        if (!changed)
            break;
        rounds_done++;
    }
    p->counts.rounds = rounds_done;
    }

    {
    /* --- Final compaction: assign dense reduced indices to survivors. -- */
    int64_t rcol = 0, rrow = 0;
    for (int64_t j = 0; j < nc; j++)
        if (!col_dead[j])
            rcol++;
    for (int64_t i = 0; i < nr; i++)
        if (!row_dead[i])
            rrow++;

    p->col_map  = jm_alloc_array(nc, sizeof *p->col_map);
    p->row_map  = jm_alloc_array(nr, sizeof *p->row_map);
    p->orig_col = jm_alloc_array(rcol, sizeof *p->orig_col);
    p->orig_row = jm_alloc_array(rrow, sizeof *p->orig_row);
    if (!p->col_map || !p->row_map || (rcol > 0 && !p->orig_col) ||
        (rrow > 0 && !p->orig_row)) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    int64_t rj = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (col_dead[j]) { p->col_map[j] = -1; continue; }
        p->col_map[j] = rj;
        p->orig_col[rj] = j;
        rj++;
    }
    int64_t ri = 0;
    for (int64_t i = 0; i < nr; i++) {
        if (row_dead[i]) { p->row_map[i] = -1; continue; }
        p->row_map[i] = ri;
        p->orig_row[ri] = i;
        ri++;
    }
    assert(rj == rcol && ri == rrow);

    if (rcol == nc && rrow == nr) {
        /* Nothing was removed: NONE, and no reduced model is built (publish
         * takes the un-presolved path). The round counter is deliberately
         * not part of this test any more. An activity-range tightening
         * makes a round productive without removing anything, and it is
         * never published, so a run that only tightened has left the model
         * the simplex sees exactly as it was — copying it would cost a
         * model-sized allocation to hand back what the caller already
         * had. Nothing else can survive a round unchanged in both counts:
         * every other family removes a row or a column. */
        p->outcome = JM_PRESOLVE_NONE;
        goto cleanup_scratch;
    }

    /* Struct-copy first so cfg, sense, the log callback and the tolerances
     * carry over — then every pointer field that would otherwise alias m
     * is overwritten below, before anything reads it. Nothing here writes
     * to m (D-06): every array `reduced` ends up with is its own
     * allocation, never a pointer borrowed from the caller's model.
     *
     * obj_offset is saved first: the round loop above already accumulated
     * every fixed/empty column's cost*value contribution onto it, and the
     * struct-copy below would otherwise overwrite that with m's own
     * (pre-presolve) offset alone. */
    const double accumulated_offset = p->reduced.obj_offset;
    p->reduced = *m;
    p->reduced.obj_offset = m->obj_offset + accumulated_offset;
    p->reduced.num_col = rcol;
    p->reduced.num_row = rrow;
    p->reduced.rowwise_valid = false;
    p->reduced.ar_start = nullptr;
    p->reduced.ar_index = nullptr;
    p->reduced.ar_value = nullptr;
    p->reduced.scale_valid = false;
    p->reduced.scale_clamped = false;
    p->reduced.row_scale = nullptr;
    p->reduced.col_scale = nullptr;
    p->reduced.sol_col = nullptr;
    p->reduced.sol_row = nullptr;
    p->reduced.sol_dual = nullptr;
    p->reduced.sol_redcost = nullptr;
    p->reduced.sol_col_status = nullptr;
    p->reduced.sol_row_status = nullptr;
    p->reduced.start_col_status = nullptr;
    p->reduced.start_row_status = nullptr;
    p->reduced.solve_status = JAOS_SOLVE_NOT_RUN;
    p->reduced.objective = 0.0;
    p->reduced.solve_work = 0;
    p->reduced.solve_iters = 0;
    p->reduced.solve_time = 0.0;
    p->reduced.err[0] = '\0';

    p->reduced.col_cost  = jm_alloc_array(rcol, sizeof(double));
    p->reduced.col_lower = jm_alloc_array(rcol, sizeof(double));
    p->reduced.col_upper = jm_alloc_array(rcol, sizeof(double));
    p->reduced.row_lower = jm_alloc_array(rrow, sizeof(double));
    p->reduced.row_upper = jm_alloc_array(rrow, sizeof(double));
    p->reduced.a_start   = jm_alloc_array(rcol + 1, sizeof(int64_t));
    if ((rcol > 0 && (!p->reduced.col_cost || !p->reduced.col_lower ||
                      !p->reduced.col_upper)) ||
        (rrow > 0 && (!p->reduced.row_lower || !p->reduced.row_upper)) ||
        !p->reduced.a_start) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    for (int64_t ri2 = 0; ri2 < rrow; ri2++) {
        const int64_t i = p->orig_row[ri2];
        p->reduced.row_lower[ri2] = cur_rl[i];
        p->reduced.row_upper[ri2] = cur_ru[i];
    }

    /* The reduced CSC prefix and every surviving column's surviving
     * entries — an entry belongs in the reduced matrix only when both its
     * column AND its row are still alive. Charged nothing (D-14 checkpoint,
     * 02-02): a one-time structural copy, not a round computing a
     * reduction. */
    p->reduced.a_start[0] = 0;
    for (int64_t rj2 = 0; rj2 < rcol; rj2++) {
        const int64_t j = p->orig_col[rj2];
        p->reduced.col_cost[rj2]  = m->col_cost[j];
        /* pub_*, not cur_*: an activity-range tightening is used to reason
         * with and is never handed to the simplex — see the file header. */
        p->reduced.col_lower[rj2] = cur_cl[j];
        p->reduced.col_upper[rj2] = cur_cu[j];
        int64_t n = 0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            if (!row_dead[m->a_index[k]])
                n++;
        p->reduced.a_start[rj2 + 1] = p->reduced.a_start[rj2] + n;
    }
    p->reduced.num_nz = p->reduced.a_start[rcol];

    p->reduced.a_index = jm_alloc_array(p->reduced.num_nz, sizeof(int64_t));
    p->reduced.a_value = jm_alloc_array(p->reduced.num_nz, sizeof(double));
    if (p->reduced.num_nz > 0 &&
        (!p->reduced.a_index || !p->reduced.a_value)) {
        ret = JAOS_ERR_OUT_OF_MEMORY;
        goto cleanup_scratch;
    }

    for (int64_t rj2 = 0; rj2 < rcol; rj2++) {
        const int64_t j = p->orig_col[rj2];
        int64_t dst = p->reduced.a_start[rj2];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (row_dead[i])
                continue;
            p->reduced.a_index[dst] = p->row_map[i];
            p->reduced.a_value[dst] = m->a_value[k];
            dst++;
        }
    }

    /* Mapped into reduced indices before build_warm_basis ever reads it
     * (D-08). A removed row or column recorded basic has no reduced
     * counterpart to carry that status; dropping it undercounts the basic
     * total, and build_warm_basis already falls back to the slack basis
     * whenever the count is short of nrow — safe, never wrong, only colder
     * than a fuller mapping could be. */
    if (m->start_col_status != nullptr && m->start_row_status != nullptr) {
        p->reduced.start_col_status =
            jm_alloc_array(rcol, sizeof *p->reduced.start_col_status);
        p->reduced.start_row_status =
            jm_alloc_array(rrow, sizeof *p->reduced.start_row_status);
        if ((rcol > 0 && !p->reduced.start_col_status) ||
            (rrow > 0 && !p->reduced.start_row_status)) {
            ret = JAOS_ERR_OUT_OF_MEMORY;
            goto cleanup_scratch;
        }
        for (int64_t j = 0; j < nc; j++) {
            const int64_t rjj = p->col_map[j];
            if (rjj >= 0)
                p->reduced.start_col_status[rjj] = m->start_col_status[j];
        }
        for (int64_t i = 0; i < nr; i++) {
            const int64_t rii = p->row_map[i];
            if (rii >= 0)
                p->reduced.start_row_status[rii] = m->start_row_status[i];
        }

        /* A row-removed-by-folding-into-a-column pair (JM_PS_SINGLETON_ROW)
         * needs its own pass, second and separate from the naive per-index
         * copy above: the caller's supplied status for the surviving column
         * can be BASIC while the row it was just folded into is gone from
         * reduced space entirely, and BASIC there is not a literal "make it
         * basic" instruction — the reduced problem has one fewer row and no
         * slot to spend on it (confirmed empirically: without this pass,
         * exactly the caller-supplied-basis and postsolve-published-status
         * warm-start tests fail their 0-iteration assertions).
         *
         * The row's own supplied status is read directly rather than the
         * column's value, so this works whether the caller supplied the
         * basis by hand (jaos_set_basis, no sol_col yet) or it came from a
         * previous solve's own postsolve correction (sol_col populated):
         * the row's activity equals a_ij * x_j exactly, so "the row rests
         * at its lower/upper bound" translates directly into "x_j rests at
         * the corresponding implied bound" via the same sign relationship
         * jm_presolve_run's own row-pass used to compute
         * implied_lo/implied_hi for this same row a few lines up. */
        for (int64_t r = 0; r < p->arena_len; r++) {
            const jm_presolve_rec *rec = &p->arena[r];
            if (rec->tag != JM_PS_SINGLETON_ROW)
                continue;
            const int64_t j = rec->index2;
            const int64_t rjj = p->col_map[j];
            if (rjj < 0)
                continue;   /* column j was itself later removed */

            const jaos_basis_status rst = m->start_row_status[rec->index];
            if (rst == JAOS_BASIS_AT_LOWER || rst == JAOS_BASIS_AT_UPPER) {
                const bool row_at_hi = (rst == JAOS_BASIS_AT_UPPER);
                const bool x_at_lo = (rec->coef > 0.0) ? !row_at_hi
                                                        : row_at_hi;
                p->reduced.start_col_status[rjj] =
                    x_at_lo ? JAOS_BASIS_AT_LOWER : JAOS_BASIS_AT_UPPER;
            } else if (m->start_col_status[j] == JAOS_BASIS_BASIC &&
                      m->sol_col != nullptr) {
                /* The row's own supplied status did not disambiguate
                 * (BASIC or out of range); fall back to comparing the
                 * column's last published value against the bound this
                 * round's own tightening just computed. */
                if (m->sol_col[j] == cur_cl[j])
                    p->reduced.start_col_status[rjj] = JAOS_BASIS_AT_LOWER;
                else if (m->sol_col[j] == cur_cu[j])
                    p->reduced.start_col_status[rjj] = JAOS_BASIS_AT_UPPER;
            }
        }
    }

    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;
    }

done:
    /* Recorded on `reduced` so the SOLVED path (jm_postsolve_solved, below)
     * has presolve's own charge to publish even though no sx ever runs to
     * report one itself. On the REDUCED path this is overwritten by
     * publish()'s own m->solve_work = s->work.units once the reduced
     * model's solve finishes -- s->work started from this same total
     * (jm_dual_simplex threads it in before sx_init), so nothing here is
     * lost, only superseded by the fuller figure. On INFEASIBLE/UNBOUNDED
     * the same is true: jm_postsolve_solved is not the caller for those,
     * but the charge is set here regardless so it is never left stale. */
    p->reduced.solve_work = (w != nullptr) ? w->units : 0;

cleanup_scratch:
    free(col_dead); free(row_dead); free(row_frozen);
    free(col_pending_dual); free(row_traffic);
    free(cur_cl); free(cur_cu);
    free(cur_rl); free(cur_ru);
    free(col_deg); free(row_deg);
    ps_free_rowwise(&rw);
    return ret;
}

/* --------------------------------------------------------------------- */
/* Postsolve replay                                                      */
/* --------------------------------------------------------------------- */

/* One record's worth of replay, shared between jm_postsolve_expand (a
 * reduced solve happened, some rows/columns survived it) and
 * jm_postsolve_solved (nothing survived, every value was determined by
 * presolve alone). Both call this in the same strict LIFO order (D-07);
 * the only difference between the two callers is what orig->sol_dual/
 * sol_col/sol_col_status already hold for SURVIVING rows/columns before
 * this loop starts — solved has none, expand has whatever the reduced
 * solve published, mapped in by each caller before this runs. */
static void ps_replay_one(jaos_model *orig, const jm_presolve *p, int64_t r)
{
    const jm_presolve_rec *rec = &p->arena[r];

    switch (rec->tag) {
    case JM_PS_FIXED_COL:
    case JM_PS_EMPTY_COL: {
        const int64_t j = ps_restore_index(rec->index, orig->num_col);
        assert(j >= 0 && j < orig->num_col);

        orig->sol_col[j] = ps_published(rec->value);
        /* Permitted by the checker's own rule: at a fixed column (or an
         * empty one, published at exactly one of its own bounds) the
         * published value sits at whichever bound sign_condition checks,
         * so any status is as correct as any other for a genuinely fixed
         * column (check.c, "fixed -> anything"); for an empty column it
         * sits at the specific bound the favourable-cost choice picked,
         * and AT_LOWER/AT_UPPER is recorded to match. */
        orig->sol_col_status[j] =
            (rec->value == orig->col_upper[j] &&
             rec->value != orig->col_lower[j])
                ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_AT_LOWER;

        /* d_j = c_j - sum_k a_kj * y_k, over the column's ORIGINAL entries
         * and whatever original row duals are already known at this point
         * in the LIFO walk — every row this column touches is either a
         * surviving row (its dual set before any replay ran) or a row
         * whose OWN record replays later in this same walk (fired earlier
         * in forward time, hence pushed at a lower arena index, hence
         * visited after this one) and starts this loop at 0 either way, so
         * reading it here is always well-defined, never uninitialized. */
        double dw = rec->cost;
        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            assert(i >= 0 && i < orig->num_row);
            dw -= orig->a_value[k] * orig->sol_dual[i];
        }
        orig->sol_redcost[j] = ps_published(dw);

        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            orig->sol_row[i] = ps_published(orig->sol_row[i] +
                                            orig->a_value[k] * rec->value);
        }
        break;
    }

    case JM_PS_EMPTY_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);
        /* Explicit, not left to the caller's initial zeroing: a survived
         * row's dual is set by the "surviving rows" copy loop *before*
         * this replay runs, so under JAOS_PRESOLVE_FAULT_OFFBYONE, a
         * write that only relied on the pre-zeroed default would corrupt
         * nothing when this record's restore index is redirected onto a
         * surviving row's already-correct slot — confirmed the hard way,
         * a fault test that could not fail before this line existed. A
         * zero row dual always satisfies sign_condition regardless of
         * where the (always-zero, since nothing touches this row)
         * activity sits relative to its own bounds. */
        orig->sol_row[i] = 0.0;
        orig->sol_dual[i] = 0.0;
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }

    case JM_PS_SINGLETON_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        /* x_j's own value/status/reduced cost is already final by this
         * point in the walk — either from the reduced solve's own copy
         * (column j survived) or from column j's own, later-firing record
         * replayed earlier in this same LIFO walk (column j was itself
         * eliminated by further cascading after this row folded into it).
         *
         * Three cases, discriminated by which of the column's CURRENT
         * (post-fold) bounds x_j actually rests at, per the reduced
         * solve's own determination — not by re-deriving it from floating
         * comparisons here:
         *
         *   basic/free  -> d_j must be exactly 0 (interior, complementary
         *                  slackness): y_i = d'_j / a_ij, which is 0/a_ij
         *                  since a correctly dual-feasible d'_j is already
         *                  0 for a basic variable.
         *   at the bound the ROW's own implied bound produced
         *               -> y_i = d'_j / a_ij cancels row i's contribution
         *                  from d_j entirely (d_j = 0), which is what the
         *                  ORIGINAL problem requires here: x_j sits
         *                  strictly inside its own original bound on this
         *                  side, so only interior (d_j = 0) satisfies it.
         *   at the bound the COLUMN's own original bound produced
         *               -> y_i = 0, d_j = d'_j unchanged: the reduced
         *                  solve's own sign requirement at that bound is
         *                  identical to the original problem's, since the
         *                  bound itself is identical.
         *
         * Row i's own sign condition is satisfied by construction in every
         * case: w == 0 always passes (sign_condition's own trivial-zero
         * rule), and where y_i = d'_j / a_ij is nonzero, it is nonzero with
         * exactly the sign the reduced solve's own dual feasibility at
         * that bound already guarantees — see 02-03-SUMMARY.md for the
         * full derivation this comment summarizes. */
        /* 02-04 replaced 02-03's status-based discriminator with the
         * question the checker actually asks, because the status stopped
         * being able to answer it. 02-03 read sol_col_status[j] and
         * row_tightens_lo/hi to decide whether x_j was resting at a bound
         * the row induced or at one the column already had. That works
         * while the reduced solve is what put x_j where it is. It stops
         * working once presolve determines x_j itself: a column the fold
         * pinned to a point is republished by the fixed-column rule with
         * whichever of AT_LOWER/AT_UPPER matches its ORIGINAL bounds, and
         * that status no longer says anything about which side the row was
         * responsible for. A model where the row induces a lower bound of 3
         * on a column whose own upper bound is also 3 lands in exactly that
         * hole, and 02-04's own activity-range families produce it.
         *
         * The question, asked directly: does leaving this row's multiplier
         * at zero already satisfy x_j's sign condition? The test is exact
         * rather than windowed — a nonzero multiplier has to point at a
         * bound the value is resting on, and "resting on" for a value
         * presolve itself assigned is equality. Being conservative here
         * costs nothing: the repair below is sound whenever it is taken.
         *
         * Where zero does not work, the row absorbs the whole reduced cost:
         * y_i = d'_j / a_ij makes d_j exactly zero, which satisfies x_j's
         * condition at ANY value. The row's own condition comes out right
         * by construction rather than by luck. A positive d'_j means the
         * reduced solve had x_j resting at the folded LOWER bound, and it
         * is not the column's own (or zero would have worked), so it is the
         * row's — which for a_ij > 0 means a_ij·x_j = rl_i, the row resting
         * at its lower bound, where a positive multiplier is exactly what
         * is permitted; and y_i = d'_j / a_ij is positive. For a_ij < 0 the
         * same folded lower bound comes from ru_i, the row rests at its
         * upper bound where a negative multiplier is permitted, and
         * dividing by a negative a_ij delivers one. The two d'_j < 0 cases
         * are the mirror image. */
        const double d0 = orig->sol_redcost[j];
        const double v0 = orig->sol_col[j];
        const bool zero_works =
            d0 == 0.0 ||
            (d0 > 0.0 && v0 == orig->col_lower[j]) ||
            (d0 < 0.0 && v0 == orig->col_upper[j]);

        double y_i;
        if (zero_works) {
            y_i = 0.0;
            /* orig->sol_redcost[j] already holds d'_j, and the status the
             * column arrived with is the one its own bounds justify. */
        } else {
            /* x_j rests at a bound the ROW induced. In the ORIGINAL problem
             * that bound does not exist, so x_j is interior there —
             * publishing AT_LOWER/AT_UPPER as the reduced solve reported it
             * would claim a bound the original column never had, and the
             * row-count invariant a caller reads jaos_basis against would
             * be counting a genuinely interior variable as nonbasic. */
            y_i = d0 / rec->coef;
            orig->sol_redcost[j] = 0.0;
            orig->sol_col_status[j] = JAOS_BASIS_BASIC;
        }
#if defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
        /* D-10's per-family fault for singleton row: this family's real
         * risk is the dual choice, not the index (JAOS_PRESOLVE_FAULT_
         * OFFBYONE already covers every tag's index uniformly). Under
         * this build, always take the value 02-03-RESEARCH.md names as
         * the wrong one to use unconditionally — "the value the reduced
         * solve produced rather than the value that makes the full
         * reduced cost satisfy the checker's rule" — regardless of which
         * bound (if any) is actually responsible for x_j's position. */
        y_i = orig->sol_redcost[j] / rec->coef;
        orig->sol_redcost[j] = orig->sol_redcost[j] - rec->coef * y_i;
#endif
        orig->sol_dual[i] = ps_published(y_i);

        const double xv = orig->sol_col[j];
        orig->sol_row[i] = ps_published(rec->coef * xv);
        const double act = orig->sol_row[i];
        if (act == orig->row_lower[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
        else if (act == orig->row_upper[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_UPPER;
        else
            orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }

    case JM_PS_SINGLETON_COL: {
        const int64_t i = rec->index;
        const int64_t j = ps_restore_index(rec->index2, orig->num_col);
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        /* Row i survived (it was only relaxed, never removed), so its
         * dual and its "rest" activity (every OTHER live column's
         * contribution) are already final, copied in by the caller's own
         * row loop before any arena replay ran. x_j, cost 0, is recovered
         * as whichever point of its own range brings the row's FULL
         * activity back inside its ORIGINAL bounds -- always possible by
         * construction of the relaxation this record's own forward pass
         * computed. */
        const double rest = orig->sol_row[i];
        const double rl = orig->row_lower[i], ru = orig->row_upper[i];
        double lo_j, hi_j;
        if (rec->coef > 0.0) {
            lo_j = isfinite(rl) ? (rl - rest) / rec->coef : -HUGE_VAL;
            hi_j = isfinite(ru) ? (ru - rest) / rec->coef : HUGE_VAL;
        } else {
            lo_j = isfinite(ru) ? (ru - rest) / rec->coef : -HUGE_VAL;
            hi_j = isfinite(rl) ? (rl - rest) / rec->coef : HUGE_VAL;
        }
        /* The intersection of the column's own range with what the row
         * needs is non-empty by construction of the relaxation
         * jm_presolve_run computed when this record was pushed — any point
         * in it is equally optimal (cost 0), so the lower end is picked
         * with no further search. */
        const double want_lo = rec->lo > lo_j ? rec->lo : lo_j;
        const double want_hi = rec->hi < hi_j ? rec->hi : hi_j;
        (void)want_hi;   /* used only by the assert below, which -DNDEBUG
                          * (the release build) compiles away entirely */
        assert(want_lo <= want_hi);
        const double xv = want_lo;

        orig->sol_col[j] = ps_published(xv);
        orig->sol_col_status[j] =
            (xv == rec->lo) ? JAOS_BASIS_AT_LOWER :
            (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;
        /* d_j = c_j(=0) - a_ij * y_i, using row i's own, already-final
         * dual — no cancellation needed, since a cost-0 column carries no
         * sign requirement the row's own dual could conflict with (see
         * file header: this is exactly why the family is restricted to
         * cost 0). */
        orig->sol_redcost[j] = ps_published(-rec->coef * orig->sol_dual[i]);
        orig->sol_row[i] = ps_published(rest + rec->coef * xv);
        break;
    }

    case JM_PS_FREE_COL_SINGLETON: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        const int64_t j = rec->index2;
        assert(i >= 0 && i < orig->num_row);
        assert(j >= 0 && j < orig->num_col);

        /* Mutual singleton (T-02-09's own concern: two indices from one
         * record): row i had no other live entry when this fired, so its
         * whole activity comes from column j alone. x_j is free and
         * cost-0, so its own sign condition (both bounds infinite) can
         * only be satisfied by d_j == 0 exactly -- forcing y_i == 0
         * exactly, which in turn always satisfies row i's own condition
         * (the trivial w == 0 case, regardless of where the activity
         * actually sits). x_j's value only has to land inside the row's
         * own (already fully shifted, recorded at fire time) range;
         * whichever finite end is available is as good as any other. */
        const double target = isfinite(rec->lo) ? rec->lo :
                              (isfinite(rec->hi) ? rec->hi : 0.0);
        const double xv = target / rec->coef;

        orig->sol_col[j] = ps_published(xv);
        /* JAOS_BASIS_FREE is documented as "nonbasic AT ZERO" — publishing
         * it for a nonzero xv would claim a status this variable does not
         * have (confirmed the hard way: a JAOS_NO_PRESOLVE run of the same
         * model reports this column BASIC, since a free variable can only
         * be nonbasic at the one point — zero — its own bounds pin it to
         * nowhere else, exactly the row-count reasoning JM_PS_SINGLETON_ROW's
         * own status correction above already applies). */
        orig->sol_col_status[j] = (xv == 0.0) ? JAOS_BASIS_FREE
                                              : JAOS_BASIS_BASIC;
        orig->sol_redcost[j] = 0.0;
        orig->sol_dual[i] = 0.0;
        /* orig->sol_row[i] is written here using only x_j's own
         * contribution — any OTHER column also touching row i (a
         * value-determined one, fixed or emptied in an earlier round of
         * the SAME forward pass, hence replayed LATER in this LIFO walk)
         * adds its own share afterward, the same accumulate-on-replay
         * pattern JM_PS_FIXED_COL/JM_PS_EMPTY_COL already use. */
        orig->sol_row[i] = ps_published(rec->coef * xv);
        /* Row i's status is not derived from its activity here (that
         * activity is only x_j's own share so far, not the row's final
         * one — computing a bound-relative status from it would be
         * judging an incomplete number) — it is the row-count invariant's
         * mirror image of x_j's own status instead. Removing this pair
         * drops both num_row and num_col by one, so restoring it needs to
         * add back exactly one basic entry between the two, not two, not
         * zero: whichever of {x_j, row i} is NOT basic is nonbasic at
         * whatever the checker tolerates unconditionally for a dual of
         * exactly 0 (established above) — the specific bound named costs
         * nothing to get right, so AT_LOWER is not an arbitrary filler,
         * it is the side ps_published(0)-style zero-dual sign_condition
         * always accepts regardless of where the activity actually is. */
        orig->sol_row_status[i] =
            (orig->sol_col_status[j] == JAOS_BASIS_FREE)
                ? JAOS_BASIS_BASIC : JAOS_BASIS_AT_LOWER;
        break;
    }

    case JM_PS_REDUNDANT_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        /* The cleanest of the three: a row whose activity range lies wholly
         * inside its own bounds can never bind, so it carries no multiplier
         * and y_i = 0 satisfies sign_condition unconditionally — the trivial
         * w == 0 case, whatever the activity turns out to be. Removing it
         * changed no other column's reduced cost either, since every d_j is
         * c_j minus a sum in which this row's term is now zero.
         *
         * Its activity is NOT assigned here. jm_postsolve_expand seeds every
         * dead row with the surviving columns' contributions before this walk
         * starts, and every removed column adds its own share when its record
         * replays, so this slot is already accumulating and an assignment
         * would wipe whichever half arrived first.
         *
         * BASIC because of the row-count invariant: this record removes one
         * row and no column, so restoring it has to add back exactly one
         * basic entry, and the row is the only thing it restores. */
        orig->sol_dual[i] = 0.0;
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }

    case JM_PS_FORCING_ROW: {
        const int64_t i = ps_restore_index(rec->index, orig->num_row);
        assert(i >= 0 && i < orig->num_row);

        /* The one family here whose dual is a derivation rather than a zero.
         *
         * Row i's range touched one of its own bounds, so every live column
         * in it was pinned at the bound attaining that extreme, and each of
         * those bounds is one the ORIGINAL column had (checked before the
         * family fires). Each pinned column therefore carries a one-sided
         * sign requirement: at its lower bound d_j >= 0, at its upper bound
         * d_j <= 0. With this row's multiplier still zero, d_j reads
         * d_j^0 = c_j - sum_k a_kj y_k, and setting y_i moves it by
         * -a_ij * y_i.
         *
         * Take the row forced at its UPPER bound (the minimum activity
         * reached it). Then a_ij > 0 pins x_j at its lower bound, needing
         * d_j^0 - a_ij y_i >= 0, i.e. y_i <= d_j^0 / a_ij; and a_ij < 0 pins
         * x_j at its upper bound, needing d_j^0 - a_ij y_i <= 0, which after
         * dividing by the negative a_ij is the SAME inequality. So one
         * bound serves every column in the row: y_i = min over j of
         * d_j^0 / a_ij, and the row resting at its upper bound requires
         * y_i <= 0 in its own right, so the minimum is taken with 0. The
         * lower-bound case is the mirror image with max and >= 0.
         *
         * Moving y_i in that direction can only push every other d_j in the
         * row further into its own permitted side, which is why one value
         * satisfies all of them at once rather than trading them off.
         *
         * The columns are the `index2` records IMMEDIATELY BEFORE this one
         * in the arena — pushed there by the forward pass precisely so this
         * record replays first and their own reduced costs, computed after
         * it, already see the multiplier it chose.
         *
         * What this does not control: a row removed EARLIER in forward time
         * replays LATER, so its own multiplier still reads zero here and a
         * d_j^0 above is stale by that row's contribution. The forward pass
         * refuses to fire this family when any of its columns touches such a
         * row, which is what makes the derivation exact rather than nearly
         * exact. */
        double y = 0.0;
        for (int64_t t = 1; t <= rec->index2; t++) {
            const jm_presolve_rec *cr = &p->arena[r - t];
            assert(cr->tag == JM_PS_FIXED_COL);
            const int64_t j = cr->index;
            assert(j >= 0 && j < orig->num_col);

            double d0 = cr->cost;
            for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++)
                d0 -= orig->a_value[k] * orig->sol_dual[orig->a_index[k]];

            const double lim = d0 / cr->coef;
            if (t == 1)
                y = lim;
            else if (rec->row_tightens_hi ? (lim < y) : (lim > y))
                y = lim;
        }
        if (rec->row_tightens_hi ? (y > 0.0) : (y < 0.0))
            y = 0.0;

        orig->sol_dual[i] = ps_published(y);
        /* Row-count invariant: this record and the `index2` fixed-column
         * records before it remove one row and index2 columns between them,
         * and the columns are all nonbasic, so the row takes the single
         * basic slot the restoration owes. */
        orig->sol_row_status[i] = JAOS_BASIS_BASIC;
        break;
    }
    }
}

JAOS_NODISCARD jaos_status jm_postsolve_expand(jm_presolve *p)
{
    jaos_model *orig = p->orig;
    const jaos_model *red = &p->reduced;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    orig->solve_status = red->solve_status;
    orig->solve_iters  = red->solve_iters;
    orig->solve_work   = red->solve_work;
    orig->solve_time   = red->solve_time;

    if (red->solve_status != JAOS_SOLVE_OPTIMAL) {
        /* Same convention as publish()'s own non-optimal branch: nothing to
         * report, zeroed rather than left holding a previous solve's
         * answer. */
        orig->objective = 0.0;
        memset(orig->sol_col, 0, (size_t)orig->num_col * sizeof(double));
        memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
        memset(orig->sol_redcost, 0, (size_t)orig->num_col * sizeof(double));

        /* A partial (not-yet-optimal) basis is still worth resuming from —
         * publish()'s own non-optimal branch already remembers it for the
         * un-reduced path, and 02-01 documented this path as not offering
         * the same courtesy ("a fuller mapping is later work"). 02-03 is
         * where a model this genuinely reduces stopped being unreachable
         * (a row-tightened column with no fixed columns anywhere is
         * exactly the shape 02-01's own reduction could never fire on) —
         * status only, no values, since a stopping point is not a
         * solution and orig->sol_col/sol_row/sol_dual/sol_redcost stay
         * zeroed above regardless of what this maps.
         *
         * Read from red->start_col_status/start_row_status, not
         * red->sol_col_status/sol_row_status: publish()'s own non-optimal
         * branch writes the interrupted status into the sol_* pair only
         * long enough for jm_model_remember_basis to copy it into start_*,
         * then zeroes sol_col_status/sol_row_status unconditionally right
         * after (every entry reads as JAOS_BASIS_BASIC=0) — by the time
         * this function runs, sol_col_status/sol_row_status hold that
         * zeroed placeholder, and start_* is where the real snapshot
         * survived (confirmed empirically: reading sol_* here reported
         * every surviving row/column BASIC, overcounting nbasic for the
         * reduced problem by exactly the number of columns and rows that
         * were not actually basic). Null only for JAOS_SOLVE_NUMERICAL_ERROR
         * (deliberately excluded from remember_basis, D-whatever the
         * un-reduced path already cites) — left at the defaults below,
         * matching "no warm memory offered" for that one outcome. */
        for (int64_t i = 0; i < orig->num_row; i++) {
            const int64_t ri = p->row_map[i];
            orig->sol_row_status[i] =
                (ri >= 0 && red->start_row_status != nullptr)
                    ? red->start_row_status[ri] : JAOS_BASIS_BASIC;
        }
        for (int64_t j = 0; j < orig->num_col; j++) {
            const int64_t rj = p->col_map[j];
            orig->sol_col_status[j] =
                (rj >= 0 && red->start_col_status != nullptr)
                    ? red->start_col_status[rj] : JAOS_BASIS_AT_LOWER;
        }
        if (red->start_col_status == nullptr ||
            red->start_row_status == nullptr) {
            /* Nothing survived to remember (NUMERICAL_ERROR): leave
             * orig->start_* exactly as it was, the same courtesy 02-01's
             * un-reduced path extends. */
            return JAOS_OK;
        }
        /* Two corrections to the defaults just written, both keyed off the
         * arena rather than invented per-tag inline above:
         *
         * FREE_COL_SINGLETON's column is FREE, not AT_LOWER, matching the
         * optimal path's own choice for the same tag.
         *
         * SINGLETON_ROW's removed row defaults to whichever of AT_LOWER/
         * AT_UPPER the row's own *structural* tightening direction names
         * (row_tightens_lo/hi), not BASIC. This is deliberately not an
         * attempt at precision — there is no established VALUE behind an
         * interrupted solve's column for this row to be judged against —
         * it exists so jm_presolve_run's own warm-start correction pass
         * (which reads exactly this field on the *next* solve) takes its
         * definite branch instead of its value-comparison fallback, which
         * assumes a real published value and would misread the zeroed
         * placeholder above as a match. A row this fell back to BASIC
         * would leave the ambiguity for the next solve to resolve against
         * sol_col values that were never real numbers to begin with. */
        for (int64_t r = 0; r < p->arena_len; r++) {
            const jm_presolve_rec *rec = &p->arena[r];
            if (rec->tag == JM_PS_FREE_COL_SINGLETON) {
                orig->sol_col_status[rec->index2] = JAOS_BASIS_FREE;
            } else if (rec->tag == JM_PS_SINGLETON_ROW) {
                orig->sol_row_status[rec->index] =
                    rec->row_tightens_hi ? JAOS_BASIS_AT_UPPER :
                    rec->row_tightens_lo ? JAOS_BASIS_AT_LOWER :
                                            JAOS_BASIS_BASIC;
            }
        }

        (void)jm_model_remember_basis(orig);
        return JAOS_OK;
    }

    orig->objective = red->objective;

    /* Zeroed before anything below runs: a dead row/column's slot is
     * filled in only by its own arena record's replay, later in this same
     * function, and must read a known 0 (never garbage) if some OTHER
     * record's replay reads it first (JM_PS_FIXED_COL's redcost sum, for
     * a column touching a row that has not been replayed yet). */
    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));

    /* Surviving rows: same row, same coefficients other than what a
     * removed column already folded off it — value, dual and status the
     * reduced solve found are exactly the original problem's too. */
    for (int64_t i = 0; i < orig->num_row; i++) {
        const int64_t ri = p->row_map[i];
        if (ri < 0)
            continue;
        orig->sol_dual[i] = red->sol_dual[ri];
        orig->sol_row_status[i] = red->sol_row_status[ri];
        orig->sol_row[i] = red->sol_row[ri];
    }

    /* Surviving columns: same column, same coefficients, same row duals —
     * so the value, status and reduced cost the reduced solve found are
     * exactly the original problem's too, copied through rather than
     * recomputed. */
    for (int64_t j = 0; j < orig->num_col; j++) {
        const int64_t rj = p->col_map[j];
        if (rj < 0)
            continue;
        orig->sol_col[j] = red->sol_col[rj];
        orig->sol_col_status[j] = red->sol_col_status[rj];
        orig->sol_redcost[j] = red->sol_redcost[rj];
    }

    /* Every dead row's activity is seeded with what the SURVIVING columns
     * contribute to it, because those columns push no record and so add
     * nothing during the replay below. A removed column adds its own share
     * when its record replays, and the two halves can arrive in either
     * order, which is why a row-removing tag accumulates into this slot
     * rather than assigning to it.
     *
     * Only dead rows: a surviving row's activity came from the reduced
     * solve a few lines above and is already the original problem's, since
     * a removed column's contribution was folded into that row's bounds
     * rather than left in its activity. */
    for (int64_t j = 0; j < orig->num_col; j++) {
        if (p->col_map[j] < 0)
            continue;
        const double xv = orig->sol_col[j];
        for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
            const int64_t i = orig->a_index[k];
            if (p->row_map[i] < 0)
                orig->sol_row[i] += orig->a_value[k] * xv;
        }
    }

    /* Strictly LIFO (D-07). */
    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r);

    (void)jm_model_remember_basis(orig);
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p)
{
    jaos_model *orig = p->orig;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    /* Every column presolve fixed, emptied or eliminated: nothing is left
     * for the simplex to run on, so this outcome always publishes OPTIMAL
     * (INFEASIBLE and UNBOUNDED are their own, separate outcomes, handled
     * by jm_dual_simplex without ever calling this function). */
    orig->solve_status = JAOS_SOLVE_OPTIMAL;
    orig->solve_iters  = 0;
    /* presolve's own charge (D-14): jm_presolve_run already recorded it on
     * `reduced` for exactly this path -- no sx is built here, so nothing
     * else in this function would otherwise report it. */
    orig->solve_work   = p->reduced.solve_work;
    /* No clock is read here. Seconds are a development number that is
     * reported and never enters a baseline (D17); presolve's own cost,
     * timed or billed, starts being counted at all in 02-02. A presolve-only
     * solve reporting 0.0 is honest about what this plan measures, not a
     * claim that the work took no time. */
    orig->solve_time   = 0.0;
    orig->objective = p->reduced.obj_offset;
    orig->presolve_num_row = p->reduced.num_row;
    orig->presolve_num_col = p->reduced.num_col;
    orig->presolve_num_nz  = p->reduced.num_nz;

    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));

    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r);

    (void)jm_model_remember_basis(orig);
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_infeasible_or_unbounded(jm_presolve *p,
    jaos_solve_status status)
{
    jaos_model *orig = p->orig;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    orig->solve_status = status;
    orig->solve_iters  = 0;
    orig->solve_work   = p->reduced.solve_work;
    orig->solve_time   = 0.0;
    orig->objective = 0.0;
    orig->presolve_num_row = p->reduced.num_row;
    orig->presolve_num_col = p->reduced.num_col;
    orig->presolve_num_nz  = p->reduced.num_nz;

    /* Same convention as publish()'s own non-optimal branch: nothing to
     * report, zeroed rather than left holding a previous solve's answer.
     * No basis is offered either (Claude's discretion, matching 02-01's
     * own non-optimal path): a verdict presolve reached with no basis
     * ever built has no warmer starting point to offer than the one
     * already on the model. */
    memset(orig->sol_col, 0, (size_t)orig->num_col * sizeof(double));
    memset(orig->sol_row, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_dual, 0, (size_t)orig->num_row * sizeof(double));
    memset(orig->sol_redcost, 0, (size_t)orig->num_col * sizeof(double));
    memset(orig->sol_col_status, 0,
           (size_t)orig->num_col * sizeof *orig->sol_col_status);
    memset(orig->sol_row_status, 0,
           (size_t)orig->num_row * sizeof *orig->sol_row_status);
    return JAOS_OK;
}
