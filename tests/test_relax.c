#include "jaos.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *over_reach(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {10.0, 10.0};
    const double rl[1] = {30.0}, ru[1] = {INFINITY};
    const int64_t s[3] = {0, 1, 2};
    const int64_t ix[2] = {0, 0};
    const double v[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

static jaos_solve_status after_moving(const jaos_model *src,
                                      const double *rm, const double *cm)
{
    const int64_t nr = jaos_num_row(src), nc = jaos_num_col(src);
    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(src, &c));
    for (int64_t i = 0; i < nr; i++) {
        if (rm[i] == 0.0)
            continue;
        double lo, hi;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(c, i, &lo, &hi));
        if (rm[i] < 0.0) lo += rm[i]; else hi += rm[i];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(c, i, lo, hi));
    }
    for (int64_t j = 0; j < nc; j++) {
        if (cm[j] == 0.0)
            continue;
        double lo, hi;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(c, j, &lo, &hi));
        if (cm[j] < 0.0) lo += cm[j]; else hi += cm[j];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(c, j, lo, hi));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
    const jaos_solve_status st = jaos_status_of(c);
    jaos_model_free(c);
    return st;
}

static void test_a_row_that_asks_more_than_the_columns_can_give(void)
{
    jaos_model *m = over_reach();
    double rm[1], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));

    TEST_ASSERT_EQUAL_DOUBLE(10.0, rep.total);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, after_moving(m, rm, cm));
    jaos_model_free(m);
}

static void test_the_scope_decides_which_bound_moves(void)
{
    jaos_model *m = over_reach();
    double rm[1], cm[2];
    jaos_relax_report rep;

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_ROWS, rm, cm, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(1, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT64(0, rep.cols_moved);

    TEST_ASSERT_EQUAL_DOUBLE(-10.0, rm[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, after_moving(m, rm, cm));

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_COLS, rm, cm, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(0, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT64(1, rep.cols_moved);

    TEST_ASSERT_EQUAL_DOUBLE(10.0, cm[0] + cm[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, after_moving(m, rm, cm));

    jaos_model_free(m);
}

static void test_a_feasible_model_moves_nothing(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {10.0, 10.0};
    const double rl[1] = {5.0}, ru[1] = {INFINITY};
    const int64_t s[3] = {0, 1, 2};
    const int64_t ix[2] = {0, 0};
    const double v[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    double rm[1], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(0, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT64(0, rep.cols_moved);
    TEST_ASSERT_EQUAL_INT64(-1, rep.at_row);
    TEST_ASSERT_EQUAL_INT64(-1, rep.at_col);
    jaos_model_free(m);
}

static void test_the_callers_model_is_untouched(void)
{
    jaos_model *m = over_reach();
    double rm[1], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));

    TEST_ASSERT_TRUE(rep.work_units > 0);
    jaos_model_free(m);
}

static void test_two_rows_share_the_violation(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double c[1] = {0.0};
    const double cl[1] = {-INFINITY}, cu[1] = {INFINITY};
    const double rl[2] = {7.0, -INFINITY}, ru[2] = {INFINITY, 3.0};
    const int64_t s[2] = {0, 2};
    const int64_t ix[2] = {0, 1};
    const double v[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    double rm[2], cm[1];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_ROWS, rm, cm, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, rep.total);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, rep.largest);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, after_moving(m, rm, cm));
    jaos_model_free(m);
}

static void test_an_inverted_box_has_no_relaxation(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[1] = {0.0};
    const double cl[1] = {5.0}, cu[1] = {3.0};
    const double rl[1] = {0.0}, ru[1] = {INFINITY};
    const int64_t s[2] = {0, 1};
    const int64_t ix[1] = {0};
    const double v[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    double rm[1], cm[1];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, rep.status);
    jaos_model_free(m);
}

static void test_a_scope_that_is_not_one_of_the_three_is_refused(void)
{
    jaos_model *m = over_reach();
    double rm[1], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_feasrelax(m, (jaos_relax_scope)0, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_feasrelax(m, (jaos_relax_scope)4, rm, cm, &rep));
    jaos_model_free(m);
}

static int64_t basic_count(const jaos_model *m)
{
    const int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    jaos_basis_status *cs = calloc((size_t)(nc + 1), sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)(nr + 1), sizeof *rs);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t k = 0;
    for (int64_t j = 0; j < nc; j++) k += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < nr; i++) k += rs[i] == JAOS_BASIS_BASIC;
    free(cs);
    free(rs);
    return k;
}

static jaos_model *two_rows_conflict(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {INFINITY, INFINITY};
    const double rl[2] = {-INFINITY, 2.0}, ru[2] = {1.0, INFINITY};
    const int64_t s[3] = {0, 2, 4};
    const int64_t ix[4] = {0, 1, 0, 1};
    const double v[4] = {1.0, 1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}

static void test_an_infeasible_answer_publishes_its_basis(void)
{
    jaos_model *m = two_rows_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic_count(m));

    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    jaos_model_free(m);
}

static void test_a_verdict_presolve_reached_publishes_no_basis(void)
{
    jaos_model *m = over_reach();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_basis_status cs[2], rs[1];
    const jaos_status got = jaos_basis(m, cs, rs);
#if defined(JAOS_NO_PRESOLVE)

    TEST_ASSERT_EQUAL_INT(JAOS_OK, got);
    TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic_count(m));
#else
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, got);
#endif
    jaos_model_free(m);
}

static void test_an_inverted_box_publishes_no_basis(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[1] = {0.0};
    const double cl[1] = {5.0}, cu[1] = {3.0};
    const double rl[1] = {0.0}, ru[1] = {INFINITY};
    const int64_t s[2] = {0, 1};
    const int64_t ix[1] = {0};
    const double v[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_basis_status cs[1], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
    jaos_model_free(m);
}

static void test_a_model_that_never_solved_publishes_no_basis(void)
{
    jaos_model *m = over_reach();
    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
    jaos_model_free(m);
}

static void test_an_unbounded_answer_publishes_its_basis(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {-1.0, -1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {INFINITY, INFINITY};
    const double rl[1] = {-INFINITY}, ru[1] = {1.0};
    const int64_t s[3] = {0, 1, 2};
    const int64_t ix[2] = {0, 0};
    const double v[2] = {1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic_count(m));
    double ray[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, ray));
    jaos_model_free(m);
}

static void test_the_published_basis_is_one_set_basis_takes(void)
{
    jaos_model *m = two_rows_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_basis_status cs[2], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(c, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(c, 0, -INFINITY, 3.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(c));
    jaos_model_free(c);
    jaos_model_free(m);
}

static void test_a_hostile_warm_start_still_reaches_the_answer(void)
{

    jaos_model *a = two_rows_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(a));
    jaos_basis_status cs[2], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(a, cs, rs));
    jaos_model_free(a);

    jaos_model *b = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&b));
    {
        const double c[2] = {1.0, 1.0};
        const double cl[2] = {0.0, 0.0}, cu[2] = {INFINITY, INFINITY};
        const double rl[2] = {-INFINITY, -INFINITY};
        const double ru[2] = {1.0, 4.0};
        const int64_t s[3] = {0, 2, 4};
        const int64_t ix[4] = {0, 1, 0, 1};
        const double v[4] = {1.0, 1.0, 1.0, 1.0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(b, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         4, s, ix, v));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(b, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(b));
    double obj = -1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, obj);
    jaos_model_free(b);
}

static void test_a_second_solve_reaches_the_same_verdict(void)
{
    jaos_model *m = two_rows_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic_count(m));
    jaos_model_free(m);
}

static void test_an_sos_set_the_rows_alone_do_not_break(void)
{
    /* x >= 2 and y >= 2, and an SOS1 set that lets only one of them be
       nonzero. The rows alone are satisfiable and the set is what breaks
       the model, so a relaxation that drops the set moves nothing. */
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {10.0, 10.0};
    const double rl[2] = {2.0, 2.0}, ru[2] = {INFINITY, INFINITY};
    const int64_t s[3] = {0, 1, 2};
    const int64_t ix[2] = {0, 1};
    const double v[2] = {1.0, 1.0};
    const int64_t sc[2] = {0, 1};
    const double sw[2] = {1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(m, 1, 2, sc, sw));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    double rm[2], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_ROWS, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, rep.status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(1, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, after_moving(m, rm, cm));
    jaos_model_free(m);
}

static void test_a_semi_continuous_column_may_take_its_zero(void)
{
    /* x is zero or between 5 and 10, and the row caps it at zero. The
       zero satisfies both, so nothing has to move. A copy that forgets
       the column is semi-continuous sees 5 <= x <= 0 and moves a bound. */
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[1] = {1.0};
    const double cl[1] = {5.0}, cu[1] = {10.0};
    const double rl[1] = {-INFINITY}, ru[1] = {0.0};
    const int64_t s[2] = {0, 1};
    const int64_t ix[1] = {0};
    const double v[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_semicontinuous(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double rm[1], cm[1];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, rep.status);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(0, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT64(0, rep.cols_moved);
    jaos_model_free(m);
}

static void test_an_indicator_row_that_is_switched_off_holds_nothing(void)
{
    /* Row 0 holds only while z is 1, and z is fixed at 0, so x <= 3 and
       the row asking x >= 5 never applies. A copy that forgets the
       indicator reads the row as always on and moves a bound. */
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {1.0, 0.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {3.0, 0.0};
    const double rl[1] = {5.0}, ru[1] = {INFINITY};
    const int64_t s[3] = {0, 1, 1};
    const int64_t ix[1] = {0};
    const double v[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_indicator(m, 0, 1, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double rm[1], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, rep.status);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(0, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT64(0, rep.cols_moved);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_an_sos_set_the_rows_alone_do_not_break);
    RUN_TEST(test_a_semi_continuous_column_may_take_its_zero);
    RUN_TEST(test_an_indicator_row_that_is_switched_off_holds_nothing);
    RUN_TEST(test_a_hostile_warm_start_still_reaches_the_answer);
    RUN_TEST(test_a_second_solve_reaches_the_same_verdict);
    RUN_TEST(test_a_row_that_asks_more_than_the_columns_can_give);
    RUN_TEST(test_the_scope_decides_which_bound_moves);
    RUN_TEST(test_a_feasible_model_moves_nothing);
    RUN_TEST(test_the_callers_model_is_untouched);
    RUN_TEST(test_two_rows_share_the_violation);
    RUN_TEST(test_an_inverted_box_has_no_relaxation);
    RUN_TEST(test_a_scope_that_is_not_one_of_the_three_is_refused);
    RUN_TEST(test_an_infeasible_answer_publishes_its_basis);
    RUN_TEST(test_a_verdict_presolve_reached_publishes_no_basis);
    RUN_TEST(test_an_inverted_box_publishes_no_basis);
    RUN_TEST(test_a_model_that_never_solved_publishes_no_basis);
    RUN_TEST(test_an_unbounded_answer_publishes_its_basis);
    RUN_TEST(test_the_published_basis_is_one_set_basis_takes);
    return UNITY_END();
}
