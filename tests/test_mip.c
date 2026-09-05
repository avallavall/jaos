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
#include <stdio.h>
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
    /* The root was fractional and the cuts closed it: one node. The
     * tree without them is the next test's. */
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
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
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
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
    /* Known before a relaxation is solved: the rounded bounds cross
     * (D292). */
    TEST_ASSERT_EQUAL_INT64(0, rep.nodes);
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

/* The two switches of D289, the cuts turned off and the dive turned on, on
 * the knapsack and on the model whose relaxation is 1.5: the answer is
 * the same either way, which is what a switch has to show. */
static void test_without_cuts_or_dive_the_tree_branches_to_the_same_answer(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_TRUE(rep.nodes >= 2);       /* the root was fractional */
    const int64_t dived = rep.nodes;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 2);
    (void)dived;
    /* A negative count restores the default and the cuts come back. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_cut_rounds_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    jaos_model_free(m);

    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 3);
    jaos_model_free(m);
}

/* A mixed model, enumerated by hand: max 3x + 2y + w with 2x + y + w <=
 * 5.5, x + 2y + w <= 4.5, x and y integer in [0, 3], w continuous in
 * [0, 1]. Of the integer pairs inside both rows, (2, 1) leaves w at most
 * 0.5 and is worth 8.5; (2, 0) allows w = 1 and is worth 7; (1, 1) 6;
 * (0, 2) 4.5. The relaxation with w at 0 sits at x = 13/6, y = 7/6,
 * worth 8.83, so the root is fractional and the cut has a continuous
 * column and an upper-bounded one to get right. */
static void test_a_cut_over_continuous_columns_keeps_the_integer_optimum(void)
{
    const double cost[3] = { 3.0, 2.0, 1.0 }, cl[3] = { 0, 0, 0 };
    const double cu[3] = { 3.0, 3.0, 1.0 };
    const double rl[2] = { -INFINITY, -INFINITY }, ru[2] = { 5.5, 4.5 };
    const int64_t as[4] = { 0, 2, 4, 6 }, ai[6] = { 0, 1, 0, 1, 0, 1 };
    const double av[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 2, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         6, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
        if (pass == 1)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 8.5, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, x[2]);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0)
            TEST_ASSERT_TRUE(rep.cuts >= 1);
        else
            TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
        jaos_check_report ck;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_check_solution(m, x, nullptr, 1e-7, &ck));
        TEST_ASSERT_TRUE(ck.primal_feasible);
        jaos_model_free(m);
    }
}

/* The rounding heuristic (D290) on a model where it must fire: max x + y
 * with x + y <= 3.6, x <= 2.2 and y <= 1.4 as rows -- a bound would be
 * rounded inward before the root (D292) -- both integer and non-negative.
 * The relaxation sits at (2.2, 1.4), which rounds to (2, 1), inside every
 * row and worth 3, and 3 is the optimum: no integer x exceeds 2 nor y 1. With the heuristic off the tree finds the same point and
 * reports no heuristic point, which is what the switch has to show. The
 * cuts are off so the root stays fractional either way. */
static void test_the_rounding_heuristic_takes_the_relaxations_neighbour(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[3] = { -INFINITY, -INFINITY, -INFINITY };
    const double ru[3] = { 3.6, 2.2, 1.4 };
    const int64_t as[3] = { 0, 2, 4 }, ai[4] = { 0, 1, 0, 2 };
    const double av[4] = { 1.0, 1.0, 1.0, 1.0 };
    for (int on = 1; on >= 0; on--) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 3, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         4, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, on != 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[2], ra[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, ra, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
        /* The activities a heuristic point publishes are its own. */
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, ra[0]);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (on) {
            TEST_ASSERT_TRUE(rep.heuristic_points >= 1);
            TEST_ASSERT_EQUAL_INT64(1, rep.first_incumbent_node);
        } else {
            TEST_ASSERT_EQUAL_INT64(0, rep.heuristic_points);
            TEST_ASSERT_TRUE(rep.first_incumbent_node >= 2);
        }
        jaos_check_report ck;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_check_solution(m, x, nullptr, 1e-7, &ck));
        TEST_ASSERT_TRUE(ck.primal_feasible);
        jaos_model_free(m);
    }
}

/* A rounding that lands outside a row is never taken: max x + y with
 * 2x + 2y <= 3 rounds (1.5, 0) to (2, 0), which breaks the row, so the
 * count stays 0 and the tree finds 1 as before. */
static void test_an_infeasible_rounding_is_not_taken(void)
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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.heuristic_points);
    jaos_model_free(m);
}

/* The tree's log lines (D290): one when it starts, one for the root, one
 * when it ends, at JAOS_LOG_SUMMARY; and the same bits either way. */
static char g_log[4096];
static void capture_log(void *user, jaos_log_level level, const char *line)
{
    (void)level; (void)user;
    const size_t have = strlen(g_log);
    snprintf(g_log + have, sizeof g_log - have, "%s\n", line);
}

static void test_the_tree_logs_its_start_root_and_end(void)
{
    jaos_model *m = knapsack();
    double x1[3], x2[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x1, nullptr, nullptr, nullptr));
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x2, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    TEST_ASSERT_NOT_NULL(strstr(g_log, "branch and bound: 3 integer columns of 3"));
    TEST_ASSERT_NOT_NULL(strstr(g_log, "root: relaxation"));
    TEST_ASSERT_NOT_NULL(strstr(g_log, "branch and bound: optimal after"));
    jaos_model_free(m);
}

/* The node limit and the incumbent callback (D291), on the model whose
 * root rounds to its optimum. A limit of one node stops after the root,
 * as NODE_LIMIT, with the rounding's point held for jaos_mip_incumbent
 * and refused by jaos_solution; the callback saw that point once, at
 * node 1, marked as the heuristic's; and a callback that says STOP ends
 * the search as INTERRUPTED with the incumbent kept. */
typedef struct {
    int calls;
    jaos_incumbent last;
    double x[2];
    jaos_callback_action answer;
} seen_t;

static jaos_callback_action see_incumbent(const jaos_incumbent *inc, void *user)
{
    seen_t *s = user;
    s->calls++;
    s->last = *inc;
    s->x[0] = inc->col_value[0];
    s->x[1] = inc->col_value[1];
    return s->answer;
}

static jaos_model *neighbour_model(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[3] = { -INFINITY, -INFINITY, -INFINITY };
    const double ru[3] = { 3.6, 2.2, 1.4 };
    const int64_t as[3] = { 0, 2, 4 }, ai[4] = { 0, 1, 0, 2 };
    const double av[4] = { 1.0, 1.0, 1.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    return m;
}

static void test_a_node_limit_stops_with_the_incumbent_the_callback_saw(void)
{
    jaos_model *m = neighbour_model();
    seen_t seen = { .answer = JAOS_CALLBACK_CONTINUE };
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_node_limit(m, -1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_incumbent_callback(m, see_incumbent, &seen));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NODE_LIMIT, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("node limit reached",
                             jaos_solve_status_str(JAOS_SOLVE_NODE_LIMIT));
    double x[2], obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_incumbent(m, x, &obj));
    TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
    TEST_ASSERT_EQUAL_INT(1, seen.calls);
    TEST_ASSERT_EQUAL_INT64(1, seen.last.node);
    TEST_ASSERT_TRUE(seen.last.by_rounding);
    TEST_ASSERT_EQUAL_INT64(2, seen.last.num_col);
    TEST_ASSERT_TRUE(seen.x[0] == 2.0 && seen.x[1] == 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, seen.last.objective);
    TEST_ASSERT_TRUE(seen.last.bound >= 3.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    TEST_ASSERT_TRUE(rep.has_incumbent);

    /* No limit, and a callback that stops on the first incumbent. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(m, 0));
    seen.answer = JAOS_CALLBACK_STOP;
    seen.calls = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, seen.calls);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.has_incumbent);

    /* And the callback removed: the search runs to its optimum, the same
     * bits as with a callback that always continued. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_incumbent_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
    jaos_model_free(m);
}

/* The branching rule (D292) changes the tree and never the answer: both
 * rules on the knapsack and on the neighbour model, cuts off so the
 * root branches, reach the same optimum and the same point; a value
 * outside the enum is refused. */
static void test_both_branching_rules_reach_the_same_optimum(void)
{
    const jaos_branching rules[2] = { JAOS_BRANCH_MOST_FRACTIONAL,
                                      JAOS_BRANCH_PSEUDOCOST };
    for (int r = 0; r < 2; r++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_branching(m, rules[r]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_model_free(m);

        m = neighbour_model();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_branching(m, rules[r]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
        jaos_model_free(m);
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_branching(m, (jaos_branching)7));
    jaos_model_free(m);
}

/* An integer column bounded in [1.2, 2.8] holds the integer 2 only, and
 * the root's rounded bounds say so: the relaxation is solved at x = 2
 * without a branch. */
static void test_a_fractional_bound_on_an_integer_column_is_rounded_inward(void)
{
    const double cost[1] = { -1.0 }, cl[1] = { 1.2 }, cu[1] = { 2.8 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] == 2.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    /* And the model's own bounds are untouched: the copy was rounded. */
    double lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(1.2, lo);
    TEST_ASSERT_EQUAL_DOUBLE(2.8, hi);
    jaos_model_free(m);
}

/* Strong branching until reliable (D293): on the knapsack with the cuts
 * off, reliability 0 solves one relaxation per node and reliability 8
 * solves more, since the root's fractional column has its two children
 * probed and the node is put back with one more solve; the answer is the
 * same either way, the probes are billed, and a negative value restores
 * the default. */
static void test_strong_branching_probes_are_counted_and_change_no_answer(void)
{
    int64_t solves0 = 0, nodes0 = 0, work0 = 0;
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, pass == 0 ? 0 : 8));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            TEST_ASSERT_EQUAL_INT64(rep.nodes, rep.lp_solves);
            solves0 = rep.lp_solves;
            nodes0 = rep.nodes;
            work0 = jaos_work_units(m);
        } else {
            TEST_ASSERT_TRUE(rep.lp_solves > rep.nodes);
            TEST_ASSERT_TRUE(rep.lp_solves > solves0);
            TEST_ASSERT_TRUE(rep.nodes <= nodes0);
            TEST_ASSERT_TRUE(jaos_work_units(m) > work0 || rep.nodes < nodes0);
        }
        /* A negative value restores the default, and the tree is the
         * default's again. */
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_reliability_set);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        jaos_model_free(m);
    }
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
    RUN_TEST(test_without_cuts_or_dive_the_tree_branches_to_the_same_answer);
    RUN_TEST(test_a_cut_over_continuous_columns_keeps_the_integer_optimum);
    RUN_TEST(test_the_rounding_heuristic_takes_the_relaxations_neighbour);
    RUN_TEST(test_an_infeasible_rounding_is_not_taken);
    RUN_TEST(test_the_tree_logs_its_start_root_and_end);
    RUN_TEST(test_a_node_limit_stops_with_the_incumbent_the_callback_saw);
    RUN_TEST(test_both_branching_rules_reach_the_same_optimum);
    RUN_TEST(test_a_fractional_bound_on_an_integer_column_is_rounded_inward);
    RUN_TEST(test_strong_branching_probes_are_counted_and_change_no_answer);
    return UNITY_END();
}
