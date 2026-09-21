/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

static jaos_model *load(int64_t nc, int64_t nr, jaos_obj_sense sense,
                        const double *c, const double *rl, const double *ru,
                        const int64_t *s, const int64_t *ix, const double *v)
{
    double cl[8], cu[8];
    for (int64_t j = 0; j < nc; j++) {
        cl[j] = 0.0;
        cu[j] = INFINITY;
    }
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, nc, nr, sense, 0.0, c, cl, cu, rl, ru, s[nc], s,
                     ix, v));
    return m;
}

static jaos_model *make_doubleton(jaos_obj_sense sense, double sign,
                                  bool infeasible)
{
    const double c[] = {2.0 * sign, 1.0 * sign, 3.0 * sign, 1.0 * sign};
    if (!infeasible) {
        const double rl[] = {1.0, 3.0, -INFINITY, 2.0};
        const double ru[] = {1.0, INFINITY, 10.0, INFINITY};
        const int64_t s[] = {0, 3, 5, 7, 9};
        const int64_t ix[] = {0, 1, 2, 0, 3, 1, 3, 2, 3};
        const double v[] = {1, 1, 1, -1, 1, 1, 1, 1, 1};
        return load(4, 4, sense, c, rl, ru, s, ix, v);
    }
    const double rl[] = {1.0, 3.0, -INFINITY, 2.0, -INFINITY};
    const double ru[] = {1.0, INFINITY, 10.0, INFINITY, 1.0};
    const int64_t s[] = {0, 3, 6, 9, 11};
    const int64_t ix[] = {0, 1, 2, 0, 3, 4, 1, 3, 4, 2, 3};
    const double v[] = {1, 1, 1, -1, 1, 1, 1, 1, 1, 1, 1};
    return load(4, 5, sense, c, rl, ru, s, ix, v);
}

static void expect_checked_optimum(jaos_model *m, int64_t nc, int64_t nr,
                                   double objective, int64_t aggregated)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, objective, obj);

    jaos_presolve_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_presolve_result(m, &pr));
    TEST_ASSERT_EQUAL_INT64(aggregated, pr.aggregated_col);

    double x[8], y[8];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[8], rs[8];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t nbasic = 0;
    for (int64_t k = 0; k < nc; k++)
        nbasic += cs[k] == JAOS_BASIS_BASIC;
    for (int64_t k = 0; k < nr; k++)
        nbasic += rs[k] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(nr, nbasic);
}

static void test_an_implied_free_column_leaves_by_its_doubleton_row(void)
{
    jaos_model *m = make_doubleton(JAOS_MINIMIZE, 1.0, false);
    expect_checked_optimum(m, 4, 4, 8.0, 1);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 8.0, obj);
    jaos_model_free(m);
}

static void test_the_substitution_holds_for_a_maximization(void)
{
    jaos_model *m = make_doubleton(JAOS_MAXIMIZE, -1.0, false);
    expect_checked_optimum(m, 4, 4, -8.0, 1);
    jaos_model_free(m);
}

static void test_a_substituted_column_is_resolved_after_its_survivor(void)
{
    const double c[] = {1.0, 1.0, 1.0, 1.0};
    const double rl[] = {1.0, 2.0, 5.0, 1.0, -INFINITY};
    const double ru[] = {1.0, 2.0, INFINITY, INFINITY, 20.0};
    const int64_t s[] = {0, 2, 5, 8, 11};
    const int64_t ix[] = {0, 2, 0, 1, 3, 1, 3, 4, 2, 3, 4};
    const double v[] = {1, 1, -1, 1, 1, -1, 1, 1, 1, 1, 1};
    jaos_model *m = load(4, 5, JAOS_MINIMIZE, c, rl, ru, s, ix, v);
    expect_checked_optimum(m, 4, 5, 7.0, 2);

    double x[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, x[1]);
    jaos_model_free(m);
}

static void test_a_certificate_found_after_substitution_proves_the_model(void)
{
    jaos_model *m = make_doubleton(JAOS_MINIMIZE, 1.0, true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    jaos_presolve_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_presolve_result(m, &pr));
    TEST_ASSERT_EQUAL_INT64(1, pr.aggregated_col);

    double ray[5];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, ray));
    jaos_certificate_report cr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_certificate(m, ray, TOL, &cr));
    TEST_ASSERT_TRUE(cr.certified);
    jaos_model_free(m);
}

static void test_a_ray_found_after_substitution_moves_the_substituted_column(void)
{
    const double c[] = {0.0, -1.0, 1.0};
    const double rl[] = {1.0, 3.0, 2.0};
    const double ru[] = {1.0, INFINITY, INFINITY};
    const int64_t s[] = {0, 2, 4, 6};
    const int64_t ix[] = {0, 1, 0, 2, 1, 2};
    const double v[] = {1, 1, -1, 1, 1, 1};
    jaos_model *m = load(3, 3, JAOS_MINIMIZE, c, rl, ru, s, ix, v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    jaos_presolve_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_presolve_result(m, &pr));
    TEST_ASSERT_EQUAL_INT64(1, pr.aggregated_col);

    double ray[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, ray));
    TEST_ASSERT_TRUE(ray[1] > 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, ray[1], ray[0]);
    jaos_ray_report rr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, ray, TOL, &rr));
    TEST_ASSERT_TRUE(rr.certified);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_an_implied_free_column_leaves_by_its_doubleton_row);
    RUN_TEST(test_the_substitution_holds_for_a_maximization);
    RUN_TEST(test_a_substituted_column_is_resolved_after_its_survivor);
    RUN_TEST(test_a_certificate_found_after_substitution_proves_the_model);
    RUN_TEST(test_a_ray_found_after_substitution_moves_the_substituted_column);
    return UNITY_END();
}
