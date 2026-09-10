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
    const double cost[2] = {1.0, 1.0}, cl[2] = {0.0, 0.0}, cu[2] = {10.0, 10.0};
    const double rl[2] = {1.0, -jaos_infinity()}, ru[2] = {jaos_infinity(), -1.0};
    const int64_t as[3] = {0, 2, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {1.0, 1.0, -1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    double ray[2];
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, ray));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_certificate(m, ray, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_TRUE(m->solve_barrier_iters > 0);
    TEST_ASSERT_TRUE(jaos_iterations(m) > m->solve_barrier_iters);
    jaos_model_free(m);
}

static void test_the_barrier_hands_an_unbounded_lp_to_the_dual_for_its_ray(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/unbounded.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    double *ray = calloc((size_t)jaos_num_col(m), sizeof *ray);
    TEST_ASSERT_NOT_NULL(ray);
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, ray));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, ray, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    free(ray);
    jaos_model_free(m);
}

static void test_the_barrier_verdict_matches_the_dual_bit_for_bit(void)
{
    jaos_model *a = nullptr, *b = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(a, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(b, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(a));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(b));
    const int64_t nr = jaos_num_row(a);
    double *ra = calloc((size_t)nr, sizeof *ra), *rb = calloc((size_t)nr, sizeof *rb);
    TEST_ASSERT_NOT_NULL(ra);
    TEST_ASSERT_NOT_NULL(rb);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(a, ra));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(b, rb));
    TEST_ASSERT_EQUAL_MEMORY(ra, rb, (size_t)nr * sizeof *ra);
    free(ra);
    free(rb);
    jaos_model_free(a);
    jaos_model_free(b);
}

typedef struct {
    int dense_lines;
    long long dense_count;
} dense_log;

static void catch_dense(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    dense_log *d = user;
    const char *p = strstr(line, " dense columns left out");
    if (p == nullptr)
        return;
    const char *q = p;
    while (q > line && q[-1] >= '0' && q[-1] <= '9')
        q--;
    d->dense_count = strtoll(q, nullptr, 10);
    d->dense_lines++;
}

static jaos_model *dense_column_lp(void)
{
    constexpr int64_t R = 40;
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    double cost[R + 1], cl[R + 1], cu[R + 1], rl[R], ru[R];
    int64_t as[R + 2], ai[2 * R];
    double av[2 * R];
    for (int64_t i = 0; i < R; i++) {
        cost[i] = 1.0 + 0.01 * (double)i;
        cl[i] = 0.0;
        cu[i] = jaos_infinity();
        rl[i] = -jaos_infinity();
        ru[i] = 1.0 + 0.1 * (double)(i % 3);
        as[i] = i;
        ai[i] = i;
        av[i] = 1.0;
    }
    cost[R] = 0.5;
    cl[R] = 0.0;
    cu[R] = jaos_infinity();
    as[R] = R;
    for (int64_t i = 0; i < R; i++) {
        ai[R + i] = i;
        av[R + i] = 1.0 + 0.5 * (double)(i % 2);
    }
    as[R + 1] = 2 * R;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, R + 1, R, JAOS_MAXIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                2 * R, as, ai, av));
    return m;
}

static void test_a_dense_column_leaves_the_normal_matrix_and_the_answer_holds(void)
{
    jaos_model *m = dense_column_lp();
    const double dual = solve_with(m, JAOS_ALGORITHM_DUAL);
    dense_log d = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, catch_dense, &d));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    const double barrier = solve_with(m, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_EQUAL_INT(1, d.dense_lines);
    TEST_ASSERT_EQUAL_INT64(1, d.dense_count);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * (1.0 + fabs(dual)), dual, barrier);
    double x[41], y[40];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);

    jaos_model *a = dense_column_lp(), *b = dense_column_lp();
    (void)solve_with(a, JAOS_ALGORITHM_BARRIER);
    (void)solve_with(b, JAOS_ALGORITHM_BARRIER);
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(a), jaos_work_units(b));
    double xa[41], xb[41];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(a, xa, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(b, xb, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(xa, xb, sizeof xa);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_a_separable_qp_solves_by_the_barrier_and_the_checker_accepts(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g_quad.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_DUAL, jaos_algorithm_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_TRUE(m->solve_barrier_iters > 0);
    TEST_ASSERT_EQUAL_INT64(m->solve_barrier_iters, jaos_iterations(m));
    double obj = 0.0, x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 3.0, y[0]);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, rep.primal_objective);

    jaos_model *a = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(a, "tests/data/g_quad.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(m), jaos_work_units(a));
    double xa[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(a, xa, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(x, xa, sizeof x);
    jaos_model_free(a);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_sense(m, JAOS_MAXIMIZE));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, -1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 1, -1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, -2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 1, -2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -4.0, obj);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "convex"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, -2.0));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PRIMAL));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "barrier"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PDLP));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -4.0, obj);
    jaos_model_free(m);
}

static void test_a_qp_whose_rows_leave_no_interior_still_solves(void)
{
    constexpr int64_t N = 6;
    constexpr int64_t M = 2;
    const double cost[N] = {-19, -19, -19, -19, -19, -19};
    const double quad[N] = {4, 2, 6, 4, 8, 12};
    const double cl[N] = {0, 0, 0, 0, 0, 0};
    const double cu[N] = {1, 0, 1, 0, 1, 1};
    const double rl[M] = {3.0, 1.0}, ru[M] = {3.0, 1.0};
    const int64_t as[N + 1] = {0, 1, 3, 5, 7, 9, 10};
    const int64_t ai[10] = {0, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    const double av[10] = {1, 1, 2, 1, 1, 1, 1, 1, 1, 1};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, M, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     10, as, ai, av));
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, j, quad[j]));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0, x[N];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -331.0 / 7.0, obj);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0 / 7.0, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0 / 7.0, x[4]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, x[5]);
    jaos_model_free(m);
}

/* The augmented system and the normal equations answer the same
   question: eliminating dz from the first turns it into the second.  So
   a model solved through either must give the same optimum.  The
   augmented form is the one that keeps its shape when Q stops being
   diagonal, which is what a full Q needs. */
static void solve_both_ways(const char *path, double tol)
{
    double obj[2] = {0.0, 0.0};
    int st[2] = {0, 0};
    for (int way = 0; way < 2; way++) {
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, path));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
        m->cfg.barrier_augmented = way == 1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        st[way] = (int)jaos_status_of(m);
        if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[way]));
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(st[0], st[1],
        "the augmented system and the normal equations must agree");
    if (st[0] == JAOS_SOLVE_OPTIMAL)
        TEST_ASSERT_DOUBLE_WITHIN(tol * (1.0 + fabs(obj[0])), obj[0], obj[1]);
}

static void test_the_augmented_system_agrees_with_the_normal_equations(void)
{
    solve_both_ways("tests/data/solve1.mps", 1e-6);
    solve_both_ways("tests/data/t1.mps", 1e-6);
    solve_both_ways("tests/data/unbounded.mps", 1e-6);
    solve_both_ways("tests/data/t4_int.mps", 1e-6);
}

static void test_the_augmented_system_takes_every_bound_kind(void)
{
    double obj[2] = {0.0, 0.0};
    double x[2][4];
    for (int way = 0; way < 2; way++) {
        jaos_model *m = every_bound_kind_lp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
        m->cfg.barrier_augmented = way == 1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[way]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, x[way], nullptr, nullptr, nullptr));
        jaos_model_free(m);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-6 * (1.0 + fabs(obj[0])), obj[0], obj[1]);
    for (int j = 0; j < 4; j++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-5, x[0][j], x[1][j]);
}

static void test_the_augmented_system_takes_a_fixed_column(void)
{

    for (int way = 0; way < 2; way++) {
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        const double inf = jaos_infinity();
        const double cost[3] = {1.0, 2.0, -1.0};
        const double cl[3] = {0.0, 3.0, 0.0};
        const double cu[3] = {inf, 3.0, 4.0};
        const double rl[2] = {2.0, -inf}, ru[2] = {inf, 9.0};
        const int64_t as[4] = {0, 2, 4, 6};
        const int64_t ai[6] = {0, 1, 0, 1, 0, 1};
        const double av[6] = {1.0, 1.0, 1.0, 2.0, 1.0, 1.0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                         6, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
        m->cfg.barrier_augmented = way == 1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double v[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, v, nullptr, nullptr, nullptr));
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0, v[1]);
        jaos_model_free(m);
    }
}

static void test_the_augmented_system_is_bit_identical_across_runs(void)
{
    double first[4];
    for (int run = 0; run < 2; run++) {
        jaos_model *m = every_bound_kind_lp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_algorithm(m, JAOS_ALGORITHM_BARRIER));
        m->cfg.barrier_augmented = true;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double x[4];
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, x, nullptr, nullptr, nullptr));
        if (run == 0)
            memcpy(first, x, sizeof x);
        else
            TEST_ASSERT_EQUAL_MEMORY(first, x, sizeof x);
        jaos_model_free(m);
    }
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
    RUN_TEST(test_the_barrier_hands_an_unbounded_lp_to_the_dual_for_its_ray);
    RUN_TEST(test_the_barrier_verdict_matches_the_dual_bit_for_bit);
    RUN_TEST(test_a_dense_column_leaves_the_normal_matrix_and_the_answer_holds);
    RUN_TEST(test_a_separable_qp_solves_by_the_barrier_and_the_checker_accepts);
    RUN_TEST(test_a_qp_whose_rows_leave_no_interior_still_solves);
    RUN_TEST(test_the_augmented_system_agrees_with_the_normal_equations);
    RUN_TEST(test_the_augmented_system_takes_every_bound_kind);
    RUN_TEST(test_the_augmented_system_takes_a_fixed_column);
    RUN_TEST(test_the_augmented_system_is_bit_identical_across_runs);
    return UNITY_END();
}
