/* Presolve/postsolve tests: the round trip for the one reduction this plan
 * ships (a column fixed as loaded), and the two structural invariants D-01
 * makes non-negotiable — every array publish leaves is in the caller's own
 * index space, and the caller's model is untouched by a reducing solve.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

/* min x0 + x1 + 5*x2  s.t.  x0 + x1 + 3*x2 >= 1,  0 <= x0,x1 <= 10,
 * x2 fixed at 2 as loaded (col_lower == col_upper).
 *
 * Presolve drops x2: its cost*value (5*2=10) folds into the objective
 * offset, and its 3*2=6 folds into the row's lower bound, giving a reduced
 * problem of min x0+x1 (+10) s.t. x0+x1 >= -5, 0<=x0,x1<=10 — trivially
 * satisfied by the cold-start slack basis at x0=x1=0, so this solves in
 * zero iterations. By hand: reduced objective is 0 + 10 = 10; adding back
 * x2's own contribution to the row gives an original activity of 6, which
 * sits strictly inside (1, +inf) — the row's dual must therefore be exactly
 * zero for complementary slackness, and it is, by construction, since the
 * reduced solve's own logical is basic. Every quantity here is an integer
 * a double represents exactly, so nothing above is subject to rounding: the
 * hand-derived value below is what a presolve-off solve of the same problem
 * computes too, which is what this plan's own <verify> command — running
 * this same test suite a second time under
 * `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` — actually checks. A single process
 * cannot flip that compile-time switch, so the "presolve-off" half of the
 * comparison lives in the second build, not in a second solve here. */
static jaos_model *make_one_fixed_column(void)
{
    const double c[]  = {1.0, 1.0, 5.0};
    const double cl[] = {0.0, 0.0, 2.0}, cu[] = {10.0, 10.0, 2.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}

static void test_fixed_column_round_trip(void)
{
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
    const double expected_x2 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x2, &x[2], sizeof x[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    /* The row-count invariant jaos_set_basis enforces on any basis handed
     * in (src/model.c) must hold on a postsolved one too, in original
     * indices, with no help from the checker to see it (D-10). */
    jaos_basis_status cs[3], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 3; j++)
        basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 1; i++)
        basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(1, basic);   /* == num_row */

    /* A fixed column carries no sign condition (check.c's sign_condition:
     * "fixed -> anything") so any status is correct; this plan always
     * publishes AT_LOWER for one, which is asserted directly here rather
     * than left implicit. */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[2]);

    jaos_model_free(m);
}

/* min 2*x0 + 3*x1  s.t.  x0 + x1 <= 10,  x0 fixed at 1, x1 fixed at 4.
 * Every column presolve fixes: outcome is JM_PRESOLVE_SOLVED, no sx is
 * built and the simplex never runs. By hand: objective is 2*1 + 3*4 = 14,
 * activity is 1 + 4 = 5, strictly inside the row's own bounds, so its dual
 * is zero — which the SOLVED path publishes unconditionally, matching
 * complementary slackness for exactly the reason the round-trip test's row
 * is interior too. */
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

static void test_all_columns_fixed_solves_with_no_iterations(void)
{
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
}

/* D-06's structural half: jaos_model's own CSC and bound arrays are
 * untouched by a solve that reduces the model, whether or not the
 * reduction leaves anything for the simplex to run on. Captured before the
 * solve and compared after under memcmp, which is what "never writes to
 * m" actually means as a test rather than as a comment. */
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixed_column_round_trip);
    RUN_TEST(test_all_columns_fixed_solves_with_no_iterations);
    RUN_TEST(test_original_arrays_survive_a_reducing_solve);
    return UNITY_END();
}
