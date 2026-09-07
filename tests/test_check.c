/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "unity.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

#define EXACT_D(want, got)                                                 \
    TEST_ASSERT_TRUE_MESSAGE((want) == (got), #got " is not exactly " #want)

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
    const double y[] = {-1.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_dual_violation);
    jaos_model_free(m);
}

static void test_t1_flags_complementarity_break(void)
{
    jaos_model *m = make_t1();

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
    const double x[] = {-1.0, 0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, TOL, &r));

    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_col_violation);

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
    const double y[] = {3.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_dual_violation);
    jaos_model_free(m);
}

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

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.max_dual_violation);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.1 / 1.1, r.objective_gap);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

static void test_an_implied_bound_makes_the_dropped_term_finite(void)
{
    const double c[] = {0.0, -1e-7};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {1e6};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double origin[] = {0.0, 0.0};
    const double y0[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, origin, y0, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.max_dual_violation);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.1, r.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.gap_negative);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.1, r.relative_suboptimality);

    TEST_ASSERT_TRUE(r.gap_certified);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.max_dropped_multiplier);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.1, r.certified_suboptimality);

    const double best[] = {0.0, 1e6};
    const double ybest[] = {-1e-7};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, best, ybest, 1e-6, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_TRUE(r.gap_certified);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.max_dropped_multiplier);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.certified_suboptimality);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -0.1, r.primal_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -0.1, r.dual_objective);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.relative_suboptimality);
    jaos_model_free(m);
}

static void test_an_unbounded_ray_is_counted_unless_its_rate_is_real(void)
{
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    const double x[] = {1.0, 0.0};
    const double y[] = {0.0};
    jaos_check_report r;

    const double tiny[] = {0.0, -1e-9};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, tiny, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_INT64(1, r.unquantified_rays);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.certified_suboptimality);
    TEST_ASSERT_FALSE(r.gap_certified);
    jaos_model_free(m);

    const double real[] = {0.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, real, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_INT64(0, r.unquantified_rays);
    TEST_ASSERT_TRUE(isinf(r.certified_suboptimality));
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

static jaos_model *make_scaled_row(double cost, double coef, double row_lo)
{
    const double c[] = {cost};
    const double cl[] = {1.0}, cu[] = {1.0};
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

static void test_a_row_at_its_bound_to_its_own_precision_is_accepted(void)
{
    jaos_model *m = make_scaled_row(1e9, 1e10, 1e10 - 1e-3);
    const double x[] = {1.0};
    const double y[] = {28.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(1e-3 > 1e-6);
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.max_dual_violation);

    TEST_ASSERT_TRUE(r.objective_gap < 1e-6);
    TEST_ASSERT_TRUE(r.dual_feasible);
    jaos_model_free(m);
}

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

static void test_a_waived_row_that_still_costs_is_refused_by_the_gap(void)
{
    jaos_model *m = make_scaled_row(0.0, 1e6, 1e6 - 0.5);
    const double x[] = {1.0};
    const double y[] = {1000.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.max_dual_violation);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.primal_objective);
    TEST_ASSERT_EQUAL_DOUBLE(-500.0, r.dual_objective);
    TEST_ASSERT_TRUE(r.objective_gap > 0.9);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

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

    const double x[] = {1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, r.max_dual_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

static void test_the_gap_can_be_two_large_halves_cancelling(void)
{
    const double c[] = {1e9, 1e-7};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, 9e9};
    const double rl[] = {1.0}, ru[] = {INFINITY};

    const int64_t s[] = {0, 1, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double x[] = {1.0 - 9e-7, 9e9};
    const double y[] = {1e9};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 9e-7, r.max_row_violation);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.max_dual_violation);

    TEST_ASSERT_TRUE(r.objective_gap < 1e-15);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 900.0, r.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 900.0, r.gap_negative);

    double scale = 1.0 + fabs(r.primal_objective) + fabs(r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, r.objective_gap,
                              fabs(r.gap_positive - r.gap_negative) / scale);
    jaos_model_free(m);
}

static void test_the_objective_is_read_with_its_compensation(void)
{
    const double c[] = {1e25, 1e8, -1e25, 1.0};
    const double cl[] = {1.0, 1.0, 1.0, 0.0};
    const double cu[] = {1.0, 1.0, 1.0, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 1, 1, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double x[] = {1.0, 1.0, 1.0, 1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    const double want = 100000001.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &r.primal_objective, sizeof want);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.gap_positive);

    TEST_ASSERT_DOUBLE_WITHIN(1e-16, 1.0 / (1.0 + want),
                              r.relative_suboptimality);
    TEST_ASSERT_TRUE(r.relative_suboptimality < 1e-6);

    const double scale =
        1.0 + fabs(r.primal_objective) + fabs(r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-18, r.objective_gap,
                              fabs(r.gap_positive - r.gap_negative) / scale);
    jaos_model_free(m);
}

static void test_a_clean_point_carries_no_negative_half(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {1.0};
    jaos_check_report r;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.gap_negative);
    jaos_model_free(m);
}

static void test_the_relative_row_residue_is_reported_and_decides_nothing(void)
{
    const double c[] = {0.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1e6, -1e6};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double x[] = {1.0 - 0x1p-40, 1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    const double viol = 1e6 * 0x1p-40;
    const double traffic = 2e6 - viol;
    TEST_ASSERT_DOUBLE_WITHIN(1e-18, viol, r.max_row_violation);
    TEST_ASSERT_DOUBLE_WITHIN(1e-24, viol / traffic,
                              r.max_row_violation_relative);

    TEST_ASSERT_TRUE(r.primal_feasible);
    jaos_model_free(m);
}

static void test_an_exempt_multiplier_still_moves_the_dual_objective(void)
{
    const double w = ldexp(1.0, -40);
    const double c[] = {w};
    const double cl[] = {0.0}, cu[] = {2048.0};
    const double rl[] = {1024.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double x[] = {2048.0};
    const double y[] = {w};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);

    EXACT_D(0.0, r.max_dual_violation);

    EXACT_D(ldexp(1.0, -30), r.dual_objective);
    EXACT_D(ldexp(1.0, -29), r.primal_objective);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_TRUE(r.gap_certified);
    EXACT_D(r.primal_objective - r.dual_objective,
            r.gap_positive - r.gap_negative);

    jaos_model_free(m);
}

static void test_a_dropped_term_has_no_magnitude_exemption(void)
{
    const double c[] = {-1e-15};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double x[] = {0.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    EXACT_D(0.0, r.max_dual_violation);

    TEST_ASSERT_EQUAL_INT64(1, r.dropped_terms);
    EXACT_D(1e-15, r.max_dropped_multiplier);
    TEST_ASSERT_FALSE(r.gap_certified);

    TEST_ASSERT_EQUAL_INT64(1, r.unquantified_rays);
    EXACT_D(0.0, r.certified_suboptimality);

    jaos_model_free(m);
}

static void test_a_step_from_outside_a_row_bound_is_zero_not_negative(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {5.0};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double y[] = {0.0};
    jaos_check_report r;

    const double past[] = {5.0 + 1e-10};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, past, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    EXACT_D(0.0, r.certified_suboptimality);
    TEST_ASSERT_EQUAL_INT64(0, r.unquantified_rays);

    const double inside[] = {4.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, inside, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    EXACT_D(1.0, r.certified_suboptimality);

    jaos_model_free(m);
}

static void test_the_implied_box_is_exactly_what_the_row_implies(void)
{
    const double c[] = {1.0};
    const double cl[] = {-INFINITY}, cu[] = {INFINITY};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double y[] = {0.5};
    jaos_check_report r;

    const double opt[] = {3.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, opt, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    EXACT_D(3.0, r.primal_objective);
    EXACT_D(3.0, r.dual_objective);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_TRUE(r.gap_certified);

    const double inside[] = {10.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, inside, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    EXACT_D(10.0, r.primal_objective);
    EXACT_D(3.0, r.dual_objective);
    EXACT_D(7.0, r.gap_positive);
    EXACT_D(0.0, r.gap_negative);
    EXACT_D(r.primal_objective - r.dual_objective, r.gap_positive);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);

    jaos_model_free(m);
}

static void test_an_infinite_term_is_counted_not_summed(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {-INFINITY, 1.0}, cu[] = {INFINITY, 1.0};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double x[] = {2.0, 1.0};
    const double y[] = {0.5};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    EXACT_D(3.0, r.primal_objective);

    EXACT_D(3.0, r.dual_objective);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_TRUE(r.gap_certified);

    jaos_model_free(m);
}

static void test_certificate_of_a_simplex_proved_infeasibility(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {2.0};
    const double rl[] = {4.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    double y[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));

    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_TRUE_MESSAGE(rep.certified,
                             "the published ray must certify its own "
                             "model");
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, rep.inf_rows);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.sup_columns);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.gap);
    jaos_model_free(m);
}

static void test_certificate_lifted_through_a_forcing_row(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve answers this model");
#elif defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("the lift restores through the faulted index");
#else
    const double c[] = {0.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {5.0, 5.0, 2.0};
    const double rl[] = {-INFINITY, 3.0}, ru[] = {0.0, INFINITY};
    const int64_t s[] = {0, 2, 3, 4}, ix[] = {0, 1, 0, 1};
    const double v[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    double y[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, y[1]);

    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, rep.inf_rows);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.sup_columns);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, rep.gap);
    jaos_model_free(m);
#endif
}

static void test_certificate_of_a_reduced_solve_is_lifted(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve reduces this model");
#elif defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("the lift restores through the faulted index");
#else
    const double c[] = {0.0, 1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0}, cu[] = {5.0, 5.0, 2.0, 2.0};
    const double rl[] = {-INFINITY, 3.0, -INFINITY};
    const double ru[] = {0.0, INFINITY, 1.0};
    const int64_t s[] = {0, 2, 3, 5, 7}, ix[] = {0, 1, 0, 1, 2, 1, 2};
    const double v[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     7, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    double y[3] = {0.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    TEST_ASSERT_TRUE_MESSAGE(y[0] < 0.0, "the forcing row must carry a "
                             "multiplier toward its upper side");
    TEST_ASSERT_TRUE(y[1] > 0.0);
    TEST_ASSERT_TRUE(y[2] < 0.0);

    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);

    y[0] = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    jaos_model_free(m);
#endif
}

static void test_certificate_lifted_through_a_singleton_row_fold(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve answers this model");
#else
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 2.0};
    const double rl[] = {-INFINITY, 5.0}, ru[] = {2.0, INFINITY};
    const int64_t s[] = {0, 2, 3}, ix[] = {0, 1, 1};
    const double v[] = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    double y[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_ASSERT_FALSE_MESSAGE(rep.certified,
                              "a lift restoring one row off must not "
                              "certify");
#else
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, y[1]);
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, rep.inf_rows);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.sup_columns);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, rep.gap);
#endif
    jaos_model_free(m);
#endif
}

static void test_certificate_of_an_empty_row(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve answers this model");
#else
    const double c[] = {0.0, 0.0};
    const double cl[] = {3.0, 1.0}, cu[] = {3.0, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {2.0};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    double y[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, rep.inf_rows);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -4.0, rep.sup_columns);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.gap);
    jaos_model_free(m);
#endif
}

static void test_certificate_of_a_frozen_row(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve answers this model");
#else
    const double c[] = {0.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {1.0, 2.0, 1.0};
    const double rl[] = {5.0, -INFINITY}, ru[] = {INFINITY, 10.0};
    const int64_t s[] = {0, 1, 3, 4}, ix[] = {0, 0, 1, 1};
    const double v[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    double y[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, rep.inf_rows);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, rep.sup_columns);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.gap);
    jaos_model_free(m);
#endif
}

static void test_certificate_refused_on_inverted_bounds(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve answers this model");
#else
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {5.0}, ru[] = {3.0};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_certificate(m, y));
    jaos_model_free(m);
#endif
}

static void test_a_wrong_certificate_is_rejected(void)
{
    jaos_model *m = make_t1();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double y[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_certificate(m, y));

    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_FALSE_MESSAGE(rep.certified,
                              "a feasible model must reject every ray");
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, rep.inf_rows);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, rep.sup_columns);
    jaos_model_free(m);
}

static void test_a_certificate_needing_an_absent_bound_is_rejected(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {4.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    const double y[1] = {1.0};
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, y, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    TEST_ASSERT_TRUE_MESSAGE(isinf(rep.sup_columns) && rep.sup_columns > 0,
                             "the report must say which side died");
    jaos_model_free(m);
}

static void test_ray_of_a_simplex_proved_unboundedness(void)
{
    const double c[] = {-1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    double d[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_TRUE_MESSAGE(d[0] > 0.0, "the ray must move x0 upward");

    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_TRUE_MESSAGE(rep.certified,
                             "the published ray must certify its own "
                             "model");
    TEST_ASSERT_TRUE(rep.rate < 0.0);
    jaos_model_free(m);
}

static void test_ray_of_a_presolve_proved_unboundedness(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve answers this model");
#else
    const double c[] = {-1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, 10.0};
    const double rl[] = {-INFINITY}, ru[] = {3.0};
    const int64_t s[] = {0, 0, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    double d[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, d[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, d[1]);
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, rep.rate);
    jaos_model_free(m);
#endif
}

static void test_ray_lifted_through_a_singleton_column(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve reduces this model");
#elif defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("the lift restores through the faulted index");
#else
    const double c[] = {-1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, -1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    double d[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_TRUE(d[0] > 0.0);
    TEST_ASSERT_TRUE_MESSAGE(d[1] > 0.0, "x1 must move with x0 to keep "
                             "row 0 under its ceiling");
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);

    d[1] = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    TEST_ASSERT_TRUE(rep.max_row_escape > 0.0);
    jaos_model_free(m);
#endif
}

static void test_ray_lifted_through_an_implied_free_column(void)
{
#if defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("presolve-path test — runs only in the default "
                        "build, where presolve reduces this model");
#elif defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("the lift restores through the faulted index");
#else
    const double c[] = {1.0, 0.0, 0.5};
    const double cl[] = {-INFINITY, 0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY, INFINITY};
    const double rl[] = {1.0, -INFINITY}, ru[] = {1.0, 5.0};
    const int64_t s[] = {0, 1, 3, 4}, ix[] = {0, 0, 1, 1};
    const double v[] = {1.0, 1.0, 1.0, -1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    double d[3] = {0.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_TRUE(d[1] > 0.0);
    TEST_ASSERT_TRUE_MESSAGE(d[0] < 0.0, "x0 must fall as x1 rises to "
                             "hold the equality row");
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_TRUE(rep.rate < 0.0);

    d[0] = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    TEST_ASSERT_TRUE(rep.max_row_escape > 0.0);
    jaos_model_free(m);
#endif
}

static void test_a_wrong_ray_is_rejected(void)
{
    jaos_model *m = make_t1();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double d[2] = {1.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_unbounded_ray(m, d));

    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_FALSE_MESSAGE(rep.certified,
                              "a bounded model must reject the ray");
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, rep.max_col_escape);
    jaos_model_free(m);
}

static void test_the_dual_objective_keeps_a_term_the_wide_type_would_lose(void)
{
    const double c[] = {1e25, 1.0, -1e25};
    const double cl[] = {1.0, 1.0, 1.0};
    const double cu[] = {1.0, 1.0, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t st[] = {0, 1, 2, 3}, ix[] = {0, 0, 0};
    const double v[] = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, st, ix, v));

    const double x[] = {1.0, 1.0, 1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    const double want = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &r.dual_objective, sizeof want);

    TEST_ASSERT_EQUAL_MEMORY(&want, &r.primal_objective, sizeof want);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.gap_positive);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.gap_negative);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.objective_gap);
    TEST_ASSERT_TRUE(r.dual_feasible);
    jaos_model_free(m);
}

static void test_the_ray_rate_keeps_a_term_the_wide_type_would_lose(void)
{
    const double c[] = {1e25, -1.0, -1e25};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t st[] = {0, 1, 2, 3}, ix[] = {0, 0, 0};
    const double v[] = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, st, ix, v));

    const double d[] = {1.0, 1.0, 1.0};
    jaos_ray_report rr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rr));

    const double want = -1.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &rr.rate, sizeof want);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, rr.max_col_escape);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rr.max_row_escape);
    TEST_ASSERT_FALSE(rr.certified);
    jaos_model_free(m);
}

static void test_an_overflowing_gap_term_does_not_poison_the_accumulator(void)
{
    const double c[]  = {-9.269728764946841e+299};
    const double cl[] = {-9.8070032592151977e+299};
    const double cu[] = {-8.6967484764852674e+299};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t st[] = {0, 1}, ix[] = {0};
    const double v[]  = {7.7022702441179374e+299};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, st, ix, v));

    const double x[] = {4.4129140369091983e+299};
    const double y[] = {-4.3379028085657922e+299};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &r));

    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);

    TEST_ASSERT_FALSE(isnan(r.gap_positive));
    TEST_ASSERT_FALSE(isnan(r.gap_negative));
    TEST_ASSERT_TRUE(r.gap_positive >= 0.0);
    TEST_ASSERT_TRUE(r.gap_negative >= 0.0);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_gap_can_be_two_large_halves_cancelling);
    RUN_TEST(test_the_objective_is_read_with_its_compensation);
    RUN_TEST(test_the_dual_objective_keeps_a_term_the_wide_type_would_lose);
    RUN_TEST(test_the_ray_rate_keeps_a_term_the_wide_type_would_lose);
    RUN_TEST(test_an_overflowing_gap_term_does_not_poison_the_accumulator);
    RUN_TEST(test_a_clean_point_carries_no_negative_half);
    RUN_TEST(test_the_relative_row_residue_is_reported_and_decides_nothing);
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
    RUN_TEST(test_an_implied_bound_makes_the_dropped_term_finite);
    RUN_TEST(test_an_unbounded_ray_is_counted_unless_its_rate_is_real);
    RUN_TEST(test_an_exempt_multiplier_still_moves_the_dual_objective);
    RUN_TEST(test_a_dropped_term_has_no_magnitude_exemption);
    RUN_TEST(test_a_step_from_outside_a_row_bound_is_zero_not_negative);
    RUN_TEST(test_the_implied_box_is_exactly_what_the_row_implies);
    RUN_TEST(test_an_infinite_term_is_counted_not_summed);
    RUN_TEST(test_check_rejects_bad_arguments);
    RUN_TEST(test_certificate_of_a_simplex_proved_infeasibility);
    RUN_TEST(test_certificate_lifted_through_a_forcing_row);
    RUN_TEST(test_certificate_of_a_reduced_solve_is_lifted);
    RUN_TEST(test_certificate_lifted_through_a_singleton_row_fold);
    RUN_TEST(test_certificate_of_an_empty_row);
    RUN_TEST(test_certificate_of_a_frozen_row);
    RUN_TEST(test_certificate_refused_on_inverted_bounds);
    RUN_TEST(test_a_wrong_certificate_is_rejected);
    RUN_TEST(test_a_certificate_needing_an_absent_bound_is_rejected);
    RUN_TEST(test_ray_of_a_simplex_proved_unboundedness);
    RUN_TEST(test_ray_of_a_presolve_proved_unboundedness);
    RUN_TEST(test_ray_lifted_through_a_singleton_column);
    RUN_TEST(test_ray_lifted_through_an_implied_free_column);
    RUN_TEST(test_a_wrong_ray_is_rejected);
    return UNITY_END();
}
