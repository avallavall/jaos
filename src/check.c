/* Independent solution checker.
 *
 * Judges a claimed solution against the model exactly as it was loaded —
 * original space, no scaling, no access to any solver bookkeeping.
 *
 * What the independence actually rests on, since this is easy to get wrong:
 *
 *   - Independent inputs. The only things that enter are the model as
 *     loaded and the claimed (x, y). No basis, no factorization, no solver
 *     state. A solver that reasoned its way to a wrong answer cannot pass
 *     that reasoning in here.
 *   - Redundant identities. Primal feasibility, dual sign conditions,
 *     complementary slackness and the primal-dual gap are checked
 *     together, and they are not independent of each other. The dual
 *     objective is accumulated from bounds while activities come from a
 *     separate scatter over the matrix, so a corrupted dot product shows
 *     up as a nonzero gap even if it also corrupts the reduced costs. The
 *     system is overdetermined; one broken kernel cannot satisfy all four.
 *   - Better arithmetic. Accumulation is in long double, so the oracle's
 *     own rounding error is an order below what it is judging. A checker
 *     no more accurate than the solver measures partly itself.
 *
 * What it does NOT protect against, stated so nobody assumes otherwise:
 * a model built wrongly by the loader. Checker and solver read the same
 * a_start/a_index/a_value, and if those are wrong both agree about the
 * wrong problem. Only external ground truth — reference optima — closes
 * that, which is what the Netlib gate is for.
 *
 * The reduced-cost loop here resembles the one in src/simplex.c, and they
 * stay apart: sharing would make this file link against solver internals,
 * the exact coupling it exists to forbid. They do not even read the same
 * numbers any more: the solver's loop runs on the scaled copy, this one on
 * the model as it was loaded.
 *
 * Everything is computed in minimize-canonical form: for maximization the
 * cost and the duals are negated internally (sigma), which turns the sign
 * conditions documented in jaos.h into one set of rules.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double max2(double a, double b) { return a > b ? a : b; }

/* Violation of "v must lie in [lo, hi]" as a raw magnitude. */
static double interval_violation(double v, double lo, double hi)
{
    double viol = 0.0;
    if (isfinite(lo))
        viol = max2(viol, lo - v);
    if (isfinite(hi))
        viol = max2(viol, v - hi);
    return viol;
}

/* Sign-condition violation for a multiplier w attached to a value v with
 * bounds [lo, hi], in minimize-canonical form:
 *   at lower  -> w >= 0
 *   at upper  -> w <= 0
 *   interior  -> w == 0   (this is complementary slackness)
 *   fixed     -> anything
 * "At a bound" is judged within tol * scale, and a multiplier at or below
 * tol is held to no condition at all.
 *
 * `scale` is what the value being tested is made of, and it is not decoration.
 * A row activity is a sum, and a sum that cancels cannot be pinned to an
 * absolute tolerance: row 3 of Netlib's `finnis` adds terms whose magnitudes
 * total 4.0e10 and lands 1.5e-6 from its bound, where one ulp at 4.0e10 is
 * 7.6e-6. Judged absolutely at 1e-6 that row is "not at its bound" and its
 * multiplier of 28 is reported as a violation of 28 — a demand for seventeen
 * correct decimal digits, which no double-precision answer can meet and no
 * amount of solver work can produce. The scale asks instead for the tolerance
 * as a fraction of what the row actually carries.
 *
 * What stops that from being a licence is that this test is a diagnostic and
 * not the proof. The proof is the gap, and the two are tied together
 * exactly: P - D = sum over v of w_v (v - bound_v), so a value waived here at
 * a distance d with a multiplier w contributes w * d to the gap, which is
 * checked separately and is not relaxed by any of this. Waiving a large
 * distance on a large multiplier therefore cannot certify a bad solution; it
 * shows up in the gap at full size. What the scale removes is only the case
 * where the distance is noise at the row's own precision, and there w * d is
 * negligible by construction. tests/test_check.c builds the case that must
 * still be rejected and confirms it is.
 *
 * Also accumulates the multiplier's contribution to the dual objective: w
 * picks the bound its sign points at, and every multiplier contributes,
 * including the ones exempt from the condition. A meaningful multiplier
 * pointing at an infinite bound is itself a dual violation; a negligible
 * one pointing there contributes nothing and violates nothing, which is
 * also why the infinite-bound test comes before any w * bound product.
 *
 * And the same term again, split by sign. The identity the gap rests on is
 * P - D = sum of w_v (v - bound_v), so the gap is the difference of two
 * sums and not a sum: when a term is negative it cancels a positive one
 * elsewhere and the total goes quiet while neither half does. Keeping them
 * apart costs one add and is what makes the difference between "these two
 * objectives agree" and "these two objectives agree because two large
 * quantities cancelled" visible from outside (D24). */
/* What the dual walk accumulates. Bundled rather than passed as five
 * out-parameters, which is where a caller starts handing them over in the
 * wrong order. */
typedef struct {
    long double dual_obj;
    long double pos, neg;

    /* The largest multiplier whose term the dual objective could not take,
     * and how many there were.
     *
     * A multiplier whose sign points at an infinite bound has no `w * bound`
     * to contribute — the term is minus infinity, because the dual objective
     * of a variable free in the improving direction is unbounded below. The
     * sum then belongs to a *different* problem, one where that variable had
     * a finite bound, and `P - D = sum w_v (v - bound_v)` no longer holds for
     * the problem that was asked about. So `gap_positive` stops being the
     * bound it is documented as being, and nothing in the report used to say
     * so (D47).
     *
     * These two are what says so. They decide nothing — no verdict reads
     * them — because deciding would need a threshold, and D47 measured that
     * no local test on a reduced cost separates the harmful case from the
     * harmless one: what makes a dropped term cost anything is the distance
     * the variable travels, which is a property of the polytope and not of
     * the column. What the caller gets is the fact and its size. */
    double dropped_max;
    int64_t dropped_n;

    /* The largest suboptimality any one dropped term certifies. See
     * certified_step: it is `|w|` times a distance the point can actually
     * travel, so it is a lower bound on `P - P*` and owes nothing to a
     * reference value. Zero means nothing was certified, which is not the
     * same as nothing being wrong. */
    long double certified;

    /* Dropped columns that can move without limit while every row stays
     * inside its bounds, and whose multiplier this checker nevertheless calls
     * zero.
     *
     * They are counted rather than certified, and the distinction is the
     * whole reason this field exists. Where the step is finite the product
     * `|w| * t` is self-limiting — a multiplier that is really roundoff
     * certifies a roundoff-sized suboptimality and does no harm, which is
     * what lets the certificate work with no threshold at all. Where the step
     * is infinite the product is infinite for *any* nonzero multiplier,
     * including 1e-17, so it stops being a certificate and becomes the
     * question D47 proved cannot be answered locally: is this multiplier
     * real? Reporting infinity there would claim five models of the reference
     * set are unbounded when every one of them matches a published finite
     * optimum. */
    int64_t rays;
} dual_acc;

static void split_term(long double t, long double *pos, long double *neg)
{
    if (t > 0.0L)
        *pos += t;
    else
        *neg -= t;    /* kept as a magnitude, so both halves are >= 0 */
}

/* Records a term the dual objective could not take. Every nonzero multiplier
 * pointing at an infinite bound counts, with no magnitude exemption: the
 * exemption below is about whether a *sign condition* is violated, and this
 * is about whether a *sum* is complete. A multiplier of 1e-15 is
 * indistinguishable from zero as a sign test and still drops a term of minus
 * infinity from an identity, and the whole content of D47 is that the two
 * questions have different answers. */
static void note_dropped(dual_acc *a, double w)
{
    a->dropped_n++;
    if (fabs(w) > a->dropped_max)
        a->dropped_max = fabs(w);
}

/* How far column j can travel in direction `dir` before some row leaves its
 * bounds, with **every other variable held exactly where it is**.
 *
 * That restriction is the whole point, and it is what makes the answer a
 * certificate rather than an estimate. The simplex direction lets the basics
 * absorb the move and travels further, but computing it needs `B^-1 a_j` —
 * which is what D47 costed route B at, because the checker would then need a
 * basis and a factorization of its own, and the thing that verifies the
 * answer would start sharing machinery with the thing that produced it.
 * Moving one column alone needs neither. It travels less far, so the
 * suboptimality it certifies is smaller than the true one; a smaller *lower*
 * bound is still a lower bound, and it is arrived at from the row activities
 * this file already computes and nothing else.
 *
 * The column's own opposite bound never limits it: this is only ever called
 * where that bound is infinite, which is what made the term droppable.
 *
 * Room is clamped at zero because the point being judged may sit a tolerance
 * outside a bound, and a negative slack would otherwise certify a negative
 * improvement — which is not a weaker claim, it is a wrong one.
 *
 * Returns HUGE_VAL when nothing blocks: the objective then improves without
 * limit along a direction that is feasible at every point of it, which is a
 * proof of unboundedness and not merely of suboptimality. */
static long double certified_step(const jaos_model *m, int64_t j, double dir,
                                  const long double *act)
{
    long double t = HUGE_VALL;
    for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
        int64_t i = m->a_index[k];
        long double per_t = (long double)m->a_value[k] * dir;
        if (per_t == 0.0L)
            continue;

        long double room, limit;
        if (per_t > 0.0L) {
            if (!isfinite(m->row_upper[i]))
                continue;
            room = (long double)m->row_upper[i] - act[i];
            limit = (room > 0.0L ? room : 0.0L) / per_t;
        } else {
            if (!isfinite(m->row_lower[i]))
                continue;
            room = act[i] - (long double)m->row_lower[i];
            limit = (room > 0.0L ? room : 0.0L) / -per_t;
        }
        if (limit < t)
            t = limit;
    }
    return t;
}

/* The bounds the constraints imply for a variable the model left unbounded.
 *
 * **Why this closes D47's hole, and why it needs no factorization.** The dual
 * objective's term for a variable is `w * bound`, and where the bound on the
 * improving side is infinite the term is minus infinity: `sign_condition`
 * drops it, and `gap_positive` stops being the bound `jaos.h` documents.
 * D47 read that as needing the simplex direction — how far the variable can
 * actually travel — and costed route B at a basis and a factorization inside
 * the checker, which D18 forbids taking from the solver.
 *
 * It does not need one. The rows already bound the variable, and reading that
 * off them uses nothing but the model. From `rl <= sum_k a_ik x_k <= ru`,
 * holding every other variable inside its own box gives a finite range for
 * `x_j` whenever the rest of the row is bounded the right way — the standard
 * activity-based tightening presolve does, applied here for a different
 * purpose.
 *
 * **The bound it derives is implied, not imposed, and that is what makes it
 * sound.** Every feasible point already satisfies it, so adding it changes
 * neither the feasible region nor `P*`. A dual bound valid for the tightened
 * problem is therefore valid for the original — which is exactly the claim
 * `gap_positive` makes and could not support.
 *
 * On D47's own constructed case — `min -1e-7 x2` subject to `x1 + x2 <= 1e6`
 * with both variables non-negative and neither bounded above — the row gives
 * `x2 <= 1e6`, the dropped term becomes `-1e-7 * 1e6`, and `gap_positive`
 * goes from 0 to **0.1**, which is the true suboptimality exactly.
 *
 * Where the rows imply nothing the bound stays infinite and the term is still
 * dropped. That is the honest outcome and it is counted as before: this
 * narrows the hole to the cases that genuinely have no finite lever, it does
 * not pretend to close all of them.
 *
 * Accumulated in `long double` for the reason the rest of this file is: the
 * oracle's rounding has to sit below what it judges. Terms that are infinite
 * are counted rather than summed, so one unbounded variable in a row does not
 * poison the bound for the others — the row can still bound `x_j` when `x_j`
 * is the only unbounded term in it. */
static void implied_bounds(const jaos_model *m, double *cl, double *cu,
                           long double *lo_sum, long double *up_sum,
                           int64_t *lo_inf, int64_t *up_inf)
{
    const int64_t nr = m->num_row, nc = m->num_col;

    for (int64_t i = 0; i < nr; i++) {
        lo_sum[i] = 0.0L;
        up_sum[i] = 0.0L;
        lo_inf[i] = 0;
        up_inf[i] = 0;
    }

    /* Each row's activity range over the column boxes, infinities counted
     * rather than propagated. */
    for (int64_t j = 0; j < nc; j++) {
        const double xl = m->col_lower[j], xu = m->col_upper[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double aij = m->a_value[k];
            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            if (isfinite(t_lo))
                lo_sum[i] += (long double)aij * t_lo;
            else
                lo_inf[i]++;
            if (isfinite(t_up))
                up_sum[i] += (long double)aij * t_up;
            else
                up_inf[i]++;
        }
    }

    for (int64_t j = 0; j < nc; j++) {
        cl[j] = m->col_lower[j];
        cu[j] = m->col_upper[j];
        const bool want_lo = !isfinite(cl[j]), want_up = !isfinite(cu[j]);
        if (!want_lo && !want_up)
            continue;

        const double xl = m->col_lower[j], xu = m->col_upper[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double aij = m->a_value[k];
            if (aij == 0.0)
                continue;

            /* This column's own share of each end of the row's range, and
             * whether the rest of the row is finite once it is taken out. */
            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            const bool rest_lo_finite =
                lo_inf[i] == (isfinite(t_lo) ? 0 : 1);
            const bool rest_up_finite =
                up_inf[i] == (isfinite(t_up) ? 0 : 1);
            const long double rest_lo =
                lo_sum[i] - (isfinite(t_lo) ? (long double)aij * t_lo : 0.0L);
            const long double rest_up =
                up_sum[i] - (isfinite(t_up) ? (long double)aij * t_up : 0.0L);

            /* a_ij x_j <= ru - min(rest)  and  a_ij x_j >= rl - max(rest).
             * Dividing by a_ij swaps the two when it is negative. */
            if (rest_lo_finite && isfinite(m->row_upper[i])) {
                const long double lim =
                    ((long double)m->row_upper[i] - rest_lo) / aij;
                if (aij > 0.0) {
                    if (want_up && (double)lim < cu[j])
                        cu[j] = (double)lim;
                } else if (want_lo && (double)lim > cl[j]) {
                    cl[j] = (double)lim;
                }
            }
            if (rest_up_finite && isfinite(m->row_lower[i])) {
                const long double lim =
                    ((long double)m->row_lower[i] - rest_up) / aij;
                if (aij > 0.0) {
                    if (want_lo && (double)lim > cl[j])
                        cl[j] = (double)lim;
                } else if (want_up && (double)lim < cu[j]) {
                    cu[j] = (double)lim;
                }
            }
        }
    }
}

static double sign_condition(double v, double lo, double hi, double w,
                             double tol, double scale, dual_acc *a)
{
    double window = tol * scale;
    bool at_lo = isfinite(lo) && v <= lo + window;
    bool at_hi = isfinite(hi) && v >= hi - window;

    /* The magnitude exemption applies to the sign condition and stops
     * there. A multiplier indistinguishable from zero should not force a
     * variable onto a bound — but it still contributes w * bound to the
     * dual objective, because D(y) = sum_v min over the variable's range
     * of w_v t is a function of y alone, and truncating its terms by
     * magnitude is not part of that definition.
     *
     * Dropping them, which is what this used to do, discards a quantity
     * that is small only if the bound is: a multiplier of 1e-7 on a
     * variable resting on a bound of 1e6 carries 0.1 of dual objective,
     * and losing it while the primal still counts c_j v_j invents a gap
     * proportional to tol. That is what rejected pilot-ja.
     *
     * Two rules that also close that case were tried and are wrong.
     * Contributing w * v makes every term vanish, so on a model whose
     * multipliers all sit under tol the gap is identically zero for any
     * feasible point — it certifies the whole polytope. Choosing the bound
     * by which one v is nearest, as HiGHS does for its own diagnostic,
     * produces negative terms that cancel real residuals elsewhere, and on
     * a free variable computes (-inf + inf) * 0.5.
     *
     * Keeping the definition intact is what makes the gap mean something:
     * P - D = sum_v w_v (v - bound_v), every term non-negative, so an
     * accepted solution has P - P* <= gap proved by weak duality. */
    bool negligible = fabs(w) <= tol;

    if (w > 0.0) {
        if (!isfinite(lo)) {
            note_dropped(a, w);
            return negligible ? 0.0 : w;
        }
        a->dual_obj += (long double)w * lo;
        split_term((long double)w * ((long double)v - lo), &a->pos, &a->neg);
        return (negligible || at_lo) ? 0.0 : w;
    }
    if (w < 0.0) {
        if (!isfinite(hi)) {
            note_dropped(a, w);
            return negligible ? 0.0 : -w;
        }
        a->dual_obj += (long double)w * hi;
        split_term((long double)w * ((long double)v - hi), &a->pos, &a->neg);
        return (negligible || at_hi) ? 0.0 : -w;
    }
    return 0.0;   /* a zero multiplier contributes to neither */
}

jaos_status jaos_check_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, double tol,
    jaos_check_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (!(isfinite(tol) && tol >= 0.0))
        return JAOS_ERR_INVALID_INPUT;
    if (m->num_col > 0 && col_value == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    for (int64_t j = 0; j < m->num_col; j++)
        if (isnan(col_value[j]))
            return JAOS_ERR_INVALID_INPUT;
    if (row_dual != nullptr)
        for (int64_t i = 0; i < m->num_row; i++)
            if (isnan(row_dual[i]))
                return JAOS_ERR_INVALID_INPUT;

    memset(out, 0, sizeof *out);

    /* Row activities from the CSC copy: column-order scatter-add. The order
     * is fixed by the data structure, so the result is deterministic (D8).
     * Accumulated wider than the values being judged, so what the report
     * measures is the solver's error and not the checker's. */
    long double *act = jm_calloc_array(m->num_row, sizeof(long double));
    /* What each activity is made of, accumulated in the same pass: the sum
     * of the magnitudes of the terms, which is the scale the sum's own
     * precision is set by. Only the dual sign conditions read it. */
    long double *traffic = jm_calloc_array(m->num_row, sizeof(long double));
    if (act == nullptr || traffic == nullptr) {
        free(act);
        free(traffic);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        double xj = col_value[j];
        if (xj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            long double term = (long double)m->a_value[k] * xj;
            act[m->a_index[k]] += term;
            traffic[m->a_index[k]] += fabsl(term);
        }
    }

    /* Primal side. */
    double col_viol = 0.0, row_viol = 0.0, row_viol_rel = 0.0;
    long double primal_obj = m->obj_offset;
    for (int64_t j = 0; j < m->num_col; j++) {
        col_viol = max2(col_viol, interval_violation(col_value[j],
                                    m->col_lower[j], m->col_upper[j]));
        primal_obj += (long double)m->col_cost[j] * col_value[j];
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        double viol = interval_violation((double)act[i], m->row_lower[i],
                                         m->row_upper[i]);
        row_viol = max2(row_viol, viol);
        /* The same residue against what the row is made of. A row summing
         * terms that total 4.0e10 in magnitude cannot place its activity
         * more finely than an ulp at that size, so 1e-6 of absolute
         * residue on it is a different quantity from 1e-6 on a row
         * carrying 0.7 — and the absolute test cannot tell them apart.
         * D24 refused to make the *predicate* relative and keeps the
         * measurement here instead, where it decides nothing: the biggest
         * relative residue and the biggest absolute one need not even be
         * the same row, and that is the point of reporting both. */
        row_viol_rel = max2(row_viol_rel,
                            viol / max2(1.0, (double)traffic[i]));
    }

    out->max_col_violation = col_viol;
    out->max_row_violation = row_viol;
    out->max_row_violation_relative = row_viol_rel;
    out->primal_objective = (double)primal_obj;
    out->primal_feasible = col_viol <= tol && row_viol <= tol;

    /* Dual side, minimize-canonical. */
    if (row_dual != nullptr) {
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double dual_viol = 0.0;
        dual_acc a = {0};
        a.dual_obj = sigma * m->obj_offset;   /* canonical offset */

        /* What the constraints imply about the variables the model left
         * unbounded, which is what stops the dual objective's term for them
         * being minus infinity (D87). Allocation failure is not fatal: the
         * bounds fall back to the model's own and the checker behaves exactly
         * as it did before, which is a weaker report rather than a wrong one.
         *
         * A row's own range comes free with them — it is the activity range
         * over the column boxes, which `implied_bounds` computes on the way —
         * so a free row's multiplier stops being dropped too. */
        double *icl = jm_calloc_array(m->num_col, sizeof(double));
        double *icu = jm_calloc_array(m->num_col, sizeof(double));
        long double *rlo = jm_calloc_array(m->num_row, sizeof(long double));
        long double *rup = jm_calloc_array(m->num_row, sizeof(long double));
        int64_t *rli = jm_calloc_array(m->num_row, sizeof(int64_t));
        int64_t *rui = jm_calloc_array(m->num_row, sizeof(int64_t));
        const bool implied = icl != nullptr && icu != nullptr &&
                             rlo != nullptr && rup != nullptr &&
                             rli != nullptr && rui != nullptr;
        if (implied)
            implied_bounds(m, icl, icu, rlo, rup, rli, rui);

        for (int64_t i = 0; i < m->num_row; i++) {
            double rl = m->row_lower[i], ru = m->row_upper[i];
            if (implied) {
                if (!isfinite(rl) && rli[i] == 0)
                    rl = (double)rlo[i];
                if (!isfinite(ru) && rui[i] == 0)
                    ru = (double)rup[i];
            }
            dual_viol = max2(dual_viol,
                sign_condition((double)act[i], rl, ru, sigma * row_dual[i],
                               tol, max2(1.0, (double)traffic[i]), &a));
        }

        for (int64_t j = 0; j < m->num_col; j++) {
            /* Reduced cost d_j = c_j - a_j' y, canonicalized. */
            long double dw = m->col_cost[j];
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                dw -= (long double)m->a_value[k] * row_dual[m->a_index[k]];
            double d = (double)dw;
            /* A column value is not a sum of cancelling terms — it is one
             * published number — so its scale is its own magnitude, which is
             * the ordinary mixed absolute/relative form and not the argument
             * above. The row case is the one that needed it. */
            dual_viol = max2(dual_viol,
                sign_condition(col_value[j],
                               implied ? icl[j] : m->col_lower[j],
                               implied ? icu[j] : m->col_upper[j],
                               sigma * d, tol, max2(1.0, fabs(col_value[j])),
                               &a));

            /* Where that call had to drop a term, this is where the matrix is
             * in scope to say what the drop is worth. The condition is asked
             * again rather than plumbed back out: it is two comparisons, and
             * a field meaning "did the previous call drop" would be state
             * that is only valid between two statements.
             *
             * Rows are not done, and that is a limitation rather than an
             * oversight: a row's activity is not something that can be moved
             * on its own, so there is no single-entity direction to measure a
             * step along. What a dropped row multiplier is worth needs the
             * simplex direction, which is the part that costs a
             * factorization. */
            const double w = sigma * d;
            const bool drops = (w > 0.0 && !isfinite(m->col_lower[j])) ||
                               (w < 0.0 && !isfinite(m->col_upper[j]));
            if (drops) {
                long double t = certified_step(m, j, w > 0.0 ? -1.0 : 1.0,
                                               act);
                if (isinf((double)t) && fabs(w) <= tol) {
                    /* An unbounded ray whose rate this checker calls zero.
                     * Counted, not certified — see dual_acc. The test is the
                     * same `negligible` the sign condition uses, and reusing
                     * it is the point: it is this checker's own definition of
                     * a multiplier being nonzero, not a second number chosen
                     * to make an awkward case go away. */
                    a.rays++;
                } else {
                    long double gain = (long double)fabs(w) * t;
                    if (gain > a.certified)
                        a.certified = gain;
                }
            }
        }

        /* The gap is the identity that catches a corrupted dot product even
         * when the sign conditions it also corrupts still pass, so it is
         * formed at the wider precision and narrowed only at the end. */
        long double true_dual_obj = sigma * a.dual_obj;
        long double diff = fabsl(primal_obj - true_dual_obj);
        long double scale = 1.0L + fabsl(primal_obj) + fabsl(true_dual_obj);
        double gap = (double)(diff / scale);

        out->checked_duals = true;
        out->max_dual_violation = dual_viol;
        out->dual_objective = (double)true_dual_obj;
        out->objective_gap = gap;
        /* Whether the identity the two halves come from was complete. Not a
         * verdict and not compared against anything: it says whether
         * gap_positive is the bound it is documented as being, which is a
         * different question from whether this point is good. */
        out->max_dropped_multiplier = a.dropped_max;
        out->dropped_terms = a.dropped_n;
        out->gap_certified = a.dropped_n == 0;
        out->certified_suboptimality = (double)a.certified;
        out->unquantified_rays = a.rays;
        /* Reported in the objective's own units rather than against the
         * scale above, because what they are for is the bound
         * P - P* <= gap_positive, and a bound is only usable in the units
         * of the thing it bounds. The gap is recovered from them by the
         * division the line above does: |pos - neg| is the same quantity
         * as `diff`, arrived at along an independent route, which is the
         * kind of redundancy the rest of this file is built out of. */
        out->gap_positive = (double)a.pos;
        out->gap_negative = (double)a.neg;
        out->dual_feasible = dual_viol <= tol && gap <= tol;

        free(icl);
        free(icu);
        free(rlo);
        free(rup);
        free(rli);
        free(rui);
    }

    free(act);
    free(traffic);
    return JAOS_OK;
}
