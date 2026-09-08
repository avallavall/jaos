/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

constexpr double CHECK_TOL = 1e-6;

static jaos_model *two_column_lp(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[2] = {1.0, 2.0}, cl[2] = {0.0, 0.0}, cu[2] = {5.0, 5.0};
    const double rl[1] = {2.0}, ru[1] = {5.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                2, as, ai, av));
    return m;
}

static jaos_model *every_bound_kind_lp(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double inf = jaos_infinity();
    const double cost[4] = {3.0, 2.0, -1.0, 4.0};
    const double cl[4] = {0.0, -inf, -inf, 1.0};
    const double cu[4] = {4.0, 6.0, inf, 1.0};
    const double rl[3] = {5.0, 1.0, -4.0};
    const double ru[3] = {5.0, 3.0, inf};
    const int64_t as[5] = {0, 2, 4, 6, 6};
    const int64_t ai[6] = {0, 1, 0, 2, 0, 1};
    const double av[6] = {1.0, 1.0, 1.0, 1.0, 1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 4, 3, JAOS_MAXIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                6, as, ai, av));
    return m;
}

static double solve_with(jaos_model *m, jaos_algorithm alg)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, alg));
    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    return obj;
}

static void test_the_barrier_is_the_third_algorithm(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_BARRIER, jaos_algorithm_of(m));
    TEST_ASSERT_TRUE(m->cfg.barrier);
    TEST_ASSERT_FALSE(m->cfg.force_primal);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PRIMAL));
    TEST_ASSERT_FALSE(m->cfg.barrier);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "algorithm", "barrier"));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_BARRIER, jaos_algorithm_of(m));
    char buf[32];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_get_option(m, "algorithm", buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("barrier", buf);
    jaos_model_free(m);
}

static void test_the_barrier_solves_the_two_column_lp(void)
{
    jaos_model *m = two_column_lp();
    const double obj = solve_with(m, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2.0, obj);
    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 2.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 0.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 1.0, y[0]);
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    TEST_ASSERT_TRUE(jaos_work_units(m) > 0);

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_the_barrier_agrees_with_the_dual_on_every_bound_kind(void)
{
    jaos_model *m = every_bound_kind_lp();
    const double dual = solve_with(m, JAOS_ALGORITHM_DUAL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 21.0, dual);
    const double barrier = solve_with(m, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6 * (1.0 + fabs(dual)), dual, barrier);
    double x[4], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 6.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -2.0, x[2]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, x[3]);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    jaos_model_free(m);
}

static void test_the_barrier_is_bit_identical_across_runs(void)
{
    jaos_model *a = every_bound_kind_lp();
    jaos_model *b = every_bound_kind_lp();
    (void)solve_with(a, JAOS_ALGORITHM_BARRIER);
    (void)solve_with(b, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(a), jaos_work_units(b));
    TEST_ASSERT_EQUAL_INT64(jaos_iterations(a), jaos_iterations(b));
    double xa[4], xb[4], ya[3], yb[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(a, xa, nullptr, ya, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(b, xb, nullptr, yb, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(xa, xb, sizeof xa);
    TEST_ASSERT_EQUAL_MEMORY(ya, yb, sizeof ya);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_the_barrier_stops_at_the_work_limit(void)
{
    jaos_model *m = every_bound_kind_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    double obj;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    jaos_model_free(m);
}

static void test_the_barrier_leaves_a_mip_to_the_simplex(void)
{
    jaos_model *m = two_column_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, obj);
    jaos_model_free(m);
}

static void test_the_crossover_publishes_a_vertex_with_a_basis(void)
{
    jaos_model *m = two_column_lp();
    (void)solve_with(m, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_TRUE(m->solve_barrier_iters > 0);
    TEST_ASSERT_TRUE(jaos_iterations(m) >= m->solve_barrier_iters);
    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, x[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, x[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, y[0]);
    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int basics = (cs[0] == JAOS_BASIS_BASIC) + (cs[1] == JAOS_BASIS_BASIC) +
                 (rs[0] == JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT(1, basics);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_the_crossover_reaches_the_dual_on_every_bound_kind(void)
{
    jaos_model *m = every_bound_kind_lp();
    const double obj = solve_with(m, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 21.0, obj);
    double x[4], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, x[2]);
    jaos_basis_status cs[4], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_without_the_crossover_the_point_is_interior(void)
{
    jaos_model *m = two_column_lp();
    m->cfg.barrier_no_crossover = true;
    (void)solve_with(m, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_EQUAL_INT64(m->solve_barrier_iters, jaos_iterations(m));
    double x[2], obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2.0, obj);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 2.0, x[0]);
    jaos_model_free(m);
}

static void test_the_barrier_does_not_call_an_infeasible_lp_optimal(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[2] = {1.0, 1.0}, cl[2] = {0.0, 0.0}, cu[2] = {1.0, 1.0};
    const double rl[2] = {3.0, -jaos_infinity()}, ru[2] = {jaos_infinity(), 1.0};
    const int64_t as[3] = {0, 2, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {1.0, 1.0, 1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_NOT_EQUAL(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_barrier_is_the_third_algorithm);
    RUN_TEST(test_the_barrier_solves_the_two_column_lp);
    RUN_TEST(test_the_barrier_agrees_with_the_dual_on_every_bound_kind);
    RUN_TEST(test_the_barrier_is_bit_identical_across_runs);
    RUN_TEST(test_the_barrier_stops_at_the_work_limit);
    RUN_TEST(test_the_barrier_leaves_a_mip_to_the_simplex);
    RUN_TEST(test_the_crossover_publishes_a_vertex_with_a_basis);
    RUN_TEST(test_the_crossover_reaches_the_dual_on_every_bound_kind);
    RUN_TEST(test_without_the_crossover_the_point_is_interior);
    RUN_TEST(test_the_barrier_does_not_call_an_infeasible_lp_optimal);
    return UNITY_END();
}
