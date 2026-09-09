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

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static jaos_model *every_bound_kind_lp(void)
{
    jaos_model *m = fresh();
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

static void test_concurrent_is_the_fifth_algorithm(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_DUAL, jaos_algorithm_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_CONCURRENT, jaos_algorithm_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_DUAL));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_DUAL, jaos_algorithm_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_algorithm(m, (jaos_algorithm)9));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "concurrent"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_option(m, "algorithm", "concurrent"));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_CONCURRENT, jaos_algorithm_of(m));
    char buf[64];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_get_option(m, "algorithm", buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("concurrent", buf);
    jaos_model_free(m);
}

static void test_concurrent_agrees_with_the_dual_on_every_bound_kind(void)
{
    jaos_model *a = every_bound_kind_lp();
    jaos_model *b = every_bound_kind_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(b, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(a));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_TRUE(oa == ob);

    const int64_t nc = jaos_num_col(a), nr = jaos_num_row(a);
    double *xa = calloc((size_t)nc, sizeof *xa), *xb = calloc((size_t)nc, sizeof *xb);
    double *ya = calloc((size_t)nr, sizeof *ya), *yb = calloc((size_t)nr, sizeof *yb);
    TEST_ASSERT_NOT_NULL(xa);
    TEST_ASSERT_NOT_NULL(xb);
    TEST_ASSERT_NOT_NULL(ya);
    TEST_ASSERT_NOT_NULL(yb);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(a, xa, nullptr, ya, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(b, xb, nullptr, yb, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(xa, xb, (size_t)nc * sizeof *xa);
    TEST_ASSERT_EQUAL_MEMORY(ya, yb, (size_t)nr * sizeof *ya);
    free(xa);
    free(xb);
    free(ya);
    free(yb);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_concurrent_is_bit_identical_across_runs(void)
{
    double first[4];
    int64_t work = 0;
    for (int run = 0; run < 2; run++) {
        jaos_model *m = every_bound_kind_lp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double x[4];
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(m, x, nullptr, nullptr, nullptr));
        if (run == 0) {
            memcpy(first, x, sizeof x);
            work = jaos_work_units(m);
        } else {
            TEST_ASSERT_EQUAL_MEMORY(first, x, sizeof x);
            TEST_ASSERT_EQUAL_INT64(work, jaos_work_units(m));
        }
        jaos_model_free(m);
    }
}

static void test_concurrent_publishes_a_basis_and_the_checker_takes_it(void)
{
    jaos_model *m = every_bound_kind_lp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_TRUE(m->sol_basis_ok);

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    free(cs);
    free(rs);

    double *x = calloc((size_t)nc, sizeof *x);
    double *y = calloc((size_t)nr, sizeof *y);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    free(x);
    free(y);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_concurrent_certifies_an_infeasible_model(void)
{
    jaos_model *m = fresh();
    const double cost[2] = {1.0, 1.0}, cl[2] = {0.0, 0.0}, cu[2] = {10.0, 10.0};
    const double rl[2] = {1.0, -jaos_infinity()}, ru[2] = {jaos_infinity(), -1.0};
    const int64_t as[3] = {0, 2, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {1.0, 1.0, -1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    double ray[2];
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, ray));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_certificate(m, ray, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    jaos_model_free(m);
}

static void test_concurrent_certifies_an_unbounded_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_mps(m, "tests/data/unbounded.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
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

static void test_a_work_limit_stops_concurrent_and_it_resumes(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const jaos_solve_status st = jaos_status_of(m);
    TEST_ASSERT_TRUE(st == JAOS_SOLVE_WORK_LIMIT || st == JAOS_SOLVE_OPTIMAL);
    TEST_ASSERT_TRUE(jaos_work_units(m) > 0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_concurrent_leaves_a_mip_to_the_tree(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t4_int.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double conc = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &conc));

    jaos_model *d = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(d, "tests/data/t4_int.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(d));
    double dual = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(d, &dual));
    TEST_ASSERT_TRUE(conc == dual);
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(d), jaos_work_units(m));
    jaos_model_free(m);
    jaos_model_free(d);
}

static void test_concurrent_refuses_a_quadratic_objective(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_lp(m, "tests/data/g_quad.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic term"));
    jaos_model_free(m);
}

static void test_three_threads_give_the_same_answer_and_the_same_work(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT64(1, jaos_threads_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_threads(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_threads(m, -3));
    TEST_ASSERT_EQUAL_INT64(1, jaos_threads_of(m));
    jaos_model_free(m);

    static const char *const files[] = {"tests/data/solve1.mps",
                                        "tests/data/t1.mps",
                                        "tests/data/unbounded.mps"};
    for (size_t f = 0; f < sizeof files / sizeof *files; f++) {
        double obj[2] = {0.0, 0.0};
        int64_t work[2] = {0, 0};
        int status[2] = {0, 0};
        double first[2][64];
        int64_t nc = 0;
        for (int run = 0; run < 2; run++) {
            m = fresh();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, files[f]));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_threads(m, run == 0 ? 1 : 3));
            TEST_ASSERT_EQUAL_INT64(run == 0 ? 1 : 3, jaos_threads_of(m));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            status[run] = (int)jaos_status_of(m);
            work[run] = jaos_work_units(m);
            nc = jaos_num_col(m);
            TEST_ASSERT_TRUE(nc <= 64);
            if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL) {
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[run]));
                TEST_ASSERT_EQUAL_INT(JAOS_OK,
                    jaos_solution(m, first[run], nullptr, nullptr, nullptr));
            } else {
                memset(first[run], 0, sizeof first[run]);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_EQUAL_INT(status[0], status[1]);
        TEST_ASSERT_EQUAL_INT64(work[0], work[1]);
        TEST_ASSERT_TRUE(obj[0] == obj[1]);
        TEST_ASSERT_EQUAL_MEMORY(first[0], first[1],
                                 (size_t)nc * sizeof(double));
    }
}

static void test_a_work_limit_with_threads_still_stops_and_resumes(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_threads(m, 3));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const jaos_solve_status st = jaos_status_of(m);
    TEST_ASSERT_TRUE(st == JAOS_SOLVE_WORK_LIMIT || st == JAOS_SOLVE_OPTIMAL);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

typedef struct {
    int64_t calls;
} counter;

static jaos_callback_action count_progress(const jaos_progress *p, void *user)
{
    (void)p;
    counter *c = user;
    c->calls++;
    return JAOS_CALLBACK_CONTINUE;
}

static void test_the_callers_progress_callback_still_runs_with_threads(void)
{
    counter c = {0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_algorithm(m, JAOS_ALGORITHM_CONCURRENT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_threads(m, 3));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_progress_callback(m, count_progress, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_TRUE(c.calls > 0);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_concurrent_is_the_fifth_algorithm);
    RUN_TEST(test_concurrent_agrees_with_the_dual_on_every_bound_kind);
    RUN_TEST(test_concurrent_is_bit_identical_across_runs);
    RUN_TEST(test_concurrent_publishes_a_basis_and_the_checker_takes_it);
    RUN_TEST(test_concurrent_certifies_an_infeasible_model);
    RUN_TEST(test_concurrent_certifies_an_unbounded_model);
    RUN_TEST(test_a_work_limit_stops_concurrent_and_it_resumes);
    RUN_TEST(test_concurrent_leaves_a_mip_to_the_tree);
    RUN_TEST(test_concurrent_refuses_a_quadratic_objective);
    RUN_TEST(test_three_threads_give_the_same_answer_and_the_same_work);
    RUN_TEST(test_a_work_limit_with_threads_still_stops_and_resumes);
    RUN_TEST(test_the_callers_progress_callback_still_runs_with_threads);
    return UNITY_END();
}
