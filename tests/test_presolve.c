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

    /* The basis this publishes is wrong, and pinned as a change
     * detector rather than as a contract. jaos.h promises exactly
     * num_row of the num_col + num_row statuses are basic; three of
     * these six are, against num_row = 2. The same model built with
     * -DJAOS_NO_PRESOLVE publishes row0 AT_LOWER and two basic, which
     * is the right answer: row0's activity rests on its bound, and
     * neither singleton column left a basic slot free, both having been
     * recovered strictly inside their own box.
     *
     * Not asserted here: WHICH status row0 carries. That is what the
     * repair changes, and a test demanding today's value would fail the
     * person fixing it. The count is what says the repair landed —
     * expect this 3 to become 2. TODO.md carries the defect; re-pin
     * there, deliberately.
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
#if defined(JAOS_NO_PRESOLVE)
    TEST_ASSERT_EQUAL_INT64(2, basic);
#else
    TEST_ASSERT_EQUAL_INT64(3, basic);
#endif

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

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
#if !defined(JAOS_NO_PRESOLVE)
    /* Postsolve's own choice, and only presolve's to make: the row takes
     * the single basic slot its removal owes back, because the columns it
     * pinned are all nonbasic. The un-presolved simplex reaches the same
     * point by its own route and leaves this row at its upper bound with a
     * column basic instead, which is an equally valid basis for the same
     * answer — so this one assertion is presolve-side only, while the count
     * below holds for both builds. */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[0]);   /* the forcing row */
#endif
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

    /* x0 is NOT asserted to be inside [0, 1e9], and that is deliberate: it
     * is not. The collapse publishes the midpoint of the two ends, which is
     * 1e9 + 2.4e-7 here, a quarter of a microunit above the column's own
     * upper bound. That is the open item TODO.md carries as "a collapsed
     * fold leaves a bound no record owns" — the midpoint is symmetric in the
     * two ends on purpose, so replacing it is a decision about what a
     * collapsed record should record and not a clamp to bolt on here.
     *
     * What IS asserted is that the overshoot here is no larger than the
     * window that admitted the collapse. "Here" is load-bearing and the
     * sentence must not be read as a general bound: this row's traffic is
     * zero, so its window is the bound scale alone. On a row that traffic
     * has moved, the window carries a traffic/|a| term and the overshoot
     * bound grows with it — up to 4 * DBL_EPSILON * traffic / |a|, which
     * nothing in the file caps. That is the containment item, stated as a
     * quantity rather than as a worry.
     *
     * Under PRESOLVE_TIGHTEN_EPS this same model overshot by 0.2 out of a
     * window of 1.0, which is the difference 02-09 makes at this site. If
     * the containment item is closed later, this assertion still holds and
     * the one above it can be tightened to the box. */
    const double window = 8.0 * DBL_EPSILON * 1e9;   /* PRESOLVE_ROUND_ULPS */
    TEST_ASSERT_TRUE(x[0] >= 0.0);
    TEST_ASSERT_TRUE(x[0] <= 1e9 + window);
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
     * exactly why its rule has to be asked in the right space. */
    jaos_model *m = make_maximised_empty_column(-INFINITY, 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 5.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);
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

    RUN_TEST(test_a_frozen_row_missed_at_scale_is_refused);

    RUN_TEST(test_a_maximised_singleton_row_is_owed_its_multiplier);
    RUN_TEST(test_a_maximised_empty_column_takes_its_upper_bound);
    RUN_TEST(test_a_maximised_empty_column_is_not_unbounded_downwards);
    RUN_TEST(test_a_maximised_forcing_row_is_owed_its_multiplier);
    return UNITY_END();
}
