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
#include <float.h>

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
    /* 02-04 moved this model, and the move is the reduction working rather
     * than a pin drifting. Once x1 is fixed at 2 the only row reads
     * x0 + x2 >= -1 with x0 in [0,10] and x2 in [0,1]: its minimum activity
     * is 0, its upper bound is infinite, so the row can never bind and the
     * redundant-row family drops it. Both remaining columns then go empty
     * and take their own favourable bounds, and presolve answers the whole
     * model with no simplex run — JM_PRESOLVE_SOLVED, not REDUCED. The
     * objective is unchanged at 10 and test_fixed_column_round_trip still
     * checks it against the checker. */
    TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_SOLVED, p.outcome);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.redundant_row);
    TEST_ASSERT_EQUAL_INT64(2, p.counts.empty_col);
    TEST_ASSERT_EQUAL_INT64(2, p.counts.rounds);
    /* Declared for the other families and zero until the plan that makes
     * each fire (D-13) — checked directly so a future reduction that
     * forgets to increment its own field is caught here, not inferred from
     * a total that moved for an unrelated reason. */
    TEST_ASSERT_EQUAL_INT64(0, p.counts.empty_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.singleton_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.forcing_row);
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
    /* 2, not the 1 this read before 02-04, and the fault is the same fault.
     * The model now reduces to nothing (see test_fixed_col_counter_is_exact
     * for why), so three column records replay under the fault instead of
     * one and each lands on the next column's slot: x2 takes x1's value of
     * 2 against its own upper bound of 1, and x1 takes x0's 0 against its
     * own fixed value of 2. The larger of the two violations is x1's. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_col_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);

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
 * slot, which this test does not attempt to survive.
 *
 * 02-03: row1 also drops to a singleton (on x2) once x1 fixes and folds
 * away, a cascade 02-01's own single-family world never exercised — so
 * this positive test is now guarded against JAOS_PRESOLVE_FAULT_WRONGDUAL
 * too, the same reasoning as every other positive test in this file. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
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
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault-injection "
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
/* 02-04 moved this figure from 8202 to 3, and the drop is what the
 * activity-range families do rather than a pin drifting: there is no
 * reduced model left to solve. x1's own nonzero costs JM_WORK_NONZERO=1
 * while jm_presolve_run shifts row 0's bound by it (02-02's nonzero-only
 * charge); the redundant-row reading of row 0 costs its two live nonzeros;
 * the two empty columns that follow are charged nothing, and no sx is ever
 * built. 8202 was that same 1 plus a whole reduced solve — one
 * settle-and-recheck factorization and price over 1 row and 2 columns —
 * and the whole of it is gone. Measured under WSL with `make clean`
 * between the two builds, not derived from each other. */
constexpr int64_t PRESOLVE_MODEL_WORK_PINNED = 3;
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

/* ======================================================================= *
 * 02-03: empty rows, empty columns, singleton rows, singleton columns and
 * the free-column-singleton case, and the path that publishes without a
 * simplex run at all.
 *
 * Every positive test below is guarded to skip under EITHER fault build
 * (JAOS_PRESOLVE_FAULT_OFFBYONE, extended in 02-03 to every tag's index,
 * or JAOS_PRESOLVE_FAULT_WRONGDUAL, singleton row's own dual-choice
 * fault) — both are build-wide and corrupt every reducing solve in the
 * binary, the same reason 02-01's own positive tests are guarded. Each
 * negative test runs only under the one build meant to trip it.
 * ======================================================================= */

/* -- Empty row ----------------------------------------------------------- *
 *
 * min x0 + x1  s.t.  row0 (empty, no column touches it): -1 <= 0 <= 1
 *                    row1: x0 + x1 >= 1
 *                    row2: x0 + x1 <= 100
 * x0, x1 in [0, 10].
 *
 * row1 and row2 both survive untouched (degree 2, neither empty nor
 * singleton) — only row0 is removed. By hand: row1 forces x0+x1>=1 at
 * minimum cost 1 (the split is degenerate since both costs are equal;
 * fixed via check_solution rather than an exact split). row0's dual is
 * always 0 (its activity is always 0, nothing touches it), which is why
 * the fault-injection victim below is row1, not row0: row1 alone carries
 * a nonzero dual (2, derived from x0's own stationarity once row2 is
 * confirmed interior) for the fault to corrupt detectably. */
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_empty_row_model(void)
{
    const double c[]  = {2.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {-1.0, 1.0, -INFINITY};
    const double ru[] = {1.0, INFINITY, 100.0};
    /* col0 (x0): row1, row2.  col1 (x1): row1, row2.  row0: nothing. */
    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {1, 2, 1, 2};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_empty_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_empty_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 2.0;   /* 2*1 + 3*0, x0 pushed to its bound */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    /* row0's dual is exactly 0 — the empty row's only possible answer. */
    const double expected_y0 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[0]);   /* the empty row */
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 3; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(3, basic);   /* == num_row */

    jaos_model_free(m);
#endif
}

/* Negative sibling (D-10): under JAOS_PRESOLVE_FAULT_OFFBYONE, row0's
 * record restores at index 1 instead of 0 — row1's slot — overwriting
 * row1's genuinely nonzero dual (2.0, by hand) with row0's own answer
 * (always 0.0). row1's activity is at its own lower bound (1), which
 * tolerates any nonnegative dual including the wrong zero, so this is
 * caught on the COLUMN side instead: x0 is basic (interior to [0,10])
 * and needs its reduced cost to be exactly zero, which the checker
 * recomputes independently from the corrupted row1 dual it was handed —
 * 2 - 1*0 = 2, not 0. */
static void test_empty_row_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_empty_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* -- Empty column ---------------------------------------------------------
 *
 * min x0 + x1 - x2  s.t.  row0: x0 + x1 >= 1
 * x0, x1 in [0, 10]; x2 in [0, 5], touching no row at all.
 *
 * row0 survives untouched (degree 2). x2 is empty; its cost is negative
 * (favours the upper bound), so presolve fixes it at 5 directly — no
 * simplex run needed for x2's own value, and its reduced cost is exactly
 * its own cost (no row entries to subtract anything from). */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_empty_col_model(void)
{
    const double c[]  = {1.0, 1.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, 5.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    /* col0: row0.  col1: row0.  col2: nothing. */
    const int64_t s[]  = {0, 1, 2, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_empty_col_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_empty_col_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = -4.0;   /* 1 (from row0) + (-1)*5 */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[1], dj[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));

    const double expected_x2 = 5.0;
    const double expected_dj2 = -1.0;   /* c2 - 0, x2 has no row entries */
    TEST_ASSERT_EQUAL_MEMORY(&expected_x2, &x[2], sizeof x[2]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj2, &dj[2], sizeof dj[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* Negative sibling: x2's record restores one past its own index, landing
 * out of range for a 3-column model (index 3) — guarded against by the
 * arena replay's own assert in a debug build, so instead this fault is
 * exercised on a model with a fourth column to give it a real (wrong)
 * slot to land in. col3 is an ordinary, nonzero-cost column touching
 * row0 alongside col0/col1 — no reduction fires on it at all, so it is
 * not itself a record needing a landing spot of its own (the trap the
 * first version of this test fell into: a fourth column *fixed* at 0
 * pushed its own record to index 4, one past even the widened model).
 * col3's own cost (0.5) is the cheapest way to satisfy row0, so its true
 * optimum is 1 (not 0 — a second trap the first version fell into,
 * assuming the lower bound without checking which column row0 actually
 * prefers), bounded [0, 3] to keep col2's fixed value (5) genuinely
 * outside it rather than landing there by coincidence. */
static void test_empty_col_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    const double c[]  = {1.0, 1.0, -1.0, 0.5};
    const double cl[] = {0.0, 0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 5.0, 3.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    /* col0: row0.  col1: row0.  col2: nothing.  col3: row0, cheapest, so
     * it is what actually satisfies row0 (col3 = 1, not 0) — bounded
     * [0, 3], excluding col2's fixed value (5) so landing it there is a
     * real violation rather than a coincidentally valid one (the trap
     * the first version of this test fell into, at [0, 10]). */
    const int64_t s[]  = {0, 1, 2, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[4], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, r.max_col_violation);

    jaos_model_free(m);
#endif
}

/* Empty column, direct unboundedness (D19's one exception): x2's cost is
 * negative and its favourable bound (upper) is infinite, so there is
 * nowhere for it to be fixed — a sound proof with no ray, per this
 * plan's own scope. No fault-injection guard: this is a direct outcome
 * check, not a postsolved-value round trip. */
static void test_empty_col_reports_unbounded(void)
{
    const double c[]  = {1.0, 1.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, INFINITY};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    jaos_model_free(m);
}

/* -- Singleton row --------------------------------------------------------
 *
 * min 2*x0  s.t.  row0: x0 >= -10
 * x0 in [0, 5].
 *
 * row0's own implied bound (-10) is looser than x0's own original lower
 * bound (0), so x0's own bound is what actually governs — the branch
 * this family's dual recovery calls "col's own bound is binding",
 * y_i = 0. row0 is a singleton (its only entry, x0), and once it is
 * removed x0 itself becomes an empty column (nothing left to touch it),
 * so this whole model resolves to JM_PRESOLVE_SOLVED with no simplex run
 * at all — the reduced-to-nothing case this plan's Task 2 also asks for,
 * exercised here as the same model. By hand: x0 = 0 (its own favourable,
 * cost-positive bound), objective 0, row0's activity 0, strictly
 * interior to [-10, inf), so its dual must be exactly 0 — this is the
 * fault's own target below. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_singleton_row_model(void)
{
    const double c[] = {2.0};
    const double cl[] = {0.0}, cu[] = {5.0};
    const double rl[] = {-10.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1};
    const int64_t ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}
#endif

static void test_singleton_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));   /* SOLVED, no sx */

    const double expected_obj = 0.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[1], y[1], dj[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x0 = 0.0, expected_y0 = 0.0, expected_dj0 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj0, &dj[0], sizeof dj[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* Negative sibling: singleton row's own risk is the dual, not the index
 * (this plan's file header). Under JAOS_PRESOLVE_FAULT_WRONGDUAL, row0's
 * dual is set to x0's reduced cost divided by its coefficient
 * unconditionally — 2.0/1.0 = 2.0 — instead of the 0.0 the "col's own
 * bound governs" branch requires here. row0's activity (0) sits well
 * inside its own (-10, inf) range, so its sign condition demands w == 0
 * exactly; a published dual of 2.0 is a direct, undiluted violation. */
static void test_singleton_row_wrong_dual(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_WRONGDUAL");
#else
    jaos_model *m = make_singleton_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[1], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* -- Singleton column (bounded, cost 0) -----------------------------------
 *
 * min x0  s.t.  row0: x0 + x1 <= 10
 * x0 in [0, 20]; x1 in [1, 3], cost 0.
 *
 * x1 is a bounded singleton column at cost 0: presolve relaxes row0 to
 * absorb x1's whole range (row0's own upper bound drops from 10 to 9,
 * the most x1 could ever need to leave for x0) and removes x1, leaving
 * x0 alone in row0. By hand: minimizing x0 with no lower pressure gives
 * x0 = 0, trivially satisfying the relaxed row0 (0 <= 9); x1 is then
 * recovered from row0's own (interior, dual 0) activity as its own
 * lower bound, 1 — any point in [1,3] is equally optimal at cost 0, and
 * the lower end is what this family's postsolve always picks. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_singleton_col_model(void)
{
    const double c[] = {1.0, 0.0};
    const double cl[] = {0.0, 1.0}, cu[] = {20.0, 3.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};
    /* col0 (x0): row0.  col1 (x1): row0. */
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

static void test_singleton_col_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_col_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 0.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x0 = 0.0, expected_x1 = 1.0;
    const double expected_y0 = 0.0, expected_dj1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj1, &dj[1], sizeof dj[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* Negative sibling: this family's own index2 (the removed column, x1) is
 * what the fault redirects — not index (the row, which only survives and
 * is never "restored" in the sense the other tags' index is), a
 * correction made after the row-index fault turned out to be undetectable
 * by construction: x1's clamped value can only ever come out at rec->lo
 * or higher, and any row's own (bound, activity) pair satisfies bound <=
 * activity by that row's own feasibility, so no row swap could ever push
 * the clamp below rec->lo where a difference would show.
 *
 * With a third, ordinary column (x2, cost 1, bounds [5, 10], no reduction
 * fires on it) added to row0 for the fault to land on: x1's own correct
 * value (1.0) overwrites x2's correct one (5.0), landing outside x2's
 * own [5, 10] — a primal violation of exactly 4.0. */
static void test_singleton_col_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    const double c[]  = {1.0, 0.0, 1.0};
    const double cl[] = {0.0, 1.0, 5.0}, cu[] = {20.0, 3.0, 10.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};
    /* col0: row0.  col1: row0.  col2: row0. */
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, r.max_col_violation);

    jaos_model_free(m);
#endif
}

/* -- Singleton col composed with an earlier removal ------------------------
 *
 * min 2*x0 + x2 + 3*x3  s.t.  row0: x0 + x1 = 10   row1: x2 + x3 >= 1
 * x0 in [4, 4] (loaded fixed); x1 in [0, 100], cost 0; x2, x3 in [0, 5].
 *
 * x0 fixes first (FIXED_COL, shifting row0's bounds to [6, 6]), THEN x1
 * fires as a bounded singleton col — so x1's record replays BEFORE x0's
 * adds its 4 back into row0's activity. The replay must judge x1 against
 * the shifted pair [6, 6] it recorded, not the original [10, 10]: against
 * the original it publishes x1 = 10, x0's replay then adds 4, and the row
 * ends at 14 — a row violation of exactly 4.0 that the objective cannot
 * see (x1's cost is 0). Written after a real defect: this shape is
 * 02-03's gate rejection, 16 of 19 instances (on ken-07 every violated
 * row read viol == the later-replayed contributions, exactly — review of
 * 2026-08-13). row1/x2/x3 keep the reduced model nonempty so the replay
 * runs on the ordinary jm_postsolve_expand path the gate exercises. By
 * hand: x = {4, 6, 1, 0}, y = {0, 1}, obj = 9. */
static void test_singleton_col_after_fixed_col(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {2.0, 0.0, 1.0, 3.0};
    const double cl[] = {4.0, 0.0, 0.0, 0.0}, cu[] = {4.0, 100.0, 5.0, 5.0};
    const double rl[] = {10.0, 1.0}, ru[] = {10.0, INFINITY};
    /* col0: row0.  col1: row0.  col2: row1.  col3: row1. */
    const int64_t s[]  = {0, 1, 2, 3, 4};
    const int64_t ix[] = {0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 9.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 6.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* -- Two removals around one singleton col, jm_postsolve_solved path -------
 *
 * min 7*x0 + 5*x2 + x3  s.t.  row0: x0 + x1 + x2 = 20   row1: x2 = 3
 * x0 in [4, 4]; x1 in [0, 100], cost 0; x2 in [0, 10]; x3 in [2, 5],
 * in no row at all.
 *
 * Both halves of the replay's invariant, which the sibling test above
 * only half exercises (there `rest` is still 0 when the singleton col
 * replays). Here row1 folds x2 to [3, 3] and x2 then fixes, so x2's
 * record is pushed AFTER x1's and replays BEFORE it: `rest` is 3, not 0,
 * when x1 is recovered. x0 fixes BEFORE x1, so its 4 is still missing at
 * that moment and its shift is in the recorded pair ([16, 16], not the
 * original [20, 20]). Judging against the original pair publishes
 * x1 = 20 - 3 = 17 and leaves row0 at 24 — a row violation of exactly
 * 4.0 the objective cannot see, x1's cost being 0.
 *
 * Every column leaves, so rcol == 0 and postsolve runs on the
 * jm_postsolve_solved path. That path seeds no activity at all, which
 * makes the replay the ONLY source of `rest` here; none of the 19 gate
 * rejections of 2026-08-13 took it. row0 survives the whole way, frozen
 * by x1's relaxation, so its published basis status also comes from
 * nowhere but that path's own initialisation — asserted below, since a
 * status left unwritten there is read from the heap and copied into the
 * next solve's warm start (valgrind sees it; ASan and UBSan do not).
 * By hand: x = {4, 13, 3, 2}, obj = 45. */
static void test_singleton_col_between_two_removals_solved_path(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {7.0, 0.0, 5.0, 1.0};
    const double cl[] = {4.0, 0.0, 0.0, 2.0}, cu[] = {4.0, 100.0, 10.0, 5.0};
    const double rl[] = {20.0, 3.0}, ru[] = {20.0, 3.0};
    /* col0: row0.  col1: row0.  col2: row0, row1.  col3: none. */
    const int64_t s[]  = {0, 1, 2, 4, 4};
    const int64_t ix[] = {0, 0, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 45.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 13.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    /* **The repair landed and this is re-pinned at the right answer.** This
     * read 3 against `num_row = 2` and its own comment said to expect 2 once
     * the defect closed; D138's singleton-column exchange takes row0's
     * logical out of the basis, and both builds now publish 2. The `#if` that
     * asserted the two builds separately is gone with the gap it described.
     *
     * Still not asserted: WHICH status row0 carries. The count is the
     * contract `jaos.h` states; the individual status is not.
     *
     * The count is only worth pinning because the statuses are
     * initialised. Measured with the memsets removed, this assertion reads
     * 2 and fails against the 3 — but that 2 came out of the heap, and
     * another byte there would have read 3 and passed. A pinned number
     * over an uninitialised read does not detect anything; it flips a
     * coin. */
    jaos_basis_status cs[4], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 4; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    for (int64_t i = 0; i < 2; i++) basic += (rs[i] == JAOS_BASIS_BASIC);
    /* The reference build is asserted here rather than skipped, so it says
     * the right answer out loud instead of saying nothing. The two numbers
     * ARE the defect: 3 is what the replay publishes, 2 is what the model
     * has, and the gap closes when TODO.md's item lands. Before 2026-08-18
     * this line read 3 unconditionally and `make test
     * EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` was red on it, which meant nobody
     * ran the project's only oracle for output no predicate reads. */
    TEST_ASSERT_EQUAL_INT64(2, basic);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* -- The two rounding shapes behind netlib's wrong basis counts (D257) --
 *
 * Both models reduce to nothing, so postsolve alone publishes the basis;
 * the reference build reaches the same point through the simplex and is
 * the oracle for the count and for the column's value. Each shape's
 * numbers are chosen so that the recovery division rounds the wrong way. */

/* The exact tie the division rounds INWARD (D140's 80 declines).
 *
 * min x0  s.t.  x0 + 3*x1 >= 3,  x0 in [0, 10],  x1 in [0, 0.7] cost 0
 *
 * x1 is a cost-0 singleton column: the row's lower bound absorbs 3*0.7 =
 * 2.0999999999999996 and becomes 0.9000000000000004. The singleton row then
 * folds that into x0's lower bound and x0 empties out at it. At replay, the
 * row's logical rests at that absorbed end, so the exact recovery of x1 is
 * its own upper bound 0.7 -- and (3 - 0.9000000000000004) / 3 is
 * 0.6999999999999998, one ulp inside it. The old replay published that as
 * an interior BASIC x1 beside a nonbasic row and a basic x0: two basics
 * against one row. Now the bound is published, nonbasic, and the count is
 * exact. In the reference build x1 is nonbasic at 0.7 by construction, so
 * every assertion holds in both builds. */
static void test_an_exact_tie_the_division_rounds_inward_publishes_the_bound(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 0.7};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* The premise of the shape: the division really does round inward. */
    const double absorbed = 3.0 * 0.7;
    const double rest = 3.0 - absorbed;
    TEST_ASSERT_TRUE((3.0 - rest) / 3.0 < 0.7);

    double x[2], act[1], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));
    const double expected_x1 = 0.7;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, cs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[0]);
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    basic += (rs[0] == JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT64(1, basic);

    jaos_model_free(m);
#endif
}

/* The interior recovery whose folded activity misses the bound by an ulp
 * (D141's 152 declines).
 *
 * min x0  s.t.  x0 + 3*x1 >= 1,  x0 in [0.1, 10],  x1 in [0, 5] cost 0
 *
 * x1's range takes the row's lower bound to -14, so the reduced row is
 * slack and x0 rests on its own lower bound 0.1. At replay x1 must supply
 * 0.3: interior, so BASIC, and the row's logical leaves for the lower end
 * the division targeted. The replayed activity 0.1 + 3 * 0.3 is
 * 0.9999999999999999, and the old exchange, asking it to equal 1 bit for
 * bit, declined and left the row basic too. The end is known from the
 * arithmetic, not read back from its rounding. */
static void test_an_interior_recovery_takes_the_row_out_whatever_the_ulps_say(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.1, 0.0}, cu[] = {10.0, 5.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* The premise of the shape: the folded activity misses 1 by an ulp. */
    TEST_ASSERT_TRUE(0.1 + 3.0 * ((1.0 - 0.1) / 3.0) != 1.0);

    double x[2], act[1], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));
    const double expected_x0 = 0.1;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, act[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[0]);
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    basic += (rs[0] == JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT64(1, basic);

    jaos_model_free(m);
#endif
}

/* A column a singleton row fixed strictly inside its own box is basic.
 *
 * min x1  s.t.  2*x0 = 4,  x0 in [0, 10] cost 0,  x1 in [0, 1] (no row)
 *
 * The row folds x0 to the point 2, which is neither of the caller's bounds
 * on x0. Its reduced cost is zero, so until D257 the replay left x0 with
 * the nonbasic status it had at the point and gave the row's logical the
 * basic slot: a right count naming a nonbasic variable that rests on no
 * bound it has. x0 is basic and the row's logical is out. The reference
 * build publishes the row's logical at one of its two equal ends; which
 * one is the simplex's to choose, so only "not basic" is asserted. */
static void test_a_column_a_row_fixed_inside_its_box_is_basic(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 1.0};
    const double rl[] = {4.0}, ru[] = {4.0};
    const int64_t s[]  = {0, 1, 1};
    const int64_t ix[] = {0};
    const double v[]   = {2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x0 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_TRUE(rs[0] != JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[1]);
    /* A basic variable's reduced cost is zero. */
    const double zero = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&zero, &dj[0], sizeof dj[0]);

    jaos_model_free(m);
#endif
}

/* -- The basis count promise, on a column the implied free family declines --
 *
 * min x0  s.t.  row0: x0 + x1 = 7,  x0 in [0, 20],  x1 in [0, 100] cost 0
 *
 * `jaos.h` promises exactly num_row of the num_col + num_row statuses are
 * basic. This model publishes TWO against num_row = 1, and that is the
 * defect `TODO.md` carries under "the basis the singleton-column family
 * publishes breaks the count promise" — it names this exact model. The
 * column is recovered strictly inside its own box and published basic, and
 * so is the row it was relaxed out of.
 *
 * **It exists because the file's other pin turned out to be fragile.**
 * `test_singleton_col_between_two_removals_solved_path` publishes 3
 * against num_row = 2 and still does, but D118 measured a one-line change
 * of family order that gave its column to the implied free column
 * singleton — which removes the row too, so that model read the correct 2
 * and detected nothing at all. The change was refused for other reasons
 * (D118), and the lesson stands: a pin that only holds while one family
 * loses a race is not a pin.
 *
 * This model cannot lose that race. The implied free family DECLINES it
 * on the margin, not on the order: row0's implied box for x1 is
 * [7 - 20, 7 - 0] = [-13, 7], and -13 is below x1's own lower bound of 0.
 * The assertion below on `implied_free_col == 0` is what says so.
 *
 * The 2 is pinned as a change detector, not as a contract. **Expect it to
 * become 1 when the repair lands, and re-pin there, deliberately.** Which
 * status each entity carries is not asserted: that is what the repair
 * changes, and a test demanding today's value would fail the person fixing
 * it. */
static void test_the_basis_count_promise_breaks_on_a_declined_column(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {20.0, 100.0};
    const double rl[] = {7.0}, ru[] = {7.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    /* The ANSWER is right, in both builds, and that is why no predicate of
     * the three instance sets can see this. */
    const double expected_x0 = 0.0, expected_x1 = 7.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    basic += (rs[0] == JAOS_BASIS_BASIC);
    /* The reference build is the oracle and it publishes the promised count.
     * Presolve's replay used to publish one too many, and the two numbers
     * were asserted separately because the gap between them WAS the defect.
     * **D138 closed it**: the singleton column's exchange takes its surviving
     * row's logical out of the basis when the column comes back interior, so
     * both builds now publish 1 and one assertion says so. Re-pinned
     * deliberately, as the comment this replaces asked. */
    TEST_ASSERT_EQUAL_INT64(1, basic);

    /* And the family really is the one that fired: without this the count
     * above stays wrong for a reason the test stopped covering the day the
     * implied free family starts taking this column too. */
    jaos_model *m2 = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m2));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m2, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m2, &p, nullptr));
    TEST_ASSERT_EQUAL_INT64(1, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.implied_free_col);
    jm_presolve_free(&p);
    jaos_model_free(m2);

    jaos_model_free(m);
#endif
}

/* -- Two singleton cols on one row -----------------------------------------
 *
 * min 3*x0 + x3 + 2*x4  s.t.  row0: x0 + x1 + x2 = 10   row1: x3 + x4 >= 1
 * x0 in [4, 4]; x1 in [0, 100], cost 0; x2 in [0, 3], cost 0;
 * x3, x4 in [0, 5].
 *
 * The stacked shape. Nothing stops it: the bounded singleton col at
 * src/presolve.c checks !free_col and never row_frozen[i], so x2 fires
 * on a row x1 has already frozen, in the same column pass. Each record
 * then records a DIFFERENT pair — x1's is [6, 6] (x0's shift only), x2's
 * is [-94, 6] (x1's relaxation too) — which is the case where "the later
 * replay undoes its own shift" is false and containment is what carries
 * it: a relaxation moves the two ends by cmax and cmin, and those are
 * not equal.
 *
 * This is also the shape that makes the OLD code abort rather than
 * answer: against the original [10, 10] pair, x2's intersection comes
 * out empty (want_lo = 10 > want_hi = 3), so assert(want_lo <= want_hi)
 * fires under the dev build the suite uses, and -DNDEBUG (how
 * bench/run is built) publishes x2 = 10 against a box of [0, 3] — a
 * COLUMN bound violation of 7.0 alongside the row's 4.0, which neither
 * other new test produces. row1 keeps the reduced model non-empty so
 * this runs on jm_postsolve_expand. By hand: x = {4, 6, 0, 1, 0},
 * obj = 13. */
/* A cost-0 singleton column open below, on a row that asks nothing from
 * below either: x0 free, x1 in (-inf, 1], row0 = x0 + x1 <= 10, no
 * costs; and the same with row0 free. The replay used to publish x1 at
 * its lower bound, which is -inf, and the row's activity as NaN, on an
 * answer that said OPTIMAL. Found by the IIS filter, whose re-solves are
 * all zero-cost models with relaxed bounds (D264). The point must be
 * finite, the checker must accept it, the row's activity must be the
 * sum of the published values, and the basis must have one basic. */
static void test_singleton_col_open_below_publishes_a_finite_point(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double rus[2] = {10.0, INFINITY};
    for (int shape = 0; shape < 2; shape++) {
        const double c[]  = {0.0, 0.0};
        const double cl[] = {-INFINITY, -INFINITY}, cu[] = {INFINITY, 1.0};
        const double rl[] = {-INFINITY}, ru[] = {rus[shape]};
        const int64_t s[]  = {0, 1, 2};
        const int64_t ix[] = {0, 0};
        const double v[]   = {1.0, 1.0};
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         2, s, ix, v));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        /* The shape reaches the family: presolve decides it whole. */
        TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

        double x[2], act[1], y[1], dj[2];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));
        TEST_ASSERT_TRUE_MESSAGE(isfinite(x[0]) && isfinite(x[1]),
                                 "a published value is not finite");
        TEST_ASSERT_TRUE(isfinite(act[0]));
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, x[0] + x[1], act[0]);
        TEST_ASSERT_TRUE(x[1] <= 1.0);
        TEST_ASSERT_TRUE(act[0] <= rus[shape]);

        jaos_check_report r;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
        TEST_ASSERT_TRUE(r.primal_feasible);
        TEST_ASSERT_TRUE(r.dual_feasible);

        jaos_basis_status cs[2], rs[1];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
        const int basics = (cs[0] == JAOS_BASIS_BASIC) +
                           (cs[1] == JAOS_BASIS_BASIC) +
                           (rs[0] == JAOS_BASIS_BASIC);
        TEST_ASSERT_EQUAL_INT(1, basics);
        jaos_model_free(m);
    }
#endif
}

static void test_two_singleton_cols_on_one_row(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {3.0, 0.0, 0.0, 1.0, 2.0};
    const double cl[] = {4.0, 0.0, 0.0, 0.0, 0.0};
    const double cu[] = {4.0, 100.0, 3.0, 5.0, 5.0};
    const double rl[] = {10.0, 1.0}, ru[] = {10.0, INFINITY};
    /* col0: row0.  col1: row0.  col2: row0.  col3: row1.  col4: row1. */
    const int64_t s[]  = {0, 1, 2, 3, 4, 5};
    const int64_t ix[] = {0, 0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 13.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[5], y[2], dj[5];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 6.0, expected_x2 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x2, &x[2], sizeof x[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    jaos_model_free(m);

    /* Both columns really did fire the family, on the same row: without
     * this the answer above stays right for a reason the test stopped
     * covering the day either one is caught by some other reduction. */
    jaos_model *m2 = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m2));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m2, 5, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m2, &p, nullptr));
    TEST_ASSERT_EQUAL_INT64(2, p.counts.singleton_col);
    jm_presolve_free(&p);
    jaos_model_free(m2);
#endif
}

/* -- Free column singleton -------------------------------------------------
 *
 * min 2*x0  s.t.  row0: x0 + x1 = 5
 *      min 3*x2 + 4*x3  s.t.  row1: x2 + x3 >= 1
 * x0 in [3, 3] (loaded fixed); x1 free ([-inf, inf]), cost 0; x2, x3 in
 * [0, 10] — two independent blocks in one model so row1 has a genuinely
 * nonzero dual to serve as the fault's victim, sharing nothing with
 * row0/x0/x1.
 *
 * x0 fixes first (FIXED_COL), dropping row0 to a singleton on x1 — which
 * is free and cost 0, so it fires as a MUTUAL singleton (row0 has no
 * other live entry either) rather than a plain singleton row: both row0
 * and x1 are removed by one JM_PS_FREE_COL_SINGLETON record. By hand:
 * x1 = 5 - 3 = 2 (row0's own bounds, both 5, minus x0's fixed
 * contribution); row0's dual and x1's reduced cost are always exactly 0
 * for this family (the file header's derivation). row1's block solves
 * independently: x2 = 1 (cheaper, pushed to satisfy row1 exactly),
 * x3 = 0, row1's dual = 3 (from x2's own stationarity, x2 interior to
 * [0,10]). Total objective: 2*3 + 3*1 + 4*0 = 9. */
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_free_col_singleton_model(void)
{
    const double c[]  = {2.0, 0.0, 3.0, 4.0};
    const double cl[] = {3.0, -INFINITY, 0.0, 0.0};
    const double cu[] = {3.0, INFINITY, 10.0, 10.0};
    const double rl[] = {5.0, 1.0}, ru[] = {5.0, INFINITY};
    /* col0 (x0): row0.  col1 (x1): row0.  col2 (x2): row1.  col3 (x3): row1. */
    const int64_t s[]  = {0, 1, 2, 3, 4};
    const int64_t ix[] = {0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_free_col_singleton_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_free_col_singleton_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 9.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 2.0, expected_y0 = 0.0, expected_dj1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj1, &dj[1], sizeof dj[1]);

    /* BASIC, not FREE: x1's value (2.0) is not zero, and JAOS_BASIS_FREE
     * is specifically "nonbasic at zero" — a JAOS_NO_PRESOLVE run of this
     * same model agrees (confirmed directly). */
    jaos_basis_status cs[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* Negative sibling: row0's record restores at index 1 instead of 0 —
 * row1's slot — overwriting row1's genuinely nonzero dual (3.0) with
 * row0's own answer (always 0.0). row1's own activity sits exactly at
 * its lower bound, which tolerates any nonnegative dual including the
 * wrong zero (the same shape as the empty-row fault above), so this is
 * caught on x2's own side instead: x2 is basic (interior to [0,10]) and
 * needs a reduced cost of exactly zero, which the checker recomputes
 * from the corrupted row1 dual it was handed — 3 - 1*0 = 3, not 0. */
static void test_free_col_singleton_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_free_col_singleton_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[4], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* -- Implied free column singleton (D105) ---------------------------------
 *
 * min 3*x0 + x1 + x2
 *   row0 (==):  x0 + x1 + x2 = 6
 *   row1 (>=):  x1 + x2 >= 1
 *   x1, x2 in [0, 2]
 *
 * x0 has one matrix entry and it is in row0, an equality. The rest of row0
 * reaches [0, 4], so the row already confines x0 to [2, 6]. Whether the
 * family fires is decided by x0's own box alone, and the three models below
 * differ in nothing else:
 *
 *   lower bound   implied box    fires   optimum
 *   -100          [2, 6]         yes     x0 = 2, obj 10
 *      4          [2, 6]         no      x0 = 4, obj 14
 *      2          [2, 6]         no      x0 = 2, obj 10
 *
 * The middle one is the case the family MUST refuse. Substituting there
 * drops the bound x0 >= 4, which is the bound the optimum rests on: the
 * relaxed model answers 10 at x0 = 2, a full 4 below the true optimum and
 * one unit outside x0's own declared box. Nothing about that announces
 * itself — the objective is better, not worse, and a -DJAOS_NO_PRESOLVE
 * build would be the one reporting the larger number.
 *
 * The last one is the margin's own canary. The row implies exactly x0 >= 2
 * and the column declares exactly x0 >= 2, so a margin of zero fires and any
 * margin above zero declines. It is written down here because the same flip
 * is what the sweep reads on `maros-r7`: 4 of its 984 candidate rows sit at
 * this exact equality, so 984 at zero and 980 at anything above it.
 *
 * By hand, after substitution on the first model: y0 = c0/a = 3, the two
 * surviving costs become 1 - 3 = -2 each, and the offset takes 3*6 = 18. The
 * reduced problem is min -2*x1 - 2*x2 + 18 over the same row1, whose optimum
 * puts both at their upper bound of 2, so x0 = 6 - 4 = 2 and the objective
 * is 18 - 8 = 10. Every quantity is an integer a double holds exactly, so
 * these are the same numbers a presolve-off build computes. */
static jaos_model *make_implied_free_col_model(double x0_lower)
{
    const double c[]  = {3.0, 1.0, 1.0};
    const double cl[] = {x0_lower, 0.0, 0.0};
    const double cu[] = {100.0, 2.0, 2.0};
    const double rl[] = {6.0, 1.0}, ru[] = {6.0, INFINITY};
    /* col0: row0.  col1: rows 0,1.  col2: rows 0,1. */
    const int64_t s[]  = {0, 1, 3, 5};
    const int64_t ix[] = {0,   0, 1,   0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    return m;
}

static void test_an_implied_free_column_is_substituted_out(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_implied_free_col_model(-100.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[2], dj[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double ex0 = 2.0, ex1 = 2.0, ex2 = 2.0;
    const double ey0 = 3.0, ey1 = 0.0, ed0 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&ex0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&ex1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&ex2, &x[2], sizeof x[2]);
    TEST_ASSERT_EQUAL_MEMORY(&ey0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&ey1, &y[1], sizeof y[1]);
    /* x0 is strictly inside its own box, so nothing but zero is permitted
     * here — this is the whole reason the elimination is exact. */
    TEST_ASSERT_EQUAL_MEMORY(&ed0, &dj[0], sizeof dj[0]);

    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    /* jaos.h promises exactly num_row of the num_col + num_row statuses are
     * basic, and this record restores one row and one column, so it has to
     * add back exactly one basic entry between the two. Pinned here because
     * a family that publishes one basic too many costs the next solve its
     * warm start silently (TODO.md carries the singleton column's own
     * version of this defect). */
    int64_t nbasic = 0;
    for (int64_t k = 0; k < 3; k++)
        if (cs[k] == JAOS_BASIS_BASIC) nbasic++;
    for (int64_t k = 0; k < 2; k++)
        if (rs[k] == JAOS_BASIS_BASIC) nbasic++;
    TEST_ASSERT_EQUAL_INT64(2, nbasic);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* The case the family must reject, and the reason it is worth a test of its
 * own: firing here is not caught by anything else in this suite. The answer
 * comes back OPTIMAL, the checker is handed a point that satisfies every
 * row, and the objective is BETTER than the truth. */
static void test_an_implied_bound_outside_the_box_is_refused(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_implied_free_col_model(4.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 14.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    /* x1 and x2 are not separately determined here — every split of their
     * sum costs the same — so only x0 is asserted, and it is the one the
     * refused reduction would have got wrong. */
    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double ex0 = 4.0;
    TEST_ASSERT_EQUAL_MEMORY(&ex0, &x[0], sizeof x[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* The warm start a correct basis used to lose to the mapping (D143, D144,
 * retried and landed after D148's certificate guard).
 *
 * x0 is a cost-0 bounded singleton on row 0, so presolve removes it and the
 * first solve publishes it BASIC at an interior recovery with row 0's
 * logical swapped out to a bound — an EXACT count of 2. On the re-solve the
 * same reduction removes x0 again, the mapping drops its BASIC status, and
 * the reduced start reaches build_warm_basis one member SHORT. Before the
 * repair that was a silent cold fallback; now a logical is promoted — on
 * this model by the FIXED-ORDER loop, not the uncovered-rows one: the
 * surviving basic column touches both reduced rows, so nothing is
 * uncovered, and row 0's logical is the first nonbasic one in index order.
 * It is also the exact member the first solve's swap took out. The
 * uncovered-rows branch has no small test; the campaign reaches it on
 * `pilot-we` and `ship08l` (the table below), and TODO.md carries no debt
 * for one. */
static void test_a_short_mapped_basis_is_repaired_and_warm_survives(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("the mapping under test does not exist without presolve");
#else
    /* col0 (x0): row0 only, cost 0.  col1, col2: rows 0 and 1, cost 1. */
    const double c[]  = {0.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, 10.0};
    const double rl[] = {5.0, 3.0}, ru[] = {8.0, INFINITY};
    const int64_t s[]  = {0, 1, 3, 5};
    const int64_t ix[] = {0,   0, 1,   0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const double obj1 = 3.0;   /* min x1+x2 with x1+x2 >= 3 */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&obj1, &obj, sizeof obj);

    /* The scenario's premise, asserted rather than assumed: the publish is
     * exact (2 basics for 2 rows) and x0 is the interior BASIC member the
     * mapping will drop. */
    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    int64_t nb = 0;
    for (int64_t k = 0; k < 3; k++) nb += cs[k] == JAOS_BASIS_BASIC;
    for (int64_t k = 0; k < 2; k++) nb += rs[k] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, nb);

    /* One bound moves; the answer goes, the stored basis stays. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_row_bounds(m, 1, 4.0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const double obj2 = 4.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&obj2, &obj, sizeof obj);

    /* The pin that says the warm start survived. The repaired mapped basis
     * is the re-solve's own optimal basis, so the warm solve prices and
     * stops — and D148's certificate reads exactly zero on it, so no cold
     * restart intervenes. On the tree without the repair this reads the
     * cold count instead (validated by running exactly that tree), and a
     * future change that costs this warm start again will move it. */
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    jaos_model_free(m);
#endif
}

/* Both sides of WARM_REPAIR_MAX_SHORT (D151). The constant is a threshold, so
 * a test that only shows the repair firing would pass with the cap removed
 * entirely; the case it must REJECT is the half that pins the number.
 *
 * The observable is the repair's own DETAIL log line rather than an iteration
 * count, and that choice is measured rather than stylistic: on models this
 * small the warm and cold counts coincide by accident — the same construction
 * reads warm == cold at k = 2 and k = 4 while the repair fires at both.
 *
 * The construction replicates the block above k times. One block is 3 columns
 * and 2 rows sharing nothing with any other, and each contributes exactly one
 * interior BASIC singleton column for the mapping to drop, so the shortfall is
 * k. That is not assumed: the boundary below lands at k = 4 repairing and
 * k = 5 refusing, which is the constant, and any other shortfall-per-block
 * would move it. */
/* The three below exist for test_the_warm_repair_stops_at_its_cap alone, and
 * that test's body compiles out under the reference build and under either
 * fault build. Without the same guard here they are defined and never called,
 * and -Werror=unused-function fails the build outright — which is what three
 * of the five configurations did between D151 and 2026-08-19, silently,
 * because make hands back the last binary when no source file changed.
 * `make configs` builds all five now and is what catches the next one. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && \
    !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL) && !defined(JAOS_NO_PRESOLVE)
static int g_warm_repairs;

static void count_warm_repair(void *user, jaos_log_level level,
                              const char *line)
{
    (void)user;
    (void)level;
    if (strstr(line, "arrived short and was repaired") != nullptr)
        g_warm_repairs++;
}

/* Returns whether the repair fired on a k-block model's warm re-solve.
 *
 * On a FAILING assert Unity longjmps out of here, so the ten allocations and
 * the model leak and an ASan build will attach about eleven LSan reports to
 * the failure. That is the test failing, not a memory defect in the solver —
 * read the assertion first (found in review). */
static int repair_fires_at(int k)
{
    const int64_t nrow = 2 * k, ncol = 3 * k, nnz = 5 * k;
    double *c = calloc((size_t)ncol, sizeof *c);
    double *cl = calloc((size_t)ncol, sizeof *cl);
    double *cu = calloc((size_t)ncol, sizeof *cu);
    double *rl = calloc((size_t)nrow, sizeof *rl);
    double *ru = calloc((size_t)nrow, sizeof *ru);
    int64_t *st = calloc((size_t)ncol + 1, sizeof *st);
    int64_t *ix = calloc((size_t)nnz, sizeof *ix);
    double *va = calloc((size_t)nnz, sizeof *va);
    /* All eight, not just the last: the fill loop below writes through every
     * one of them, so checking only `va` leaves seven null dereferences the
     * arrays being tiny does not rule out (found in review). */
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu);
    TEST_ASSERT_NOT_NULL(rl);
    TEST_ASSERT_NOT_NULL(ru);
    TEST_ASSERT_NOT_NULL(st);
    TEST_ASSERT_NOT_NULL(ix);
    TEST_ASSERT_NOT_NULL(va);

    int64_t nz = 0;
    for (int64_t b = 0; b < k; b++) {
        const int64_t r0 = 2 * b, r1 = 2 * b + 1, j0 = 3 * b;
        c[j0] = 0.0;     cl[j0] = 0.0;     cu[j0] = 10.0;
        c[j0 + 1] = 1.0; cl[j0 + 1] = 0.0; cu[j0 + 1] = 10.0;
        c[j0 + 2] = 1.0; cl[j0 + 2] = 0.0; cu[j0 + 2] = 10.0;
        rl[r0] = 5.0; ru[r0] = 8.0;
        rl[r1] = 3.0; ru[r1] = INFINITY;
        ix[nz] = r0; va[nz] = 1.0; nz++;            /* the singleton */
        st[j0 + 1] = nz;
        ix[nz] = r0; va[nz] = 1.0; nz++;
        ix[nz] = r1; va[nz] = 1.0; nz++;
        st[j0 + 2] = nz;
        ix[nz] = r0; va[nz] = 1.0; nz++;
        ix[nz] = r1; va[nz] = 1.0; nz++;
        st[j0 + 3] = nz;
    }

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ncol, nrow, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, st, ix, va));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* The premise, asserted rather than assumed: the publish is exact and
     * every block leaves its singleton interior BASIC, so the mapping is
     * about to drop exactly k members. */
    jaos_basis_status *cs = calloc((size_t)ncol, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nrow, sizeof *rs);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t nb = 0, singles = 0;
    for (int64_t j = 0; j < ncol; j++) nb += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < nrow; i++) nb += rs[i] == JAOS_BASIS_BASIC;
    for (int64_t b = 0; b < k; b++) singles += cs[3 * b] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(nrow, nb);
    TEST_ASSERT_EQUAL_INT64(k, singles);

    g_warm_repairs = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_log_callback(m, count_warm_repair, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 1, 4.0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const int fired = g_warm_repairs;
    jaos_model_free(m);
    free(c); free(cl); free(cu); free(rl); free(ru);
    free(st); free(ix); free(va); free(cs); free(rs);
    return fired;
}
#endif

static void test_the_warm_repair_stops_at_its_cap(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("the mapping under test does not exist without presolve");
#else
    /* WARM_REPAIR_MAX_SHORT lives at file scope in src/simplex.c and no test
     * can read it, so the shipping value 4 is written here as a literal. That
     * makes this a pinned change-detector test on purpose: moving the
     * constant fails these four lines, and whoever moves it has to come here,
     * re-read the sweep in simplex.c and re-pin deliberately. */
    TEST_ASSERT_TRUE(repair_fires_at(1));
    TEST_ASSERT_TRUE(repair_fires_at(4));

    /* One past the cap the repair is refused and the solve falls back to
     * cold. This is the half the constant exists for: delete the cap and
     * these two fail, raise it and they fail. */
    TEST_ASSERT_FALSE(repair_fires_at(5));
    TEST_ASSERT_FALSE(repair_fires_at(6));
#endif
}

/* A basis that maps LONG onto the reduced model falls back to cold, and this
 * pins it. The construction: a caller basis with an exact orig-space count
 * (so jaos_set_basis accepts it) on a model where presolve removes a row
 * whose stored logical is NONBASIC and drops no stored-basic column — the
 * mapped count equals the orig count while the reduced model has one row
 * fewer, and build_warm_basis refuses the excess. D145 is why this stays a
 * refusal and is pinned: a warm start from a repaired-by-count basis made
 * eight instances publish a wrong objective as optimal, so any future
 * count-repair or trim has to move this test deliberately, behind a fix for
 * the termination defect that let those eight through. */
static void test_a_long_mapped_basis_falls_back_cold(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("the mapping under test does not exist without presolve");
#else
    /* row0 = x0 + x1 <= 100 is redundant on the boxes and is removed;
     * row1 = x0 + x1 >= 3 is the reduced problem. */
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {-INFINITY, 3.0}, ru[] = {100.0, INFINITY};
    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {0, 1,   0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    /* Exact in orig space: two basics for two rows, both logicals out. */
    const jaos_basis_status cs[] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    const jaos_basis_status rs[] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_AT_LOWER};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const double eobj = 3.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&eobj, &obj, sizeof obj);

    /* The premise, asserted rather than assumed: the redundant row WAS
     * removed, so the two mapped basics were judged against one reduced
     * row. Without this line a change that stops the removal would leave
     * the pin below green for the wrong reason (an exact map, warmed). */
    TEST_ASSERT_EQUAL_INT64(1, m->presolve_num_row);

    /* The cold count, pinned. */
    TEST_ASSERT_EQUAL_INT64(1, jaos_iterations(m));

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_NO_PRESOLVE)
/* The three counts side by side, which is the only place the middle model's
 * refusal and the canary's refusal can be told apart from "the family is not
 * built yet". A change that stops the family firing at all still passes both
 * value tests above; it fails here. */
static void test_the_implied_free_counter_reads_its_three_models(void)
{
    const double lowers[3] = {-100.0, 4.0, 2.0};
    const int64_t expect[3] = {1, 0, 0};
    for (int t = 0; t < 3; t++) {
        jaos_model *m = make_implied_free_col_model(lowers[t]);
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT64(expect[t], p.counts.implied_free_col);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
}
#endif

/* -- A range row whose shifted bounds collapse is not an equality ----------
 *
 * min x1
 *   row0:  1e17*x0 + x1 + x2  in [1, 2]     x0 fixed at 1, x1 cost 1
 *   row1:        x2 + x3      >= 0
 *   x1 in [-1e18, 1e18], x2 and x3 in [0, 1]
 *
 * x0 is fixed as loaded, so it leaves in the first column pass and takes
 * exactly 1e17 off both of row0's bounds. `ulp(1e17)` is 16, so `1 - 1e17`
 * and `2 - 1e17` are the same double: the row's own width is gone and
 * `cur_rl == cur_ru` reads true on a row the caller wrote as a range.
 *
 * x1 is then a degree-1 column with a nonzero cost, so the D95 families
 * decline it and it reaches the implied free column singleton. Testing only
 * the current pair, that family fired and pinned an activity the model had
 * only bounded. It now tests the ORIGINAL pair as well and declines.
 *
 * x2 keeps a live entry in row1 so row0 never drops to degree 1, which is
 * what stops the singleton-row fold from consuming it first and makes the
 * counter below say something about this family rather than that one.
 *
 * **The answer this model publishes is wrong either way, and that is not
 * what is asserted.** The width was destroyed by the shift, before any
 * family looked at the row. What is asserted is the family staying inside
 * the scope its measurement covers. `TODO.md` carries the shift.
 */
#if !defined(JAOS_NO_PRESOLVE)
static void test_a_range_row_that_shifted_into_an_equality_is_declined(void)
{
    const double c[]  = {0.0, 1.0, 0.0, 0.0};
    const double cl[] = {1.0, -1e18, 0.0, 0.0};
    const double cu[] = {1.0, 1e18, 1.0, 1.0};
    const double rl[] = {1.0, 0.0}, ru[] = {2.0, INFINITY};
    /* col0: row0.  col1: row0.  col2: rows 0,1.  col3: row1. */
    const int64_t s[]  = {0, 1, 2, 4, 5};
    const int64_t ix[] = {0,   0,   0, 1,   1};
    const double v[]   = {1e17, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));

    /* The shift really does collapse the pair — asserted rather than
     * assumed, because a test that stops discriminating when a coefficient
     * changes is worse than no test. */
    TEST_ASSERT_EQUAL_DOUBLE(1.0 - 1e17, 2.0 - 1e17);

    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);      /* x0 did leave */
    TEST_ASSERT_EQUAL_INT64(0, p.counts.implied_free_col);
    jm_presolve_free(&p);
    jaos_model_free(m);
}
#endif

/* The canary's own answer, so a margin change that flips it cannot also
 * change what the model publishes without being noticed. */
static void test_an_implied_bound_at_exact_equality_is_declined(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_implied_free_col_model(2.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double ex0 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&ex0, &x[0], sizeof x[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* -- The row activity two older families were leaving short ------------------
 *
 * min x2
 *   row0 (==):  x0 + x1 = 10          x0 free, cost 0
 *   row1 (>=):  x1 + x2 >= 1          x1 in [0, 1], x2 in [0, 0.5], cost 0/1
 *
 * x0 is free, cost 0 and the only entry in row0, but row0 has a second live
 * entry, so the mutual-singleton branch declines it and it falls through to
 * the implied free column singleton, which fires with y0 = 0. x1 is then a
 * bounded cost-0 singleton in row1 and leaves by JM_PS_SINGLETON_COL.
 *
 * That is the ordering the defect needs. x1 was live in row0 when the
 * substitution fired, so row0's recorded bound does NOT net it out, and
 * JM_PS_SINGLETON_COL used to write only its own row — leaving row0's
 * activity short by x1's share at the moment the substitution's own replay
 * divides by it.
 *
 * Before the repair this published x0 = 10 and missed row0 by 1.0. The
 * objective is 0 either way and correct either way, and every column sits
 * inside its own box either way, so nothing but a row could catch it. That is
 * why it survived three campaigns: no digest covers a row activity
 * (bench/run.c hashes the columns and the duals) and the checker recomputes
 * its own from the columns it is handed.
 *
 * Written from numerics-reviewer's own reproducer, 2026-08-15, after it
 * pointed out the repair had landed with the gate's checker column on
 * greenbeb, modszk1 and tuff as its whole cover. */
static void test_a_removed_column_pays_every_row_it_touches(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0, 0.0, 1.0};
    const double cl[] = {-INFINITY, 0.0, 0.0};
    const double cu[] = {INFINITY, 1.0, 0.5};
    const double rl[] = {10.0, 1.0}, ru[] = {10.0, INFINITY};
    /* col0: row0.  col1: rows 0,1.  col2: row1. */
    const int64_t s[]  = {0, 1, 3, 4};
    const int64_t ix[] = {0,   0, 1,   1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], row[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, row, y, nullptr));

    /* The published row activity, which is the thing that was wrong and the
     * one output no predicate of the three instance sets reads. Asserted
     * before the values, because a solve that gets x right and row0 wrong is
     * exactly what shipped. */
    const double erow0 = 10.0;
    TEST_ASSERT_EQUAL_MEMORY(&erow0, &row[0], sizeof row[0]);

    /* And the same number recomputed from the columns, which is what the
     * checker would do. The two agreeing is the whole point. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, x[0] + x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* Negative sibling: the record's row index reads 1 instead of 0, so row0's
 * multiplier of 3 lands on row1 and row0 keeps the zero it was initialised
 * with. x0's own value survives that unharmed — row1 carries the same two
 * columns, so the sum subtracted comes out the same — and the fault is
 * caught on x0's reduced cost instead. The checker recomputes it from the
 * duals it was handed: 3 - 1*0 = 3, against a column sitting strictly
 * inside its own box, where the only permitted value is zero. */
static void test_implied_free_col_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_implied_free_col_model(-100.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* -- Reduced to nothing: the cases no existing test in this tree reaches -- */

/* A model presolve proves infeasible before the simplex ever runs: an
 * empty row whose bounds exclude zero. jaos_load_lp accepts it without
 * complaint (deciding feasibility is the solver's job, not the loader's)
 * and jaos_solve reports the refusal as a solve outcome. */
static void test_empty_row_reports_infeasible(void)
{
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {1.0}, ru[] = {2.0};   /* excludes zero */
    const int64_t s[] = {0, 0};                /* col0 touches no row */
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     0, s, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    /* jaos_basis refuses to publish a basis behind a refusal. */
    jaos_basis_status cs[1], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));

    jaos_model_free(m);
}

/* A zero-row model: every column is empty by construction (there is
 * nowhere for an entry to point), so presolve fixes each one at its own
 * favourable bound and the whole model resolves with no simplex run,
 * publishing a complete, checked answer. */
static void test_zero_row_model_solves(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build; "
                        "only one column exists for the offset to land on, "
                        "so the fault would read out of bounds rather than "
                        "corrupt a real slot");
#else
    const double c[]  = {1.0, -1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 5.0};
    const int64_t s[] = {0, 0, 0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 0, JAOS_MINIMIZE, 0.0, c, cl, cu, nullptr, nullptr,
                     0, s, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = -5.0;   /* 1*0 + (-1)*5 */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);

    jaos_basis_status cs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(0, basic);   /* == num_row */

    jaos_model_free(m);
#endif
}

/* A zero-column model: every row is empty by construction, so presolve
 * either proves infeasible (if some row's bounds exclude zero) or drops
 * them all, publishing a complete, checked answer with no columns to
 * report and no simplex run. */
static void test_zero_col_model_solves(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build; "
                        "only two rows exist, the second offset would read "
                        "out of bounds rather than corrupt a real slot");
#else
    const double rl[] = {-1.0, 0.0}, ru[] = {1.0, 0.0};
    const int64_t s[] = {0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 0, 2, JAOS_MINIMIZE, 0.0, nullptr, nullptr, nullptr,
                     rl, ru, 0, s, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = 0.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, nullptr, nullptr, y, nullptr));
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, nullptr, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, rs));
    int64_t basic = 0;
    for (int64_t i = 0; i < 2; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, basic);   /* == num_row: both rows are
                                          * empty and every empty row
                                          * publishes basic */

    jaos_model_free(m);
#endif
}

/* D-13's white-box counters, for every family this plan adds, on one
 * model built to fire each exactly once (except the plain FIXED_COL
 * this plan does not add new, and pins at 1 since col5's own load is
 * fixed as loaded, 02-01's family).
 *
 * Rows:
 *   row0: empty (no column touches it).
 *   row1, row2: the coupling pair col0/col1 both survive in, degree >=2
 *               throughout, never singleton or empty.
 *   row3: singleton on col3 (folds a bound); col3 also touches row2, so
 *         it does not go empty once row3 is gone — the trap the first
 *         version of this test fell into.
 *   row4: col0 (untouched, degree survives at 3) and col4 (singleton,
 *         bounded, cost 0) — col4's own removal relaxes and freezes
 *         row4, so it is never re-examined by the row pass at degree 1.
 *   row5: col5 (loaded fixed) and col6 (free, cost 0) — col5 fixes
 *         first, dropping row5 to a mutual singleton with col6.
 *
 * Columns: col0 [0,10] cost 1 (rows 1,2,4); col1 [0,10] cost 1 (rows
 * 1,2); col2 [0,5] cost -1 (empty); col3 [0,5] cost 2 (rows 2,3); col4
 * [1,10] cost 0 (row 4); col5 [1,1] cost 5 (row 5); col6 free cost 0
 * (row 5). */
static void test_all_five_counters_move_independently(void)
{
    const double c[]  = {1.0, 1.0, -1.0, 2.0, 0.0, 5.0, 0.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, -INFINITY};
    const double cu[] = {10.0, 10.0, 5.0, 5.0, 10.0, 1.0, INFINITY};
    const double rl[] = {-1.0, 1.0, -INFINITY, -10.0, -INFINITY, 2.0};
    const double ru[] = {1.0, INFINITY, 100.0, INFINITY, 10.0, 2.0};
    /* col0: rows 1,2,4.  col1: rows 1,2.  col2: none.  col3: rows 2,3.
     * col4: row 4.  col5: row 5.  col6: row 5. */
    const int64_t s[]  = {0, 3, 5, 5, 7, 8, 9, 10};
    const int64_t ix[] = {1, 2, 4,   1, 2,   2, 3,   4,   5,   5};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 7, 6, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     10, s, ix, v));

    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));

    TEST_ASSERT_EQUAL_INT64(1, p.counts.empty_row);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.singleton_row);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.free_col_singleton);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);   /* col5, loaded fixed */
    /* 02-04: row2 (-inf <= col0 + col1 + col3 <= 100) has a maximum
     * activity of 25 against an upper bound of 100 and no lower bound at
     * all, so it can never bind and the redundant-row family drops it.
     * col3 then loses its last live row and goes empty, which is why the
     * empty-column count is 2 here and was 1 before this plan. Neither is
     * a new reduction firing where it should not: the model was written to
     * keep col0 and col1 at degree >= 2, and it still does — col0 survives
     * in rows 1 and 4, col1 in row 1. */
    TEST_ASSERT_EQUAL_INT64(1, p.counts.redundant_row);
    TEST_ASSERT_EQUAL_INT64(2, p.counts.empty_col);

    jm_presolve_free(&p);
    jaos_model_free(m);
}

/* -- Forcing row ----------------------------------------------------------
 *
 * min -x0 - x1 + x2 + 3*x3
 *   row0: x0 + x1 <= 0        x0, x1 in [0, 10]
 *   row1: x2 + x3 >= 1        x2, x3 in [0, 10]
 *
 * row0's minimum activity is 0 and its upper bound is 0, so the row is
 * forced: every column in it is pinned at the bound that attains that
 * minimum, which for a positive coefficient is the column's own lower
 * bound. Both are pinned at 0, the row is removed, and row1 is left for the
 * simplex.
 *
 * The dual is what this model exists to test. By hand: row1 rests at its
 * lower bound with y1 = 1, x2 is basic at 1 and x3 nonbasic at 0. Row0's
 * own multiplier has to be -1: x0 is pinned at its LOWER bound and needs a
 * non-negative reduced cost, and c0 - a*y0 = -1 - y0 is non-negative only
 * for y0 <= -1. Publishing y0 = 0 instead — the value a family that did not
 * derive its dual would leave behind — makes that reduced cost -1 on a
 * variable resting at its lower bound, which the checker refuses at exactly
 * that magnitude. So this test distinguishes the derivation from the
 * absence of one. */
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_forcing_row_model(void)
{
    const double c[]  = {-1.0, -1.0, 1.0, 3.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 10.0, 10.0};
    const double rl[] = {-INFINITY, 1.0};
    const double ru[] = {0.0, INFINITY};
    /* col0: row0.  col1: row0.  col2: row1.  col3: row1. */
    const int64_t s[]  = {0, 1, 2, 3, 4};
    const int64_t ix[] = {0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_forcing_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_forcing_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 1.0;   /* x2 = 1, everything else at 0 */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    const double expected_x0 = 0.0, expected_x1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    /* The derived multiplier, asserted directly rather than left to the
     * checker alone — the checker would accept any y0 <= -1, and this is
     * the one the derivation names. */
    const double expected_y0 = -1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[4], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    /* The basis behind that multiplier. y0 = -1 is nonzero, so row0's
     * logical is NOT basic: it rests at the upper bound the range attained,
     * and the pinned column whose ratio set y0 is basic in its place, with
     * a zero reduced cost (D257). Until D257 postsolve published the row
     * basic beside its nonzero multiplier, a pair no basic solution has,
     * and this assertion was presolve-side only because the un-presolved
     * simplex already published the row at its upper bound. Both builds
     * now agree, so it holds for both, and so does the count. */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[0]);   /* the forcing row */
    TEST_ASSERT_TRUE(cs[0] == JAOS_BASIS_BASIC || cs[1] == JAOS_BASIS_BASIC);
    int64_t basic = 0;
    for (int64_t j = 0; j < 4; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 2; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, basic);   /* == num_row */

    jaos_model_free(m);
#endif
}

/* Negative sibling (D-10). Under JAOS_PRESOLVE_FAULT_OFFBYONE every record's
 * restore index moves on by one: row0's forcing record lands on row1 and its
 * two column records land on x1 and x2, so x2's own solved value of 1 is
 * overwritten with 0 and row1's activity falls to 0 against a lower bound of
 * 1. The primal side is where this shows, and it shows at full size. */
static void test_forcing_row_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_forcing_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[4], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.primal_feasible);

    jaos_model_free(m);
#endif
}

/* -- Redundant row --------------------------------------------------------
 *
 * min x0 + 3*x1
 *   row0: x0 + x1 >= 1        x0, x1 in [0, 10]
 *   row1: x0 + x1 <= 100
 *
 * row1's maximum activity is 20 against an upper bound of 100 and it has no
 * lower bound at all, so it can never bind and is dropped with no column
 * fixed. Its multiplier is zero, which is the whole reason this family needs
 * no derivation: a zero multiplier satisfies the checker's sign condition
 * unconditionally, wherever the activity turns out to sit.
 *
 * By hand: x0 = 1, x1 = 0, objective 1, y0 = 1, y1 = 0. x0 is basic and
 * interior to [0, 10], so its reduced cost has to be exactly zero —
 * c0 - y0 - y1 = 1 - 1 - 0 — which is what makes the fault below visible. */
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_redundant_row_model(void)
{
    const double c[]  = {1.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {1.0, -INFINITY};
    const double ru[] = {INFINITY, 100.0};
    /* col0: rows 0,1.  col1: rows 0,1. */
    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {0, 1, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_redundant_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_redundant_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 1.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    const double expected_y1 = 0.0;   /* the redundant row's only answer */
    TEST_ASSERT_EQUAL_MEMORY(&expected_y1, &y[1], sizeof y[1]);

    /* The dropped row's activity is reported, not left at zero: it is the
     * one thing about a redundant row that has to be summed rather than
     * assumed, and it comes from columns the reduced solve returned. */
    double rowact[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, rowact, y, nullptr));
    const double expected_act1 = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_act1, &rowact[1], sizeof rowact[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
#if !defined(JAOS_NO_PRESOLVE)
    /* Presolve-side only, for the reason the forcing-row test gives. */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[1]);   /* the redundant row */
#endif
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 2; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, basic);   /* == num_row */

    jaos_model_free(m);
#endif
}

/* Negative sibling (D-10): row1's record restores at row0's slot instead of
 * its own, overwriting row0's genuinely nonzero multiplier (1) with the
 * redundant row's always-zero one. x0 is basic and interior, so its reduced
 * cost must be exactly zero and the checker recomputes it from the row duals
 * it was handed: 1 - 0 - 0 = 1, a dual violation of exactly the multiplier
 * that was lost. */
static void test_redundant_row_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_redundant_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* -- The two branches one step apart --------------------------------------
 *
 * min x0 + x1
 *   row0: x0 >= B             x0 in [0, 5]
 *   row1: x0 + x1 >= 6        x1 in [0, 10]
 *
 * row0 is a singleton, so it folds into a bound on x0: x0 >= B. What that
 * meets is x0's own opposite bound, 5, and the three values of B below are
 * the three answers presolve has to give.
 *
 *   B = 5             the fold lands exactly ON the opposite bound. The
 *                     interval collapses to a point, which is a legitimate
 *                     reduction: x0 is fixed at 5 and the model has an
 *                     optimum. Refusing here turns a solvable model into a
 *                     refused one, which is the mirror-image catastrophe of
 *                     accepting an infeasible one.
 *   B = 5 + 1e-6      one step PAST it, by more than the epsilon. The
 *                     interval is empty and the model is infeasible.
 *   B = nextafter(5)  one representable step past it, which is 8.9e-16 —
 *                     one ulp of 5, against a window of eight of them.
 *                     Presolve treats it as a point, and so does the
 *                     un-presolved solve, whose own primal tolerance is
 *                     1e-7. The two builds agree, which is the property
 *                     that matters. This is the case that would break first
 *                     if PRESOLVE_ROUND_ULPS were cut to 1.
 *
 * The plan asked for the middle case to be "one representable step" past.
 * It cannot be: an epsilon that could separate 5 from nextafter(5) would be
 * smaller than the rounding in the row-bound shifts that produce B, so the
 * third case is here instead, pinning what the epsilon actually buys. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_singleton_fold_boundary_model(double b)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 10.0};
    const double rl[] = {b, 6.0};
    const double ru[] = {INFINITY, INFINITY};
    /* col0: rows 0,1.  col1: row1. */
    const int64_t s[]  = {0, 2, 3};
    const int64_t ix[] = {0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}
#endif

static void test_a_fold_onto_the_opposite_bound_fixes_the_column(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_fold_boundary_model(5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 6.0;   /* x0 = 5, x1 = 1 */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double expected_x0 = 5.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_a_fold_one_step_past_the_opposite_bound_is_infeasible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_fold_boundary_model(5.0 + 1e-6);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

static void test_a_fold_inside_the_epsilon_does_not_flip_the_verdict(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_fold_boundary_model(nextafter(5.0, 6.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    /* The claim is that this holds under BOTH builds — run again under
     * EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE and the un-presolved simplex has to
     * agree, since its own primal tolerance is 1e-7 and the conflict is
     * 8.9e-16. A presolve that refused here would disagree with the solver
     * it is supposed to be transparent to. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, obj);
    jaos_model_free(m);
#endif
}

/* -- An optimum sitting exactly on the tightening boundary ----------------
 *
 * min x0 + x1 + x2
 *   row0: x0 + x1 >= 10       x0 in [-100, 100]
 *   row1: x0 + x2 <= 7        x1, x2 in [0, 3]
 *
 * The activity ranges imply x0 >= 10 - 3 = 7 from row0 and x0 <= 7 - 0 = 7
 * from row1, so the true optimum sits exactly on the boundary the tightening
 * computes: x0 = 7, x1 = 3, x2 = 0, objective 10, and it is unique. This is
 * T-02-11's own test — a bound rounded the wrong way by one epsilon excludes
 * that point, and the answer that comes back is feasible, checker-clean and
 * simply not optimal, which nothing else in this suite would notice.
 *
 * Run under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE the same assertion has to hold
 * with no tightening in the picture at all, which is what makes it a
 * comparison rather than a self-report. */
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_tightening_boundary_model(void)
{
    const double c[]  = {1.0, 1.0, 1.0};
    const double cl[] = {-100.0, 0.0, 0.0}, cu[] = {100.0, 3.0, 3.0};
    const double rl[] = {10.0, -INFINITY};
    const double ru[] = {INFINITY, 7.0};
    /* col0: rows 0,1.  col1: row0.  col2: row1. */
    const int64_t s[]  = {0, 2, 3, 4};
    const int64_t ix[] = {0, 1, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_an_optimum_on_the_tightening_boundary_survives(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_tightening_boundary_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    /* Not bit-exact, and deliberately so: where the collapse fires, x0 is
     * the midpoint of an interval two outward roundings wide, which is 7 to
     * within half an ulp rather than 7 exactly. The gate's own acceptance
     * is relative at 1e-6 (docs/tolerances.md); this is three decades
     * inside it. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, obj);

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* D-13's white-box test for the three families 02-04 adds: each counter
 * reads an exact integer on a model built to fire that outcome, not a floor
 * a single reduction would also satisfy. */
static void test_activity_range_counters_are_exact(void)
{
#if defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("model builders are compiled out under this build");
#else
    {
        jaos_model *m = make_forcing_row_model();
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT64(1, p.counts.forcing_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.redundant_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    {
        jaos_model *m = make_redundant_row_model();
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT64(1, p.counts.redundant_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.forcing_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    {
        jaos_model *m = make_tightening_boundary_model();
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        /* Zero, and pinned as zero on purpose: 02-04 built the bound
         * tightening family, measured it against the standard set and did
         * not ship it (src/presolve.c says why, at the reading that would
         * have been the fourth). A nonzero here means someone lit it again
         * without re-running the campaign that refused it. */
        TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

/* -- The two constants, pinned to the sweeps that set them ----------------
 *
 * docs/tolerances.md carries presolve's constants with the sweep behind
 * each. A document cannot fail, so these two tests do it instead: each
 * pins an exact integer or an exact verdict that moves the moment its
 * constant does, so retuning one silently is not available.
 *
 * Both call jm_presolve_run directly rather than jaos_solve, which is what
 * lets them mean the same thing under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE — the
 * constants live in this file whether or not the solver consults it. */

/* A chain of `n` singleton rows, each resolving one link per round:
 *   row 0: x_0 = 1;  row k: -x_{k-1} + x_k = 0.
 * Longer than the cap on purpose, so what it reports IS the cap. */
static jaos_model *make_cascading_chain(int64_t n)
{
    double *c = calloc((size_t)n, sizeof *c);
    double *cl = calloc((size_t)n, sizeof *cl);
    double *cu = calloc((size_t)n, sizeof *cu);
    double *rl = calloc((size_t)n, sizeof *rl);
    double *ru = calloc((size_t)n, sizeof *ru);
    int64_t *s = calloc((size_t)n + 1, sizeof *s);
    int64_t *ix = calloc(2 * (size_t)n, sizeof *ix);
    double *v = calloc(2 * (size_t)n, sizeof *v);
    TEST_ASSERT_NOT_NULL(c); TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu); TEST_ASSERT_NOT_NULL(rl);
    TEST_ASSERT_NOT_NULL(ru); TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(ix); TEST_ASSERT_NOT_NULL(v);

    int64_t nz = 0;
    for (int64_t j = 0; j < n; j++) {
        c[j] = 1.0;
        cl[j] = 0.0;
        cu[j] = 100.0;
        s[j] = nz;
        ix[nz] = j;   v[nz] = 1.0;   nz++;
        if (j < n - 1) { ix[nz] = j + 1; v[nz] = -1.0; nz++; }
    }
    s[n] = nz;
    rl[0] = 1.0; ru[0] = 1.0;
    for (int64_t i = 1; i < n; i++) { rl[i] = 0.0; ru[i] = 0.0; }

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, n, n, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, s, ix, v));
    free(c); free(cl); free(cu); free(rl); free(ru);
    free(s); free(ix); free(v);
    return m;
}

/* JM_PRESOLVE_ROUNDS = 16, set where the propagation stops changing: swept
 * over the standard set at 1, 2, 4, 8, 16, 32, 64, 128 with `make clean`
 * between settings, rows removed 6060, 7178, 7549, 7596, 7598, 7598, 7598,
 * 7598 and the cost flat at 97.2 s to 103.6 s. What last moved this count
 * was that sweep, and nothing since. The raw readings are in
 * bench/measurements/02-04/. */
static void test_the_round_cap_is_the_one_its_sweep_set(void)
{
    jaos_model *m = make_cascading_chain(40);
    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
    /* Exact, not a floor: the chain is 40 links long and resolves one per
     * round, so this reads the cap and nothing else. */
    TEST_ASSERT_EQUAL_INT64(16, p.counts.rounds);
    TEST_ASSERT_EQUAL_INT64(16, p.counts.fixed_col);
    TEST_ASSERT_EQUAL_INT64(16, p.counts.singleton_row);
    jm_presolve_free(&p);
    jaos_model_free(m);
}

/* PRESOLVE_ROUND_ULPS = 8, and this is the pair that separates it. The model
 * is make_singleton_fold_boundary_model's: a singleton row folding to a lower
 * bound on x0 that meets x0's own upper bound of 5. Nothing was removed from
 * row 0 before the fold, so its traffic is zero and the window is the bound
 * scale alone: 8 * DBL_EPSILON * 5 = 8.88e-15.
 *
 * A conflict of 1.6e-14 is outside it and the model is refused; a conflict of
 * 5e-15 is inside it and the interval collapses to a point. The two are one
 * step apart in the constant, which is what makes them a pin rather than two
 * loose assertions: at 16 ulps the first flips to solved, at 4 ulps the
 * second flips to refused.
 *
 * Last moved by the 02-09 fix, which took this site off PRESOLVE_TIGHTEN_EPS.
 * The pair used to be 1e-8 refused and 1e-10 collapsed, separating a window
 * of 1e-9 times 5. That window was five hundred thousand times wider than
 * the rounding it was standing in for, and on a model of magnitude 1e9 it
 * published a column a fifth of a unit outside its own declared bound. The
 * old lower value, 1e-10, is now correctly refused: on a bound of 5 it is
 * about 1.1e5 ulps, which is a real conflict and not a residue. */
static void test_the_fold_window_is_rounding_and_nothing_more(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("model builder is compiled out under either fault "
                        "build");
#else
    {
        jaos_model *m = make_singleton_fold_boundary_model(5.0 + 1.6e-14);
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    {
        jaos_model *m = make_singleton_fold_boundary_model(5.0 + 5e-15);
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        /* Two, not one: row 0's fold collapses x0's interval and the
         * fixed-column rule takes it, which drops row 1 to a singleton
         * that the next round folds as well. */
        TEST_ASSERT_EQUAL_INT64(2, p.counts.singleton_row);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

/* -- Two singleton rows folding into one column ---------------------------
 *
 *   min  x0 + x1
 *   r0:  x0        >= 2       singleton: tightens x0's lower to 2
 *   r1:  x0        <= 10      singleton: tightens x0's upper to 10
 *   r2:  x0 + x1   <= 100     survives, and keeps x0 from becoming an empty
 *                             column, which would take the SOLVED path instead
 *   x0, x1 in [0, inf), both cost 1.
 *
 * By hand: x0 = 2, x1 = 0, objective 2. x0 rests on the lower bound r0
 * produced, so r0 is the row owed the multiplier and y_r0 = 1 exactly. r1's
 * activity is 2 against (-inf, 10] — strictly interior — so y_r1 must be
 * exactly 0, and any nonzero value there is a dual violation of that size.
 *
 * The order is what this protects. r0 is pushed first and therefore replays
 * LAST, so r1 is the record that reaches the reduced cost first. Before the
 * bound-ownership test, r1 took the whole of it (y_r1 = 1, and r0 then found
 * a zero and published none) — the defect this model is the minimum case of,
 * and the one five standard-set instances were rejected on: 25fv47, bnl1,
 * bnl2, e226 and vtp-base. The multipliers are asserted per row rather than
 * through the checker alone, because two wrong duals whose sum is right
 * would pass a checker and still be wrong. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_two_folds_one_column_model(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 10.0, 100.0};
    const int64_t s[] = {0, 3, 4};
    const int64_t ix[] = {0, 1, 2, 2};
    const double v[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_two_folds_the_owning_row_takes_the_multiplier(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_two_folds_one_column_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[3], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));

    const double expected_x0 = 2.0, expected_x1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    /* The whole point: r0 carries it, r1 carries nothing, r2 is interior. */
    const double expected_y0 = 1.0, expected_y1 = 0.0, expected_y2 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y1, &y[1], sizeof y[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y2, &y[2], sizeof y[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* What the CHECKER must refuse, which is not the same as a detector for the
 * rule above. Under JAOS_PRESOLVE_FAULT_WRONGDUAL the multiplier is taken
 * unconditionally — what r1 did before the ownership test existed — and the
 * checker has to report it at exactly the multiplier's own magnitude. A rule
 * that let this through would let every multiplier of this shape through.
 *
 * This test passes against the pre-fix code too, and that is a property of
 * the checker rather than a hole in the test: jaos_check_solution is handed
 * x and y and recomputes d_j itself, so the same violation of 1 lands on the
 * column there and on row 1 here. The positive test above is the change
 * detector — it reports 1 before the fix and 0 after. */
static void test_two_folds_wrong_dual(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_WRONGDUAL");
#else
    jaos_model *m = make_two_folds_one_column_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

/* -- A frozen row still has to be satisfiable ------------------------------
 *
 *   min x0  s.t.  x0 + x1 = 100,  x0 in [4,4],  x1 in [0,3]
 *
 * There is no feasible point: x0 is pinned at 4, so x1 would have to be 96
 * against a box of [0,3]. x1 has cost 0 and degree 1, so the cost-0 singleton
 * column family relaxes row0 to [97, 100] and freezes it — and a frozen row
 * is skipped by the row pass and by the activity pass alike, so until the
 * feasibility test that runs after the round loop, nothing asked whether the
 * row could still be satisfied.
 *
 * What it did instead is the reason this test exists: postsolve published
 * x1 = 96 against its own box and the solver reported OPTIMAL, with the
 * checker reading a column violation of 93. A wrong answer, not a slow one,
 * and `-DNDEBUG` — which is how `bench/run` is built — removed the assert
 * that was the only thing noticing.
 *
 * The verdict is what this asserts. There is no solution to hand the checker
 * once the answer is INFEASIBLE, so the 93 is described here rather than
 * measured: it is what `jaos_check_solution` read from the point the old code
 * published, and `bench/measurements/02-08/` holds that reading. */
/* Guarded like every other fixture in this file whose only callers are
 * positive tests. Both of them compile out under either fault build, and
 * -Werror=unused-function then refuses the whole translation unit: from the
 * commit that added this model until now, `make test
 * EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` did not build at all, so the
 * instrument-validation hook every family's negative test depends on could
 * not be run. Nothing announced it, because the plain build and the
 * reference build are the two that CI and the loop actually run. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_frozen_row_infeasible_model(void)
{
    const double c[]  = {1.0, 0.0};
    const double cl[] = {4.0, 0.0};
    const double cu[] = {4.0, 3.0};
    const double rl[] = {100.0};
    const double ru[] = {100.0};
    const int64_t s[] = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_a_frozen_row_that_cannot_be_satisfied_is_infeasible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_frozen_row_infeasible_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

/* The same model judged by the reference build, which is the only oracle for
 * output no predicate of the three sets reads. Presolve compiles out, so the
 * verdict comes from the simplex alone and has nothing to do with the repair
 * being tested above. If these two ever disagree, one of them is wrong and
 * the campaign cannot say which. */
static void test_the_frozen_row_model_agrees_with_the_reference_build(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("reference half — runs under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    jaos_model *m = make_frozen_row_infeasible_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

/* The case the frozen-row test must REFUSE to fire on, which is the half that
 * matters: a feasible model reported INFEASIBLE is the mirror catastrophe, and
 * it is what D97 refused a whole family over.
 *
 *   min x0  s.t.  x0 + x1 = 100,  x0 in [4,4],  x1 in [0,96]
 *
 * The same shape as the model above with x1's upper bound raised to exactly
 * the value the row needs. It is feasible by a slack of exactly zero: row 0
 * relaxes to [4, 100], its surviving column reaches 4, and the two meet at the
 * single point that satisfies both. Any sign error in the comparison, or a
 * window applied to the wrong side of it, turns this into INFEASIBLE.
 *
 * Zero slack rather than a comfortable margin on purpose. A model that passes
 * with room to spare would also pass under a test that had drifted; this one
 * sits exactly on the boundary the comparison is about. The campaign shows the
 * pass firing on none of the 94 standard instances, but a campaign is not a
 * test: nothing in the suite would notice if the tolerance form changed and
 * started refusing feasible models. */
static void test_a_frozen_row_that_is_exactly_satisfiable_is_not_refused(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {4.0, 0.0};
    const double cu[] = {4.0, 96.0};
    const double rl[] = {100.0};
    const double ru[] = {100.0};
    const int64_t s[] = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double expected_x0 = 4.0, expected_x1 = 96.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/* -- The two windows, at a scale where 1e-9 stopped being small ------------
 *
 * Both of these are cases the code must REJECT, and both were accepted until
 * 02-09. They exist because the constant they separate is relative: a window
 * of "1e-9 times the scale" is 2.0 on a model whose terms are 1e9, and 2.0 is
 * not a rounding allowance for anything.
 *
 * The reference build is the oracle for both. `-DJAOS_NO_PRESOLVE` returns
 * INFEASIBLE on every one of them, which is what makes "presolve says
 * OPTIMAL" a defect rather than a difference of opinion.
 *
 *   row0: 1e9*x0 + 1e9*x1 == 2e9 + gap,  x0 and x1 both fixed at 1.
 *
 * The fixed-column family removes both columns and subtracts 1e9 from the
 * row's bounds twice, so the row empties carrying 2e9 of traffic. What is
 * left in cur_rl is exactly `gap`, and the window it is tested against is
 * 8 * DBL_EPSILON * 2e9 = 3.55e-6. */
static jaos_model *make_emptied_row_at_scale(double gap)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1.0, 1.0}, cu[] = {1.0, 1.0};
    const double rl[] = {2e9 + gap}, ru[] = {2e9 + gap};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1e9, 1e9};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

static void test_an_emptied_row_missed_by_more_than_rounding_is_refused(void)
{
    /* 1.5 against a window of 3.55e-6. Under PRESOLVE_TIGHTEN_EPS the window
     * was 2.0 and this model came back OPTIMAL, with the checker reporting
     * max_row_violation = 1.5 on the answer presolve had just certified. */
    jaos_model *m = make_emptied_row_at_scale(1.5);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_an_emptied_row_missed_by_rounding_alone_is_kept(void)
{
    /* The other side, and the one that stops this being a test that passes by
     * refusing everything. 1e-6 is about two ulps of 2e9 — the row bound
     * cannot be stated more precisely than that — so it is inside the window
     * and the model has an optimum. Both columns are fixed, so the objective
     * is 0 whatever happens; the verdict is the whole assertion. */
    jaos_model *m = make_emptied_row_at_scale(1e-6);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

/*   min x0  s.t.  r0: x0 >= rl0 (singleton),  x0 in [0, 1e9]
 *
 * r0 folds to a lower bound on x0 that lands past x0's own upper bound. The
 * window here is the bound scale, 8 * DBL_EPSILON * 1e9 = 1.78e-6, and the
 * traffic term is zero because nothing was removed from r0 before the fold.
 */
static jaos_model *make_fold_past_the_box_at_scale(double rl0)
{
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {1e9};
    const double rl[] = {rl0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

static void test_a_fold_past_the_box_at_scale_is_refused(void)
{
    /* 0.4 against a window of 1.78e-6. Under PRESOLVE_TIGHTEN_EPS the window
     * was 1.0, the interval "collapsed", and the published x0 was
     * 1000000000.2 — a fifth of a unit above the column's own declared upper
     * bound of 1e9, with the checker reporting max_col_violation = 0.2. A
     * published value outside a bound the caller stated is the worst shape
     * available here, because nothing downstream re-reads that bound. */
    jaos_model *m = make_fold_past_the_box_at_scale(1e9 + 0.4);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_fold_onto_the_box_at_scale_still_collapses(void)
{
    /* The other side: 5e-7 is four ulps of 1e9, inside the window of eight,
     * so the interval is a point and x0 is fixed there. Refusing this would
     * be the mirror-image catastrophe — a solvable model reported INFEASIBLE
     * — which is the failure the old, far wider window could never produce
     * and the new one can. That is the whole reason this test is here and
     * not just its partner above. */
    jaos_model *m = make_fold_past_the_box_at_scale(1e9 + 5e-7);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

#if defined(JAOS_NO_PRESOLVE)
    /* The reference build answers INFEASIBLE and it is not wrong: in exact
     * arithmetic `x0 >= 1e9 + 5e-7` and `x0 <= 1e9` have no common point,
     * and the simplex has no window on a bound conflict. Presolve's fold
     * has one, four ulps wide at this scale, and that window is the whole
     * subject of this test.
     *
     * So the two builds disagree here, deliberately, and the disagreement
     * is pinned rather than skipped: a change to PRESOLVE_ROUND_ULPS that
     * makes the presolved path agree with the reference has removed the
     * window this test exists for, and this line is where that shows.
     * Before 2026-08-18 this test was simply red under the reference
     * build, which is why nobody ran it. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
    return;
#endif

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));

    /* **x0 is asserted to be inside [0, 1e9], and until D158 it was not.**
     *
     * The collapse publishes the midpoint of the two ends, and the midpoint
     * of a collapsed pair can lie outside the column's own box: this model
     * published 1e9 + 2.4e-7, a quarter of a microunit above the upper bound
     * the caller declared. The overshoot was bounded only by the window that
     * admitted the collapse, and that window carries a traffic/|a| term
     * nothing in the file caps — up to 4 * DBL_EPSILON * traffic / |a|. It
     * was TODO.md's only open item with no stated bound.
     *
     * The midpoint is now clamped into the column's box, which is D152's
     * repair on the other family and the same argument: the stored bound is
     * exact and the implied one came out of a division, so the derived end
     * is the one that gives way. The symmetry the midpoint exists for is
     * kept, because the clamp reads the box rather than which end was
     * tightened.
     *
     * **The collapse itself is unchanged and this test still turns on it.**
     * `1e9 + 5e-7` is four ulps of 1e9, inside the eight-ulp window, so the
     * interval is still a point and x0 is still fixed there rather than the
     * model being refused. What moved is only where that point lands. If
     * PRESOLVE_ROUND_ULPS shrinks below four, the collapse stops happening
     * and the OPTIMAL assertion above fails first, which is the pin that
     * matters.
     *
     * Under PRESOLVE_TIGHTEN_EPS this same model overshot by 0.2 out of a
     * window of 1.0, which is the difference 02-09 makes at this site. */
    TEST_ASSERT_TRUE(x[0] >= 0.0);
    TEST_ASSERT_TRUE(x[0] <= 1e9);

    /* **The residue did not vanish; it moved, and the quantity that moved is
     * pinned here rather than left implicit.** The midpoint split the
     * admitted gap between the column bound and the row; the clamp puts the
     * whole of it on the row. This model reads col 2.38e-7 / row 2.38e-7
     * before D158 and col 0 / row 4.77e-7 after. The bound on the row side is
     * the window that admitted the collapse, and asserting it is what stops a
     * later change trading the column violation for an unbounded row one.
     *
     * `primal_feasible` is deliberately NOT asserted. It is an absolute test
     * at the caller's tolerance, and on a model whose gap sits near the top
     * of the window the doubled row residual can cross a CHECK_TOL the split
     * stayed under — `x0 >= 1e9 + 1.5e-6` goes from true to false. That is
     * the honest reading rather than a regression, since the model is
     * infeasible by the whole gap whichever point is published, and D158
     * records it. Pinning it here would pin the wrong thing. */
    const double window = 8.0 * DBL_EPSILON * 1e9;   /* PRESOLVE_ROUND_ULPS */
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.max_col_violation);
    TEST_ASSERT_TRUE(rep.max_row_violation <= window);

    jaos_model_free(m);
}

/* -- The frozen-row window has to cover the traffic, not the bound ---------
 *
 *   min x1  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
 *                 x0 in [1,1]        fixed: subtracts 1e9, row traffic 1e9
 *                 x2 in [0,1] cost 0, degree 1: relaxes R and FREEZES it
 *                 x1 in [g, 10] cost 1: the surviving column
 *
 * After the fixed column `cur_ru` is 0 and `cur_rl` is -inf, so a window
 * scaled by the row's own BOUNDS is `8 * DBL_EPSILON * max(1, 0)` —
 * 1.78e-15, which says nothing about the 1e9 that was subtracted to reach
 * them. The window that covers that traffic is 1.78e-6, nine decades wider.
 *
 * The model is infeasible by exactly g, so the pair separates the two:
 *
 *   g = 1e-10   about a thousandth of one ulp of 1e9. An infeasibility the
 *               arithmetic cannot represent, and the reference build — which
 *               computes the activity directly and never folds — solves it.
 *               **The bound-scaled window reported INFEASIBLE here**, which
 *               is a wrong answer of the worst shape this file names: a
 *               solvable model refused, with nothing downstream to recover it.
 *   g = 1e-4    840 ulps of 1e9. Real, and refused under either window.
 *
 * Both halves are asserted, and the reference build is the oracle for each.
 * Measured beside them: over the three sets the bound-scaled window was
 * smaller than the error the comparison carries on 6934 of 19114 frozen rows,
 * by up to 45930x, while flipping no verdict on any of them — so the campaign
 * cannot see this and only a constructed pair can (D159). */
static jaos_model *make_frozen_traffic_model(double g)
{
    const double c[]  = {0.0, 1.0, 0.0};
    const double cl[] = {1.0, g,    0.0};
    const double cu[] = {1.0, 10.0, 1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}

static void test_a_frozen_row_is_not_refused_below_its_own_traffic(void)
{
    jaos_model *m = make_frozen_traffic_model(1e-10);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    /* Asserted on BOTH builds. They must agree, and before D159 they did
     * not: presolve refused what the reference build solves. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

/* The half that matters more: widening a window is only admissible if it still
 * refuses what it was built to refuse. This is the guard on that.
 *
 *   min x1  s.t.  R: 1e9*x0 + x1 == 1e9 + 100
 *                 x0 in [1,1]              fixed: cur_rl = cur_ru = 100,
 *                                          row_traffic = 1e9
 *                 x1 in [0,3] cost 0, degree 1: relaxes R to [97,100],
 *                                          freezes it, and EMPTIES it
 *
 * Genuinely infeasible — x1 can reach 3 against a shortfall of 100 — and
 * every product in it is exact, so no rounding argument can excuse it.
 *
 * **The emptied row is the whole point of the construction.** The frozen-row
 * test is the last word only where the row reaches the reduced model with no
 * live column for the simplex to refuse it with. Raise PRESOLVE_ROUND_ULPS to
 * 1e12 and this model turns OPTIMAL, which is what a guard on a widening has
 * to be able to do.
 *
 * **The first version of this test used a model whose row keeps a live
 * column** — `1e9*x0 + x1 + x2 <= 1e9` with x1 bounded below by 1e-4 — and it
 * could not fail. At ROUND_ULPS = 1e12 it still read INFEASIBLE, because the
 * simplex refused the surviving row at PRIMAL_TOL whatever the window did. It
 * was reading the pipeline and not the window, so the widening had no guard at
 * all. Found by `numerics-reviewer`, who built this replacement (D159). */
static void test_a_frozen_row_emptied_and_still_short_is_refused(void)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1.0, 0.0};
    const double cu[] = {1.0, 3.0};
    const double rl[] = {1e9 + 100.0};
    const double ru[] = {1e9 + 100.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1e9, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    /* Asserted on both builds: the reference build refuses it too. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

/* -- The same defect at the pass that is NOT frozen -----------------------
 *
 *   min x1 + x2  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
 *                      x0 in [1,1]        fixed: cur_ru = 0, row_traffic 1e9
 *                      x1 in [1e-10, 10] cost 1
 *                      x2 in [0, 1]      cost 1   <- cost != 0, R never freezes
 *
 * D159's model with one cost changed from 0 to 1. That one change stops the
 * cost-0 singleton family relaxing the row, so R stays live and the ACTIVITY
 * pass judges it instead of the frozen-row test. Its window is
 * `ps_row_tol(&rg)`, the activity half alone, which knows nothing about the
 * 1e9 subtracted from `cur_ru` — so presolve refused a model the reference
 * build solves, exactly as the frozen-row test did before D159.
 *
 * The infeasibility is 1e-10 against a row activity of 1e9: a thousandth of
 * one ulp, which no arithmetic here can represent. Found by
 * `numerics-reviewer` while reviewing D159, and repaired by D160.
 *
 * The repair gives clause 1 its own window and leaves FORCING and REDUNDANT
 * on the old one, because a wider window makes those two fire MORE and
 * widening the forcing window cost 02-04 a campaign. So the guard on this
 * test is not a raised constant — it is that the reference build agrees. */
static void test_the_activity_pass_is_not_refused_below_its_own_traffic(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    /* It asserts an exact answer, so an injected index fault breaks it by
     * design. `make configs` caught the missing guard; the plain build alone
     * would not have. */
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0, 1.0, 1.0};
    const double cl[] = {1.0, 1e-10, 0.0};
    const double cu[] = {1.0, 10.0,  1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    /* Asserted on BOTH builds. They must agree, and before D160 they did not. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* **The ANSWER, not just the status.** This model reaches OPTIMAL through
     * FORCING pinning x1 and x2 and deleting the row — every row the widened
     * window rescues arrives at clause 2 with its condition already true — so
     * a wrong pin would leave the status unchanged and this test green
     * (`numerics-reviewer`). Both builds read exactly this. */
    double obj = 0.0, x[3] = {0.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    TEST_ASSERT_EQUAL_DOUBLE(1e-10, obj);
    TEST_ASSERT_EQUAL_DOUBLE(1.0,   x[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1e-10, x[1]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0,   x[2]);
    jaos_model_free(m);
#endif
}

/* The FROZEN-ROW window had the same defect, and it predates D159.
 *
 *   min 0  s.t.  R: -1e12 <= x0 + x1 <= 0,  x0 and x1 both cost 0 in [1e-4, 1]
 *
 * Both columns are cost-0 bounded singletons, so both relax and freeze R,
 * which is then empty. It is infeasible by 2e-4 — the columns cannot go below
 * 1e-4 each — and the frozen-row test is the last word, because an emptied
 * frozen row is deleted with everything else and the simplex never sees it.
 *
 * `ps_bound_scale(-1e12, 0)` is 1e12, so the window was 1.78e-3 and swallowed
 * it: presolve published **`optimal` with x = {1e-4, 1e-4}** against `ru = 0`,
 * on every tree since that window was written. The window comes from the row's
 * LOWER bound for a test on the UPPER side.
 *
 * The second half is the control and it is what makes this a test rather than
 * an anecdote: the SAME model with `rl = -INFINITY` was refused correctly all
 * along, which is only explicable if the finite lower bound was supplying the
 * number. Both halves are asserted on the reference build too.
 *
 * Found by `numerics-reviewer` while reviewing D160, whose repair at the
 * activity pass is the same single term (D161). */
static void test_a_frozen_rows_window_ignores_the_far_bound(void)
{
    for (int k = 0; k < 2; k++) {
        const double c[]  = {0.0, 0.0};
        const double cl[] = {1e-4, 1e-4};
        const double cu[] = {1.0, 1.0};
        const double rl[] = {k == 0 ? -1e12 : -INFINITY};
        const double ru[] = {0.0};
        const int64_t s[]  = {0, 1, 2};
        const int64_t ix[] = {0, 0};
        const double v[]   = {1.0, 1.0};

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         2, s, ix, v));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_model_free(m);
    }
}

/* The two cases clause 1's window must still REFUSE, and the first of them is
 * what refuted the window's first version.
 *
 * C: `min x0 + x1 s.t. -1e12 <= x0 + x1 <= 0, x0 in [1e-3, 1], x1 in [0, 1]`
 *
 *    Infeasible by exactly 1e-3, and nothing is ever removed from the row, so
 *    `row_traffic` is 0 and `rg.traffic` is 2.001. The first version of D160
 *    put `ps_bound_scale(rl, ru)` in the window, which reads the row's LOWER
 *    bound of -1e12 and gives 1.78e-3 for a test on the UPPER side. It
 *    published **`optimal` with an objective of 0.001**, and the row was
 *    pinned by FORCING and deleted on the way out, so the simplex never saw
 *    it either. Delete the `rg.traffic` term instead and this still passes —
 *    only removing the bound-scale term repairs it.
 *
 * E: D160's own model with the shortfall raised from 1e-10 to 1.0, against a
 *    window of 1.78e-6. It pins the window from the TIGHT side, which the
 *    accept test above cannot.
 *
 * Both are asserted on the reference build too. Found by `numerics-reviewer`.  */
static void test_the_activity_pass_still_refuses_a_real_shortfall(void)
{
    {   /* C */
        const double c[]  = {1.0, 1.0};
        const double cl[] = {1e-3, 0.0};
        const double cu[] = {1.0,  1.0};
        const double rl[] = {-1e12};
        const double ru[] = {0.0};
        const int64_t s[]  = {0, 1, 2};
        const int64_t ix[] = {0, 0};
        const double v[]   = {1.0, 1.0};
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         2, s, ix, v));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_model_free(m);
    }
    {   /* E */
        const double c[]  = {0.0, 1.0, 1.0};
        const double cl[] = {1.0, 1.0,  0.0};
        const double cu[] = {1.0, 10.0, 1.0};
        const double rl[] = {-INFINITY};
        const double ru[] = {1e9};
        const int64_t s[]  = {0, 1, 2, 3};
        const int64_t ix[] = {0, 0, 0};
        const double v[]   = {1e9, 1.0, 1.0};
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         3, s, ix, v));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_model_free(m);
    }
}

/* The same window, short by the NUMBER of removals rather than by their scale.
 *
 *   row R:  x0 + x1 + (k smalls) + w1 + w2  ==  k*2^-25 + 1e-7
 *   row S:  x1 + z                          == -1e9
 *
 *   x0     fixed at +1e9
 *   x1     in [-1e9-1, -1e9+1], NOT fixed at load time
 *   smalls fixed at 2^-25, which is a quarter of an ulp of 1e9
 *   w1, w2 in [0, 2e-7], cost 1, so no family relaxes them
 *   z      fixed at 0, and this is what delays x1 by one round
 *
 * `cur_rl[i]` and `cur_ru[i]` are a plain running difference, so their error
 * grows with the number of terms subtracted from them and not only with the
 * scale of those terms. The window counted a fixed eight ulps, which covers
 * about three removals (D162).
 *
 * Round 1 removes x0 and all k smalls while x1 is still free, so each small is
 * a quarter of an ulp of an accumulator of magnitude 1e9 and rounds away.
 * Round 2 folds row S, fixes x1 and only then subtracts it, and cur_rl comes
 * back to `k*2^-25 + 1e-7` where the truth is 1e-7. At k = 256 that error is
 * 7.63e-6 against a shipped window of 8 * DBL_EPSILON * 2e9 = 3.55e-6, and
 * clause 1 of the activity pass returns INFEASIBLE on a model whose feasible
 * point is exactly representable: x0 = 1e9, x1 = -1e9, every small at 2^-25,
 * w1 = T - 2^-17 and w2 = 0 make the activity exactly T.
 *
 * **A pin, not a loose assertion**: at k = 128 the shipped window is wide
 * enough and both trees accept; at k = 256 only the counted window does.
 *
 * **The reference build cannot arbitrate this one and it is worth saying why.**
 * `-DJAOS_NO_PRESOLVE` reads INFEASIBLE at every k, including the k where the
 * shipped window accepts, because the solver sums the row in column order and
 * loses the same 256 terms presolve lost. That is a defect of its own and it is
 * TODO.md's; it is not evidence about this window, and the exact feasible point
 * above is what settles the question instead.
 *
 * So this asserts presolve's OUTCOME and not the published answer. The answer
 * on this model is not the true optimum on any build — the simplex meets the
 * same unrepresentable row — and pinning it would pin a wrong number. */
static void test_the_window_counts_the_shifts_and_not_only_their_scale(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    enum { KS = 256, NC = KS + 5, NNZ = KS + 6 };
    static double c[NC], cl[NC], cu[NC], av[NNZ];
    static int64_t as[NC + 1], ai[NNZ];
    const double t = ldexp(1.0, -25);

    /* Two models: the feasible one, and the same shape 1e-2 away from any
     * feasible point, which no window in this range may accept. */
    for (int half = 0; half < 2; half++) {
        const double T = (double)KS * t + 1e-7 + (half == 0 ? 0.0 : 1e-2);
        const double rl[] = {T, -1e9}, ru[] = {T, -1e9};
        int64_t nz = 0;

        for (int64_t j = 0; j < NC; j++) {
            as[j] = nz;
            c[j] = 0.0;
            if (j == 0) {                        /* x0 */
                cl[j] = cu[j] = 1e9;
                ai[nz] = 0; av[nz++] = 1.0;
            } else if (j == 1) {                 /* x1, fixed in round 2 */
                cl[j] = -1e9 - 1.0; cu[j] = -1e9 + 1.0;
                ai[nz] = 0; av[nz++] = 1.0;
                ai[nz] = 1; av[nz++] = 1.0;
            } else if (j < KS + 2) {             /* the smalls */
                cl[j] = cu[j] = t;
                ai[nz] = 0; av[nz++] = 1.0;
            } else if (j < KS + 4) {             /* w1, w2 */
                cl[j] = 0.0; cu[j] = 2e-7; c[j] = 1.0;
                ai[nz] = 0; av[nz++] = 1.0;
            } else {                             /* z */
                cl[j] = cu[j] = 0.0;
                ai[nz] = 1; av[nz++] = 1.0;
            }
        }
        as[NC] = nz;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         nz, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

/* The shift count multiplies TWO scales, and the test above exercises one of
 * them. This is the other.
 *
 *   x_big + (256 columns fixed at 2^-25) + w1 + w2 == 1e9
 *   x_big  in [0, 1e9 - 2^-17], cost 1
 *   w1, w2 in [0, 2^-23], cost 1
 *
 * Exactly feasible: `x_big = 1e9 - 2^-17`, every small at `2^-25` summing to
 * exactly `2^-17`, `w1 = w2 = 0`, activity exactly 1e9.
 *
 * Round 1 removes the 256 fixed columns. Each is a quarter of an ulp of an
 * accumulator of magnitude 1e9 and rounds away, so `cur_rl` stays at 1e9 while
 * the truth is `1e9 - 7.6294e-6`. The row keeps degree 3, so clause 1 of the
 * activity pass judges it: `max_act = 1e9 - 7.391e-6 < rl - itol`.
 *
 * **What separates this from the test above is which half of `ps_shift_excess`
 * carries the window.** Here `row_traffic` is `2^-17 = 7.6e-6`, below the
 * floor of 1, and the bound is 1e9 — so the whole window comes from
 * `ps_end_scale(rl)`. Replace that call with a constant 1.0 and the window
 * falls from 5.862e-5 to 1.776e-6 against a residue of 7.391e-6, and this test
 * goes red. Nothing else in the suite does that: the test above lands
 * `cur_rl` at 7.75e-6, so `ps_end_scale` reads its floor there and the traffic
 * half carries everything (`numerics-reviewer`).
 *
 * The second half is the control the widening needs, and it is near the edge
 * rather than a decade away: the same model 2e-4 from any feasible point,
 * against a window of 5.862e-5. A factor of 3.5, so a window much wider than
 * this one stops refusing it. */
static void test_the_shift_count_scales_by_the_end_it_is_testing(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    enum { KS = 256, NC = KS + 3 };
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double small = ldexp(1.0, -25);   /* a quarter ulp of 1e9 */
    const double lost  = ldexp(1.0, -17);   /* KS * small, exactly */
    const double wcap  = ldexp(1.0, -23);

    for (int half = 0; half < 2; half++) {
        const double rl[] = {1e9}, ru[] = {1e9};
        for (int64_t j = 0; j < NC; j++) {
            as[j] = j; ai[j] = 0; av[j] = 1.0; c[j] = 0.0;
            if (j == 0) {
                c[j] = 1.0; cl[j] = 0.0;
                cu[j] = 1e9 - lost - (half == 0 ? 0.0 : 2e-4);
            } else if (j < KS + 1) {
                cl[j] = cu[j] = small;
            } else {
                c[j] = 1.0; cl[j] = 0.0; cu[j] = wcap;
            }
        }
        as[NC] = NC;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         NC, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

/* The singleton row's FOLD reads the same running difference and counted a
 * fixed eight ulps until D163 — the fourth read, where D162 repaired three.
 *
 *   x_big + (256 columns fixed at 2^-25) == 1e9,  x_big in [0, 1e9 - 2^-17]
 *
 * The test above with the two `w` columns deleted, so the row is a singleton
 * on `x_big` once round 1 has taken the smalls. Round 2 folds it and asks
 * whether `[1e9, 1e9]` meets `[0, 1e9 - 2^-17]`: `implied_lo` is `cur_rl / a`
 * and carries all 256 lost roundings, so `1e9 > (1e9 - 2^-17) + 1.77636e-6`
 * and presolve refused a model whose feasible point is exactly representable.
 *
 * **This one the reference build arbitrates**, which is what D162's own model
 * could not do: `-DJAOS_NO_PRESOLVE` reads OPTIMAL at 999999999.99999237.
 * Both builds are asserted below.
 *
 * The second half is the control, 1e-3 from any feasible point against a
 * window of 5.862e-5. Found by `numerics-reviewer` reviewing D162
 * (bench/measurements/02-73/). */
static void test_the_singleton_fold_counts_the_shifts_too(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    enum { KS = 256, NC = KS + 1 };
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double small = ldexp(1.0, -25);
    const double lost  = ldexp(1.0, -17);

    for (int half = 0; half < 2; half++) {
        const double rl[] = {1e9}, ru[] = {1e9};
        for (int64_t j = 0; j < NC; j++) {
            as[j] = j; ai[j] = 0; av[j] = 1.0; c[j] = 0.0;
            if (j == 0) {
                c[j] = 1.0; cl[j] = 0.0;
                cu[j] = 1e9 - lost - (half == 0 ? 0.0 : 1e-3);
            } else {
                cl[j] = cu[j] = small;
            }
        }
        as[NC] = NC;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         NC, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);

        /* The whole solve, on both builds, because this model has an oracle:
         * the answer is the same with presolve and without it. */
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(half == 0 ? JAOS_SOLVE_OPTIMAL
                                        : JAOS_SOLVE_INFEASIBLE,
                              jaos_status_of(m));
        jaos_model_free(m);
    }
#endif
}

/* ~~**A PINNED WRONG ANSWER.**~~ **The repair landed and this test is what
 * announced it** (D165). It was written for D164 asserting the INFEASIBLE JAOS
 * gave, with a note saying to expect OPTIMAL at 1.1920928955078125e-07; the
 * compensation went in, the pin fired with `Expected 2 Was 1`, and it now
 * asserts the answer instead. Both builds reach the same point to the last
 * bit.
 *
 *   row S:  x1 + (256 y_s fixed at 2^-25) == 1e9      x1 in [1e9-1, 1e9+1]
 *   row R:  x1 + w1 + w2 == 1e9 - 63*2^-23            w1, w2 in [0, 2^-23]
 *
 * Feasible exactly at `x1 = 1e9 - 2^-17`, every `y_s` at `2^-25`,
 * `w1 = 2^-23`, `w2 = 0`. Row S sums to exactly 1e9 and row R to exactly
 * `1e9 - 63*2^-23`; every value is a dyadic rational a double holds exactly,
 * and the reference build reaches that point with both residuals at 0.
 *
 * Round 1 removes the 256 smalls from row S, each a quarter of an ulp of 1e9,
 * and every one rounds away — so `cur_rl[S]` stays at 1e9 against a truth of
 * `1e9 - 7.6294e-6`. Round 2 folds row S and **fixes x1 at 1e9**, wrong by
 * that amount, and the fold's own test does not fire because `new_lo` equals
 * `new_hi` there. Round 2's column pass subtracts that value from row R, which
 * is charged ONE shift at its own traffic, and clause 1 refuses row R.
 *
 * **The window repair was refused before this one landed, and that is worth
 * keeping** (D164). Carrying an error weight from the fold into the receiving
 * row's window does stop the refusal — and then the solve publishes `optimal`
 * with `w1 = w2 = 0`, an objective of 0, and **both rows violated by 7.6e-06**,
 * which is 7.5 times `CHECK_TOL`. A window decides whether to refuse; it
 * cannot correct a value that is already wrong, so widening it converts a loud
 * failure into a silent one. Readings in `bench/measurements/02-74/`.
 *
 * D165 repaired it upstream instead: `cur_rl`/`cur_ru` keep their residue, so
 * `cur_rl[S]` reads `1e9 - 2^-17` and the fold fixes x1 at the value the model
 * actually has. Nothing downstream inherits anything, because there is nothing
 * to inherit.
 *
 * The second half is the control: row R's bound moved 1e-3, refused on every
 * build for a reason that has nothing to do with any of this. Found by
 * `numerics-reviewer` reviewing D162. */
static void test_a_folds_value_carries_its_rows_error_into_the_next(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("pinned change-detector — skipped under either fault "
                        "build");
#else
    enum { KS = 256, NC = KS + 3, NNZ = KS + 4 };
    static double c[NC], cl[NC], cu[NC], av[NNZ];
    static int64_t as[NC + 1], ai[NNZ];
    const double small = ldexp(1.0, -25);
    const double wcap  = ldexp(1.0, -23);

    for (int half = 0; half < 2; half++) {
        const double target = 1e9 - 63.0 * wcap + (half == 0 ? 0.0 : 1e-3);
        const double rl[] = {1e9, target}, ru[] = {1e9, target};
        int64_t nz = 0;
        for (int64_t j = 0; j < NC; j++) {
            as[j] = nz; c[j] = 0.0;
            if (j == 0) {                       /* x1, in both rows */
                cl[j] = 1e9 - 1.0; cu[j] = 1e9 + 1.0;
                ai[nz] = 0; av[nz++] = 1.0;
                ai[nz] = 1; av[nz++] = 1.0;
            } else if (j < KS + 1) {            /* the y_s, on row S */
                cl[j] = cu[j] = small;
                ai[nz] = 0; av[nz++] = 1.0;
            } else {                            /* w1, w2 on row R */
                c[j] = 1.0; cl[j] = 0.0; cu[j] = wcap;
                ai[nz] = 1; av[nz++] = 1.0;
            }
        }
        as[NC] = nz;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         nz, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        /* The feasible half is accepted and the control is refused, and this
         * runs in the reference build too because `jm_presolve_run` is the
         * same code there — `-DJAOS_NO_PRESOLVE` only stops `jaos_solve`
         * consulting it. */
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);

        /* **The ANSWER and not only the status, on BOTH builds.** The two
         * agree here to the last bit — `x1 = 1e9 - 2^-17`, `w1 = 2^-23`,
         * `w2 = 0`, both rows at residual zero — and before D165 the shipping
         * build refused this outright. A status-only assertion would have gone
         * green on a point that meets neither row. */
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(half == 0 ? JAOS_SOLVE_OPTIMAL
                                        : JAOS_SOLVE_INFEASIBLE,
                              jaos_status_of(m));
        if (half == 0) {
            double obj = 0.0, x[NC];
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, x, nullptr, nullptr, nullptr));
            TEST_ASSERT_EQUAL_DOUBLE(wcap, obj);
            TEST_ASSERT_EQUAL_DOUBLE(1e9 - ldexp(1.0, -17), x[0]);
            TEST_ASSERT_EQUAL_DOUBLE(wcap, x[KS + 1]);
            TEST_ASSERT_EQUAL_DOUBLE(0.0,  x[KS + 2]);
        }
        jaos_model_free(m);
    }
#endif
}

/* An INVERTED column box reaches the same collapse branch, and the clamp has
 * no box to clamp into there.
 *
 *   min x0  s.t.  r0: x0 >= 0,  x0 in [1e9, 1e9 - 5e-7]
 *
 * `include/jaos.h` says `xl > xu` is legal input to be reported infeasible,
 * not refused at load. With the bounds crossed, `new_lo > new_hi` holds for
 * any row at all, so this branch is reached on a model that has nothing to do
 * with rounding — and D158's first version asserted containment there and
 * ABORTED, which is an abort on legal input.
 *
 * It is a real regression risk and not a curiosity: `make test` and
 * `make sanitize` both build with asserts on, and the suite's only other
 * inverted-box model (`[5.0, 1.0]`, in tests/test_model.c) has a gap about
 * 4.5e14 times the window, so it takes the INFEASIBLE branch above and never
 * reaches the collapse. Nothing in the suite covered this until now.
 * Found by `numerics-reviewer`.
 *
 * What is asserted is that the midpoint STANDS: x0 lands strictly between the
 * two crossed bounds rather than on either of them, which is what "the clamp
 * did not fire" looks like from outside. The behaviour is bit-identical to
 * the tree before D158.
 *
 * Since D259 the solve's entry refuses a box inverted by more than the same
 * window, so this model reaches the fold only because 5e-7 is inside eight
 * ulps of 1e9 (1.78e-6, a factor 3.55 of margin). The window sits on the
 * JAOS_PRESOLVE_ROUND_ULPS_VALUE sweep hook: at two ulps or fewer the entry
 * refuses this model and the assertion on jaos_solution is what fails. */
static void test_a_collapse_on_an_inverted_box_keeps_the_midpoint(void)
{
    const double c[]  = {1.0};
    const double cl[] = {1e9}, cu[] = {1e9 - 5e-7};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

#if defined(JAOS_NO_PRESOLVE)
    /* No fold, so no collapse and no clamp. The simplex sees the crossed
     * bounds directly; this half exists so the test builds under the
     * reference build rather than being skipped there. */
    (void)0;
#else
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    TEST_ASSERT_TRUE(x[0] < 1e9);
    TEST_ASSERT_TRUE(x[0] > 1e9 - 5e-7);
#endif
    jaos_model_free(m);
}

/* -- MAXIMIZE -------------------------------------------------------------
 *
 * Presolve had no MAXIMIZE case at all before 02-09, and the file never read
 * m->sense. Every cost-direction and dual-sign rule in it is stated for
 * minimise, which is the canonical form src/check.c:561 and src/simplex.c:665
 * both convert into — and presolve was the one stage that did not convert.
 *
 * netlib is entirely MINIMIZE, so no campaign on any of the three sets can
 * see this. These tests are the whole cover for that half of the public
 * enum, and the reference build is the oracle for each.
 *
 *   max 3*x0 + 2*x1
 *   r0: x0 + x1 <= 4
 *   r1: x0      <= 2        singleton: folds into an upper bound on x0
 *   x0, x1 >= 0
 *
 * By hand: x0 = 2, x1 = 2, objective 10. x0 rests on the bound r1 produced,
 * so r1 is the row owed the multiplier. -DJAOS_NO_PRESOLVE publishes
 * y = [2, 1] and d = [0, 0], and so must this. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_maximised_singleton_row_model(void)
{
    const double c[]  = {3.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};
    const int64_t s[]  = {0, 2, 3};
    const int64_t ix[] = {0, 1, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}
#endif

static void test_a_maximised_singleton_row_is_owed_its_multiplier(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_maximised_singleton_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* Bit exact: every coefficient and bound here is a small integer, so the
     * optimum carries no rounding to compare against. */
    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[2], d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, d));

    /* The dual is where the defect lived. Presolve published y = [2, 0] and
     * d = [1, 0]: the sign test read the raw reduced cost, which is the
     * negative of the canonical one under MAXIMIZE, so the singleton row
     * declined a multiplier it was owed and the cost stayed on the column.
     * The objective was right and the answer was wrong. */
    const double expected_y1 = 1.0, expected_d0 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y1, &y[1], sizeof y[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_d0, &d[0], sizeof d[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

/*   max x1,  x1 in [lo, hi],  x1 in no row at all (an empty column).
 *   x0 is only there to keep the model from having no matrix entries.
 *
 * The favourable side of a MAXIMIZE column with a positive cost is its UPPER
 * bound. ps_empty_col_value reads "cost > 0 wants the lower bound", which is
 * the minimise rule, so presolve put x1 at `lo` and published an objective
 * that was not the optimum. With lo = -inf it went further and reported
 * UNBOUNDED on a model whose optimum is 5. */
static jaos_model *make_maximised_empty_column(double lo, double hi)
{
    const double c[]  = {0.0, 1.0};
    const double cl[] = {0.0, lo}, cu[] = {10.0, hi};
    const double rl[] = {0.0}, ru[] = {10.0};
    const int64_t s[]  = {0, 1, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

static void test_a_maximised_empty_column_takes_its_upper_bound(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_maximised_empty_column(0.0, 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 5.0;   /* was 0.0: the lower bound */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    const double expected_x1 = 5.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    jaos_model_free(m);
#endif
}

static void test_a_maximised_empty_column_is_not_unbounded_downwards(void)
{
    /* The infinite side is the one the objective does not want. Reporting
     * UNBOUNDED here is a wrong verdict, not a conservative one, and D19
     * makes this family the only one allowed to report it at all — which is
     * exactly why its rule has to be asked in the right space.
     *
     * **The verdict is asserted in every configuration and only the objective
     * is guarded off** (`numerics-reviewer`, D169). Both faults live in
     * postsolve — `ps_restore_index`, and the `JAOS_PRESOLVE_FAULT_WRONGDUAL`
     * branch — while UNBOUNDED is decided earlier, in `ps_empty_col_value`
     * during the scan, so neither of them can flip this status. Guarding the
     * whole test would have thrown D19's own check away in two of the five
     * configurations, which is what the first version of this guard did. */
    jaos_model *m = make_maximised_empty_column(-INFINITY, 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    /* The objective is summed from the published values since D169, so a
     * corrupted postsolved value reaches it now and did not before. Under
     * OFFBYONE, `ps_restore_index` swaps the two slots and it reads
     * 0*5 + 1*0 = 0 rather than 5 — the fault doing its job. */
    const double expected_obj = 5.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);
#endif
    jaos_model_free(m);
}

/*   max x0 + x1
 *   r0: x0 + x1 <= 0
 *   x0, x1 in [0, 1]
 *
 * r0 is forcing: the box gives it an activity range of [0, 2] against an upper
 * bound of 0, so the minimum meets the bound and every column in the row is
 * pinned at the end that produces it. The only feasible point is the origin.
 *
 * This is the site with the most sign reasoning in the file — a min-or-max
 * selection over the columns' limits, then a one-sided clamp — and both flip
 * under sigma. It is also the one site where a wrong sigma passes the whole
 * suite, because the objective is 0 either way and only the multiplier moves.
 * The reference build publishes y = [1] and d = [0, 0]; before 02-09 presolve
 * published y = [0] and left a dual violation of 1. */
static void test_a_maximised_forcing_row_is_owed_its_multiplier(void)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {0.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1], d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, d));

    const double zero = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&zero, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&zero, &x[1], sizeof x[1]);

    /* The multiplier, which is the whole point of the test. */
    const double expected_y0 = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
}

/* The frozen-row test's window, and the model that walked through it.
 * src/presolve.c carries the derivation beside the constant; the short form is
 * that a window of "1e-9 times the row's bound magnitude" is 1.0 on a row of
 * magnitude 1e9, and the violation here is 0.5.
 *
 * Found by review on 2026-08-14 and confirmed against the reference build,
 * which returns INFEASIBLE. Before the fix this reached jm_postsolve_solved
 * and published OPTIMAL with x1 half a unit above its own upper bound; in a
 * build with assertions it tripped `want_lo <= want_hi` instead. It is the
 * same half D102 closed, escaping through the window rather than around the
 * test, which is why it needs its own case and not a line in D102's. */
static void test_a_frozen_row_missed_at_scale_is_refused(void)
{
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.5, 0.0}, cu[] = {0.5, 1e9};
    const double rl[] = {1e9 + 1.0}, ru[] = {1e9 + 1.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

/* §1c (bench/measurements/02-18/): JM_PS_IMPLIED_FREE_COL recovers its
 * column from sol_row's replayed accumulation, and a plain running sum's
 * error grows with the row's degree while the family's margin promises
 * 8 ulps of the traffic. This is 02-18's model: one equality row of 2000
 * identical terms — identical, so the accumulation is order-independent and
 * its error is one exact number — plus the singleton S, whose lower bound
 * sits 2.5 margins below the implied bound. The uncompensated replay
 * published S = -4.9471855163574219e-06, which is 4.06e-6 BELOW the bound
 * the caller stated and 11.4x the margin's promise, predicted bit for bit
 * before the run. With ps_row_add's compensation the recovery lands within
 * rounding of the implied bound and inside the box. This test fails on the
 * uncompensated tree by construction; that failure is its evidence. */
static void test_the_recovered_column_respects_the_bound_it_was_promised(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("white-box presolve test — the reference build has "
                        "no replay to exercise");
#else
    enum { IFN = 2000 };
    const double V = 50000.667694415897;
    const double b = 100001335.38883179;
    const double L = -8.8819028033554831e-07;
    const int64_t ncol = IFN + 1;

    double  *c  = malloc((size_t)ncol * sizeof *c);
    double  *cl = malloc((size_t)ncol * sizeof *cl);
    double  *cu = malloc((size_t)ncol * sizeof *cu);
    int64_t *st = malloc(((size_t)ncol + 1) * sizeof *st);
    int64_t *ix = malloc((size_t)ncol * sizeof *ix);
    double  *v  = malloc((size_t)ncol * sizeof *v);
    double  *x  = malloc((size_t)ncol * sizeof *x);
    TEST_ASSERT_NOT_NULL(c);  TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu); TEST_ASSERT_NOT_NULL(st);
    TEST_ASSERT_NOT_NULL(ix); TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_NOT_NULL(x);

    /* S first: nonzero cost, so the cost-0 singleton family cannot take it
     * before the implied free one sees it. */
    c[0] = 1.0e-9; cl[0] = L; cu[0] = INFINITY;
    st[0] = 0; ix[0] = 0; v[0] = 1.0;
    for (int64_t k = 1; k < ncol; k++) {
        c[k] = -1.0; cl[k] = 0.0; cu[k] = 1.0;
        st[k] = k; ix[k] = 0; v[k] = V;
    }
    st[ncol] = ncol;
    const double rl[] = {b}, ru[] = {b};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ncol, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     ncol, st, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    /* Vacuity guard: presolve must have consumed the whole model, or the
     * simplex computed x[0] directly and the assertions below hold without
     * the replay ever running. Found in review. */
    TEST_ASSERT_EQUAL_INT64(0, m->presolve_num_col);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));

    /* The bound the caller stated. The uncompensated replay breaks this by
     * 4.06e-6. */
    TEST_ASSERT_TRUE_MESSAGE(x[0] >= L,
        "recovered column published outside its own lower bound");
    /* And the value is the implied bound to within rounding, not the
     * accumulation's drift: b - 2000*V is exactly 0 in doubles, the exact
     * implied bound is within an ulp of b of that, and the compensated
     * recovery adds a few more. 1e-7 is ~4x that headroom and 50x under
     * the uncompensated -4.9e-6. */
    TEST_ASSERT_TRUE_MESSAGE(fabs(x[0] - (b - (double)IFN * V)) <= 1.0e-7,
        "recovered column carries the accumulation's drift");

    free(c); free(cl); free(cu); free(st); free(ix); free(v); free(x);
    jaos_model_free(m);
#endif
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

    RUN_TEST(test_empty_row_round_trip);
    RUN_TEST(test_empty_row_index_off_by_one);
    RUN_TEST(test_empty_col_round_trip);
    RUN_TEST(test_empty_col_index_off_by_one);
    RUN_TEST(test_empty_col_reports_unbounded);
    RUN_TEST(test_singleton_row_round_trip);
    RUN_TEST(test_singleton_row_wrong_dual);
    RUN_TEST(test_two_folds_the_owning_row_takes_the_multiplier);
    RUN_TEST(test_two_folds_wrong_dual);
    RUN_TEST(test_a_frozen_row_that_cannot_be_satisfied_is_infeasible);
    RUN_TEST(test_a_frozen_row_that_is_exactly_satisfiable_is_not_refused);
    RUN_TEST(test_the_frozen_row_model_agrees_with_the_reference_build);
    RUN_TEST(test_singleton_col_round_trip);
    RUN_TEST(test_singleton_col_index_off_by_one);
    RUN_TEST(test_singleton_col_after_fixed_col);
    RUN_TEST(test_singleton_col_between_two_removals_solved_path);
    RUN_TEST(test_an_exact_tie_the_division_rounds_inward_publishes_the_bound);
    RUN_TEST(test_an_interior_recovery_takes_the_row_out_whatever_the_ulps_say);
    RUN_TEST(test_a_column_a_row_fixed_inside_its_box_is_basic);
    RUN_TEST(test_the_basis_count_promise_breaks_on_a_declined_column);
    RUN_TEST(test_a_short_mapped_basis_is_repaired_and_warm_survives);
    RUN_TEST(test_the_warm_repair_stops_at_its_cap);
    RUN_TEST(test_a_long_mapped_basis_falls_back_cold);
    RUN_TEST(test_singleton_col_open_below_publishes_a_finite_point);
    RUN_TEST(test_two_singleton_cols_on_one_row);
    RUN_TEST(test_free_col_singleton_round_trip);
    RUN_TEST(test_free_col_singleton_index_off_by_one);

    RUN_TEST(test_an_implied_free_column_is_substituted_out);
    RUN_TEST(test_an_implied_bound_outside_the_box_is_refused);
    RUN_TEST(test_an_implied_bound_at_exact_equality_is_declined);
    RUN_TEST(test_a_removed_column_pays_every_row_it_touches);
    RUN_TEST(test_the_recovered_column_respects_the_bound_it_was_promised);
    RUN_TEST(test_implied_free_col_index_off_by_one);
#if !defined(JAOS_NO_PRESOLVE)
    RUN_TEST(test_the_implied_free_counter_reads_its_three_models);
    RUN_TEST(test_a_range_row_that_shifted_into_an_equality_is_declined);
#endif

    RUN_TEST(test_empty_row_reports_infeasible);
    RUN_TEST(test_zero_row_model_solves);
    RUN_TEST(test_zero_col_model_solves);
    RUN_TEST(test_all_five_counters_move_independently);

    RUN_TEST(test_forcing_row_round_trip);
    RUN_TEST(test_forcing_row_index_off_by_one);
    RUN_TEST(test_redundant_row_round_trip);
    RUN_TEST(test_redundant_row_index_off_by_one);
    RUN_TEST(test_a_fold_onto_the_opposite_bound_fixes_the_column);
    RUN_TEST(test_a_fold_one_step_past_the_opposite_bound_is_infeasible);
    RUN_TEST(test_a_fold_inside_the_epsilon_does_not_flip_the_verdict);
    RUN_TEST(test_an_optimum_on_the_tightening_boundary_survives);
    RUN_TEST(test_activity_range_counters_are_exact);
    RUN_TEST(test_the_round_cap_is_the_one_its_sweep_set);
    RUN_TEST(test_the_fold_window_is_rounding_and_nothing_more);

    RUN_TEST(test_an_emptied_row_missed_by_more_than_rounding_is_refused);
    RUN_TEST(test_an_emptied_row_missed_by_rounding_alone_is_kept);
    RUN_TEST(test_a_fold_past_the_box_at_scale_is_refused);
    RUN_TEST(test_a_fold_onto_the_box_at_scale_still_collapses);
    RUN_TEST(test_a_collapse_on_an_inverted_box_keeps_the_midpoint);
    RUN_TEST(test_a_frozen_row_is_not_refused_below_its_own_traffic);
    RUN_TEST(test_a_frozen_row_emptied_and_still_short_is_refused);
    RUN_TEST(test_the_activity_pass_is_not_refused_below_its_own_traffic);
    RUN_TEST(test_the_activity_pass_still_refuses_a_real_shortfall);
    RUN_TEST(test_the_window_counts_the_shifts_and_not_only_their_scale);
    RUN_TEST(test_the_shift_count_scales_by_the_end_it_is_testing);
    RUN_TEST(test_the_singleton_fold_counts_the_shifts_too);
    RUN_TEST(test_a_folds_value_carries_its_rows_error_into_the_next);
    RUN_TEST(test_a_frozen_rows_window_ignores_the_far_bound);

    RUN_TEST(test_a_frozen_row_missed_at_scale_is_refused);

    RUN_TEST(test_a_maximised_singleton_row_is_owed_its_multiplier);
    RUN_TEST(test_a_maximised_empty_column_takes_its_upper_bound);
    RUN_TEST(test_a_maximised_empty_column_is_not_unbounded_downwards);
    RUN_TEST(test_a_maximised_forcing_row_is_owed_its_multiplier);
    return UNITY_END();
}
