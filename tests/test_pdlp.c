/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
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

static void test_pdlp_is_the_fourth_algorithm(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PDLP));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_PDLP, jaos_algorithm_of(m));
    TEST_ASSERT_TRUE(m->cfg.pdlp);
    TEST_ASSERT_FALSE(m->cfg.barrier);
    TEST_ASSERT_FALSE(m->cfg.force_primal);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_FALSE(m->cfg.pdlp);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "algorithm", "pdlp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_PDLP, jaos_algorithm_of(m));
    char buf[32];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_get_option(m, "algorithm", buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("pdlp", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_algorithm(m, (jaos_algorithm)4));
    jaos_model_free(m);
}

static void test_pdlp_solves_the_two_column_lp_to_a_vertex(void)
{
    jaos_model *m = two_column_lp();
    const double obj = solve_with(m, JAOS_ALGORITHM_PDLP);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, obj);
    TEST_ASSERT_TRUE(m->solve_barrier_iters > 0);
    TEST_ASSERT_TRUE(jaos_iterations(m) >= m->solve_barrier_iters);
    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, x[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, x[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, y[0]);
    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    const int basics = (cs[0] == JAOS_BASIS_BASIC) + (cs[1] == JAOS_BASIS_BASIC) +
                       (rs[0] == JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT(1, basics);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_pdlp_agrees_with_the_dual_on_every_bound_kind(void)
{
    jaos_model *m = every_bound_kind_lp();
    const double dual = solve_with(m, JAOS_ALGORITHM_DUAL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 21.0, dual);
    const double pdlp = solve_with(m, JAOS_ALGORITHM_PDLP);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, dual, pdlp);
    double x[4], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, x[2]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, x[3]);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_pdlp_is_bit_identical_across_runs(void)
{
    jaos_model *a = every_bound_kind_lp();
    jaos_model *b = every_bound_kind_lp();
    (void)solve_with(a, JAOS_ALGORITHM_PDLP);
    (void)solve_with(b, JAOS_ALGORITHM_PDLP);
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(a), jaos_work_units(b));
    TEST_ASSERT_EQUAL_INT64(jaos_iterations(a), jaos_iterations(b));
    TEST_ASSERT_EQUAL_INT64(a->solve_barrier_iters, b->solve_barrier_iters);
    double xa[4], xb[4], ya[3], yb[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(a, xa, nullptr, ya, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(b, xb, nullptr, yb, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(xa, xb, sizeof xa);
    TEST_ASSERT_EQUAL_MEMORY(ya, yb, sizeof ya);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_pdlp_stops_at_the_work_limit(void)
{
    jaos_model *m = every_bound_kind_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PDLP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    double obj;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    jaos_model_free(m);
}

static void test_pdlp_leaves_a_mip_to_the_simplex(void)
{
    jaos_model *m = two_column_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PDLP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, m->solve_barrier_iters);
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, obj);
    jaos_model_free(m);
}

static void test_without_the_crossover_the_pdlp_point_is_near_the_vertex(void)
{
    jaos_model *m = two_column_lp();
    m->cfg.barrier_no_crossover = true;
    (void)solve_with(m, JAOS_ALGORITHM_PDLP);
    TEST_ASSERT_EQUAL_INT64(m->solve_barrier_iters, jaos_iterations(m));
    double x[2], obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 2.0, obj);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 2.0, x[0]);
    jaos_model_free(m);
}

static void test_pdlp_hands_an_infeasible_lp_to_the_dual(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[2] = {1.0, 1.0}, cl[2] = {0.0, 0.0}, cu[2] = {10.0, 10.0};
    const double rl[2] = {1.0, -jaos_infinity()}, ru[2] = {jaos_infinity(), -1.0};
    const int64_t as[3] = {0, 2, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {1.0, 1.0, -1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PDLP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    double ray[2];
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, ray));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_certificate(m, ray, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    jaos_model_free(m);
}

static jaos_model *refused_lp(void)
{
    const double inf = jaos_infinity();
    const double cost[4] = {2.0, -1.0, -4.0, 4.0};
    const double cl[4] = {0.0, 0.0, 0.0, 0.0};
    const double cu[4] = {3.0, 4.0, 3.0, 2.0};
    const double rl[4] = {-inf, -inf, 4.0, -2.0};
    const double ru[4] = {2.0, 5.0, inf, -2.0};
    const int64_t as[5] = {0, 3, 6, 9, 13};
    const int64_t ai[13] = {0, 1, 3, 1, 2, 3, 0, 1, 2, 0, 1, 2, 3};
    const double av[13] = {2.0, -2.0, -1.0, -1.0, 1.0, 1.0,
                           2.0, 1.0, -1.0, 1.0, 1.0, 1.0, -1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 4, 4, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                13, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PDLP));
    return m;
}

static void test_a_ray_in_the_iterates_ends_a_refused_lp_early(void)
{
    jaos_model *m = refused_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    TEST_ASSERT_TRUE(jaos_iterations(m) > 4096);
    TEST_ASSERT_TRUE(jaos_iterations(m) < 50000);

    double ray[4];
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, ray));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, ray, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.certified);

    jaos_model *b = refused_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(m), jaos_work_units(b));
    TEST_ASSERT_EQUAL_INT64(jaos_iterations(m), jaos_iterations(b));
    jaos_model_free(b);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_ray_in_the_iterates_ends_a_refused_lp_early);
    RUN_TEST(test_pdlp_is_the_fourth_algorithm);
    RUN_TEST(test_pdlp_solves_the_two_column_lp_to_a_vertex);
    RUN_TEST(test_pdlp_agrees_with_the_dual_on_every_bound_kind);
    RUN_TEST(test_pdlp_is_bit_identical_across_runs);
    RUN_TEST(test_pdlp_stops_at_the_work_limit);
    RUN_TEST(test_pdlp_leaves_a_mip_to_the_simplex);
    RUN_TEST(test_without_the_crossover_the_pdlp_point_is_near_the_vertex);
    RUN_TEST(test_pdlp_hands_an_infeasible_lp_to_the_dual);
    return UNITY_END();
}
