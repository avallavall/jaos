/* Irreducible infeasible subsystems (D264): models whose IIS is worked by
 * hand, and the solver as the oracle -- the sides named, kept alone, are
 * infeasible, and dropping any one of them is feasible. The oracle is
 * what makes "irreducible" a checked word rather than a claimed one. */
#include "jaos.h"
#include "jaos_internal.h"    /* the matrix, which the API does not read back */
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* A copy of `m` with only the named sides kept, no objective, and one
 * side less when `drop_row`/`drop_col` and `drop_side` name one. */
static jaos_model *subsystem(const jaos_model *m, const jaos_iis_side *rs,
                             const jaos_iis_side *cs, int64_t drop_row,
                             int64_t drop_col, jaos_iis_side drop_side)
{
    const int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    double *zero = calloc((size_t)(nc + 1), sizeof *zero);
    double *rl = malloc((size_t)(nr + 1) * sizeof *rl);
    double *ru = malloc((size_t)(nr + 1) * sizeof *ru);
    double *cl = malloc((size_t)(nc + 1) * sizeof *cl);
    double *cu = malloc((size_t)(nc + 1) * sizeof *cu);
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_NOT_NULL(rl);
    TEST_ASSERT_NOT_NULL(ru);
    TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu);
    for (int64_t i = 0; i < nr; i++) {
        double lo, up;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, i, &lo, &up));
        int s = rs[i];
        if (i == drop_row)
            s &= ~drop_side;
        rl[i] = s & JAOS_IIS_LOWER ? lo : -INFINITY;
        ru[i] = s & JAOS_IIS_UPPER ? up : INFINITY;
    }
    for (int64_t j = 0; j < nc; j++) {
        double lo, up;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, j, &lo, &up));
        int s = cs[j];
        if (j == drop_col)
            s &= ~drop_side;
        cl[j] = s & JAOS_IIS_LOWER ? lo : -INFINITY;
        cu[j] = s & JAOS_IIS_UPPER ? up : INFINITY;
    }
    jaos_model *s = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&s));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(s, nc, nr, JAOS_MINIMIZE, 0.0, zero, cl, cu, rl, ru,
                     m->num_nz, m->a_start, m->a_index, m->a_value));
    free(zero);
    free(rl);
    free(ru);
    free(cl);
    free(cu);
    return s;
}

/* The oracle: the named sides alone are infeasible, and every one of
 * them is needed. */
static void assert_irreducible(const jaos_model *m, const jaos_iis_side *rs,
                               const jaos_iis_side *cs)
{
    jaos_model *s = subsystem(m, rs, cs, -1, -1, JAOS_IIS_NONE);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(s));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_INFEASIBLE, jaos_status_of(s),
                                  "the IIS alone is not infeasible");
    jaos_model_free(s);
    static const jaos_iis_side sides[2] = {JAOS_IIS_LOWER, JAOS_IIS_UPPER};
    for (int64_t i = 0; i < jaos_num_row(m); i++)
        for (int k = 0; k < 2; k++) {
            if (!(rs[i] & sides[k]))
                continue;
            s = subsystem(m, rs, cs, i, -1, sides[k]);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(s));
            TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_OPTIMAL,
                jaos_status_of(s), "a row side is not needed");
            jaos_model_free(s);
        }
    for (int64_t j = 0; j < jaos_num_col(m); j++)
        for (int k = 0; k < 2; k++) {
            if (!(cs[j] & sides[k]))
                continue;
            s = subsystem(m, rs, cs, -1, j, sides[k]);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(s));
            TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_OPTIMAL,
                jaos_status_of(s), "a column side is not needed");
            jaos_model_free(s);
        }
}

/* x >= 1 as a row, x <= 0 as a row, x >= 0 as its own bound: the IIS is
 * the two rows, one side each, and the column bound is not in it. */
static jaos_model *make_two_rows(void)
{
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {1.0, -INFINITY}, ru[] = {INFINITY, 0.0};
    const int64_t s[]  = {0, 2};
    const int64_t ix[] = {0, 1};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

static void test_nothing_to_find_without_an_infeasible_answer(void)
{
    jaos_model *m = make_two_rows();
    jaos_iis_side rs[2], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_iis(nullptr, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_iis(m, rs, cs, nullptr));
    /* A feasible model has none either. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 1, -INFINITY, 5.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_TRUE(strstr(jaos_model_error(m), "INFEASIBLE") != nullptr);
    jaos_model_free(m);
}

static void test_two_rows_and_the_column_bound_is_not_a_member(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    /* Positive test. The off-by-one build shuffles every postsolve restore
     * index, so both the certificate's support and the filter's counts
     * move; the sides this asks for are a claim about a correct replay.
     * The wrong-dual build rewrites only a singleton row's dual on an
     * OPTIMAL replay, and no verdict here reads a dual, so it runs. */
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_two_rows();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[2] = {9, 9}, cs[1] = {9};
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_LOWER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, rs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[0]);
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    TEST_ASSERT_TRUE(rep.from_certificate);
    /* The support is exactly the answer here, so the filter confirms it
     * once and asks once per candidate. */
    TEST_ASSERT_EQUAL_INT64(2, rep.candidates);
    TEST_ASSERT_EQUAL_INT64(3, rep.solves);
    TEST_ASSERT_TRUE(rep.work_units >= 0);
    assert_irreducible(m, rs, cs);

    /* The caller's model is untouched: same verdict, same certificate,
     * same bounds, and the work of the last solve is still the solve's. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    double y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    double lo, up;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &up));
    TEST_ASSERT_TRUE(lo == 1.0 && up == INFINITY);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &up));
    TEST_ASSERT_TRUE(lo == 0.0 && up == INFINITY);
    /* Either output array may be left out. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, nullptr, nullptr, &rep));
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    jaos_model_free(m);
#endif
}

/* x >= 1 as a row against x <= 0 as the column's own upper bound: a
 * member on each side of the model. */
static void test_a_column_bound_can_be_a_member(void)
{
    const double c[]  = {0.0};
    const double cl[] = {-INFINITY}, cu[] = {0.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[1], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_LOWER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, cs[0]);
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);
}

/* An inverted box has no certificate (jaos.h), and its two sides are the
 * IIS. An EMPTY row inverted the same way is infeasible on its lower
 * side alone: 0 >= 1 needs no upper bound to fail. */
static void test_an_inverted_box_is_its_own_iis(void)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 2.0}, cu[] = {INFINITY, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_certificate(m, y));
    jaos_iis_side rs[1], cs[2];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_BOTH, cs[1]);
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    TEST_ASSERT_FALSE(rep.from_certificate);
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);

    /* The empty inverted row. */
    const double c2[]  = {1.0};
    const double cl2[] = {0.0}, cu2[] = {1.0};
    const double rl2[] = {1.0}, ru2[] = {0.0};
    m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c2, cl2, cu2, rl2, ru2,
                     0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_LOWER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[0]);
    TEST_ASSERT_EQUAL_INT64(1, rep.members);
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);
}

/* Two infeasible subsystems that share nothing: x0 pinned above 1 and
 * below 0 by rows 0 and 1, x1 the same by rows 2 and 3, plus a feasible
 * coupling row. The answer is ONE of the two, whole, and nothing else;
 * the walk is in index order, so it is the first. */
static void test_a_model_with_two_iis_reports_one(void)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {-INFINITY, -INFINITY}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {1.0, -INFINITY, 1.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 0.0, INFINITY, 0.0, 100.0};
    const int64_t s[]  = {0, 3, 6};
    const int64_t ix[] = {0, 1, 4, 2, 3, 4};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 5, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[5], cs[2];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, rs[4]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[1]);
    const bool first = rs[0] == JAOS_IIS_LOWER && rs[1] == JAOS_IIS_UPPER &&
                       rs[2] == JAOS_IIS_NONE && rs[3] == JAOS_IIS_NONE;
    const bool second = rs[0] == JAOS_IIS_NONE && rs[1] == JAOS_IIS_NONE &&
                        rs[2] == JAOS_IIS_LOWER && rs[3] == JAOS_IIS_UPPER;
    TEST_ASSERT_TRUE_MESSAGE(first || second, "neither subsystem, whole");
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);
}

/* A subsystem the certificate over-covers. Three rows on two columns:
 *   x0 + x1 >= 4, x0 <= 1, x1 <= 1, both columns in [0, 10].
 * The ray leans on all three rows and possibly on the column bounds,
 * which are slack; the filter must drop what is not needed. */
static void test_the_deletion_filter_drops_what_the_ray_over_covers(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    /* Positive test, same reason as
     * test_two_rows_and_the_column_bound_is_not_a_member. */
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    const double c[]  = {0.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {4.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 1.0, 1.0};
    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {0, 1, 0, 2};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[3], cs[2];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_LOWER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, rs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, rs[2]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, cs[1]);
    TEST_ASSERT_EQUAL_INT64(3, rep.members);
    TEST_ASSERT_TRUE(rep.candidates >= 3);
    TEST_ASSERT_EQUAL_INT64(rep.candidates + 1, rep.solves);
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);
#endif
}

/* Maximised, with an objective that would be unbounded were the model
 * feasible: the copy has no objective, so the filter never sees it. */
static void test_the_objective_does_not_reach_the_filter(void)
{
    /* max x with x >= 1 and x <= 0 as rows: relaxing the cap leaves a
     * feasible model whose objective runs off, and the filter must read
     * that as feasible, which it can only do with the objective gone. */
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {1.0, -INFINITY}, ru[] = {INFINITY, 0.0};
    const int64_t s[]  = {0, 2};
    const int64_t ix[] = {0, 1};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[2], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);
}

/* A budget no re-solve can meet is reported, not worked around. The
 * model is one presolve's families leave alone -- every row has two
 * entries and every column three, nothing is forcing and nothing is
 * fixed -- so the simplex has to pivot in every build, and a limit of
 * one unit stops it before it decides. The infeasibility is that the
 * three caps sum to 2(x0 + x1 + x2) <= 3 against x0 + x1 + x2 >= 5. */
static void test_a_budget_stop_is_reported(void)
{
    const double c[]  = {0.0, 0.0, 0.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, 10.0};
    const double rl[] = {5.0, -INFINITY, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 1.0, 1.0, 1.0};
    const int64_t s[]  = {0, 3, 6, 9};
    const int64_t ix[] = {0, 1, 3, 0, 1, 2, 0, 2, 3};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 4, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     9, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_TRUE_MESSAGE(jaos_iterations(m) > 0,
                             "presolve decided it; the arm needs a pivot");
    jaos_iis_side rs[4], cs[3];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT64(4, rep.members);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_LOWER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, rs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, rs[2]);
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_UPPER, rs[3]);
    assert_irreducible(m, rs, cs);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    jaos_iis_report rep2;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL, jaos_iis(m, rs, cs, &rep2));
    TEST_ASSERT_TRUE(strstr(jaos_model_error(m), "work limit") != nullptr);
    TEST_ASSERT_EQUAL_INT64(1, rep2.solves);
    TEST_ASSERT_EQUAL_INT64(0, rep2.members);
    /* The caller's own answer was not touched by the stop either. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_the_answer_is_reproducible(void)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {4.0, -INFINITY, -INFINITY, 1.0};
    const double ru[] = {INFINITY, 1.0, 1.0, 3.0};
    const int64_t s[]  = {0, 3, 6};
    const int64_t ix[] = {0, 1, 3, 0, 2, 3};
    const double v[]   = {1.0, 1.0, 2.0, 1.0, 1.0, -1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 4, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[4], cs[2], rs2[4], cs2[2];
    jaos_iis_report rep, rep2;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs2, cs2, &rep2));
    TEST_ASSERT_EQUAL_MEMORY(rs, rs2, sizeof rs);
    TEST_ASSERT_EQUAL_MEMORY(cs, cs2, sizeof cs);
    TEST_ASSERT_EQUAL_INT64(rep.solves, rep2.solves);
    TEST_ASSERT_EQUAL_INT64(rep.work_units, rep2.work_units);
    assert_irreducible(m, rs, cs);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_to_find_without_an_infeasible_answer);
    RUN_TEST(test_two_rows_and_the_column_bound_is_not_a_member);
    RUN_TEST(test_a_column_bound_can_be_a_member);
    RUN_TEST(test_an_inverted_box_is_its_own_iis);
    RUN_TEST(test_a_model_with_two_iis_reports_one);
    RUN_TEST(test_the_deletion_filter_drops_what_the_ray_over_covers);
    RUN_TEST(test_the_objective_does_not_reach_the_filter);
    RUN_TEST(test_a_budget_stop_is_reported);
    RUN_TEST(test_the_answer_is_reproducible);
    return UNITY_END();
}
