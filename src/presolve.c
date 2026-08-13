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
 * This plan ships one reduction: a column whose bounds already arrived
 * equal. It removes the column, folds its cost into the objective offset
 * and its matrix contribution into every row it touched, and proves the
 * whole arena/postsolve machinery end to end while the model is otherwise
 * unchanged — D-01's rationale for choosing the scaffolding over a
 * reduction that removes more.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <assert.h>
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
 * JM_PS_FIXED_COL record's restore index reads one past where it actually
 * belongs. Compiled to the identity in every build that is not this one.
 * It exists so the round-trip test's positive result can be shown to
 * depend on the index actually being right, rather than on the checker
 * merely being unable to tell the difference -- see tests/test_presolve.c
 * for where this is caught and by which report field. Reached from both
 * postsolve replay loops, since a record's index means the same thing to
 * either one. */
static int64_t fixed_col_restore_index(const jm_presolve_rec *rec)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    return rec->index + 1;
#else
    return rec->index;
#endif
}

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

JAOS_NODISCARD jaos_status jm_presolve_run(const jaos_model *m, jm_presolve *p,
                                           jm_work *w)
{
    (void)w;   /* presolve's own cost is billed starting 02-02 */

    /* This plan's one family: a column whose bounds arrived exactly equal.
     * No bound tightening happens here — a column becomes fixed only by
     * being loaded that way — so one ascending pass over m's own columns is
     * the whole of the reduction (D-07: ascending is the only order there
     * is, and later families' cascades inherit it rather than inventing
     * their own). */
    int64_t n_fixed = 0;
    for (int64_t j = 0; j < m->num_col; j++)
        if (m->col_lower[j] == m->col_upper[j])
            n_fixed++;

    if (n_fixed == 0) {
        p->outcome = JM_PRESOLVE_NONE;
        return JAOS_OK;
    }

    const int64_t rcol = m->num_col - n_fixed;
    const int64_t rrow = m->num_row;

    p->col_map  = jm_alloc_array(m->num_col, sizeof *p->col_map);
    p->row_map  = jm_alloc_array(rrow, sizeof *p->row_map);
    p->orig_col = jm_alloc_array(rcol, sizeof *p->orig_col);
    p->orig_row = jm_alloc_array(rrow, sizeof *p->orig_row);
    if (!p->col_map || !p->row_map || !p->orig_col || !p->orig_row)
        return JAOS_ERR_OUT_OF_MEMORY;

    /* Identity: this reduction never removes a row. Filled anyway so a
     * row-removing reduction has somewhere to write starting next plan,
     * rather than this plan leaving the field meaningfully unusable. */
    for (int64_t i = 0; i < rrow; i++) {
        p->row_map[i] = i;
        p->orig_row[i] = i;
    }

    /* Struct-copy first so cfg, sense, the log callback and the tolerances
     * carry over — then every pointer field that would otherwise alias m
     * is overwritten below, before anything reads it. Nothing here writes
     * to m (D-06): every array `reduced` ends up with is its own
     * allocation, never a pointer borrowed from the caller's model. */
    p->reduced = *m;
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
    if (!p->reduced.col_cost || !p->reduced.col_lower ||
        !p->reduced.col_upper || !p->reduced.row_lower ||
        !p->reduced.row_upper || !p->reduced.a_start)
        return JAOS_ERR_OUT_OF_MEMORY;

    /* Plain double, deliberately, after a long double accumulator was
     * tried here and measured out (02-01-SUMMARY.md): it did not change
     * finnis's residual at all -- confirming the gap between the two
     * builds is a different pivot path on a genuinely reduced problem, not
     * lost precision in this shift -- and it roughly doubled pilot87's
     * iteration count (50850 -> 117653 on the standard set, `netlib`
     * campaign, both builds otherwise identical). A change with no
     * measured benefit and a measured cost on the set's most re-entry-
     * sensitive instance (D74, D89, D92) is not kept on the theory that it
     * should help. */
    for (int64_t i = 0; i < rrow; i++) {
        p->reduced.row_lower[i] = m->row_lower[i];
        p->reduced.row_upper[i] = m->row_upper[i];
    }

    /* Pass 1, ascending j: classify every column. A fixed one's cost times
     * its value goes into the objective offset, its matrix contribution
     * shifts every row it touches (a finite bound minus a finite amount
     * stays finite; an infinite one stays infinite in IEEE arithmetic, so
     * no branch is needed to keep the shift only on finite bounds), and one
     * JM_PS_FIXED_COL record is pushed. A surviving column just gets its
     * new, dense index. */
    int64_t rj = 0;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (m->col_lower[j] != m->col_upper[j]) {
            p->col_map[j] = rj;
            p->orig_col[rj] = j;
            rj++;
            continue;
        }

        p->col_map[j] = -1;
        const double v = m->col_lower[j];
        p->reduced.obj_offset += m->col_cost[j] * v;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            p->reduced.row_lower[i] -= m->a_value[k] * v;
            p->reduced.row_upper[i] -= m->a_value[k] * v;
        }

        if (!JM_GROW(p->arena, p->arena_cap, p->arena_len + 1))
            return JAOS_ERR_OUT_OF_MEMORY;
        jm_presolve_rec *rec = &p->arena[p->arena_len++];
        rec->tag = JM_PS_FIXED_COL;
        rec->index = j;
        rec->value = v;
        rec->cost = m->col_cost[j];
        p->counts.fixed_col++;
    }
    p->counts.rounds = 1;
    assert(rj == rcol);

    /* Pass 2: the reduced CSC prefix, one surviving column's worth of
     * nonzeros at a time, in the same order orig_col already fixed. */
    p->reduced.a_start[0] = 0;
    for (int64_t rj2 = 0; rj2 < rcol; rj2++) {
        const int64_t j = p->orig_col[rj2];
        p->reduced.col_cost[rj2]  = m->col_cost[j];
        p->reduced.col_lower[rj2] = m->col_lower[j];
        p->reduced.col_upper[rj2] = m->col_upper[j];
        p->reduced.a_start[rj2 + 1] =
            p->reduced.a_start[rj2] + (m->a_start[j + 1] - m->a_start[j]);
    }
    p->reduced.num_nz = p->reduced.a_start[rcol];

    p->reduced.a_index = jm_alloc_array(p->reduced.num_nz, sizeof(int64_t));
    p->reduced.a_value = jm_alloc_array(p->reduced.num_nz, sizeof(double));
    if (!p->reduced.a_index || !p->reduced.a_value)
        return JAOS_ERR_OUT_OF_MEMORY;

    /* Pass 3: the entries themselves, verbatim — row indices are untouched
     * because this reduction never removes a row. */
    for (int64_t rj2 = 0; rj2 < rcol; rj2++) {
        const int64_t j = p->orig_col[rj2];
        int64_t dst = p->reduced.a_start[rj2];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++, dst++) {
            p->reduced.a_index[dst] = m->a_index[k];
            p->reduced.a_value[dst] = m->a_value[k];
        }
    }

    /* Mapped into reduced indices before build_warm_basis ever reads it
     * (D-08). A fixed column recorded basic has no reduced counterpart to
     * carry that status; dropping it undercounts the basic total, and
     * build_warm_basis already falls back to the slack basis whenever the
     * count is short of nrow — safe, never wrong, only colder than a fuller
     * mapping could be. */
    if (m->start_col_status != nullptr && m->start_row_status != nullptr) {
        p->reduced.start_col_status =
            jm_alloc_array(rcol, sizeof *p->reduced.start_col_status);
        p->reduced.start_row_status =
            jm_alloc_array(rrow, sizeof *p->reduced.start_row_status);
        if (!p->reduced.start_col_status || !p->reduced.start_row_status)
            return JAOS_ERR_OUT_OF_MEMORY;
        for (int64_t j = 0; j < m->num_col; j++) {
            const int64_t rjj = p->col_map[j];
            if (rjj >= 0)
                p->reduced.start_col_status[rjj] = m->start_col_status[j];
        }
        for (int64_t i = 0; i < rrow; i++)
            p->reduced.start_row_status[i] = m->start_row_status[i];
    }

    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;
    return JAOS_OK;
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
        memset(orig->sol_col_status, 0,
               (size_t)orig->num_col * sizeof *orig->sol_col_status);
        memset(orig->sol_row_status, 0,
               (size_t)orig->num_row * sizeof *orig->sol_row_status);
        /* No warm-start memory is offered here (Claude's discretion,
         * 02-01): whatever orig->start_* held before this call is left
         * exactly as it was, which is always safe — never a wrong basis,
         * only possibly a colder one than a fuller mapping could offer.
         * bench/run.c's own determinism check clears the basis before every
         * comparison it makes (D-12), so nothing this phase measures
         * exercises the gap; a fuller mapping is later work. */
        return JAOS_OK;
    }

    orig->objective = red->objective;

    /* Row space is untouched by this reduction (D-01 ships columns-only),
     * so every row's dual and status carry over unchanged: the row's own
     * logical variable is literally the same one in both models, only its
     * bounds moved. Only the activity needs the fixed columns' contribution
     * added back, since that is exactly what its bounds were shifted by in
     * jm_presolve_run. */
    for (int64_t i = 0; i < orig->num_row; i++) {
        orig->sol_dual[i] = red->sol_dual[i];
        orig->sol_row_status[i] = red->sol_row_status[i];
        orig->sol_row[i] = red->sol_row[i];
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

    /* Strictly LIFO (D-07). This plan's one family is independent record to
     * record, so the order here is a determinism guarantee rather than a
     * dependency yet — a later family that cascades through this plan's
     * reduction will need it to be one. */
    for (int64_t r = p->arena_len - 1; r >= 0; r--) {
        const jm_presolve_rec *rec = &p->arena[r];
        switch (rec->tag) {
        case JM_PS_FIXED_COL: {
            const int64_t j = fixed_col_restore_index(rec);
            assert(j >= 0 && j < orig->num_col);

            orig->sol_col[j] = ps_published(rec->value);
            /* Permitted by the checker's own rule: a fixed variable's two
             * bounds are the same point, so it carries no sign condition at
             * all (sign_condition, check.c — "fixed -> anything") and any
             * status is as correct as any other. */
            orig->sol_col_status[j] = JAOS_BASIS_AT_LOWER;

            /* d_j = c_j - sum_k a_kj * y_k, over the column's ORIGINAL
             * entries and the now fully-known original row duals — every
             * row survived this reduction, so no row dual needed recovery
             * of its own before this could be computed (derived against
             * sign_condition as read, not from memory, per this plan's own
             * instruction: the numerics-reviewer task exists for exactly
             * this class of formula). */
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
        }
    }

    (void)jm_model_remember_basis(orig);
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p)
{
    jaos_model *orig = p->orig;

    jaos_status est = jm_model_ensure_solution_arrays(orig);
    if (est != JAOS_OK)
        return est;

    /* Every column presolve fixed: nothing is left for the simplex to run
     * on, so this outcome always publishes OPTIMAL (D-01's scope stops at
     * NONE, REDUCED and SOLVED — infeasibility from a fully-fixed model
     * that violates a row is 02-03's, not this plan's). */
    orig->solve_status = JAOS_SOLVE_OPTIMAL;
    orig->solve_iters  = 0;
    orig->solve_work   = 0;   /* presolve's own cost is billed starting 02-02 */
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

    for (int64_t r = p->arena_len - 1; r >= 0; r--) {
        const jm_presolve_rec *rec = &p->arena[r];
        switch (rec->tag) {
        case JM_PS_FIXED_COL: {
            const int64_t j = fixed_col_restore_index(rec);
            assert(j >= 0 && j < orig->num_col);

            orig->sol_col[j] = ps_published(rec->value);
            orig->sol_col_status[j] = JAOS_BASIS_AT_LOWER;
            /* Every dual is zero (no row survived unreduced to compute one
             * against), so d_j = c_j - 0 exactly. */
            orig->sol_redcost[j] = ps_published(rec->cost);

            for (int64_t k = orig->a_start[j]; k < orig->a_start[j + 1]; k++) {
                const int64_t i = orig->a_index[k];
                orig->sol_row[i] = ps_published(orig->sol_row[i] +
                                                orig->a_value[k] * rec->value);
            }
            break;
        }
        }
    }

    for (int64_t i = 0; i < orig->num_row; i++) {
        const double act = orig->sol_row[i];
        if (act == orig->row_lower[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
        else if (act == orig->row_upper[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_UPPER;
        else
            orig->sol_row_status[i] = JAOS_BASIS_BASIC;
    }

    (void)jm_model_remember_basis(orig);
    return JAOS_OK;
}
