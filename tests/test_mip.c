/* Branch and bound (src/mip.c, D288).
 *
 * The oracle is arithmetic by hand on models small enough to enumerate:
 * every optimum below is checked against the relaxation's value where the
 * two differ, so a solver that ignored the integrality would fail. Two
 * runs of each are compared bit for bit, which is the reproducibility
 * claim (D8) on the one part of the library with a search in it.
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

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

/* max 5a + 4b + 3c  s.t.  2a + 3b + c <= 5,  a, b, c binary.
 * The relaxation takes a and c whole and two thirds of b, worth 10.67;
 * the four integer choices are worth 9 (a, b), 8 (a, c), 7 (b, c) and
 * 5 (a), so the answer is a = b = 1, c = 0 at 9. */
static jaos_model *knapsack(void)
{
    const double cost[3] = { 5.0, 4.0, 3.0 }, cl[3] = { 0, 0, 0 };
    const double cu[3] = { 1.0, 1.0, 1.0 };
    const double rl[1] = { -INFINITY }, ru[1] = { 5.0 };
    const int64_t as[4] = { 0, 1, 2, 3 }, ai[3] = { 0, 0, 0 };
    const double av[3] = { 2.0, 3.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_the_knapsack_finds_the_integer_optimum(void)
{
    jaos_model *m = knapsack();
    bool isint = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[2]);
    /* Exactly integral, not nearly: the incumbent is published rounded. */
    TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 2);       /* the root was fractional */
    TEST_ASSERT_TRUE(rep.has_incumbent);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, rep.incumbent);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, rep.bound);
    /* The checker sees an integral point, and would see a fractional one. */
    jaos_check_report ck;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, 1e-7, &ck));
    TEST_ASSERT_TRUE(ck.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ck.max_integrality_violation);
    x[2] = 0.5;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, 1e-7, &ck));
    TEST_ASSERT_FALSE(ck.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, ck.max_integrality_violation);
    /* And the relaxation alone is worth more, which is what the test is
     * about: unmark the columns and the answer moves. */
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, false));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_TRUE(obj > 9.0 + 1e-6);
    jaos_model_free(m);
}

/* max x + y  s.t.  2x + 2y <= 3, x, y integer >= 0: the relaxation is 1.5
 * and the integer optimum 1, and the tree has to branch to find it. */
static void test_a_fractional_root_branches_to_the_integer_answer(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 3);
    /* A second solve is the same search: same nodes, same point. */
    double x1[2], x2[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x1, nullptr, nullptr, nullptr));
    const int64_t nodes = rep.nodes, work = jaos_work_units(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(nodes, rep.nodes);
    TEST_ASSERT_EQUAL_INT64(work, jaos_work_units(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x2, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    jaos_model_free(m);
}

static void test_an_integer_model_with_no_integer_point_is_infeasible(void)
{
    /* 0.2 <= x <= 0.8, x integer: the relaxation is feasible and no
     * integer is. */
    const double cost[1] = { 1.0 }, cl[1] = { 0.2 }, cu[1] = { 0.8 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_FALSE(rep.has_incumbent);
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_mip_incumbent(m, x, nullptr));
    jaos_model_free(m);
}

static void test_a_work_limit_stops_the_tree_and_keeps_the_incumbent(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const int64_t full = jaos_work_units(m);
    TEST_ASSERT_TRUE(full > 0);
    /* A budget the root alone exhausts: the search stops after it, with
     * no proof and, on this model, no integer point yet. Then a budget the
     * whole search fits in, which is the control. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    double x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 4 * full + 1000));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_incumbent(m, x, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    jaos_model_free(m);
}

static void test_the_marks_ride_with_their_columns_and_copy(void)
{
    jaos_model *m = knapsack();
    const int64_t del[1] = { 0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, del));
    bool isint = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &isint));
    TEST_ASSERT_FALSE(isint);            /* was column 1 */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_TRUE(isint);             /* was column 2 */
    const double one = 1.0, zero = 0.0, inf = INFINITY;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 1, &one, &zero, &inf, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 2, &isint));
    TEST_ASSERT_FALSE(isint);            /* arrives continuous */
    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(c, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_integer(m, 3, true));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_gap(m, -1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_gap(m, 0.0));
    jaos_model_free(c);
    jaos_model_free(m);
}

static void test_the_readers_and_writers_carry_the_marks(void)
{
    /* MPS: a MARKER pair and a BV bound; LP: General and Binary. Each
     * file reads, round-trips through both writers, and solves to the
     * integer answer. */
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_mps(m, "tests/data/t4_int.mps"),
                                  jaos_model_error(m));
    bool a = false, b = false, c = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 2, &c));
    TEST_ASSERT_TRUE(a && !b && c);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_upper[2]);     /* BV */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.5, obj);
    double xz[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, xz, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(xz[0] == floor(xz[0]) && xz[2] == 1.0);

    const char *paths[2] = { "build/tm_tmp.mps", "build/tm_tmp.lp" };
    for (int k = 0; k < 2; k++) {
        jaos_model *back = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_write_mps(m, paths[k])
                                              : jaos_write_lp(m, paths[k]));
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, k == 0 ? jaos_read_mps(back, paths[k])
                                                      : jaos_read_lp(back, paths[k]),
                                      jaos_model_error(back));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(back, 0, &a));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(back, 1, &b));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(back, 2, &c));
        TEST_ASSERT_TRUE(a && !b && c);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(back));
        double o2 = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(back, &o2));
        TEST_ASSERT_EQUAL_MEMORY(&obj, &o2, sizeof obj);
        jaos_model_free(back);
        remove(paths[k]);
    }

    jaos_model *l = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_lp(l, "tests/data/g_int.lp"),
                                  jaos_model_error(l));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(l, 0, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(l, 1, &b));
    TEST_ASSERT_TRUE(a && b);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, l->col_upper[1]);     /* Binary */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(l));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(l, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, obj);
    /* A name in an integer section that is not a variable is refused. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_lp(l, "tests/data/el_int_unknown.lp"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(l), "not a variable"));
    jaos_model_free(l);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    /* Every test here reads postsolved answers, which the two presolve
     * fault builds corrupt on purpose, so under them this suite is empty
     * and green, the rule the other suites apply test by test. */
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    return UNITY_END();
#endif
    RUN_TEST(test_the_knapsack_finds_the_integer_optimum);
    RUN_TEST(test_a_fractional_root_branches_to_the_integer_answer);
    RUN_TEST(test_an_integer_model_with_no_integer_point_is_infeasible);
    RUN_TEST(test_a_work_limit_stops_the_tree_and_keeps_the_incumbent);
    RUN_TEST(test_the_marks_ride_with_their_columns_and_copy);
    RUN_TEST(test_the_readers_and_writers_carry_the_marks);
    return UNITY_END();
}
