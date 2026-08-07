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

/* T3: what a multiplier below the tolerance contributes to the dual
 * objective. Both tests below are exact in binary floating point, so
 * neither is about rounding.
 *
 *      min   x1 + 1e-7 * x3
 *      s.t.  x1        >= 1
 *            0   <= x1 <= 10
 *            1e6 <= x3 <= 2e6        (x3 appears in no row)
 */
static jaos_model *make_t3(void)
{
    const double c[] = {1.0, 1e-7};
    const double cl[] = {0.0, 1e6}, cu[] = {10.0, 2e6};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

/* A tiny multiplier still carries dual objective when the bound it points
 * at is large, and dropping it invents a gap out of nothing.
 *
 * x1 = 1 sits in the interior with d1 = 0; x3 = 1e6 rests on its lower
 * bound with d3 = 1e-7, which is exactly where a positive reduced cost
 * belongs. Complementary slackness holds exactly and so does strong
 * duality: D = y*1 + d1*0 + d3*1e6 = 1 + 0 + 0.1 = 1.1 = P.
 *
 * d3 is below any tolerance the gate uses. Discarding its contribution
 * would leave D = 1 against P = 1.1 and reject a pair that is optimal on
 * every count, with a relative gap of 9%. This is the shape that rejected
 * pilot-ja (PLAN 2.8). */
static void test_a_tiny_multiplier_on_a_large_bound_still_counts(void)
{
    jaos_model *m = make_t3();
    const double x[] = {1.0, 1e6};
    const double y[] = {1.0};
    jaos_check_report r;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.1, r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.objective_gap);
    jaos_model_free(m);
}

/* The repair for the above must not become "small multipliers contribute
 * w * v", which cancels their term and stops measuring them at all.
 *
 *      min  1e-7 * x1   s.t.  x1 + x2 >= 1,  0 <= x1, x2 <= 1e6
 *
 * The optimum is x = (0, 1) at objective 0. The pair below claims
 * x = (1e6, 0) — feasible, and costing 0.1, which is 1e5 tolerances away
 * from optimal. With y = 0 the reduced cost d1 = 1e-7 points at x1's lower
 * bound while x1 sits at 1e6: complementary slackness is broken by exactly
 * 1e-7 * 1e6 = 0.1, the whole of the suboptimality. The sign condition is
 * waived because |d1| is small, so the gap is the only thing left that can
 * catch it, and it must. */
static void test_a_waived_sign_condition_is_still_caught_by_the_gap(void)
{
    const double c[] = {1e-7, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1e6, 1e6};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double x[] = {1e6, 0.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);      /* it is feasible, just wrong */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.max_dual_violation); /* waived */
    /* 0.1 of absolute gap over a scale of 1 + |0.1| + |0| = 1.1. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.1 / 1.1, r.objective_gap);
    TEST_ASSERT_FALSE(r.dual_feasible);
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

/* ---- The scale on the bound-proximity test ------------------------- *
 *
 * A row activity is a sum, and a sum whose terms cancel cannot be pinned to
 * an absolute tolerance. Row 3 of Netlib's `finnis` adds terms totalling
 * 4.0e10 in magnitude and lands 1.5e-6 from its bound, where one ulp at
 * 4.0e10 is 7.6e-6: judged absolutely at 1e-6 it is "not at its bound" and
 * its multiplier of 28 is reported as a violation of 28. So the window is
 * tol times what the row carries.
 *
 * That is a loosening, and it needs a case it must still reject or it is
 * just a way of making a gate go green. The three below are that case and
 * its two neighbours. They share one shape: a single column fixed at 1, so
 * that its own sign condition accepts any multiplier and the row's is the
 * only thing under test.
 *
 * What makes the loosening safe is that this test is a diagnostic and the
 * gap is the proof. P - D = sum of w_v (v - bound_v) over every entity, each
 * term non-negative on a primal-feasible point, so a row waived at distance
 * d with multiplier w still contributes exactly w*d to the gap. The waiver
 * cannot hide anything; it can only decline to report it twice.
 */
static jaos_model *make_scaled_row(double cost, double coef, double row_lo)
{
    const double c[] = {cost};
    const double cl[] = {1.0}, cu[] = {1.0};   /* fixed: accepts any dual */
    const double rl[] = {row_lo}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {coef};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

/* The finnis shape: activity 1e10, bound 1e-3 below it, so the distance is a
 * thousand times the old absolute tolerance and 1e-7 of what the row
 * carries. Accepted now; rejected before, on a solution that is right. */
static void test_a_row_at_its_bound_to_its_own_precision_is_accepted(void)
{
    jaos_model *m = make_scaled_row(1e9, 1e10, 1e10 - 1e-3);
    const double x[] = {1.0};
    const double y[] = {28.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    /* The distance is real and far above the absolute tolerance, which is
     * what makes this test say something. */
    TEST_ASSERT_TRUE(1e-3 > 1e-6);
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.max_dual_violation);
    /* w*d = 28 * 1e-3 against an objective of 1e9: real, and negligible
     * exactly as the identity says it should be. */
    TEST_ASSERT_TRUE(r.objective_gap < 1e-6);
    TEST_ASSERT_TRUE(r.dual_feasible);
    jaos_model_free(m);
}

/* The same row a hundred thousand times further out — ten times the window
 * the scale opens rather than a ten-thousandth of it. Still reported, at the
 * full magnitude of the multiplier. The scale is a scale, not an amnesty. */
static void test_a_row_genuinely_off_its_bound_is_still_reported(void)
{
    jaos_model *m = make_scaled_row(1e9, 1e10, 1e10 - 1e5);
    const double x[] = {1.0};
    const double y[] = {28.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(28.0, r.max_dual_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

/* The one that had to be built. The sign condition IS waived here — the
 * distance is half the window — and the answer is nonetheless refused,
 * because w*d is 500 and the gap carries it at full size. Under the old
 * absolute rule this was rejected on the sign condition; the point of the
 * test is that removing that route does not open a way through.
 *
 * The numbers are exact: primal objective 0, dual objective -500, and
 * 0 - (-500) is precisely 1000 * 0.5, which is the identity the waiver
 * rests on, checked rather than asserted. */
static void test_a_waived_row_that_still_costs_is_refused_by_the_gap(void)
{
    jaos_model *m = make_scaled_row(0.0, 1e6, 1e6 - 0.5);
    const double x[] = {1.0};
    const double y[] = {1000.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    /* Waived: the distance of 0.5 is inside the window of 1e-6 * 1e6. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.max_dual_violation);
    /* And refused anyway. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.primal_objective);
    TEST_ASSERT_EQUAL_DOUBLE(-500.0, r.dual_objective);
    TEST_ASSERT_TRUE(r.objective_gap > 0.9);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

/* A column value is one published number rather than a sum of cancelling
 * terms, so its scale is its own magnitude and nothing more. This pins that
 * the row argument was not quietly applied to columns as well: a column a
 * long way from its bound with a real reduced cost stays a violation
 * however large the row it sits in. */
static void test_the_column_test_did_not_inherit_the_rows_scale(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1e6};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1e10};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    /* x sits at 1, a million away from either bound, with d = 1: the sign
     * condition wants it at its lower bound and it is not. */
    const double x[] = {1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, r.max_dual_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_row_at_its_bound_to_its_own_precision_is_accepted);
    RUN_TEST(test_a_row_genuinely_off_its_bound_is_still_reported);
    RUN_TEST(test_a_waived_row_that_still_costs_is_refused_by_the_gap);
    RUN_TEST(test_the_column_test_did_not_inherit_the_rows_scale);
    RUN_TEST(test_t1_accepts_the_true_optimum);
    RUN_TEST(test_t1_flags_wrong_dual_sign);
    RUN_TEST(test_t1_flags_complementarity_break);
    RUN_TEST(test_t1_flags_primal_violation);
    RUN_TEST(test_t2_accepts_the_true_optimum_maximize);
    RUN_TEST(test_t2_flags_wrong_dual_magnitude);
    RUN_TEST(test_a_tiny_multiplier_on_a_large_bound_still_counts);
    RUN_TEST(test_a_waived_sign_condition_is_still_caught_by_the_gap);
    RUN_TEST(test_check_rejects_bad_arguments);
    return UNITY_END();
}
