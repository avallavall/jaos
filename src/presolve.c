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
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <assert.h>
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
    double *cur_cl = jm_alloc_array(nc, sizeof *cur_cl);
    double *cur_cu = jm_alloc_array(nc, sizeof *cur_cu);
    double *cur_rl = jm_alloc_array(nr, sizeof *cur_rl);
    double *cur_ru = jm_alloc_array(nr, sizeof *cur_ru);
    int64_t *col_deg = jm_alloc_array(nc, sizeof *col_deg);
    int64_t *row_deg = jm_alloc_array(nr, sizeof *row_deg);
    ps_rowwise rw = {0};

    bool ok = col_dead && row_dead && row_frozen && cur_cl && cur_cu &&
              cur_rl && cur_ru && col_deg && row_deg &&
              ps_build_rowwise(m, &rw);

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
    /* Bounded by num_row + num_col: every round that changes anything
     * removes at least one row or column, so no round can be productive
     * more than that many times (T-02-10). D-02's own, *measured* cap
     * lands on top of this structural one in 02-04 — this is the
     * correctness backstop, not the tuned value. */
    const int64_t cap = nr + nc + 1;
    int64_t rounds_done = 0;

    for (int64_t round = 0; round < cap; round++) {
        bool changed = false;

        /* --- Row pass: empty and singleton rows. --------------------- */
        for (int64_t i = 0; i < nr; i++) {
            if (row_dead[i] || row_frozen[i])
                continue;

            if (row_deg[i] == 0) {
                /* Exact comparison, on purpose — see the singleton-row
                 * intersection check below for why: this plan does not
                 * borrow a tolerance from either existing space, and
                 * 02-04 is where presolve's own is measured. */
                if (0.0 < cur_rl[i] || 0.0 > cur_ru[i]) {
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
                const double new_lo = tightens_lo ? implied_lo : cur_cl[j];
                const double new_hi = tightens_hi ? implied_hi : cur_cu[j];

                jm_work_add(w, JM_WORK_NONZERO);

                /* Exact comparison, on purpose: presolve's own tolerance
                 * space is a new one (D-04, unscaled model space), and
                 * 02-04 owns the sweep that measures it. Until that
                 * constant exists, an intersection that has collapsed at
                 * all is infeasible — no epsilon is borrowed from either
                 * of the two tolerance spaces that already exist
                 * (PRIMAL_TOL/DUAL_TOL, scaled; the checker's caller-
                 * supplied tol, original but diagnostic), since neither
                 * was measured for this decision. */
                if (new_lo > new_hi) {
                    p->outcome = JM_PRESOLVE_INFEASIBLE;
                    goto done;
                }

                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_SINGLETON_ROW,
                        .index = i, .index2 = j, .coef = a,
                        .row_tightens_lo = tightens_lo,
                        .row_tightens_hi = tightens_hi })) {
                    ret = JAOS_ERR_OUT_OF_MEMORY;
                    goto cleanup_scratch;
                }
                cur_cl[j] = new_lo;
                cur_cu[j] = new_hi;
                row_dead[i] = true;
                col_deg[j]--;
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

    if (p->counts.rounds == 0 && rcol == nc && rrow == nr) {
        /* Nothing fired at all: NONE, exactly as before this plan (no
         * reduced model built, publish() takes the un-presolved path). */
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
    free(cur_cl); free(cur_cu); free(cur_rl); free(cur_ru);
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
static void ps_replay_one(jaos_model *orig, const jm_presolve_rec *rec)
{
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
        const jaos_basis_status st = orig->sol_col_status[j];
        double y_i;
        if (st == JAOS_BASIS_BASIC || st == JAOS_BASIS_FREE) {
            y_i = 0.0;
            orig->sol_redcost[j] = 0.0;
        } else if ((st == JAOS_BASIS_AT_LOWER && rec->row_tightens_lo) ||
                  (st == JAOS_BASIS_AT_UPPER && rec->row_tightens_hi)) {
            /* x_j rests at the bound the ROW induced, tighter than (or
             * equal to) x_j's own original bound on that side. In the
             * ORIGINAL problem that bound does not exist, so x_j is
             * interior there — publishing AT_LOWER/AT_UPPER as the reduced
             * solve reported it would claim a bound the original column
             * never had; the row-count invariant a caller reads jaos_basis
             * against would be counting a genuinely interior variable as
             * nonbasic. */
            y_i = orig->sol_redcost[j] / rec->coef;
            orig->sol_redcost[j] = 0.0;
            orig->sol_col_status[j] = JAOS_BASIS_BASIC;
        } else {
            y_i = 0.0;
            /* orig->sol_redcost[j] already holds d'_j; status is already
             * whichever of AT_LOWER/AT_UPPER matches x_j's own original
             * bound, unchanged. */
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

    /* Strictly LIFO (D-07). */
    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, &p->arena[r]);

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
        ps_replay_one(orig, &p->arena[r]);

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
