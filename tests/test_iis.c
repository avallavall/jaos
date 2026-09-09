#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

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

    TEST_ASSERT_EQUAL_INT64(2, rep.candidates);
    TEST_ASSERT_EQUAL_INT64(3, rep.solves);
    TEST_ASSERT_TRUE(rep.work_units >= 0);
    assert_irreducible(m, rs, cs);

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    double y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    double lo, up;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &up));
    TEST_ASSERT_TRUE(lo == 1.0 && up == INFINITY);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &up));
    TEST_ASSERT_TRUE(lo == 0.0 && up == INFINITY);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, nullptr, nullptr, &rep));
    TEST_ASSERT_EQUAL_INT64(2, rep.members);
    jaos_model_free(m);
#endif
}

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

static void test_the_deletion_filter_drops_what_the_ray_over_covers(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)

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

static void test_the_objective_does_not_reach_the_filter(void)
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

static void test_the_subsystem_is_a_model_and_it_is_infeasible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_two_rows();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_iis_side rs[2], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));

    jaos_model *sub = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis_model(m, rs, cs, &sub));
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(sub));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(sub));

    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(sub));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_col(sub));
    double lo, up;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(sub, 0, &lo, &up));
    TEST_ASSERT_TRUE(lo == -INFINITY && up == INFINITY);

    double c = 1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(sub, 0, &c));
    TEST_ASSERT_TRUE(c == 0.0);

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &up));
    TEST_ASSERT_TRUE(lo == 0.0 && up == INFINITY);

    jaos_model_free(sub);
    jaos_model_free(m);
#endif
}

static void test_the_subsystem_drops_what_is_not_a_member(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else

    const double cost[2] = {1.0, 1.0};
    const double cl[2] = {-INFINITY, -INFINITY};
    const double cu[2] = {INFINITY, INFINITY};
    const double rl[3] = {1.0, -INFINITY, -INFINITY};
    const double ru[3] = {INFINITY, 0.0, 5.0};
    const int64_t as[3] = {0, 2, 3};
    const int64_t ai[3] = {0, 1, 2};
    const double av[3] = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "lo"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "hi"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 2, "spare"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "y"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    jaos_iis_side rs[3], cs[2];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_IIS_NONE, rs[2]);

    jaos_model *sub = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis_model(m, rs, cs, &sub));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(sub));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_col(sub));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(sub, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("lo", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(sub, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("hi", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(sub, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("x", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(sub));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(sub));

    jaos_model_free(sub);
    jaos_model_free(m);
#endif
}

static void test_a_subsystem_missing_a_member_is_feasible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_two_rows();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_iis_side rs[2], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));

    const jaos_iis_side kept = rs[0];
    rs[0] = JAOS_IIS_NONE;
    jaos_model *sub = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis_model(m, rs, cs, &sub));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(sub));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(sub));
    jaos_model_free(sub);
    rs[0] = kept;
    jaos_model_free(m);
#endif
}

static void test_the_subsystem_builder_rejects_bad_arguments(void)
{
    jaos_model *m = make_two_rows();
    jaos_iis_side rs[2] = {JAOS_IIS_LOWER, JAOS_IIS_UPPER};
    jaos_iis_side cs[1] = {JAOS_IIS_NONE};
    jaos_model *sub = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_iis_model(nullptr, rs, cs, &sub));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_iis_model(m, nullptr, cs, &sub));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_iis_model(m, rs, nullptr, &sub));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_iis_model(m, rs, cs, nullptr));
    const jaos_iis_side junk[2] = {(jaos_iis_side)7, JAOS_IIS_UPPER};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_iis_model(m, junk, cs, &sub));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis_model(m, rs, cs, &sub));
    TEST_ASSERT_NOT_NULL(sub);
    jaos_model_free(sub);
    jaos_model_free(m);
}
static void test_the_subsystem_of_a_mip_carries_no_integrality(void)
{
    const double cost[1] = {1.0}, cl[1] = {0.0}, cu[1] = {3.0};
    const double rl[2] = {2.0, -INFINITY}, ru[2] = {INFINITY, 1.0};
    const int64_t as[2] = {0, 2}, ai[2] = {0, 1};
    const double av[2] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    jaos_iis_side rs[2], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_TRUE(rep.members > 0);

    jaos_model *sub = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_iis_model(m, rs, cs, &sub));
    for (int64_t j = 0; j < jaos_num_col(sub); j++) {
        bool is_int = true;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(sub, j, &is_int));
        TEST_ASSERT_FALSE(is_int);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(sub));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(sub));
    jaos_model_free(sub);
    jaos_model_free(m);
}

static void test_an_infeasible_only_by_integrality_model_says_so(void)
{
    const double cost[1] = {1.0}, cl[1] = {0.0}, cu[1] = {1.0};
    const double rl[1] = {1.0}, ru[1] = {1.0};
    const int64_t as[2] = {0, 1}, ai[1] = {0};
    const double av[1] = {2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    jaos_iis_side rs[1], cs[1];
    jaos_iis_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL, jaos_iis(m, rs, cs, &rep));
    TEST_ASSERT_NOT_NULL(
        strstr(jaos_model_error(m), "explains its INFEASIBLE"));
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
    RUN_TEST(test_the_subsystem_is_a_model_and_it_is_infeasible);
    RUN_TEST(test_the_subsystem_drops_what_is_not_a_member);
    RUN_TEST(test_a_subsystem_missing_a_member_is_feasible);
    RUN_TEST(test_the_subsystem_builder_rejects_bad_arguments);
    RUN_TEST(test_the_subsystem_of_a_mip_carries_no_integrality);
    RUN_TEST(test_an_infeasible_only_by_integrality_model_says_so);
    return UNITY_END();
}
