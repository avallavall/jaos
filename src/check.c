/* Independent solution checker.
 *
 * Judges a claimed solution against the model exactly as it was loaded —
 * original space, no scaling, no access to any solver bookkeeping (D18).
 * It does NOT protect against a model built wrongly by the loader: checker
 * and solver read the same a_start/a_index/a_value. The reduced-cost loop
 * here resembles the one in src/simplex.c, and they stay apart: sharing
 * would make this file link against solver internals.
 *
 * Everything is computed in minimize-canonical form: for maximization the
 * cost and the duals are negated internally (sigma).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <assert.h>
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

/* What the dual walk accumulates. `pos`/`neg` are magnitudes. */
typedef struct {
    double dual_obj, dual_objc;
    double pos, posc, neg, negc;

    /* The same two sums restricted to bounds the model declared (D91). An
     * implied bound is sound but slack, so its term is live at an optimum.
     * The verdict reads these; `pos`/`neg` bound the suboptimality. */
    double pos_model, pos_modelc, neg_model, neg_modelc;

    /* The largest multiplier whose term the dual objective could not take,
     * and how many there were. No verdict reads them (D47). */
    double dropped_max;
    int64_t dropped_n;

    /* The largest suboptimality any one dropped term certifies: a lower
     * bound on `P - P*` (D73). Zero means nothing was certified. */
    double certified;

    /* Dropped columns that can move without limit while every row stays
     * inside its bounds, and whose multiplier this checker calls zero.
     * Counted rather than certified (D73). */
    int64_t rays;
} dual_acc;

/* The value of a compensated pair. A non-finite sum turns the compensation
 * into a NaN, and a NaN makes every comparison against it false, so the
 * pair reads as its sum there. Presolve's accumulator has the same rule and
 * for the same reason (D165). */
static double acc_value(double sum, double comp)
{
    return (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

/* The difference of two compensated pairs, with the corrections carried
 * through the subtraction rather than folded in before it. The two sums
 * cancel down to a small figure and the two corrections are the part of it
 * that survives; folding each pair first rounds that part away. Same
 * non-finite rule as `acc_value`, and for the same reason (D165). */
static double acc_difference(double sum_a, double comp_a,
                             double sum_b, double comp_b)
{
    if (!(isfinite(sum_a) && isfinite(comp_a) &&
          isfinite(sum_b) && isfinite(comp_b)))
        return sum_a - sum_b;
    return (sum_a - sum_b) + (comp_a - comp_b);
}

/* What `a + b` lost when it rounded to `s`, by Knuth's two-sum. Exact for
 * any finite operands whose sum does not overflow, and it needs no claim
 * about libm, which is why the file uses it rather than a wider type
 * (D169, D34). A non-finite sum carries no residue a correction could
 * hold, so zero is the only honest answer there.
 *
 * The same precondition as `jm_two_product_residue`: `FLT_EVAL_METHOD == 0`,
 * asserted in `jaos_internal.h`. Evaluated at 80 bits, `s - a` returns the
 * unrounded difference and the residue comes out zero. */
static double two_sum_residue(double a, double b, double s)
{
    if (!isfinite(s))
        return 0.0;
    const double bb = s - a;
    return (a - (s - bb)) + (b - bb);
}

/* Adds one term of the gap to the half its sign puts it in, compensated.
 * `e` is what that term's own arithmetic lost; it goes into the same half,
 * because it corrects that term and is not a term of its own. */
static void split_term(double t, double e, double *pos, double *posc,
                       double *neg, double *negc)
{
    if (t > 0.0) {
        jm_obj_add(pos, posc, t);
        if (e != 0.0)
            jm_obj_add(pos, posc, e);
    } else {
        /* Kept as a magnitude, so both halves are >= 0. The `else` takes
         * `t == 0`, where the add is a no-op and `e` is provably zero, and
         * it also takes a NaN, which poisons this half exactly as the
         * uncompensated version did: `objective_gap` reads NaN and
         * `dual_feasible` is false. Dropping a NaN instead would publish a
         * clean-looking zero gap on a point nobody evaluated. */
        jm_obj_add(neg, negc, -t);
        if (e != 0.0)
            jm_obj_add(neg, negc, -e);
    }
}

/* One product into a compensated pair, with the product's own residue
 * following it. The certificate checkers want exactly this, and none of
 * them wants the split into halves that `split_term` does. */
static void add_product(double *sum, double *comp, double a, double b)
{
    const double p = a * b;
    const double e = jm_two_product_residue(a, b, p);
    jm_obj_add(sum, comp, p);
    if (e != 0.0)
        jm_obj_add(sum, comp, e);
}

/* The term a multiplier `w` contributes at a bound `b` for a value `v`:
 * `w * (v - b)`, with what the two roundings lost returned in `*e`. The
 * subtraction is exact where it cancels (Sterbenz) and its residue is
 * recovered where it does not, so the pair `(t, *e)` is the product to
 * within one rounding of `w * dve`. */
static double bound_term(double w, double v, double b, double *e)
{
    const double dv = v - b;
    const double dve = two_sum_residue(v, -b, dv);
    const double t = w * dv;
    /* An overflowed term carries no correction. Without this the next line
     * builds `w * dve`, which overflows the other way, and `split_term`
     * then adds `+inf` and `-inf` into the same accumulator and makes it a
     * NaN -- which trips the magnitude assert on the gap halves. Reached
     * with `w` infinite and `v`, `b` near 1e300. Same rule, and the same
     * reason, as `jm_two_product_residue` and `two_sum_residue`. */
    if (!isfinite(t)) {
        *e = 0.0;
        return t;
    }
    *e = jm_two_product_residue(w, dv, t);
    if (dve != 0.0)
        *e += w * dve;
    return t;
}

/* Records a term the dual objective could not take. Every nonzero multiplier
 * pointing at an infinite bound counts, with no magnitude exemption (D47). */
static void note_dropped(dual_acc *a, double w)
{
    a->dropped_n++;
    if (fabs(w) > a->dropped_max)
        a->dropped_max = fabs(w);
}

/* How far column j can travel in direction `dir` before some row leaves its
 * bounds, with every other variable held exactly where it is. It travels
 * less far than the simplex direction, so what it certifies is a lower
 * bound (D73). Only ever called where the column's own opposite bound is
 * infinite. Room is clamped at zero because the point being judged may sit
 * a tolerance outside a bound. Returns HUGE_VAL when nothing blocks. */
static double certified_step(const jaos_model *m, int64_t j, double dir,
                             const double *act)
{
    /* Both sentences above, checked rather than written (D219). The caller
     * reaches here only where `drops` held, and `drops` is the same test on
     * the bound this direction travels away from. */
    assert(!isfinite(dir < 0.0 ? m->col_lower[j] : m->col_upper[j]));
    double t = HUGE_VAL;
    for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
        int64_t i = m->a_index[k];
        /* `dir` is +1 or -1, so this product is exact (D270). */
        const double per_t = m->a_value[k] * dir;
        if (per_t == 0.0)
            continue;

        double room, limit;
        if (per_t > 0.0) {
            if (!isfinite(m->row_upper[i]))
                continue;
            room = m->row_upper[i] - act[i];
            limit = (room > 0.0 ? room : 0.0) / per_t;
        } else {
            if (!isfinite(m->row_lower[i]))
                continue;
            room = act[i] - m->row_lower[i];
            limit = (room > 0.0 ? room : 0.0) / -per_t;
        }
        if (limit < t)
            t = limit;
    }
    /* "Room is clamped at zero", so what this returns is a distance and
     * never a negative one; a negative would turn a certified suboptimality
     * into a claim that the point is better than optimal (D219). */
    assert(t >= 0.0);
    return t;
}

/* The cap on propagation rounds. Not a quality knob: the loop exits as soon
 * as a round bounds nothing new. Sweep in docs/tolerances.md (D91). */
constexpr int64_t IMPLIED_ROUNDS = 64;

/* The bounds the constraints imply for a variable the model left unbounded
 * (D87). The bound is implied, not imposed: every feasible point already
 * satisfies it, so adding it changes neither the feasible region nor `P*`.
 * Where the rows imply nothing the bound stays infinite and the term is
 * still dropped. Terms that are infinite are counted rather than summed. */
static void implied_bounds(const jaos_model *m, double *cl, double *cu,
                           double *lo_sum, double *lo_comp,
                           double *up_sum, double *up_comp,
                           int64_t *lo_inf, int64_t *up_inf)
{
    const int64_t nr = m->num_row, nc = m->num_col;

    for (int64_t j = 0; j < nc; j++) {
        cl[j] = m->col_lower[j];
        cu[j] = m->col_upper[j];
    }

    /* Iterated (D91). Only ever tightened, never loosened: the sequence is
     * monotone, so stopping early is safe. */
    for (int64_t pass = 0; pass < IMPLIED_ROUNDS; pass++) {
    bool moved = false;

    for (int64_t i = 0; i < nr; i++) {
        lo_sum[i] = 0.0;
        lo_comp[i] = 0.0;
        up_sum[i] = 0.0;
        up_comp[i] = 0.0;
        lo_inf[i] = 0;
        up_inf[i] = 0;
    }

    /* Each row's activity range over the column boxes, infinities counted. */
    for (int64_t j = 0; j < nc; j++) {
        const double xl = cl[j], xu = cu[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double aij = m->a_value[k];
            const double t_lo = aij > 0.0 ? xl : xu;
            const double t_up = aij > 0.0 ? xu : xl;
            if (isfinite(t_lo)) {
                const double p = aij * t_lo;
                const double e = jm_two_product_residue(aij, t_lo, p);
                jm_obj_add(&lo_sum[i], &lo_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&lo_sum[i], &lo_comp[i], e);
            } else {
                lo_inf[i]++;
            }
            if (isfinite(t_up)) {
                const double p = aij * t_up;
                const double e = jm_two_product_residue(aij, t_up, p);
                jm_obj_add(&up_sum[i], &up_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&up_sum[i], &up_comp[i], e);
            } else {
                up_inf[i]++;
            }
        }
    }

    for (int64_t j = 0; j < nc; j++) {
        const bool want_lo = !isfinite(cl[j]), want_up = !isfinite(cu[j]);
        if (!want_lo && !want_up)
            continue;

        const double xl = cl[j], xu = cu[j];
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
            /* This column's term comes back out with the residue it went
             * in with, so what is left is the rest of the row and not the
             * rest plus one rounding of this column (D270). */
            const double own_lo = isfinite(t_lo) ? aij * t_lo : 0.0;
            const double own_up = isfinite(t_up) ? aij * t_up : 0.0;
            const double own_lo_e = isfinite(t_lo)
                ? jm_two_product_residue(aij, t_lo, own_lo) : 0.0;
            const double own_up_e = isfinite(t_up)
                ? jm_two_product_residue(aij, t_up, own_up) : 0.0;
            const double rest_lo =
                acc_difference(lo_sum[i], lo_comp[i], own_lo, own_lo_e);
            const double rest_up =
                acc_difference(up_sum[i], up_comp[i], own_up, own_up_e);

            /* a_ij x_j <= ru - min(rest)  and  a_ij x_j >= rl - max(rest).
             * Dividing by a_ij swaps the two when it is negative. */
            /* `isfinite(lim)`, because the range sums are `double` now and
             * can reach an infinity a `long double` could not (D277). A
             * `-inf` written into `cu[j]` passes the monotonicity assert
             * below -- everything is <= +inf -- and leaves a box no point
             * is inside. An infinite limit bounds nothing, which is what
             * the column already had. */
            if (rest_lo_finite && isfinite(m->row_upper[i])) {
                const double lim = (m->row_upper[i] - rest_lo) / aij;
                if (!isfinite(lim)) {
                    /* nothing to tighten with */
                } else if (aij > 0.0) {
                    if (want_up && lim < cu[j]) {
                        cu[j] = lim;
                        moved = true;
                    }
                } else if (want_lo && lim > cl[j]) {
                    cl[j] = lim;
                    moved = true;
                }
            }
            if (rest_up_finite && isfinite(m->row_lower[i])) {
                const double lim = (m->row_lower[i] - rest_up) / aij;
                if (!isfinite(lim)) {
                    /* nothing to tighten with */
                } else if (aij > 0.0) {
                    if (want_lo && lim > cl[j]) {
                        cl[j] = lim;
                        moved = true;
                    }
                } else if (want_up && lim < cu[j]) {
                    cu[j] = lim;
                    moved = true;
                }
            }
        }
    }

    if (!moved)
        break;   /* a round that bounded nothing new cannot enable another */
    }

    /* "Only ever tightened, never loosened", checked rather than written
     * (D219). A loosened bound would enlarge the feasible region the gap is
     * measured over, and the verdict would certify a point the model does
     * not contain. Holds by construction: a bound is written only where it
     * was infinite, and then only to a strictly tighter value. */
#ifndef NDEBUG
    for (int64_t j = 0; j < nc; j++) {
        assert(cl[j] >= m->col_lower[j]);
        assert(cu[j] <= m->col_upper[j]);
    }
#endif
}

/* Sign-condition violation for a multiplier w attached to a value v with
 * bounds [lo, hi], in minimize-canonical form:
 *   at lower  -> w >= 0
 *   at upper  -> w <= 0
 *   interior  -> w == 0   (this is complementary slackness)
 *   fixed     -> anything
 * "At a bound" is judged within tol * scale, and a multiplier at or below
 * tol is held to no condition at all. `scale` is what the value being
 * tested is made of (D23). A value waived here at a distance d with a
 * multiplier w contributes w * d to the gap, which is checked separately.
 * Also accumulates the multiplier's contribution to the dual objective: w
 * picks the bound its sign points at, and every multiplier contributes,
 * including the ones exempt from the condition (D22), split by sign (D24). */
static double sign_condition(double v, double lo, double hi, double w,
                             double tol, double scale, dual_acc *a,
                             bool lo_implied, bool hi_implied)
{
    double window = tol * scale;
    bool at_lo = isfinite(lo) && v <= lo + window;
    bool at_hi = isfinite(hi) && v >= hi - window;

    bool negligible = fabs(w) <= tol;

    if (w > 0.0) {
        if (!isfinite(lo)) {
            note_dropped(a, w);
            return negligible ? 0.0 : w;
        }
        const double c = w * lo;
        jm_obj_add(&a->dual_obj, &a->dual_objc, c);
        const double ce = jm_two_product_residue(w, lo, c);
        if (ce != 0.0)
            jm_obj_add(&a->dual_obj, &a->dual_objc, ce);
        double e;
        const double t = bound_term(w, v, lo, &e);
        split_term(t, e, &a->pos, &a->posc, &a->neg, &a->negc);
        if (!lo_implied)
            split_term(t, e, &a->pos_model, &a->pos_modelc,
                       &a->neg_model, &a->neg_modelc);
        return (negligible || at_lo) ? 0.0 : w;
    }
    if (w < 0.0) {
        if (!isfinite(hi)) {
            note_dropped(a, w);
            return negligible ? 0.0 : -w;
        }
        const double c = w * hi;
        jm_obj_add(&a->dual_obj, &a->dual_objc, c);
        const double ce = jm_two_product_residue(w, hi, c);
        if (ce != 0.0)
            jm_obj_add(&a->dual_obj, &a->dual_objc, ce);
        double e;
        const double t = bound_term(w, v, hi, &e);
        split_term(t, e, &a->pos, &a->posc, &a->neg, &a->negc);
        if (!hi_implied)
            split_term(t, e, &a->pos_model, &a->pos_modelc,
                       &a->neg_model, &a->neg_modelc);
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
     *
     * Every walk in this file is compensated in `double`, and none of them
     * accumulates in `long double` (D270, D277). Two reasons, and the first
     * is the one that had been missed. `long double` is 64 mantissa bits on
     * x86-64 and 113 on aarch64, so a figure this file publishes differs
     * between machines -- and these figures go into `bench/results/`, which
     * the gate reads. The solver refuses `long double` for exactly that
     * reason (D162, D168) and the checker did not. The second reason is
     * accuracy: a Neumaier sum loses nothing in the accumulation and
     * Dekker's split keeps each product whole, where 64 mantissa bits
     * cannot hold a binary64 product at all (D262).
     *
     * The primal walk went first, alone, so that one campaign judged one
     * thing (D270). D277 finished the rest: the dual walk's `dual_obj`, its
     * four `pos`/`neg` halves and `certified`, the reduced cost, the two
     * range sums in `implied_bounds`, and both certificate checkers. The
     * dual half is the one that DECIDES rather than reports -- a bound
     * `implied_bounds` tightens sets `sign_condition`'s window, which
     * reaches `dual_feasible` -- so on aarch64 a bound could land on the
     * other side of that window and flip a verdict. There is no type in
     * this file whose width depends on the machine now.
     *
     * What this costs, everywhere: the intermediate range drops from about
     * 1.2e4932 to 1.8e308. Three places reach it, and none of them is
     * anywhere near it on any gate instance.
     *
     * A row activity holding `1e200 * 1e200` used to reach a finite figure
     * if a matching negative term followed; it reaches `+inf` now and cannot
     * come back, because `inf + t` is `inf` for every finite `t`. The verdict
     * is safe: an infinite activity fails `primal_feasible`.
     *
     * `certified_step`'s `room` is a subtraction of two doubles and can
     * overflow where the wider type held it -- an activity at 1.4e308 inside
     * a row bound at -1.5e308. The row then blocks nothing, `certified_step`
     * returns `HUGE_VAL`, and `certified_suboptimality` publishes an
     * infinity where the wider type published about 2.9e305. Both are
     * "enormous", the figure decides nothing (D47, D73), and it is
     * published, so it is written down here rather than repaired.
     *
     * `implied_bounds`'s two range sums can reach an infinity, and that one
     * IS repaired, because a bound it writes decides: see the
     * `isfinite(lim)` guard there.
     *
     * Accepted as the price of the portability. */
    double *acts = jm_calloc_array(m->num_row, sizeof(double));
    double *actc = jm_calloc_array(m->num_row, sizeof(double));
    /* The sum of the magnitudes of each activity's terms: its scale. */
    double *traffics = jm_calloc_array(m->num_row, sizeof(double));
    double *trafficc = jm_calloc_array(m->num_row, sizeof(double));
    if (acts == nullptr || actc == nullptr ||
        traffics == nullptr || trafficc == nullptr) {
        free(acts);
        free(actc);
        free(traffics);
        free(trafficc);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        const double xj = col_value[j];
        if (xj == 0.0)
            continue;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double t = m->a_value[k] * xj;
            const double e = jm_two_product_residue(m->a_value[k], xj, t);
            jm_obj_add(&acts[i], &actc[i], t);
            if (e != 0.0)
                jm_obj_add(&acts[i], &actc[i], e);
            /* The scale carries the rounded term only, so it is short by at
             * most half an ulp of each term. It is not a reported figure
             * alone: it is D23's window, `tol * scale` in `sign_condition`,
             * so a value sitting within nnz ulps of the window's edge could
             * land on the other side of it. Nothing on the gate does. */
            jm_obj_add(&traffics[i], &trafficc[i], fabs(t));
        }
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        acts[i] = acc_value(acts[i], actc[i]);
        traffics[i] = acc_value(traffics[i], trafficc[i]);
    }
    free(actc);
    free(trafficc);
    /* The names the rest of the file reads, declared after the fold on
     * purpose. A reader placed above this line does not compile, which is
     * the only enforcement C offers that the halves are never read apart
     * (D270). */
    const double *const act = acts;
    const double *const traffic = traffics;

    /* Primal side. */
    double col_viol = 0.0, row_viol = 0.0, row_viol_rel = 0.0;
    double primal_obj = m->obj_offset, primal_objc = 0.0;
    for (int64_t j = 0; j < m->num_col; j++) {
        col_viol = max2(col_viol, interval_violation(col_value[j],
                                    m->col_lower[j], m->col_upper[j]));
        const double c = m->col_cost[j], x = col_value[j];
        /* No skip on a zero factor. `0.0 * inf` is a NaN, and that NaN
         * reaching the objective is the only signal an infinite value in a
         * zero-cost column leaves: the column violates no bound, so
         * `col_viol` says nothing about it (D270). Adding an exact zero to
         * a Neumaier accumulator is free, so the skip bought nothing. */
        const double t = c * x;
        jm_obj_add(&primal_obj, &primal_objc, t);
        const double e = jm_two_product_residue(c, x, t);
        if (e != 0.0)
            jm_obj_add(&primal_obj, &primal_objc, e);
    }
    /* The pair is worth nothing read half at a time. Folded once, here,
     * because the dual side reads it twice more (D270). */
    const double pobj = acc_value(primal_obj, primal_objc);
    for (int64_t i = 0; i < m->num_row; i++) {
        double viol = interval_violation(act[i], m->row_lower[i],
                                         m->row_upper[i]);
        row_viol = max2(row_viol, viol);
        /* The same residue against what the row is made of. The predicate
         * itself stays absolute (D24); this figure is reported. */
        row_viol_rel = max2(row_viol_rel,
                            viol / max2(1.0, traffic[i]));
    }

    /* Integrality (D288): the distance to the nearest integer, judged like
     * a bound. A model with no integer column reads 0 here. */
    double int_viol = 0.0;
    if (m->col_integer != nullptr)
        for (int64_t j = 0; j < m->num_col; j++)
            if (m->col_integer[j]) {
                const double f = fabs(col_value[j] - round(col_value[j]));
                if (f > int_viol)
                    int_viol = f;
            }
    out->max_integrality_violation = int_viol;

    out->max_col_violation = col_viol;
    out->max_row_violation = row_viol;
    out->max_row_violation_relative = row_viol_rel;
    out->primal_objective = pobj;
    out->primal_feasible = col_viol <= tol && row_viol <= tol &&
                           int_viol <= tol;

    /* Dual side, minimize-canonical. */
    if (row_dual != nullptr) {
        const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
        double dual_viol = 0.0;
        dual_acc a = {0};
        a.dual_obj = sigma * m->obj_offset;   /* canonical offset */

        /* What the constraints imply about the variables the model left
         * unbounded (D87). Allocation failure is not fatal: the bounds fall
         * back to the model's own. A row's own range comes free with them. */
        double *icl = jm_calloc_array(m->num_col, sizeof(double));
        double *icu = jm_calloc_array(m->num_col, sizeof(double));
        /* Each range sum is a compensated pair, so it needs a second array
         * beside it. They are read after the call as well as during it, so
         * the corrections have to outlive `implied_bounds` (D277). */
        double *rlo = jm_calloc_array(m->num_row, sizeof(double));
        double *rloc = jm_calloc_array(m->num_row, sizeof(double));
        double *rup = jm_calloc_array(m->num_row, sizeof(double));
        double *rupc = jm_calloc_array(m->num_row, sizeof(double));
        int64_t *rli = jm_calloc_array(m->num_row, sizeof(int64_t));
        int64_t *rui = jm_calloc_array(m->num_row, sizeof(int64_t));
        const bool implied = icl != nullptr && icu != nullptr &&
                             rlo != nullptr && rloc != nullptr &&
                             rup != nullptr && rupc != nullptr &&
                             rli != nullptr && rui != nullptr;
        if (implied)
            implied_bounds(m, icl, icu, rlo, rloc, rup, rupc, rli, rui);

        for (int64_t i = 0; i < m->num_row; i++) {
            double rl = m->row_lower[i], ru = m->row_upper[i];
            bool rl_imp = false, ru_imp = false;
            if (implied) {
                if (!isfinite(rl) && rli[i] == 0) {
                    rl = acc_value(rlo[i], rloc[i]);
                    rl_imp = true;
                }
                if (!isfinite(ru) && rui[i] == 0) {
                    ru = acc_value(rup[i], rupc[i]);
                    ru_imp = true;
                }
            }
            dual_viol = max2(dual_viol,
                sign_condition(act[i], rl, ru, sigma * row_dual[i],
                               tol, max2(1.0, traffic[i]), &a,
                               rl_imp, ru_imp));
        }

        for (int64_t j = 0; j < m->num_col; j++) {
            /* Reduced cost d_j = c_j - a_j' y, canonicalized. */
            double dw = m->col_cost[j], dwc = 0.0;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                const double aij = m->a_value[k];
                const double y = row_dual[m->a_index[k]];
                const double p = aij * y;
                const double e = jm_two_product_residue(aij, y, p);
                jm_obj_add(&dw, &dwc, -p);
                if (e != 0.0)
                    jm_obj_add(&dw, &dwc, -e);
            }
            const double d = acc_value(dw, dwc);
            /* A column value is one published number, so its scale is its
             * own magnitude. */
            dual_viol = max2(dual_viol,
                sign_condition(col_value[j],
                               implied ? icl[j] : m->col_lower[j],
                               implied ? icu[j] : m->col_upper[j],
                               sigma * d, tol, max2(1.0, fabs(col_value[j])),
                               &a,
                               implied && !isfinite(m->col_lower[j]),
                               implied && !isfinite(m->col_upper[j])));

            /* Where that call had to drop a term, this says what the drop
             * is worth. Rows are not done: no single-entity direction (D73). */
            const double w = sigma * d;
            const bool drops = (w > 0.0 && !isfinite(m->col_lower[j])) ||
                               (w < 0.0 && !isfinite(m->col_upper[j]));
            if (drops) {
                const double t = certified_step(m, j, w > 0.0 ? -1.0 : 1.0,
                                                act);
                if (isinf(t) && fabs(w) <= tol) {
                    /* An unbounded ray whose rate this checker calls zero.
                     * Counted, not certified (see dual_acc). */
                    a.rays++;
                } else {
                    const double gain = fabs(w) * t;
                    if (gain > a.certified)
                        a.certified = gain;
                }
            }
        }

        /* Two gaps (D91). The one the verdict reads is over bounds the model
         * declared, where every term must vanish at an optimum. The other
         * includes the implied bounds and is what bounds the suboptimality. */
        /* All four are magnitudes, which is what makes their difference the
         * gap rather than a sum with a sign error in it (D219). `split_term`
         * negates the negative half on the way in, so a sign fault there
         * would read as a plausible smaller gap and nothing else. */
        assert(a.pos >= 0.0 && a.neg >= 0.0);
        assert(a.pos_model >= 0.0 && a.neg_model >= 0.0);
        const double true_dual_obj = sigma * acc_value(a.dual_obj, a.dual_objc);
        const double scale = 1.0 + fabs(pobj) + fabs(true_dual_obj);
        /* The two halves are close and their difference is the answer, so
         * the corrections go through the subtraction rather than into each
         * half first: folded first, the part that survives the cancellation
         * is rounded away before it is used (D277). */
        const double gap =
            fabs(acc_difference(a.pos_model, a.pos_modelc,
                                a.neg_model, a.neg_modelc)) / scale;

        out->checked_duals = true;
        out->max_dual_violation = dual_viol;
        out->dual_objective = true_dual_obj;
        out->objective_gap = gap;

        /* The suboptimality bound, relative to the objective (D47). */
        out->relative_suboptimality =
            acc_value(a.pos, a.posc) / (1.0 + fabs(pobj));
        /* Whether the identity the two halves come from was complete (D47). */
        out->max_dropped_multiplier = a.dropped_max;
        out->dropped_terms = a.dropped_n;
        out->gap_certified = a.dropped_n == 0;
        out->certified_suboptimality = a.certified;
        out->unquantified_rays = a.rays;
        /* In the objective's own units: they are for P - P* <= gap_positive. */
        out->gap_positive = acc_value(a.pos, a.posc);
        out->gap_negative = acc_value(a.neg, a.negc);
        out->dual_feasible = dual_viol <= tol && gap <= tol;

        free(icl);
        free(icu);
        free(rlo);
        free(rloc);
        free(rup);
        free(rupc);
        free(rli);
        free(rui);
    }

    free(acts);
    free(traffics);
    return JAOS_OK;
}

/* Judges a claimed infeasibility certificate against the model as loaded:
 * original space, the model's own bounds, no solver bookkeeping (D18).
 * The claim is an impossibility: the smallest value the row bounds allow
 * y'(Ax) to take still exceeds the largest value the column bounds allow
 * (A'y)'x to take — and the two are the same number for any x, so no
 * feasible x exists. A ray that needs an infinite bound side makes its
 * sum infinite and the proof dies, whatever produced the ray (D254). */
jaos_status jaos_check_certificate(const jaos_model *m,
    const double *row_ray, double tol, jaos_certificate_report *out)
{
    if (m == nullptr || row_ray == nullptr || out == nullptr ||
        !isfinite(tol) || tol < 0.0)
        return JAOS_ERR_INVALID_INPUT;
    out->sup_columns = 0.0;
    out->inf_rows = 0.0;
    out->gap = 0.0;
    out->certified = false;

    for (int64_t i = 0; i < m->num_row; i++)
        if (!isfinite(row_ray[i]))
            return JAOS_ERR_INVALID_INPUT;

    bool bounded = true;
    double sup_cols = 0.0, sup_colsc = 0.0;
    for (int64_t j = 0; j < m->num_col && bounded; j++) {
        double asum = 0.0, acomp = 0.0, traffic = 0.0, trafficc = 0.0;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const double aij = m->a_value[p];
            const double y = row_ray[m->a_index[p]];
            const double t = aij * y;
            const double e = jm_two_product_residue(aij, y, t);
            jm_obj_add(&asum, &acomp, t);
            if (e != 0.0)
                jm_obj_add(&asum, &acomp, e);
            /* The traffic carries the rounded term only. It is the window
             * this sum is read against, not a published figure. */
            jm_obj_add(&traffic, &trafficc, fabs(t));
        }
        const double a = acc_value(asum, acomp);
        const double traf = acc_value(traffic, trafficc);
        /* How precisely this sum can be placed is set by the terms that
         * went into it (the same rule the bound-rest test above states):
         * below tol times its own traffic, (A'y)_j is a zero the
         * arithmetic cannot distinguish — read literally against an
         * infinite bound it would turn roundoff on a structurally zero
         * column into an infinite sup (D254). */
        if (fabs(a) <= tol * traf)
            continue;
        if (a > 0.0) {
            if (isfinite(m->col_upper[j]))
                add_product(&sup_cols, &sup_colsc, a, m->col_upper[j]);
            else
                bounded = false;
        } else if (a < 0.0) {
            if (isfinite(m->col_lower[j]))
                add_product(&sup_cols, &sup_colsc, a, m->col_lower[j]);
            else
                bounded = false;
        }
    }

    double inf_rows = 0.0, inf_rowsc = 0.0;
    for (int64_t i = 0; i < m->num_row && bounded; i++) {
        const double y = row_ray[i];
        if (y > 0.0) {
            if (isfinite(m->row_lower[i]))
                add_product(&inf_rows, &inf_rowsc, y, m->row_lower[i]);
            else
                bounded = false;
        } else if (y < 0.0) {
            if (isfinite(m->row_upper[i]))
                add_product(&inf_rows, &inf_rowsc, y, m->row_upper[i]);
            else
                bounded = false;
        }
    }

    if (!bounded) {
        /* The sup (or inf) really is infinite; published as such, and
         * the gap with it, so the report says which side died. */
        out->sup_columns = INFINITY;
        out->inf_rows = -INFINITY;
        out->gap = -INFINITY;
        return JAOS_OK;
    }

    out->sup_columns = acc_value(sup_cols, sup_colsc);
    out->inf_rows = acc_value(inf_rows, inf_rowsc);
    /* The two sides of the proof are close when the certificate is a near
     * miss, and their difference is the whole verdict, so the corrections
     * go through the subtraction (D277). */
    out->gap = acc_difference(inf_rows, inf_rowsc, sup_cols, sup_colsc);
    out->certified = out->gap >
        tol * (1.0 + fabs(out->sup_columns) + fabs(out->inf_rows));
    return JAOS_OK;
}

/* Judges a claimed unbounded ray against the model as loaded: original
 * space, the model's own bounds, no solver bookkeeping (D18). The claim
 * has three parts, and all three are read from the model alone: every
 * moving column points past an infinite bound side, every moving row —
 * (Ad)_i, a sum, counted only above tol times its own traffic — points
 * past an infinite row side, and the objective improves along d at a
 * rate that stands above tol times the cost terms' own size (D255). */
jaos_status jaos_check_ray(const jaos_model *m, const double *col_ray,
                           double tol, jaos_ray_report *out)
{
    if (m == nullptr || col_ray == nullptr || out == nullptr ||
        !isfinite(tol) || tol < 0.0)
        return JAOS_ERR_INVALID_INPUT;
    out->rate = 0.0;
    out->max_col_escape = 0.0;
    out->max_row_escape = 0.0;
    out->certified = false;

    for (int64_t j = 0; j < m->num_col; j++)
        if (!isfinite(col_ray[j]))
            return JAOS_ERR_INVALID_INPUT;

    /* Column direction against the column boxes. */
    for (int64_t j = 0; j < m->num_col; j++) {
        const double d = col_ray[j];
        if (d > 0.0 && isfinite(m->col_upper[j])) {
            if (d > out->max_col_escape)
                out->max_col_escape = d;
        } else if (d < 0.0 && isfinite(m->col_lower[j])) {
            if (-d > out->max_col_escape)
                out->max_col_escape = -d;
        }
    }

    /* Row movement, accumulated column-wise, with the traffic that says
     * how finely each sum can be read. */
    /* Compensated, both of them, and each needs a second array beside it.
     * `move` and `traf` are not reported figures: they are the test at the
     * bottom of this loop, and that test decides `max_row_escape`, which
     * decides `certified` (D277). */
    const size_t nrow = (size_t)(m->num_row > 0 ? m->num_row : 1);
    double *move = calloc(nrow, sizeof *move);
    double *movec = calloc(nrow, sizeof *movec);
    double *traf = calloc(nrow, sizeof *traf);
    double *trafc = calloc(nrow, sizeof *trafc);
    if (move == nullptr || movec == nullptr ||
        traf == nullptr || trafc == nullptr) {
        free(move);
        free(movec);
        free(traf);
        free(trafc);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        const double d = col_ray[j];
        if (d == 0.0)
            continue;
        for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
            const int64_t i = m->a_index[p];
            const double aij = m->a_value[p];
            add_product(&move[i], &movec[i], aij, d);
            jm_obj_add(&traf[i], &trafc[i], fabs(aij * d));
        }
    }
    for (int64_t i = 0; i < m->num_row; i++) {
        const double r = acc_value(move[i], movec[i]);
        if (fabs(r) <= tol * acc_value(traf[i], trafc[i]))
            continue;
        if (r > 0.0 && isfinite(m->row_upper[i])) {
            if (r > out->max_row_escape)
                out->max_row_escape = r;
        } else if (r < 0.0 && isfinite(m->row_lower[i])) {
            if (-r > out->max_row_escape)
                out->max_row_escape = -r;
        }
    }
    free(move);
    free(movec);
    free(traf);
    free(trafc);

    /* The rate, in the model's own sense, judged canonically. */
    double rate = 0.0, ratec = 0.0, ctraf = 0.0, ctrafc = 0.0;
    for (int64_t j = 0; j < m->num_col; j++) {
        const double c = m->col_cost[j], d = col_ray[j];
        const double t = c * d;
        const double e = jm_two_product_residue(c, d, t);
        jm_obj_add(&rate, &ratec, t);
        if (e != 0.0)
            jm_obj_add(&rate, &ratec, e);
        jm_obj_add(&ctraf, &ctrafc, fabs(t));
    }
    out->rate = acc_value(rate, ratec);
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    const bool improves =
        sigma * out->rate < -tol * (1.0 + acc_value(ctraf, ctrafc));
    out->certified = improves && out->max_col_escape == 0.0 &&
                     out->max_row_escape == 0.0;
    return JAOS_OK;
}
