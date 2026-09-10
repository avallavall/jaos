/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

constexpr double TOL = 1e-6;

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

/* minimise  1/2 (2 x0^2 + 2 x1^2 + 2 x0 x1) - 3 x0 - 3 x1
   subject to x0 + x1 <= 4, both in [0, 3].

   Q = [[2, 1], [1, 2]] is positive definite, so the objective is convex.
   Its unconstrained minimum solves Q x = (3, 3), which is x = (1, 1),
   inside every bound; the objective there is 1/2 (2 + 2 + 2) - 6 = -3. */
static jaos_model *paired_qp(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {-3.0, -3.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {3.0, 3.0};
    const double rl[1] = {-inf}, ru[1] = {4.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {2.0, 2.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    return m;
}

static void test_a_paired_q_reads_back_as_it_was_set(void)
{
    jaos_model *m = paired_qp();
    TEST_ASSERT_EQUAL_INT64(3, jaos_quadratic_nz(m));
    int64_t r[3], c[3];
    double v[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_quadratic(m, r, c, v));

    double diag[2] = {0.0, 0.0}, off = 0.0;
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_TRUE(r[k] >= c[k]);
        if (r[k] == c[k])
            diag[r[k]] = v[k];
        else
            off = v[k];
    }
    TEST_ASSERT_EQUAL_DOUBLE(2.0, diag[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, diag[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, off);

    double q = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    jaos_model_free(m);
}

static void test_a_paired_q_solves_to_the_point_it_should(void)
{
    jaos_model *m = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, obj);
    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 1.0, x[1]);
    jaos_model_free(m);
}

static void test_the_checker_judges_a_paired_q(void)
{
    jaos_model *m = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);

    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, rep.primal_objective);
    jaos_model_free(m);
}

/* The cross term is what a diagonal Q cannot say.  With the same
   diagonal and no pairing the optimum is elsewhere, so a solver that
   quietly dropped the off-diagonal entry would land there instead. */
static void test_the_cross_term_moves_the_answer(void)
{
    jaos_model *a = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    double oa = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));

    jaos_model *b = paired_qp();
    const int64_t qr[2] = {0, 1}, qc[2] = {0, 1};
    const double qv[2] = {2.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(b, 2, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT64(2, jaos_quadratic_nz(b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));

    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -4.5, ob);
    TEST_ASSERT_TRUE(fabs(oa - ob) > 1.0);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_a_non_convex_q_is_refused(void)
{

    jaos_model *m = paired_qp();
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {1.0, 1.0, 3.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not convex"));
    jaos_model_free(m);
}

static void test_a_semi_definite_q_is_taken(void)
{

    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {0.0, 0.0};
    const double cl[2] = {-inf, -inf}, cu[2] = {inf, inf};
    const double rl[1] = {2.0}, ru[1] = {2.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {1.0, 1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));

    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 0.0, obj);
    jaos_model_free(m);
}

static void test_a_paired_q_survives_a_copy(void)
{
    jaos_model *a = paired_qp();
    jaos_model *b = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(a, &b));
    TEST_ASSERT_EQUAL_INT64(jaos_quadratic_nz(a), jaos_quadratic_nz(b));
    TEST_ASSERT_EQUAL_INT64(a->q_nz, b->q_nz);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_TRUE(oa == ob);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_deleting_a_column_carries_the_pairs_with_it(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[3] = {-1.0, -1.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {5.0, 5.0, 5.0};
    const double rl[1] = {-inf}, ru[1] = {9.0};
    const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
    const double av[3] = {1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));

    const int64_t qr[5] = {0, 1, 2, 1, 2};
    const int64_t qc[5] = {0, 1, 2, 0, 1};
    const double qv[5] = {2.0, 2.0, 2.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 5, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT64(2, m->q_nz);

    const int64_t drop[1] = {1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, drop));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));

    TEST_ASSERT_EQUAL_INT64(0, m->q_nz);
    TEST_ASSERT_EQUAL_INT64(2, jaos_quadratic_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_set_quadratic_refuses_what_it_cannot_take(void)
{
    jaos_model *m = paired_qp();
    const int64_t bad_r[1] = {5}, bad_c[1] = {0};
    const double bad_v[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, 1, bad_r, bad_c, bad_v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "names columns"));

    const int64_t inf_r[1] = {0}, inf_c[1] = {0};
    const double inf_v[1] = {INFINITY};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, 1, inf_r, inf_c, inf_v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not finite"));

    const int64_t twice_r[2] = {1, 0}, twice_c[2] = {0, 1};
    const double twice_v[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, 2, twice_r, twice_c,
                                             twice_v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "given twice"));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, -1, nullptr, nullptr,
                                             nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(nullptr, 0, nullptr, nullptr,
                                             nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_quadratic(m, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_quadratic_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_paired_q_is_bit_identical_across_runs(void)
{
    double first[2];
    double obj = 0.0;
    for (int run = 0; run < 2; run++) {
        jaos_model *m = paired_qp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double x[2], o = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &o));
        if (run == 0) {
            memcpy(first, x, sizeof x);
            obj = o;
        } else {
            TEST_ASSERT_EQUAL_MEMORY(first, x, sizeof x);
            TEST_ASSERT_TRUE(obj == o);
        }
        jaos_model_free(m);
    }
}

static void test_the_other_algorithms_still_refuse_a_quadratic(void)
{
    static const jaos_algorithm no[] = {JAOS_ALGORITHM_PRIMAL,
                                        JAOS_ALGORITHM_PDLP,
                                        JAOS_ALGORITHM_CONCURRENT};
    for (size_t k = 0; k < sizeof no / sizeof *no; k++) {
        jaos_model *m = paired_qp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, no[k]));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic term"));
        jaos_model_free(m);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_paired_q_reads_back_as_it_was_set);
    RUN_TEST(test_a_paired_q_solves_to_the_point_it_should);
    RUN_TEST(test_the_checker_judges_a_paired_q);
    RUN_TEST(test_the_cross_term_moves_the_answer);
    RUN_TEST(test_a_non_convex_q_is_refused);
    RUN_TEST(test_a_semi_definite_q_is_taken);
    RUN_TEST(test_a_paired_q_survives_a_copy);
    RUN_TEST(test_deleting_a_column_carries_the_pairs_with_it);
    RUN_TEST(test_set_quadratic_refuses_what_it_cannot_take);
    RUN_TEST(test_a_paired_q_is_bit_identical_across_runs);
    RUN_TEST(test_the_other_algorithms_still_refuse_a_quadratic);
    return UNITY_END();
}
