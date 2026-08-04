/* Checker tests: two LPs solved by hand, correct and wrong answers for
 * each, in both objective senses.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "unity.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

/* T1 (minimize):  min x0 + x1  s.t.  x0 + x1 >= 1,  0 <= x <= 10.
 * Any point on the facet x0+x1 = 1 is optimal with objective 1;
 * the optimal row dual is y = 1, giving reduced costs d = (0, 0). */
static jaos_model *make_t1(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

/* T2 (maximize):  max 3x0 + 2x1  s.t.  x0 + x1 <= 4,
 * 0 <= x0 <= 2, 0 <= x1 <= 10. Optimum x = (2, 2), objective 10,
 * row dual y = 2, reduced costs d = c - A'y = (1, 0). */
static jaos_model *make_t2(void)
{
    const double c[] = {3.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {2.0, 10.0};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

static void test_t1_accepts_the_true_optimum(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {1.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.primal_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.objective_gap);
    jaos_model_free(m);
}

static void test_t1_flags_wrong_dual_sign(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {-1.0}; /* >= row at its lower bound demands y >= 0 */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    /* Two breaches: the row dual points at a bound that is not active
     * (magnitude 1), and with y = -1 the reduced costs become 2 on
     * interior columns (magnitude 2). The worst one wins. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_dual_violation);
    jaos_model_free(m);
}

static void test_t1_flags_complementarity_break(void)
{
    jaos_model *m = make_t1();
    /* Feasible but interior point: activity 4 is strictly off the bound,
     * so a nonzero dual there violates complementary slackness. */
    const double x[] = {2.0, 2.0};
    const double y[] = {1.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_dual_violation);
    jaos_model_free(m);
}

static void test_t1_flags_primal_violation(void)
{
    jaos_model *m = make_t1();
    const double x[] = {-1.0, 0.0}; /* below the column lower bound... */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, TOL, &r));

    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_col_violation);
    /* ...and activity -1 also breaches the row's lower bound of 1 by 2. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_row_violation);
    TEST_ASSERT_FALSE(r.checked_duals);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

static void test_t2_accepts_the_true_optimum_maximize(void)
{
    jaos_model *m = make_t2();
    const double x[] = {2.0, 2.0};
    const double y[] = {2.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, r.primal_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.objective_gap);
    jaos_model_free(m);
}

static void test_t2_flags_wrong_dual_magnitude(void)
{
    jaos_model *m = make_t2();
    const double x[] = {2.0, 2.0};
    const double y[] = {3.0}; /* right sign, wrong value: d = (0,-1) breaks */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    /* x1 = 2 is interior in [0,10], so its reduced cost 2-3 = -1 must be 0. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_dual_violation);
    jaos_model_free(m);
}

static void test_check_rejects_bad_arguments(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double xnan[] = {NAN, 0.5};
    jaos_check_report r;

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(nullptr, x, nullptr, TOL, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, nullptr, nullptr, TOL, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, x, nullptr, TOL, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, x, nullptr, -1.0, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, xnan, nullptr, TOL, &r));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_t1_accepts_the_true_optimum);
    RUN_TEST(test_t1_flags_wrong_dual_sign);
    RUN_TEST(test_t1_flags_complementarity_break);
    RUN_TEST(test_t1_flags_primal_violation);
    RUN_TEST(test_t2_accepts_the_true_optimum_maximize);
    RUN_TEST(test_t2_flags_wrong_dual_magnitude);
    RUN_TEST(test_check_rejects_bad_arguments);
    return UNITY_END();
}
