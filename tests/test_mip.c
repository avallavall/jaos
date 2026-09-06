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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
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
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
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
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, on != 0));
        /* D313's dive would find the same point; this test is about the
         * rounding, so the dive is off. */
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
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
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
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
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
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

/* A work cap on each probe (D294): with reliability 8 on the knapsack the
 * probes run, capped or not, and the answer is the same; a cap of 0 is no
 * cap, a negative value restores the default, and NaN and infinity are
 * refused. The tree under a cap every probe reaches is not asserted: on a
 * model this small a child can finish before the first check of the
 * limit, so whether it teaches is the solver's business. */
static void test_a_capped_probe_reaches_the_same_optimum(void)
{
    const double caps[3] = { 0.5, 0.0, 4.0 };
    for (int c = 0; c < 3; c++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, 8));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_cap(m, caps[c]));
        TEST_ASSERT_TRUE(m->cfg.mip_probe_cap_set);
        TEST_ASSERT_EQUAL_DOUBLE(caps[c], m->cfg.mip_probe_cap);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.lp_solves > rep.nodes);
        jaos_model_free(m);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_cap(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_probe_cap_set);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_probe_cap(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_probe_cap(m, INFINITY));
    TEST_ASSERT_FALSE(m->cfg.mip_probe_cap_set);
    jaos_model_free(m);
}

/* The dive's child rule (D295): every rule dives the knapsack, cuts off,
 * to the same optimum, and a value outside the enum is refused. */
static void test_every_dive_child_rule_reaches_the_same_optimum(void)
{
    const jaos_dive_child rules[4] = { JAOS_DIVE_NEARER, JAOS_DIVE_UP,
                                       JAOS_DIVE_DOWN, JAOS_DIVE_PSEUDOCOST };
    for (int r = 0; r < 4; r++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_child(m, rules[r]));
        TEST_ASSERT_EQUAL_INT((int)rules[r], m->cfg.mip_dive_child);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.nodes >= 2);
        if (r == 3) {
            TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                                  jaos_set_mip_dive_child(m, (jaos_dive_child)7));
            TEST_ASSERT_EQUAL_INT((int)JAOS_DIVE_PSEUDOCOST, m->cfg.mip_dive_child);
        }
        jaos_model_free(m);
    }
}

/* Cuts below the root (D296). max 10a + 13b + 7c + 9d + 5e over
 * 3a + 5b + 2c + 4d + 2e <= 8, binaries: the relaxation takes c, a and e
 * whole and a fifth of b, worth 24.6; the eleven feasible sets are worth
 * at most 23 (a, b), so the tree has at least one branch and the answer
 * is a = b = 1. Every depth reaches it, with the root's cuts off so the
 * node cuts are what runs; two cold searches at a depth agree bit for
 * bit; and a negative depth restores the default. */
static void test_cuts_below_the_root_reach_the_same_optimum(void)
{
    const double cost[5] = { 10.0, 13.0, 7.0, 9.0, 5.0 };
    const double cl[5] = { 0, 0, 0, 0, 0 }, cu[5] = { 1, 1, 1, 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 8.0 };
    const int64_t as[6] = { 0, 1, 2, 3, 4, 5 }, ai[5] = { 0, 0, 0, 0, 0 };
    const double av[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 };
    const int64_t depths[3] = { 0, 1, 100 };
    for (int d = 0; d < 3; d++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0, cuts1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = fresh();
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_load_lp(m, 5, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                             5, as, ai, av));
            for (int64_t j = 0; j < 5; j++)
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, depths[d]));
            TEST_ASSERT_TRUE(m->cfg.mip_cut_depth_set);
            TEST_ASSERT_EQUAL_INT64(depths[d], m->cfg.mip_cut_depth);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            TEST_ASSERT_TRUE(rep.nodes >= 2);
            if (depths[d] == 0)
                TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
            if (pass == 0) {
                nodes1 = rep.nodes;
                cuts1 = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0 && x1[2] == 0.0 &&
                         x1[3] == 0.0 && x1[4] == 0.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_cut_depth_set);
    jaos_model_free(m);
}

/* The five-item knapsack of the cut-depth test, loaded fresh. */
static jaos_model *knapsack5(void)
{
    const double cost[5] = { 10.0, 13.0, 7.0, 9.0, 5.0 };
    const double cl[5] = { 0, 0, 0, 0, 0 }, cu[5] = { 1, 1, 1, 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 8.0 };
    const int64_t as[6] = { 0, 1, 2, 3, 4, 5 }, ai[5] = { 0, 0, 0, 0, 0 };
    const double av[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     5, as, ai, av));
    for (int64_t j = 0; j < 5; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

/* A slack cut leaves the relaxation (D297): with the cuts to depth 100 and
 * the root's off, dropping on and off both reach 23 with the same point,
 * and each is bit-reproducible across two cold searches. */
static void test_dropping_slack_cuts_keeps_the_optimum(void)
{
    for (int drop = 0; drop < 2; drop++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_drop(m, drop == 1));
            TEST_ASSERT_EQUAL_INT(drop == 0, m->cfg.mip_no_cut_drop);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
}

/* Strong branching down to a depth (D298): at the root only, the root's
 * probes still run and the answer is the same; a negative depth restores
 * every depth. */
static void test_probing_at_the_root_only_reaches_the_same_optimum(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, 8));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_depth(m, 0));
    TEST_ASSERT_TRUE(m->cfg.mip_probe_depth_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.lp_solves > rep.nodes);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_depth(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_probe_depth_set);
    jaos_model_free(m);
}

/* The solution pool (D299): with room for three, the best point is the
 * incumbent, the points are distinct and feasible and in objective order,
 * and a size of one holds the incumbent alone; 0 is refused, a negative
 * size restores 1, and an index past the count is refused. */
static void test_the_solution_pool_holds_the_best_points_best_first(void)
{
    const double w[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 }, c[5] = { 10, 13, 7, 9, 5 };
    for (int size = 3; size >= 1; size -= 2) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pool_size(m, size));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        int64_t held = -1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_pool_count(m, &held));
        TEST_ASSERT_TRUE(held >= 1 && held <= size);
        double best[5], inc[5], incobj = 0.0, prev = INFINITY;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_incumbent(m, inc, &incobj));
        for (int64_t k = 0; k < held; k++) {
            double x[5], obj = 0.0, weight = 0.0, value = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_pool_solution(m, k, x, &obj));
            for (int j = 0; j < 5; j++) {
                TEST_ASSERT_TRUE(x[j] == 0.0 || x[j] == 1.0);
                weight += w[j] * x[j];
                value += c[j] * x[j];
            }
            TEST_ASSERT_TRUE(weight <= 8.0);
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, value, obj);
            TEST_ASSERT_TRUE(obj <= prev);    /* maximize: best first */
            prev = obj;
            if (k == 0) {
                memcpy(best, x, sizeof best);
                TEST_ASSERT_EQUAL_MEMORY(inc, x, sizeof inc);
                TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            } else {
                TEST_ASSERT_TRUE(memcmp(best, x, sizeof best) != 0);
            }
        }
        if (size == 1)
            TEST_ASSERT_EQUAL_INT64(1, held);
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                              jaos_mip_pool_solution(m, held, nullptr, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_pool_size(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pool_size(m, -1));
        TEST_ASSERT_EQUAL_INT64(0, m->cfg.mip_pool_size);
        jaos_model_free(m);
    }
}

/* Cover cuts (D300). On the knapsack with the Gomory cuts off, the root's
 * point a = c = 1, b = 2/3 gives the greedy cover {a, c, b} (weights 2, 1,
 * 3 pass 5 only with all three), so a + b + c <= 2 is added, and the
 * relaxation over it is a = b = 1 at 9: one node, no branch. With both
 * families off the tree branches to the same answer; a negative count
 * restores the default. A row with a continuous column gets no cover. */
static void test_a_cover_cut_closes_the_knapsack_at_the_root(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
    TEST_ASSERT_TRUE(m->cfg.mip_cover_rounds_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 2);
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_cover_rounds_set);
    jaos_model_free(m);
    /* A continuous column in the row: no cover, the tree branches. */
    m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 2, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    jaos_model_free(m);
}

/* A cap on a node's cuts (D301): with cuts to depth 100 on the five-item
 * knapsack, a cap of one per node reaches 23 with the same point, adds no
 * more cuts than no cap, and is bit-reproducible; a negative cap restores
 * the default. */
static void test_a_cap_on_a_nodes_cuts_keeps_the_optimum(void)
{
    int64_t cuts_uncapped = -1;
    for (int cap = 0; cap < 2; cap++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0, cuts1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, cap));
            TEST_ASSERT_TRUE(m->cfg.mip_node_cut_cap_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes1 = rep.nodes;
                cuts1 = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
        if (cap == 0)
            cuts_uncapped = cuts1;
        else
            TEST_ASSERT_TRUE(cuts1 <= cuts_uncapped);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_node_cut_cap_set);
    jaos_model_free(m);
}

/* The root's rounds end on a stall (D304): on the five-item knapsack with
 * the covers off, five Gomory rounds close the root at 23 with three cuts;
 * a stall of 1 -- a round must move the bound by the bound itself, which
 * none does -- ends them after the first round, so one cut and a tree;
 * 0.001 lets all three through. Every arm reaches 23 with the same point
 * and two cold searches agree. A negative fraction restores the default;
 * NaN and infinity are refused. */
static void test_a_stalled_root_round_is_the_last(void)
{
    const double stall[3] = { 0.0, 1.0, 0.001 };
    int64_t cuts[3] = { 0 }, nodes[3] = { 0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 5));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_stall(m, stall[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_cut_stall_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                cuts[arm] = rep.cuts;
                nodes[arm] = rep.nodes;
            } else {
                TEST_ASSERT_EQUAL_INT64(cuts[arm], rep.cuts);
                TEST_ASSERT_EQUAL_INT64(nodes[arm], rep.nodes);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_EQUAL_INT64(3, cuts[0]);
    TEST_ASSERT_EQUAL_INT64(1, nodes[0]);
    TEST_ASSERT_EQUAL_INT64(1, cuts[1]);
    TEST_ASSERT_TRUE(nodes[1] > 1);
    TEST_ASSERT_EQUAL_INT64(3, cuts[2]);
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_stall(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_cut_stall_set);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_cut_stall(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_cut_stall(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_cut_stall(m, -INFINITY));
    jaos_model_free(m);
}

/* A round that moves nothing ends the cuts under its node (D305): on the
 * five-item knapsack with cuts to every depth, no cap and the MIR rounds
 * off (they close the root since D309), the root's
 * phase moves the bound by less than the bound itself, so a stall of 1
 * judges it stalled and no node cuts: fewer cuts than without the stall,
 * 23 with the same point, two cold searches agreeing. With the root's
 * cuts off there is nothing to judge and the nodes under it still cut. A
 * negative fraction restores the default; NaN and infinity are refused. */
static void test_a_stalled_round_ends_the_cuts_under_it(void)
{
    int64_t cuts[2] = { 0 };
    for (int arm = 0; arm < 2; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_node_cut_stall(m, arm == 0 ? 0.0 : 1.0));
            TEST_ASSERT_TRUE(m->cfg.mip_node_cut_stall_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                cuts[arm] = rep.cuts;
                nodes1 = rep.nodes;
            } else {
                TEST_ASSERT_EQUAL_INT64(cuts[arm], rep.cuts);
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_TRUE(cuts[1] < cuts[0]);
    jaos_model *m = knapsack5();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_stall(m, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.cuts > 0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_stall(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_node_cut_stall_set);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_node_cut_stall(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_node_cut_stall(m, INFINITY));
    jaos_model_free(m);
}

/* The root's cuts may leave below a node (D306): on the five-item knapsack
 * with one Gomory and one cover round at the root, the drop on reaches 23
 * with the same point at the root only, with cuts to every depth, and
 * with a node's own drop off, and two cold searches agree each time. 0
 * keeps them and a negative value restores the default. */
static void test_root_cuts_may_leave_below_a_node(void)
{
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0, cuts1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 1));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_cut_depth(m, arm == 1 ? 100 : 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_root_cut_drop(m, 1));
            TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop_set);
            TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop);
            if (arm == 2)
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_drop(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            TEST_ASSERT_TRUE(rep.cuts >= 1);
            if (pass == 0) {
                nodes1 = rep.nodes;
                cuts1 = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_root_cut_drop(m, 0));
    TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop_set);
    TEST_ASSERT_FALSE(m->cfg.mip_root_cut_drop);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_root_cut_drop(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_root_cut_drop_set);
    jaos_model_free(m);
}

/* max 10a + 10b + 6f + 15d  s.t.  4a + 4b + 3f + 8d <= 10, all binary.
 * The relaxation is a = b = 1, f = 2/3 at 24 and the answer a = b = 1 at
 * 20. The greedy cover is {a, b, f}: the extended cover gives d, weight
 * 8, the coefficient 1, and a + b + f + d <= 2 leaves the root at
 * (1, 1/2, 0, 1/2), worth 22.5; the lifted cover gives d the coefficient
 * 2, since 8 is at least mu_2 = 4 + 4, and a + b + f + 2d <= 2 closes the
 * root at 20. */
static jaos_model *knapsack_lift(void)
{
    const double cost[4] = { 10.0, 10.0, 6.0, 15.0 };
    const double cl[4] = { 0, 0, 0, 0 }, cu[4] = { 1, 1, 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 10.0 };
    const int64_t as[5] = { 0, 1, 2, 3, 4 }, ai[4] = { 0, 0, 0, 0 };
    const double av[4] = { 4.0, 4.0, 3.0, 8.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    for (int64_t j = 0; j < 4; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

/* A lifted cover closes what the extended cover leaves (D307): one cover
 * round and nothing else on knapsack_lift is one node when lifted and a
 * tree when not, both at 20 with the same point; a negative value
 * restores the default. */
static void test_a_lifted_cover_closes_what_the_extended_cover_leaves(void)
{
    for (int lift = 0; lift < 2; lift++) {
        jaos_model *m = knapsack_lift();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_lift(m, lift));
        TEST_ASSERT_TRUE(m->cfg.mip_cover_lift_set);
        TEST_ASSERT_EQUAL_INT(lift == 1, m->cfg.mip_cover_lift);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[4];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0 && x[3] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.cuts >= 1);
        if (lift)
            TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
        else
            TEST_ASSERT_TRUE(rep.nodes >= 2);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_lift(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_cover_lift_set);
        jaos_model_free(m);
    }
}

/* max x + y  s.t.  2x + 2y <= 3, x and y integer at least 0, no upper
 * bound: the relaxation sits on x + y = 3/2 and the answer is 1. The row
 * shifted to the lower bounds and scaled by 2, the coefficient of the
 * fractional column, is x + y <= 3/2, whose rounding is x + y <= 1; at
 * scale 1 the right-hand side is integral and nothing is cut. */
static jaos_model *halved_row(void)
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
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

/* A MIR cut closes the halved row at the root (D309): one MIR round and
 * nothing else is one node at 1 with a cut; no round is a tree to the
 * same answer. On the five-item knapsack two rounds beside the defaults
 * keep 23 with the same point and two cold searches agree. A negative
 * count restores the default. */
static void test_a_mir_cut_closes_the_halved_row_at_the_root(void)
{
    for (int rounds = 1; rounds >= 0; rounds--) {
        jaos_model *m = halved_row();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, rounds));
        TEST_ASSERT_TRUE(m->cfg.mip_mir_rounds_set);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (rounds == 1) {
            TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
            TEST_ASSERT_TRUE(rep.cuts >= 1);
        } else {
            TEST_ASSERT_TRUE(rep.nodes >= 2);
            TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
        }
        jaos_model_free(m);
    }
    double x1[5], x2[5];
    int64_t nodes1 = 0, cuts1 = 0;
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 2));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            nodes1 = rep.nodes;
            cuts1 = rep.cuts;
        } else {
            TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_mir_rounds_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
}

/* A backtracking dive reaches the same optimum (D308): on the five-item
 * knapsack with the cuts off and the dive on, 0, 1 and 1000 backtracks
 * per dive reach 23 with the same point, each bit-reproducible across
 * two cold searches, and the bound published at a node limit of 1 is the
 * root's whichever way. A negative count restores the default. */
static void test_a_backtracking_dive_reaches_the_same_optimum(void)
{
    const int64_t times[3] = { 0, 1, 1000 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, times[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_backtrack_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, arm * 1000));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(m, 2));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NODE_LIMIT, jaos_status_of(m));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.bound <= 24.8 + 1e-9 && rep.bound >= 23.0 - 1e-9);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_dive_backtrack_set);
        jaos_model_free(m);
    }
}

/* MIR cuts at the nodes (D310): on the halved row with every root cut
 * off and cuts to every depth, a node's MIR round over its own bounds
 * closes the branch x <= 1 at x + y <= 1, so the tree with them adds at
 * least as many cuts as without and reaches 1 either way; on the
 * five-item knapsack the same, at 23 with the same point, two cold
 * searches agreeing. A negative value restores the default. */
static void test_mir_cuts_at_the_nodes_keep_the_optimum(void)
{
    int64_t cuts_off = 0, cuts_on = 0;
    for (int on = 0; on < 2; on++) {
        jaos_model *m = halved_row();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_mir(m, on));
        TEST_ASSERT_TRUE(m->cfg.mip_node_mir_set);
        TEST_ASSERT_EQUAL_INT(on == 1, m->cfg.mip_node_mir);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (on == 0)
            cuts_off = rep.cuts;
        else
            cuts_on = rep.cuts;
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(cuts_on >= cuts_off);
    double x1[5], x2[5];
    int64_t nodes1 = 0, cuts1 = 0;
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_mir(m, 1));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            nodes1 = rep.nodes;
            cuts1 = rep.cuts;
        } else {
            TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_mir(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_node_mir_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
}

/* A dive resume bounded by the gap (D311): on the five-item knapsack with
 * the cuts off and the dive on, no resume count and a gap of 0.01, of 1
 * and of 0.01 with a count of 2 each reach 23 with the same point, two
 * cold searches agreeing. NaN and the infinities are refused; a negative
 * fraction restores the default. */
static void test_a_dive_bounded_by_the_gap_reaches_the_same_optimum(void)
{
    const double frac[3] = { 0.01, 1.0, 0.01 };
    const int64_t count[3] = { 0, 0, 2 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, count[arm]));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_gap(m, frac[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_gap_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    /* The canary: the gap must decide something. A fraction of 1e-12
     * resumes almost never and one of 1e12 almost always, so the two
     * trees must differ -- the first form of this rule compared against
     * the heap, which a dive empties, and read the same at every
     * fraction. */
    int64_t canary[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *c = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(c, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(c, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_mip_dive_gap(c, arm == 0 ? 1e-12 : 1e12));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(c));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(c, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(c, &rep));
        canary[arm] = rep.nodes;
        jaos_model_free(c);
    }
    TEST_ASSERT_TRUE(canary[0] != canary[1]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_dive_gap(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_dive_gap(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_dive_gap(m, -INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_gap(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_dive_gap_set);
    jaos_model_free(m);
}

/* max 3x + 2y  s.t.  2x - s <= 0,  2y + s <= 3,  x + y <= 4, x and y
 * integer in [0, 3], s continuous in [0, 10]. Neither row alone gives a
 * MIR cut the point violates: the first has no integer term once s is
 * shifted, and the second's rounding is weaker than the point. Their
 * aggregate does: substituting s out of the first with the second gives
 * 2x + 2y <= 3, whose rounding at delta 2 is x + y <= 1, and the root
 * closes at 3. */
static jaos_model *aggregate_pair(void)
{
    const double cost[3] = { 3.0, 2.0, 0.0 };
    const double cl[3] = { 0, 0, 0 }, cu[3] = { 3.0, 3.0, 10.0 };
    const double rl[3] = { -INFINITY, -INFINITY, -INFINITY };
    const double ru[3] = { 0.0, 3.0, 4.0 };
    /* column-wise: x in rows 0 and 2, y in rows 1 and 2, s in rows 0, 1 */
    const int64_t as[4] = { 0, 2, 4, 6 };
    const int64_t ai[6] = { 0, 2, 1, 2, 0, 1 };
    const double av[6] = { 2.0, 1.0, 2.0, 1.0, -1.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     6, as, ai, av));
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

/* An aggregated MIR cut closes what a single row leaves (D312): on the
 * pair above with only the MIR family on, no aggregation gives no cut and
 * a tree, and one step of aggregation closes the root; both reach 3, and
 * two cold searches agree. A negative count restores the default, and
 * aggregation with the MIR rounds off does nothing. */
static void test_an_aggregated_mir_cut_closes_what_one_row_leaves(void)
{
    int64_t nodes[2] = { 0, 0 }, cuts[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        double x1[3], x2[3];
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = aggregate_pair();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 4));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_aggregate(m, arm));
            TEST_ASSERT_TRUE(m->cfg.mip_mir_aggregate_set);
            TEST_ASSERT_EQUAL_INT64(arm, m->cfg.mip_mir_aggregate);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes[arm] = rep.nodes;
                cuts[arm] = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes[arm], rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts[arm], rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_EQUAL_INT64(0, cuts[0]);
    TEST_ASSERT_TRUE(nodes[0] > 1);
    TEST_ASSERT_TRUE(cuts[1] >= 1);
    TEST_ASSERT_EQUAL_INT64(1, nodes[1]);
    /* The MIR rounds off: aggregation has nothing to aggregate. */
    jaos_model *m = aggregate_pair();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_aggregate(m, 3));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_aggregate(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_mir_aggregate_set);
    jaos_model_free(m);
}

/* The dive heuristic finds the first incumbent at the root (D313): on the
 * neighbour model with the rounding heuristic off, no dive leaves the
 * first incumbent to the tree and a dive of five puts it at node 1 with a
 * heuristic point, the answer unmoved and two cold searches agreeing. One
 * solve is not enough, since the first is the root's own relaxation. On
 * the five-item knapsack the dive goes infeasible at its first fixing and
 * gives up, which changes no answer. A negative count restores the
 * default. */
static void test_the_dive_heuristic_finds_the_first_incumbent(void)
{
    const int64_t solves[3] = { 0, 1, 5 };
    int64_t first[3] = { 0, 0, 0 }, points[3] = { 0, 0, 0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[2], x2[2];
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = neighbour_model();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_dive_heuristic(m, solves[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_heuristic_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                first[arm] = rep.first_incumbent_node;
                points[arm] = rep.heuristic_points;
            } else {
                TEST_ASSERT_EQUAL_INT64(first[arm], rep.first_incumbent_node);
                TEST_ASSERT_EQUAL_INT64(points[arm], rep.heuristic_points);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 2.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_EQUAL_INT64(0, points[0]);
    TEST_ASSERT_TRUE(first[0] > 1);
    TEST_ASSERT_EQUAL_INT64(points[1], points[0]);
    TEST_ASSERT_EQUAL_INT64(1, points[2]);
    TEST_ASSERT_EQUAL_INT64(1, first[2]);
    /* A dive that goes infeasible gives up and changes no answer. */
    jaos_model *m = knapsack5();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 20));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_dive_heuristic_set);
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
    RUN_TEST(test_without_cuts_or_dive_the_tree_branches_to_the_same_answer);
    RUN_TEST(test_a_cut_over_continuous_columns_keeps_the_integer_optimum);
    RUN_TEST(test_the_rounding_heuristic_takes_the_relaxations_neighbour);
    RUN_TEST(test_an_infeasible_rounding_is_not_taken);
    RUN_TEST(test_the_tree_logs_its_start_root_and_end);
    RUN_TEST(test_a_node_limit_stops_with_the_incumbent_the_callback_saw);
    RUN_TEST(test_both_branching_rules_reach_the_same_optimum);
    RUN_TEST(test_a_fractional_bound_on_an_integer_column_is_rounded_inward);
    RUN_TEST(test_strong_branching_probes_are_counted_and_change_no_answer);
    RUN_TEST(test_a_capped_probe_reaches_the_same_optimum);
    RUN_TEST(test_every_dive_child_rule_reaches_the_same_optimum);
    RUN_TEST(test_cuts_below_the_root_reach_the_same_optimum);
    RUN_TEST(test_dropping_slack_cuts_keeps_the_optimum);
    RUN_TEST(test_probing_at_the_root_only_reaches_the_same_optimum);
    RUN_TEST(test_the_solution_pool_holds_the_best_points_best_first);
    RUN_TEST(test_a_cover_cut_closes_the_knapsack_at_the_root);
    RUN_TEST(test_a_cap_on_a_nodes_cuts_keeps_the_optimum);
    RUN_TEST(test_a_stalled_root_round_is_the_last);
    RUN_TEST(test_a_stalled_round_ends_the_cuts_under_it);
    RUN_TEST(test_root_cuts_may_leave_below_a_node);
    RUN_TEST(test_a_lifted_cover_closes_what_the_extended_cover_leaves);
    RUN_TEST(test_a_mir_cut_closes_the_halved_row_at_the_root);
    RUN_TEST(test_a_backtracking_dive_reaches_the_same_optimum);
    RUN_TEST(test_mir_cuts_at_the_nodes_keep_the_optimum);
    RUN_TEST(test_a_dive_bounded_by_the_gap_reaches_the_same_optimum);
    RUN_TEST(test_an_aggregated_mir_cut_closes_what_one_row_leaves);
    RUN_TEST(test_the_dive_heuristic_finds_the_first_incumbent);
    return UNITY_END();
}
