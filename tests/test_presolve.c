/* Presolve/postsolve tests: the round trip for the one reduction this plan
 * ships (a column fixed as loaded), the two structural invariants D-01
 * makes non-negotiable — every array publish leaves is in the caller's own
 * index space, and the caller's model is untouched by a reducing solve —
 * and the instrument's own negative sibling: a postsolve index map wrong
 * by one, shown to be caught before any of the above is believed (D-10).
 *
 * The fault-injection guard, JAOS_PRESOLVE_FAULT_OFFBYONE, is a build-wide
 * switch (src/presolve.c) — it corrupts every reducing solve in the whole
 * binary, not just the one this file means to test with it. Every test
 * below that expects a CORRECT postsolve is therefore guarded to skip
 * itself when that build is active, and the one negative test is guarded
 * the other way: it runs only then. `make -j12 test` exits 0 either way,
 * which is what the plan's own <verify> command checks by running this
 * file under both builds.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

/* min x0 + 5*x1 + x2  s.t.  x0 + x1 + x2 >= 1,
 * 0 <= x0 <= 10, x1 fixed at 2 as loaded (col_lower == col_upper),
 * 0 <= x2 <= 1.
 *
 * Presolve drops x1: its cost*value (5*2=10) folds into the objective
 * offset and its 1*2=2 folds into the row's lower bound, giving a reduced
 * problem of min x0+x2 (+10) s.t. x0+x2 >= -1, 0<=x0<=10, 0<=x2<=1 —
 * trivially satisfied by the cold-start slack basis at x0=x2=0, so this
 * solves in zero iterations. By hand: reduced objective is 0 + 10 = 10;
 * adding x1's own contribution back gives an original row activity of
 * 0+2+0=2, strictly inside (1, +inf), so the row's dual is exactly zero
 * for complementary slackness — which is what the reduced solve's own
 * basic logical already gives, unchanged, since this reduction never
 * touches row space. Every quantity here is an integer a double represents
 * exactly, so the hand-derived values below carry no rounding: they are
 * what a presolve-off solve of the same problem computes too, which is
 * what this plan's own <verify> command — running this same test suite a
 * second time under `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` — actually checks. A
 * single process cannot flip that compile-time switch, so the
 * "presolve-off" half of the comparison lives in the second build, not in
 * a second solve here.
 *
 * x2's narrow upper bound of 1 (rather than 10) is not needed by this
 * model's own solve — it exists so this same model doubles as the fault
 * test's fixture below, where a postsolve write meant for x1 landing on
 * x2 instead is guaranteed detectable: x1's fixed value, 2.0, sits outside
 * x2's [0,1]. */
static jaos_model *make_one_fixed_column(void)
{
    const double c[]  = {1.0, 5.0, 1.0};
    const double cl[] = {0.0, 2.0, 0.0}, cu[] = {10.0, 2.0, 1.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}

static void test_fixed_column_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build; see test_fixed_column_index_map_off_by_one");
#else
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* Bit exact: see make_one_fixed_column's docstring for why this value
     * carries no rounding to compare against. */
    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    /* The fixed column publishes at its own value in original space —
     * never a reduced index, never a stale one. */
    const double expected_x1 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    /* A fixed column carries no sign condition (check.c's sign_condition:
     * "fixed -> anything") so any status is correct; this plan always
     * publishes AT_LOWER for one, which is asserted directly here rather
     * than left implicit. */
    jaos_basis_status cs[3], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[1]);

    jaos_model_free(m);
#endif
}

/* D-10's row-count invariant as its own test, standing apart from the
 * round trip above: exactly num_row entries across the two published
 * status arrays read JAOS_BASIS_BASIC, on a postsolved basis in original
 * indices — the same invariant jaos_set_basis (src/model.c) enforces on a
 * basis handed in, checkable here with no help from the checker at all. */
static void test_postsolved_basis_has_exactly_num_row_basic_entries(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_basis_status cs[3], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 3; j++)
        basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 1; i++)
        basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(1, basic);   /* == num_row */

    jaos_model_free(m);
#endif
}

/* D-13's white-box test: the counter reads an exact integer for the model
 * it was handed, not a floor a single reduction firing would also satisfy.
 * jm_presolve_run is called directly rather than through jaos_solve, since
 * jm_presolve is solve-local and never escapes a public call (D-08) — the
 * same kind of direct access tests already have to jm_lu and the nonbasic
 * bitmap. Unaffected by JAOS_PRESOLVE_FAULT_OFFBYONE: that guard lives
 * entirely in postsolve's replay, and this test never reaches postsolve. */
static void test_fixed_col_counter_is_exact(void)
{
    jaos_model *m = make_one_fixed_column();   /* exactly one fixed column */

    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
    TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_REDUCED, p.outcome);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.rounds);
    /* Declared for the other seven families and zero until the plan that
     * makes each fire (D-13) — checked directly so a future reduction that
     * forgets to increment its own field is caught here, not inferred from
     * a total that moved for an unrelated reason. */
    TEST_ASSERT_EQUAL_INT64(0, p.counts.empty_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.empty_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.singleton_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.forcing_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.redundant_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.duplicate_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.duplicate_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.dominated_col);

    jm_presolve_free(&p);
    jaos_model_free(m);
}

/* min 2*x0 + 3*x1  s.t.  x0 + x1 <= 10,  x0 fixed at 1, x1 fixed at 4.
 * Every column presolve fixes: outcome is JM_PRESOLVE_SOLVED, no sx is
 * built and the simplex never runs. By hand: objective is 2*1 + 3*4 = 14,
 * activity is 1 + 4 = 5, strictly inside the row's own bounds, so its dual
 * is zero — which the SOLVED path publishes unconditionally, matching
 * complementary slackness for exactly the reason the round-trip test's row
 * is interior too. */
/* Not built under the fault-injection guard: its only caller is skipped
 * there (see below), and an unused static function is a -Werror build
 * failure (-Wunused-function) rather than a harmless leftover. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_all_fixed(void)
{
    const double c[]  = {2.0, 3.0};
    const double cl[] = {1.0, 4.0}, cu[] = {1.0, 4.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_all_columns_fixed_solves_with_no_iterations(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_all_fixed();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = 14.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* D-06's structural half: jaos_model's own CSC and bound arrays are
 * untouched by a solve that reduces the model, whether or not the
 * reduction leaves anything for the simplex to run on. Captured before the
 * solve and compared after under memcmp, which is what "never writes to
 * m" actually means as a test rather than as a comment.
 *
 * Left unguarded: presolve's write side (jm_presolve_run) and postsolve's
 * replay side (jm_postsolve_expand) are separate functions, and the fault
 * lives entirely in the replay side — it can corrupt where a solution
 * value lands, never whether the model's own arrays are touched. This
 * assertion holds identically under every build, including the
 * fault-injection one, and is left running there on purpose as evidence of
 * exactly that separation. */
static void test_original_arrays_survive_a_reducing_solve(void)
{
    jaos_model *m = make_one_fixed_column();

    int64_t a_start[4], a_index[3];
    double a_value[3], col_lower[3], col_upper[3];
    memcpy(a_start, m->a_start, sizeof a_start);
    memcpy(a_index, m->a_index, sizeof a_index);
    memcpy(a_value, m->a_value, sizeof a_value);
    memcpy(col_lower, m->col_lower, sizeof col_lower);
    memcpy(col_upper, m->col_upper, sizeof col_upper);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_EQUAL_MEMORY(a_start, m->a_start, sizeof a_start);
    TEST_ASSERT_EQUAL_MEMORY(a_index, m->a_index, sizeof a_index);
    TEST_ASSERT_EQUAL_MEMORY(a_value, m->a_value, sizeof a_value);
    TEST_ASSERT_EQUAL_MEMORY(col_lower, m->col_lower, sizeof col_lower);
    TEST_ASSERT_EQUAL_MEMORY(col_upper, m->col_upper, sizeof col_upper);

    jaos_model_free(m);
}

/* The negative sibling of the round trip above (D-10): under
 * JAOS_PRESOLVE_FAULT_OFFBYONE, x1's postsolve record restores at index 2
 * instead of index 1 — x2's slot, not its own. x1's own slot is
 * pre-seeded below with the value it would have received anyway, so the
 * one deterministic effect left is what lands on x2: its own optimal
 * value (0.0) is overwritten with x1's fixed value (2.0), which sits
 * outside x2's declared bound of 1.0. Without the seed, x1's own slot
 * would read uninitialized memory — a real bug too, but not a
 * deterministic one, and this test means to catch the one fault it
 * injects, not whatever a fresh allocation happens to contain.
 *
 * Caught by: max_col_violation, reading exactly 1.0 (2.0 published against
 * x2's upper bound of 1.0) — interval_violation's own formula, verified
 * against src/check.c. A second, independent breach fires on the same
 * corrupted slot: jaos_check_solution never reads the published
 * sol_redcost at all (it recomputes d = c - A'y itself, from the model's
 * own cost and the row duals handed to it), so it is x2's *value* driving
 * both checks. With x2 sitting at 2.0 rather than its optimal 0.0, it is
 * no longer at its lower bound, so sign_condition's w > 0 branch — x2's
 * own cost 1.0 against an unaffected row dual of 0.0 gives w = 1.0 — can
 * no longer return zero: max_dual_violation also reads 1.0, the same
 * magnitude as the primal breach by coincidence of this model's numbers,
 * not because the two checks share a cause. Both are asserted so the
 * fault is shown caught on the primal side and the dual side, not by one
 * report field getting lucky. */
static void test_fixed_column_index_map_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_one_fixed_column();

    /* Pre-seeded with exactly what a correct postsolve would have written
     * for x1 — see the docstring above for why. */
    m->sol_col = calloc(3, sizeof(double));
    m->sol_col[1] = 2.0;
    m->sol_col_status = calloc(3, sizeof(jaos_basis_status));
    m->sol_col_status[1] = JAOS_BASIS_AT_LOWER;
    m->sol_redcost = calloc(3, sizeof(double));
    m->sol_redcost[1] = 5.0;
    m->sol_row = calloc(1, sizeof(double));
    m->sol_dual = calloc(1, sizeof(double));
    m->sol_row_status = calloc(1, sizeof(jaos_basis_status));
    TEST_ASSERT_NOT_NULL(m->sol_col);
    TEST_ASSERT_NOT_NULL(m->sol_col_status);
    TEST_ASSERT_NOT_NULL(m->sol_redcost);
    TEST_ASSERT_NOT_NULL(m->sol_row);
    TEST_ASSERT_NOT_NULL(m->sol_dual);
    TEST_ASSERT_NOT_NULL(m->sol_row_status);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_col_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* min x0 + 100*x1 + 2*x2 + 3*x3  s.t.
 *   x0 + x1        <= row0
 *   x1     + x2     <= row1
 * with row0 >= 1, row1 >= 2, x1 fixed at 5, every other column free in
 * [0,10]. Every column carries a distinct cost and x1's own is far outside
 * the others' range, so a reduced index leaking into any published array
 * — the wrong column's slot getting the wrong column's number — is
 * detectable by value, not only by array length (D-01's own index-space
 * decision). x1 touches both rows, so its postsolve recovery is exercised
 * against more than one row entry, not the single-row case the round-trip
 * test above already covers.
 *
 * By hand, presolve drops x1: obj offset gains 100*5=500, row0's lower
 * bound shifts to 1-5=-4, row1's to 2-5=-3. The reduced problem's cold
 * start (every survivor at its cost-positive lower bound, 0) leaves both
 * rows at activity 0, which is >= both shifted lower bounds — feasible
 * immediately, 0 iterations, with every row's logical basic (interior,
 * since neither shifted bound is at 0) and every row dual 0. Reduced
 * costs are then just each surviving column's own cost (y is 0 throughout):
 * x0 -> 1, x2 -> 2, x3 -> 3, and x1's own recovery is
 * 100 - (1*0 + 1*0) = 100 over its two row entries. Original activities,
 * from the matrix below (row0: x0, x1, x3; row1: x1, x2):
 * row0 = x0+x1+x3 = 0+5+0 = 5, row1 = x1+x2 = 5+0 = 5. Guarded like the
 * other positive tests: the fault redirects x1's single record to x2's
 * slot, which this test does not attempt to survive. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_distinctly_valued_reduction(void)
{
    const double c[]  = {1.0, 100.0, 2.0, 3.0};
    const double cl[] = {0.0, 5.0, 0.0, 0.0};
    const double cu[] = {10.0, 5.0, 10.0, 10.0};
    const double rl[] = {1.0, 2.0}, ru[] = {INFINITY, INFINITY};
    /* col0: row0.  col1 (fixed): row0, row1.  col2: row1.  col3: row0. */
    const int64_t s[]  = {0, 1, 3, 4, 5};
    const int64_t ix[] = {0,   0, 1,   1,   0};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    return m;
}
#endif

static void test_original_index_invariant_across_all_six_arrays(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_distinctly_valued_reduction();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 500.0;   /* 1*0 + 100*5 + 2*0 + 3*0 */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], row_act[2], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, row_act, y, dj));

    const double expected_x[4]       = {0.0, 5.0, 0.0, 0.0};
    const double expected_row_act[2] = {5.0, 5.0};
    const double expected_y[2]       = {0.0, 0.0};
    const double expected_dj[4]      = {1.0, 100.0, 2.0, 3.0};
    TEST_ASSERT_EQUAL_MEMORY(expected_x, x, sizeof x);
    TEST_ASSERT_EQUAL_MEMORY(expected_row_act, row_act, sizeof row_act);
    TEST_ASSERT_EQUAL_MEMORY(expected_y, y, sizeof y);
    TEST_ASSERT_EQUAL_MEMORY(expected_dj, dj, sizeof dj);

    jaos_basis_status cs[4], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    const jaos_basis_status expected_cs[4] = {
        JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER,
        JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER,
    };
    const jaos_basis_status expected_rs[2] = {
        JAOS_BASIS_BASIC, JAOS_BASIS_BASIC,
    };
    TEST_ASSERT_EQUAL_MEMORY(expected_cs, cs, sizeof cs);
    TEST_ASSERT_EQUAL_MEMORY(expected_rs, rs, sizeof rs);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* D-14's pinned change detector (02-02 checkpoint: nonzero-only). The model
 * above -- one fixed column, x1, touching exactly one nonzero in row 0 --
 * costs a measurably *different* total under presolve than it did before
 * this plan, not necessarily a larger one: presolve's own charge (+1, the
 * nonzero it visits) is smaller than what it saves the reduced model's own
 * solve (a 2-column, 1-row factorization and price instead of a 3-column
 * one), so the net here is a 4-unit drop, 8206 -> 8202. Both figures are
 * measured, not derived from each other. The two branches below are the
 * same model's total with the charge and without it: run under both
 * `make -j12 test` and `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE test` (the plan's
 * own <verify>), so together they are the pair that proves the new figure
 * is presolve's charge and not a change to something else. See
 * docs/work-units.md's presolve entry for what each charge counts. */
#if defined(JAOS_NO_PRESOLVE)
/* Unchanged by 02-02: presolve never runs under this build, so this is the
 * tree's pre-02-02 figure for this model, carried forward as the negative
 * control -- the same role EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE plays for every
 * other presolve claim in this file. Measured, not derived. */
constexpr int64_t PRESOLVE_MODEL_WORK_PINNED = 8206;
#else
/* x1's one nonzero costs JM_WORK_NONZERO=1 while jm_presolve_run shifts
 * row 0's bound by it -- the round's own charge, nonzero-only per the
 * 02-02 checkpoint. The rest is the reduced model's own solve: 0
 * iterations (the cold-start slack basis is already feasible), one
 * settle-and-recheck factorization and price over what presolve left, 1
 * row and 2 columns rather than 3. First pinned by 02-02, which is the
 * first plan that charges anything here at all -- 02-01 already reduced
 * this model but billed it nothing, and nothing pinned the resulting
 * figure. Measured, not derived. */
constexpr int64_t PRESOLVE_MODEL_WORK_PINNED = 8202;
#endif

static void test_presolve_bills_the_work_counter(void)
{
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(PRESOLVE_MODEL_WORK_PINNED, jaos_work_units(m));
    jaos_model_free(m);
}

/* Acceptance criterion, not merely the counters' own arithmetic: two solves
 * of the same model with the basis cleared between them report equal work
 * units, the same way bench/run.c's own double solve already checks on
 * every instance in every campaign (D-12). Checked directly here too
 * because it is presolve's charge under test, not the simplex's. */
static void test_presolve_work_is_deterministic_across_re_solves(void)
{
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const int64_t first = jaos_work_units(m);

    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(first, jaos_work_units(m));

    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixed_column_round_trip);
    RUN_TEST(test_postsolved_basis_has_exactly_num_row_basic_entries);
    RUN_TEST(test_fixed_col_counter_is_exact);
    RUN_TEST(test_all_columns_fixed_solves_with_no_iterations);
    RUN_TEST(test_original_arrays_survive_a_reducing_solve);
    RUN_TEST(test_fixed_column_index_map_off_by_one);
    RUN_TEST(test_original_index_invariant_across_all_six_arrays);
    RUN_TEST(test_presolve_bills_the_work_counter);
    RUN_TEST(test_presolve_work_is_deterministic_across_re_solves);
    return UNITY_END();
}
