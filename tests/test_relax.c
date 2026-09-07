/* The feasibility relaxation (D331), and the basis a non-optimal answer
 * publishes (D330).
 *
 * The relaxation is tested against an oracle rather than against a number
 * copied out of a run: every case moves the bounds by what the call
 * reports, re-solves, and asserts the moved model is feasible. A total
 * that is right but not achievable would pass a value assert and fail
 * that one. The other half is the lower bound -- that no smaller total
 * works -- which is asserted where the model is small enough to say by
 * hand what the smallest is.
 *
 * The basis is tested by its structure, because that is what jaos.h
 * promises about it: exactly num_row basic entries, on every answer the
 * call gives out. */
#include "jaos.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* x + y >= 30 with both columns in [0, 10]: the row's lower bound is 10
 * above anything the columns can reach. */
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

/* Applies every move the report named and answers whether what is left
 * has a feasible point. This is the oracle: a relaxation that does not
 * relax is not one. */
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
    /* Ten is the whole gap and no set of moves is smaller: the columns
     * together reach 20 and the row wants 30. */
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
    /* The row's lower bound comes down, so the move is negative. */
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, rm[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, after_moving(m, rm, cm));

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_COLS, rm, cm, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, rep.total);
    TEST_ASSERT_EQUAL_INT64(0, rep.rows_moved);
    TEST_ASSERT_EQUAL_INT64(1, rep.cols_moved);
    /* A column's upper bound goes up, so the move is positive. */
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

/* The caller's model is not solved and not touched: the relaxation runs
 * on a copy, and a caller who has an answer keeps it. */
static void test_the_callers_model_is_untouched(void)
{
    jaos_model *m = over_reach();
    double rm[1], cm[2];
    jaos_relax_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_feasrelax(m, JAOS_RELAX_BOTH, rm, cm, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));
    /* And the relaxation's own cost is reported rather than billed. */
    TEST_ASSERT_TRUE(rep.work_units > 0);
    jaos_model_free(m);
}

/* Two rows that contradict each other by 4 in total, so the relaxation
 * must find 4 and the oracle must accept the moved model. */
static void test_two_rows_share_the_violation(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    /* x >= 7 and x <= 3, with x free to move. */
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

/* An inverted box has no relaxation in this form and the call says so
 * rather than answering something. */
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

/* --- the basis behind a non-optimal answer (D330) --------------------- */

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

/* Two rows that cannot both hold, and neither of them says so on its
 * own: x + y <= 1 beside x + y >= 2, with both columns open above. Every
 * presolve family here looks at one row or one column at a time and
 * bound tightening is refused (D97), so nothing reduces this and the
 * dual simplex is what answers. That is the point of the model -- a
 * verdict presolve reaches by itself has no basis to publish, so a test
 * of the published basis has to reach the simplex. */
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
    /* The count is the promise: exactly num_row basic, so the statuses
     * are a basis of this model and not a list. */
    TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic_count(m));
    /* And the answer is still refused, because there is none. */
    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    jaos_model_free(m);
}

/* The other half of the same rule: a verdict presolve reached with no
 * simplex publishes no basis, and `over_reach` is one -- its single row
 * asks for more than its own columns can give, which one forcing-row
 * test settles. */
static void test_a_verdict_presolve_reached_publishes_no_basis(void)
{
    jaos_model *m = over_reach();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_basis_status cs[2], rs[1];
    const jaos_status got = jaos_basis(m, cs, rs);
#if defined(JAOS_NO_PRESOLVE)
    /* Without presolve the simplex answers it, and then there is a
     * basis. The reference build asserts its own answer rather than
     * skipping, because a one-sided test passes on a call that always
     * refuses (D330). */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, got);
    TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic_count(m));
#else
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, got);
#endif
    jaos_model_free(m);
}

/* A verdict reached before any simplex ran has no basis to publish, and
 * a buffer of zeros would read as one in which everything is basic. */
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

/* An unbounded answer stops on a basis too, and the ray is still there.
 * The two columns move together at the same rate, so the row they share
 * does not hold them and no single column runs off on its own -- which
 * is what keeps the reduction that proves unboundedness by itself away
 * from this model. */
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

/* The basis a refusal publishes is a basis jaos_set_basis accepts, which
 * is the whole point of publishing it: a caller can carry it to the next
 * model. jaos_set_basis refuses any count but num_row, so this asserts
 * the count a second time through the library's own gate. */
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
    /* Moved far enough to be feasible, it solves from that basis. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(c, 0, -INFINITY, 3.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(c));
    jaos_model_free(c);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
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
