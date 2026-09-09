/* SPDX-License-Identifier: Apache-2.0 */
#include <float.h>

#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

static jaos_model *make_one_fixed_column(void)
{
    const double c[]  = {1.0, 5.0, 1.0};
    const double cl[] = {0.0, 2.0, 0.0}, cu[] = {10.0, 2.0, 1.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}

static void test_fixed_column_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build; see test_fixed_column_index_map_off_by_one");
#else
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    const double expected_x1 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[3], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[1]);

    jaos_model_free(m);
#endif
}

static void test_postsolved_basis_has_exactly_num_row_basic_entries(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_basis_status cs[3], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 3; j++)
        basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 1; i++)
        basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(1, basic);

    jaos_model_free(m);
#endif
}

static void test_fixed_col_counter_is_exact(void)
{
    jaos_model *m = make_one_fixed_column();

    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));

    TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_SOLVED, p.outcome);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.redundant_row);
    TEST_ASSERT_EQUAL_INT64(2, p.counts.empty_col);
    TEST_ASSERT_EQUAL_INT64(2, p.counts.rounds);

    TEST_ASSERT_EQUAL_INT64(0, p.counts.empty_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.singleton_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.forcing_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.duplicate_row);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.duplicate_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.dominated_col);

    jm_presolve_free(&p);
    jaos_model_free(m);
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_all_fixed(void)
{
    const double c[]  = {2.0, 3.0};
    const double cl[] = {1.0, 4.0}, cu[] = {1.0, 4.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_all_columns_fixed_solves_with_no_iterations(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = make_all_fixed();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = 14.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_original_arrays_survive_a_reducing_solve(void)
{
    jaos_model *m = make_one_fixed_column();

    int64_t a_start[4], a_index[3];
    double a_value[3], col_lower[3], col_upper[3];
    memcpy(a_start, m->a_start, sizeof a_start);
    memcpy(a_index, m->a_index, sizeof a_index);
    memcpy(a_value, m->a_value, sizeof a_value);
    memcpy(col_lower, m->col_lower, sizeof col_lower);
    memcpy(col_upper, m->col_upper, sizeof col_upper);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_EQUAL_MEMORY(a_start, m->a_start, sizeof a_start);
    TEST_ASSERT_EQUAL_MEMORY(a_index, m->a_index, sizeof a_index);
    TEST_ASSERT_EQUAL_MEMORY(a_value, m->a_value, sizeof a_value);
    TEST_ASSERT_EQUAL_MEMORY(col_lower, m->col_lower, sizeof col_lower);
    TEST_ASSERT_EQUAL_MEMORY(col_upper, m->col_upper, sizeof col_upper);

    jaos_model_free(m);
}

static void test_fixed_column_index_map_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_one_fixed_column();

    m->sol_col = calloc(3, sizeof(double));
    m->sol_col[1] = 2.0;
    m->sol_col_status = calloc(3, sizeof(jaos_basis_status));
    m->sol_col_status[1] = JAOS_BASIS_AT_LOWER;
    m->sol_redcost = calloc(3, sizeof(double));
    m->sol_redcost[1] = 5.0;
    m->sol_row = calloc(1, sizeof(double));
    m->sol_dual = calloc(1, sizeof(double));
    m->sol_row_status = calloc(1, sizeof(jaos_basis_status));
    TEST_ASSERT_NOT_NULL(m->sol_col);
    TEST_ASSERT_NOT_NULL(m->sol_col_status);
    TEST_ASSERT_NOT_NULL(m->sol_redcost);
    TEST_ASSERT_NOT_NULL(m->sol_row);
    TEST_ASSERT_NOT_NULL(m->sol_dual);
    TEST_ASSERT_NOT_NULL(m->sol_row_status);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_FALSE(r.primal_feasible);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_col_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_distinctly_valued_reduction(void)
{
    const double c[]  = {1.0, 100.0, 2.0, 3.0};
    const double cl[] = {0.0, 5.0, 0.0, 0.0};
    const double cu[] = {10.0, 5.0, 10.0, 10.0};
    const double rl[] = {1.0, 2.0}, ru[] = {INFINITY, INFINITY};

    const int64_t s[]  = {0, 1, 3, 4, 5};
    const int64_t ix[] = {0,   0, 1,   1,   0};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    return m;
}
#endif

static void test_original_index_invariant_across_all_six_arrays(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault-injection "
                        "build");
#else
    jaos_model *m = make_distinctly_valued_reduction();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 500.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], row_act[2], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, row_act, y, dj));

    const double expected_x[4]       = {0.0, 5.0, 0.0, 0.0};
    const double expected_row_act[2] = {5.0, 5.0};
    const double expected_y[2]       = {0.0, 0.0};
    const double expected_dj[4]      = {1.0, 100.0, 2.0, 3.0};
    TEST_ASSERT_EQUAL_MEMORY(expected_x, x, sizeof x);
    TEST_ASSERT_EQUAL_MEMORY(expected_row_act, row_act, sizeof row_act);
    TEST_ASSERT_EQUAL_MEMORY(expected_y, y, sizeof y);
    TEST_ASSERT_EQUAL_MEMORY(expected_dj, dj, sizeof dj);

    jaos_basis_status cs[4], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    const jaos_basis_status expected_cs[4] = {
        JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER,
        JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER,
    };
    const jaos_basis_status expected_rs[2] = {
        JAOS_BASIS_BASIC, JAOS_BASIS_BASIC,
    };
    TEST_ASSERT_EQUAL_MEMORY(expected_cs, cs, sizeof cs);
    TEST_ASSERT_EQUAL_MEMORY(expected_rs, rs, sizeof rs);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

#if defined(JAOS_NO_PRESOLVE)

constexpr int64_t PRESOLVE_MODEL_WORK_PINNED = 8206;
#else

constexpr int64_t PRESOLVE_MODEL_WORK_PINNED = 3;
#endif

static void test_presolve_bills_the_work_counter(void)
{
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(PRESOLVE_MODEL_WORK_PINNED, jaos_work_units(m));
    jaos_model_free(m);
}

static void test_presolve_work_is_deterministic_across_re_solves(void)
{
    jaos_model *m = make_one_fixed_column();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const int64_t first = jaos_work_units(m);

    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(first, jaos_work_units(m));

    jaos_model_free(m);
}

#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_empty_row_model(void)
{
    const double c[]  = {2.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {-1.0, 1.0, -INFINITY};
    const double ru[] = {1.0, INFINITY, 100.0};

    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {1, 2, 1, 2};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_empty_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_empty_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 2.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    const double expected_y0 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[0]);
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 3; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(3, basic);

    jaos_model_free(m);
#endif
}

static void test_empty_row_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_empty_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_empty_col_model(void)
{
    const double c[]  = {1.0, 1.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, 5.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};

    const int64_t s[]  = {0, 1, 2, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_empty_col_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_empty_col_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = -4.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[1], dj[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));

    const double expected_x2 = 5.0;
    const double expected_dj2 = -1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x2, &x[2], sizeof x[2]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj2, &dj[2], sizeof dj[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_empty_col_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    const double c[]  = {1.0, 1.0, -1.0, 0.5};
    const double cl[] = {0.0, 0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 5.0, 3.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};

    const int64_t s[]  = {0, 1, 2, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[4], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, r.max_col_violation);

    jaos_model_free(m);
#endif
}

static void test_empty_col_reports_unbounded(void)
{
    const double c[]  = {1.0, 1.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, INFINITY};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    jaos_model_free(m);
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_singleton_row_model(void)
{
    const double c[] = {2.0};
    const double cl[] = {0.0}, cu[] = {5.0};
    const double rl[] = {-10.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1};
    const int64_t ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}
#endif

static void test_singleton_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = 0.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[1], y[1], dj[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x0 = 0.0, expected_y0 = 0.0, expected_dj0 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj0, &dj[0], sizeof dj[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_singleton_row_wrong_dual(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_WRONGDUAL");
#else
    jaos_model *m = make_singleton_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[1], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_singleton_col_model(void)
{
    const double c[] = {1.0, 0.0};
    const double cl[] = {0.0, 1.0}, cu[] = {20.0, 3.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};

    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_singleton_col_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_col_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 0.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x0 = 0.0, expected_x1 = 1.0;
    const double expected_y0 = 0.0, expected_dj1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj1, &dj[1], sizeof dj[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_singleton_col_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    const double c[]  = {1.0, 0.0, 1.0};
    const double cl[] = {0.0, 1.0, 5.0}, cu[] = {20.0, 3.0, 10.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};

    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, r.max_col_violation);

    jaos_model_free(m);
#endif
}

static void test_singleton_col_after_fixed_col(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {2.0, 0.0, 1.0, 3.0};
    const double cl[] = {4.0, 0.0, 0.0, 0.0}, cu[] = {4.0, 100.0, 5.0, 5.0};
    const double rl[] = {10.0, 1.0}, ru[] = {10.0, INFINITY};

    const int64_t s[]  = {0, 1, 2, 3, 4};
    const int64_t ix[] = {0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 9.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 6.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_singleton_col_between_two_removals_solved_path(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {7.0, 0.0, 5.0, 1.0};
    const double cl[] = {4.0, 0.0, 0.0, 2.0}, cu[] = {4.0, 100.0, 10.0, 5.0};
    const double rl[] = {20.0, 3.0}, ru[] = {20.0, 3.0};

    const int64_t s[]  = {0, 1, 2, 4, 4};
    const int64_t ix[] = {0, 0, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 45.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 13.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_basis_status cs[4], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 4; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    for (int64_t i = 0; i < 2; i++) basic += (rs[i] == JAOS_BASIS_BASIC);

    TEST_ASSERT_EQUAL_INT64(2, basic);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_an_exact_tie_the_division_rounds_inward_publishes_the_bound(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 0.7};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double absorbed = 3.0 * 0.7;
    const double rest = 3.0 - absorbed;
    TEST_ASSERT_TRUE((3.0 - rest) / 3.0 < 0.7);

    double x[2], act[1], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));
    const double expected_x1 = 0.7;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, cs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[0]);
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    basic += (rs[0] == JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT64(1, basic);

    jaos_model_free(m);
#endif
}

static void test_an_interior_recovery_takes_the_row_out_whatever_the_ulps_say(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.1, 0.0}, cu[] = {10.0, 5.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_TRUE(0.1 + 3.0 * ((1.0 - 0.1) / 3.0) != 1.0);

    double x[2], act[1], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));
    const double expected_x0 = 0.1;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, act[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[0]);
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    basic += (rs[0] == JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT64(1, basic);

    jaos_model_free(m);
#endif
}

static void test_a_column_a_row_fixed_inside_its_box_is_basic(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 1.0};
    const double rl[] = {4.0}, ru[] = {4.0};
    const int64_t s[]  = {0, 1, 1};
    const int64_t ix[] = {0};
    const double v[]   = {2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x0 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_TRUE(rs[0] != JAOS_BASIS_BASIC);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, cs[1]);

    const double zero = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&zero, &dj[0], sizeof dj[0]);

    jaos_model_free(m);
#endif
}

static void test_the_basis_count_promise_breaks_on_a_declined_column(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {20.0, 100.0};
    const double rl[] = {7.0}, ru[] = {7.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));

    const double expected_x0 = 0.0, expected_x1 = 7.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += (cs[j] == JAOS_BASIS_BASIC);
    basic += (rs[0] == JAOS_BASIS_BASIC);

    TEST_ASSERT_EQUAL_INT64(1, basic);

    jaos_model *m2 = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m2));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m2, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m2, &p, nullptr));
    TEST_ASSERT_EQUAL_INT64(1, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.implied_free_col);
    jm_presolve_free(&p);
    jaos_model_free(m2);

    jaos_model_free(m);
#endif
}

static void test_singleton_col_open_below_publishes_a_finite_point(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double rus[2] = {10.0, INFINITY};
    for (int shape = 0; shape < 2; shape++) {
        const double c[]  = {0.0, 0.0};
        const double cl[] = {-INFINITY, -INFINITY}, cu[] = {INFINITY, 1.0};
        const double rl[] = {-INFINITY}, ru[] = {rus[shape]};
        const int64_t s[]  = {0, 1, 2};
        const int64_t ix[] = {0, 0};
        const double v[]   = {1.0, 1.0};
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         2, s, ix, v));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

        TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

        double x[2], act[1], y[1], dj[2];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));
        TEST_ASSERT_TRUE_MESSAGE(isfinite(x[0]) && isfinite(x[1]),
                                 "a published value is not finite");
        TEST_ASSERT_TRUE(isfinite(act[0]));
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, x[0] + x[1], act[0]);
        TEST_ASSERT_TRUE(x[1] <= 1.0);
        TEST_ASSERT_TRUE(act[0] <= rus[shape]);

        jaos_check_report r;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
        TEST_ASSERT_TRUE(r.primal_feasible);
        TEST_ASSERT_TRUE(r.dual_feasible);

        jaos_basis_status cs[2], rs[1];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
        const int basics = (cs[0] == JAOS_BASIS_BASIC) +
                           (cs[1] == JAOS_BASIS_BASIC) +
                           (rs[0] == JAOS_BASIS_BASIC);
        TEST_ASSERT_EQUAL_INT(1, basics);
        jaos_model_free(m);
    }
#endif
}

static void test_two_singleton_cols_on_one_row(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {3.0, 0.0, 0.0, 1.0, 2.0};
    const double cl[] = {4.0, 0.0, 0.0, 0.0, 0.0};
    const double cu[] = {4.0, 100.0, 3.0, 5.0, 5.0};
    const double rl[] = {10.0, 1.0}, ru[] = {10.0, INFINITY};

    const int64_t s[]  = {0, 1, 2, 3, 4, 5};
    const int64_t ix[] = {0, 0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 13.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[5], y[2], dj[5];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 6.0, expected_x2 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x2, &x[2], sizeof x[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    jaos_model_free(m);

    jaos_model *m2 = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m2));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m2, 5, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m2, &p, nullptr));
    TEST_ASSERT_EQUAL_INT64(2, p.counts.singleton_col);
    jm_presolve_free(&p);
    jaos_model_free(m2);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_free_col_singleton_model(void)
{
    const double c[]  = {2.0, 0.0, 3.0, 4.0};
    const double cl[] = {3.0, -INFINITY, 0.0, 0.0};
    const double cu[] = {3.0, INFINITY, 10.0, 10.0};
    const double rl[] = {5.0, 1.0}, ru[] = {5.0, INFINITY};

    const int64_t s[]  = {0, 1, 2, 3, 4};
    const int64_t ix[] = {0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_free_col_singleton_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_free_col_singleton_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 9.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2], dj[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double expected_x1 = 2.0, expected_y0 = 0.0, expected_dj1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_dj1, &dj[1], sizeof dj[1]);

    jaos_basis_status cs[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_free_col_singleton_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_free_col_singleton_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[4], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

static jaos_model *make_implied_free_col_model(double x0_lower)
{
    const double c[]  = {3.0, 1.0, 1.0};
    const double cl[] = {x0_lower, 0.0, 0.0};
    const double cu[] = {100.0, 2.0, 2.0};
    const double rl[] = {6.0, 1.0}, ru[] = {6.0, INFINITY};

    const int64_t s[]  = {0, 1, 3, 5};
    const int64_t ix[] = {0,   0, 1,   0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    return m;
}

static void test_an_implied_free_column_is_substituted_out(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_implied_free_col_model(-100.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[2], dj[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    const double ex0 = 2.0, ex1 = 2.0, ex2 = 2.0;
    const double ey0 = 3.0, ey1 = 0.0, ed0 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&ex0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&ex1, &x[1], sizeof x[1]);
    TEST_ASSERT_EQUAL_MEMORY(&ex2, &x[2], sizeof x[2]);
    TEST_ASSERT_EQUAL_MEMORY(&ey0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&ey1, &y[1], sizeof y[1]);

    TEST_ASSERT_EQUAL_MEMORY(&ed0, &dj[0], sizeof dj[0]);

    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);

    int64_t nbasic = 0;
    for (int64_t k = 0; k < 3; k++)
        if (cs[k] == JAOS_BASIS_BASIC) nbasic++;
    for (int64_t k = 0; k < 2; k++)
        if (rs[k] == JAOS_BASIS_BASIC) nbasic++;
    TEST_ASSERT_EQUAL_INT64(2, nbasic);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_an_implied_bound_outside_the_box_is_refused(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_implied_free_col_model(4.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 14.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double ex0 = 4.0;
    TEST_ASSERT_EQUAL_MEMORY(&ex0, &x[0], sizeof x[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_a_short_mapped_basis_is_repaired_and_warm_survives(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("the mapping under test does not exist without presolve");
#else

    const double c[]  = {0.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {10.0, 10.0, 10.0};
    const double rl[] = {5.0, 3.0}, ru[] = {8.0, INFINITY};
    const int64_t s[]  = {0, 1, 3, 5};
    const int64_t ix[] = {0,   0, 1,   0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const double obj1 = 3.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&obj1, &obj, sizeof obj);

    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    int64_t nb = 0;
    for (int64_t k = 0; k < 3; k++) nb += cs[k] == JAOS_BASIS_BASIC;
    for (int64_t k = 0; k < 2; k++) nb += rs[k] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, nb);

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_row_bounds(m, 1, 4.0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const double obj2 = 4.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&obj2, &obj, sizeof obj);

    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && \
    !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL) && !defined(JAOS_NO_PRESOLVE)
static int g_warm_repairs;

static void count_warm_repair(void *user, jaos_log_level level,
                              const char *line)
{
    (void)user;
    (void)level;
    if (strstr(line, "arrived short and was repaired") != nullptr)
        g_warm_repairs++;
}

static int repair_fires_at(int k)
{
    const int64_t nrow = 2 * k, ncol = 3 * k, nnz = 5 * k;
    double *c = calloc((size_t)ncol, sizeof *c);
    double *cl = calloc((size_t)ncol, sizeof *cl);
    double *cu = calloc((size_t)ncol, sizeof *cu);
    double *rl = calloc((size_t)nrow, sizeof *rl);
    double *ru = calloc((size_t)nrow, sizeof *ru);
    int64_t *st = calloc((size_t)ncol + 1, sizeof *st);
    int64_t *ix = calloc((size_t)nnz, sizeof *ix);
    double *va = calloc((size_t)nnz, sizeof *va);

    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu);
    TEST_ASSERT_NOT_NULL(rl);
    TEST_ASSERT_NOT_NULL(ru);
    TEST_ASSERT_NOT_NULL(st);
    TEST_ASSERT_NOT_NULL(ix);
    TEST_ASSERT_NOT_NULL(va);

    int64_t nz = 0;
    for (int64_t b = 0; b < k; b++) {
        const int64_t r0 = 2 * b, r1 = 2 * b + 1, j0 = 3 * b;
        c[j0] = 0.0;     cl[j0] = 0.0;     cu[j0] = 10.0;
        c[j0 + 1] = 1.0; cl[j0 + 1] = 0.0; cu[j0 + 1] = 10.0;
        c[j0 + 2] = 1.0; cl[j0 + 2] = 0.0; cu[j0 + 2] = 10.0;
        rl[r0] = 5.0; ru[r0] = 8.0;
        rl[r1] = 3.0; ru[r1] = INFINITY;
        ix[nz] = r0; va[nz] = 1.0; nz++;
        st[j0 + 1] = nz;
        ix[nz] = r0; va[nz] = 1.0; nz++;
        ix[nz] = r1; va[nz] = 1.0; nz++;
        st[j0 + 2] = nz;
        ix[nz] = r0; va[nz] = 1.0; nz++;
        ix[nz] = r1; va[nz] = 1.0; nz++;
        st[j0 + 3] = nz;
    }

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ncol, nrow, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, st, ix, va));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_basis_status *cs = calloc((size_t)ncol, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nrow, sizeof *rs);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t nb = 0, singles = 0;
    for (int64_t j = 0; j < ncol; j++) nb += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < nrow; i++) nb += rs[i] == JAOS_BASIS_BASIC;
    for (int64_t b = 0; b < k; b++) singles += cs[3 * b] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(nrow, nb);
    TEST_ASSERT_EQUAL_INT64(k, singles);

    g_warm_repairs = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_log_callback(m, count_warm_repair, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 1, 4.0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const int fired = g_warm_repairs;
    jaos_model_free(m);
    free(c); free(cl); free(cu); free(rl); free(ru);
    free(st); free(ix); free(va); free(cs); free(rs);
    return fired;
}
#endif

static void test_the_warm_repair_stops_at_its_cap(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("the mapping under test does not exist without presolve");
#else

    TEST_ASSERT_TRUE(repair_fires_at(1));
    TEST_ASSERT_TRUE(repair_fires_at(4));

    TEST_ASSERT_FALSE(repair_fires_at(5));
    TEST_ASSERT_FALSE(repair_fires_at(6));
#endif
}

static void test_a_long_mapped_basis_falls_back_cold(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("the mapping under test does not exist without presolve");
#else

    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {-INFINITY, 3.0}, ru[] = {100.0, INFINITY};
    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {0, 1,   0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    const jaos_basis_status cs[] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    const jaos_basis_status rs[] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_AT_LOWER};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const double eobj = 3.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&eobj, &obj, sizeof obj);

    TEST_ASSERT_EQUAL_INT64(1, m->presolve_num_row);

    TEST_ASSERT_EQUAL_INT64(1, jaos_iterations(m));

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_NO_PRESOLVE)

static void test_the_implied_free_counter_reads_its_three_models(void)
{
    const double lowers[3] = {-100.0, 4.0, 2.0};
    const int64_t expect[3] = {1, 0, 0};
    for (int t = 0; t < 3; t++) {
        jaos_model *m = make_implied_free_col_model(lowers[t]);
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT64(expect[t], p.counts.implied_free_col);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
}
#endif

#if !defined(JAOS_NO_PRESOLVE)
static void test_a_range_row_that_shifted_into_an_equality_is_declined(void)
{
    const double c[]  = {0.0, 1.0, 0.0, 0.0};
    const double cl[] = {1.0, -1e18, 0.0, 0.0};
    const double cu[] = {1.0, 1e18, 1.0, 1.0};
    const double rl[] = {1.0, 0.0}, ru[] = {2.0, INFINITY};

    const int64_t s[]  = {0, 1, 2, 4, 5};
    const int64_t ix[] = {0,   0,   0, 1,   1};
    const double v[]   = {1e17, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));

    TEST_ASSERT_EQUAL_DOUBLE(1.0 - 1e17, 2.0 - 1e17);

    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);
    TEST_ASSERT_EQUAL_INT64(0, p.counts.implied_free_col);
    jm_presolve_free(&p);
    jaos_model_free(m);
}
#endif

static void test_an_implied_bound_at_exact_equality_is_declined(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_implied_free_col_model(2.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double ex0 = 2.0;
    TEST_ASSERT_EQUAL_MEMORY(&ex0, &x[0], sizeof x[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_a_removed_column_pays_every_row_it_touches(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0, 0.0, 1.0};
    const double cl[] = {-INFINITY, 0.0, 0.0};
    const double cu[] = {INFINITY, 1.0, 0.5};
    const double rl[] = {10.0, 1.0}, ru[] = {10.0, INFINITY};

    const int64_t s[]  = {0, 1, 3, 4};
    const int64_t ix[] = {0,   0, 1,   1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], row[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, row, y, nullptr));

    const double erow0 = 10.0;
    TEST_ASSERT_EQUAL_MEMORY(&erow0, &row[0], sizeof row[0]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, x[0] + x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_implied_free_col_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_implied_free_col_model(-100.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

static void test_empty_row_reports_infeasible(void)
{
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {1.0}, ru[] = {2.0};
    const int64_t s[] = {0, 0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     0, s, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    jaos_basis_status cs[1], rs[1];
#if defined(JAOS_NO_PRESOLVE)
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[0]);
#else
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
#endif

    jaos_model_free(m);
}

static void test_zero_row_model_solves(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build; "
                        "only one column exists for the offset to land on, "
                        "so the fault would read out of bounds rather than "
                        "corrupt a real slot");
#else
    const double c[]  = {1.0, -1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 5.0};
    const int64_t s[] = {0, 0, 0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 0, JAOS_MINIMIZE, 0.0, c, cl, cu, nullptr, nullptr,
                     0, s, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = -5.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);

    jaos_basis_status cs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(0, basic);

    jaos_model_free(m);
#endif
}

static void test_zero_col_model_solves(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build; "
                        "only two rows exist, the second offset would read "
                        "out of bounds rather than corrupt a real slot");
#else
    const double rl[] = {-1.0, 0.0}, ru[] = {1.0, 0.0};
    const int64_t s[] = {0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 0, 2, JAOS_MINIMIZE, 0.0, nullptr, nullptr, nullptr,
                     rl, ru, 0, s, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    const double expected_obj = 0.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, nullptr, nullptr, y, nullptr));
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, nullptr, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, rs));
    int64_t basic = 0;
    for (int64_t i = 0; i < 2; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, basic);

    jaos_model_free(m);
#endif
}

static void test_all_five_counters_move_independently(void)
{
    const double c[]  = {1.0, 1.0, -1.0, 2.0, 0.0, 5.0, 0.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, -INFINITY};
    const double cu[] = {10.0, 10.0, 5.0, 5.0, 10.0, 1.0, INFINITY};
    const double rl[] = {-1.0, 1.0, -INFINITY, -10.0, -INFINITY, 2.0};
    const double ru[] = {1.0, INFINITY, 100.0, INFINITY, 10.0, 2.0};

    const int64_t s[]  = {0, 3, 5, 5, 7, 8, 9, 10};
    const int64_t ix[] = {1, 2, 4,   1, 2,   2, 3,   4,   5,   5};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 7, 6, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     10, s, ix, v));

    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));

    TEST_ASSERT_EQUAL_INT64(1, p.counts.empty_row);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.singleton_row);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.singleton_col);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.free_col_singleton);
    TEST_ASSERT_EQUAL_INT64(1, p.counts.fixed_col);

    TEST_ASSERT_EQUAL_INT64(1, p.counts.redundant_row);
    TEST_ASSERT_EQUAL_INT64(2, p.counts.empty_col);

    jm_presolve_free(&p);
    jaos_model_free(m);
}

#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_forcing_row_model(void)
{
    const double c[]  = {-1.0, -1.0, 1.0, 3.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 10.0, 10.0};
    const double rl[] = {-INFINITY, 1.0};
    const double ru[] = {0.0, INFINITY};

    const int64_t s[]  = {0, 1, 2, 3, 4};
    const int64_t ix[] = {0, 0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_forcing_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_forcing_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 1.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[4], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    const double expected_x0 = 0.0, expected_x1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    const double expected_y0 = -1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[4], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[0]);
    TEST_ASSERT_TRUE(cs[0] == JAOS_BASIS_BASIC || cs[1] == JAOS_BASIS_BASIC);
    int64_t basic = 0;
    for (int64_t j = 0; j < 4; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 2; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, basic);

    jaos_model_free(m);
#endif
}

static void test_forcing_row_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_forcing_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[4], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.primal_feasible);

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_redundant_row_model(void)
{
    const double c[]  = {1.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {1.0, -INFINITY};
    const double ru[] = {INFINITY, 100.0};

    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {0, 1, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_redundant_row_round_trip(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_redundant_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 1.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    const double expected_y1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y1, &y[1], sizeof y[1]);

    double rowact[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, rowact, y, nullptr));
    const double expected_act1 = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_act1, &rowact[1], sizeof rowact[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_basis_status cs[2], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
#if !defined(JAOS_NO_PRESOLVE)

    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[1]);
#endif
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < 2; i++) basic += rs[i] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(2, basic);

    jaos_model_free(m);
#endif
}

static void test_redundant_row_index_off_by_one(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE");
#else
    jaos_model *m = make_redundant_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_singleton_fold_boundary_model(double b)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 10.0};
    const double rl[] = {b, 6.0};
    const double ru[] = {INFINITY, INFINITY};

    const int64_t s[]  = {0, 2, 3};
    const int64_t ix[] = {0, 1, 1};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}
#endif

static void test_a_fold_onto_the_opposite_bound_fixes_the_column(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_fold_boundary_model(5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 6.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double expected_x0 = 5.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_a_fold_one_step_past_the_opposite_bound_is_infeasible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_fold_boundary_model(5.0 + 1e-6);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

static void test_a_fold_inside_the_epsilon_does_not_flip_the_verdict(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_singleton_fold_boundary_model(nextafter(5.0, 6.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, obj);
    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_tightening_boundary_model(void)
{
    const double c[]  = {1.0, 1.0, 1.0};
    const double cl[] = {-100.0, 0.0, 0.0}, cu[] = {100.0, 3.0, 3.0};
    const double rl[] = {10.0, -INFINITY};
    const double ru[] = {INFINITY, 7.0};

    const int64_t s[]  = {0, 2, 3, 4};
    const int64_t ix[] = {0, 1, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_an_optimum_on_the_tightening_boundary_survives(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_tightening_boundary_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, obj);

    double x[3], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_activity_range_counters_are_exact(void)
{
#if defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("model builders are compiled out under this build");
#else
    {
        jaos_model *m = make_forcing_row_model();
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT64(1, p.counts.forcing_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.redundant_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    {
        jaos_model *m = make_redundant_row_model();
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT64(1, p.counts.redundant_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.forcing_row);
        TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    {
        jaos_model *m = make_tightening_boundary_model();
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));

        TEST_ASSERT_EQUAL_INT64(0, p.counts.tightened_bound);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

static jaos_model *make_cascading_chain(int64_t n)
{
    double *c = calloc((size_t)n, sizeof *c);
    double *cl = calloc((size_t)n, sizeof *cl);
    double *cu = calloc((size_t)n, sizeof *cu);
    double *rl = calloc((size_t)n, sizeof *rl);
    double *ru = calloc((size_t)n, sizeof *ru);
    int64_t *s = calloc((size_t)n + 1, sizeof *s);
    int64_t *ix = calloc(2 * (size_t)n, sizeof *ix);
    double *v = calloc(2 * (size_t)n, sizeof *v);
    TEST_ASSERT_NOT_NULL(c); TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu); TEST_ASSERT_NOT_NULL(rl);
    TEST_ASSERT_NOT_NULL(ru); TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(ix); TEST_ASSERT_NOT_NULL(v);

    int64_t nz = 0;
    for (int64_t j = 0; j < n; j++) {
        c[j] = 1.0;
        cl[j] = 0.0;
        cu[j] = 100.0;
        s[j] = nz;
        ix[nz] = j;   v[nz] = 1.0;   nz++;
        if (j < n - 1) { ix[nz] = j + 1; v[nz] = -1.0; nz++; }
    }
    s[n] = nz;
    rl[0] = 1.0; ru[0] = 1.0;
    for (int64_t i = 1; i < n; i++) { rl[i] = 0.0; ru[i] = 0.0; }

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, n, n, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, s, ix, v));
    free(c); free(cl); free(cu); free(rl); free(ru);
    free(s); free(ix); free(v);
    return m;
}

static void test_the_round_cap_is_the_one_its_sweep_set(void)
{
    jaos_model *m = make_cascading_chain(40);
    jm_presolve p;
    jm_presolve_init(&p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));

    TEST_ASSERT_EQUAL_INT64(16, p.counts.rounds);
    TEST_ASSERT_EQUAL_INT64(16, p.counts.fixed_col);
    TEST_ASSERT_EQUAL_INT64(16, p.counts.singleton_row);
    jm_presolve_free(&p);
    jaos_model_free(m);
}

static void test_the_fold_window_is_rounding_and_nothing_more(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("model builder is compiled out under either fault "
                        "build");
#else
    {
        jaos_model *m = make_singleton_fold_boundary_model(5.0 + 1.6e-14);
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
    {
        jaos_model *m = make_singleton_fold_boundary_model(5.0 + 5e-15);
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);

        TEST_ASSERT_EQUAL_INT64(2, p.counts.singleton_row);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
static jaos_model *make_two_folds_one_column_model(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 10.0, 100.0};
    const int64_t s[] = {0, 3, 4};
    const int64_t ix[] = {0, 1, 2, 2};
    const double v[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}
#endif

static void test_two_folds_the_owning_row_takes_the_multiplier(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_two_folds_one_column_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[3], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));

    const double expected_x0 = 2.0, expected_x1 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    const double expected_y0 = 1.0, expected_y1 = 0.0, expected_y2 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y1, &y[1], sizeof y[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_y2, &y[2], sizeof y[2]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static void test_two_folds_wrong_dual(void)
{
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("negative test — runs only under "
                        "EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_WRONGDUAL");
#else
    jaos_model *m = make_two_folds_one_column_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, r.max_dual_violation);

    jaos_model_free(m);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_frozen_row_infeasible_model(void)
{
    const double c[]  = {1.0, 0.0};
    const double cl[] = {4.0, 0.0};
    const double cu[] = {4.0, 3.0};
    const double rl[] = {100.0};
    const double ru[] = {100.0};
    const int64_t s[] = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}
#endif

static void test_a_frozen_row_that_cannot_be_satisfied_is_infeasible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_frozen_row_infeasible_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

static void test_the_frozen_row_model_agrees_with_the_reference_build(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#elif !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("reference half — runs under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    jaos_model *m = make_frozen_row_infeasible_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

static void test_a_frozen_row_that_is_exactly_satisfiable_is_not_refused(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {4.0, 0.0};
    const double cu[] = {4.0, 96.0};
    const double rl[] = {100.0};
    const double ru[] = {100.0};
    const int64_t s[] = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    const double expected_x0 = 4.0, expected_x1 = 96.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x0, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static jaos_model *make_emptied_row_at_scale(double gap)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1.0, 1.0}, cu[] = {1.0, 1.0};
    const double rl[] = {2e9 + gap}, ru[] = {2e9 + gap};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1e9, 1e9};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

static void test_an_emptied_row_missed_by_more_than_rounding_is_refused(void)
{

    jaos_model *m = make_emptied_row_at_scale(1.5);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_an_emptied_row_missed_by_rounding_alone_is_kept(void)
{

    jaos_model *m = make_emptied_row_at_scale(1e-6);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static jaos_model *make_fold_past_the_box_at_scale(double rl0)
{
    const double c[]  = {1.0};
    const double cl[] = {0.0}, cu[] = {1e9};
    const double rl[] = {rl0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

static void test_a_fold_past_the_box_at_scale_is_refused(void)
{

    jaos_model *m = make_fold_past_the_box_at_scale(1e9 + 0.4);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_fold_onto_the_box_at_scale_still_collapses(void)
{

    jaos_model *m = make_fold_past_the_box_at_scale(1e9 + 5e-7);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

#if defined(JAOS_NO_PRESOLVE)

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
    return;
#endif

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));

    TEST_ASSERT_TRUE(x[0] >= 0.0);
    TEST_ASSERT_TRUE(x[0] <= 1e9);

    const double window = 8.0 * DBL_EPSILON * 1e9;
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.max_col_violation);
    TEST_ASSERT_TRUE(rep.max_row_violation <= window);

    jaos_model_free(m);
}

static jaos_model *make_frozen_traffic_model(double g)
{
    const double c[]  = {0.0, 1.0, 0.0};
    const double cl[] = {1.0, g,    0.0};
    const double cu[] = {1.0, 10.0, 1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}

static void test_a_frozen_row_is_not_refused_below_its_own_traffic(void)
{
    jaos_model *m = make_frozen_traffic_model(1e-10);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_frozen_row_emptied_and_still_short_is_refused(void)
{
    const double c[]  = {0.0, 0.0};
    const double cl[] = {1.0, 0.0};
    const double cu[] = {1.0, 3.0};
    const double rl[] = {1e9 + 100.0};
    const double ru[] = {1e9 + 100.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1e9, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_the_activity_pass_is_not_refused_below_its_own_traffic(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0, 1.0, 1.0};
    const double cl[] = {1.0, 1e-10, 0.0};
    const double cu[] = {1.0, 10.0,  1.0};
    const double rl[] = {-INFINITY};
    const double ru[] = {1e9};
    const int64_t s[]  = {0, 1, 2, 3};
    const int64_t ix[] = {0, 0, 0};
    const double v[]   = {1e9, 1.0, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0, x[3] = {0.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    TEST_ASSERT_EQUAL_DOUBLE(1e-10, obj);
    TEST_ASSERT_EQUAL_DOUBLE(1.0,   x[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1e-10, x[1]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0,   x[2]);
    jaos_model_free(m);
#endif
}

static void test_a_frozen_rows_window_ignores_the_far_bound(void)
{
    for (int k = 0; k < 2; k++) {
        const double c[]  = {0.0, 0.0};
        const double cl[] = {1e-4, 1e-4};
        const double cu[] = {1.0, 1.0};
        const double rl[] = {k == 0 ? -1e12 : -INFINITY};
        const double ru[] = {0.0};
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
        jaos_model_free(m);
    }
}

static void test_the_activity_pass_still_refuses_a_real_shortfall(void)
{
    {
        const double c[]  = {1.0, 1.0};
        const double cl[] = {1e-3, 0.0};
        const double cu[] = {1.0,  1.0};
        const double rl[] = {-1e12};
        const double ru[] = {0.0};
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
        jaos_model_free(m);
    }
    {
        const double c[]  = {0.0, 1.0, 1.0};
        const double cl[] = {1.0, 1.0,  0.0};
        const double cu[] = {1.0, 10.0, 1.0};
        const double rl[] = {-INFINITY};
        const double ru[] = {1e9};
        const int64_t s[]  = {0, 1, 2, 3};
        const int64_t ix[] = {0, 0, 0};
        const double v[]   = {1e9, 1.0, 1.0};
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         3, s, ix, v));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_model_free(m);
    }
}

static void test_the_window_counts_the_shifts_and_not_only_their_scale(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    enum { KS = 256, NC = KS + 5, NNZ = KS + 6 };
    static double c[NC], cl[NC], cu[NC], av[NNZ];
    static int64_t as[NC + 1], ai[NNZ];
    const double t = ldexp(1.0, -25);

    for (int half = 0; half < 2; half++) {
        const double T = (double)KS * t + 1e-7 + (half == 0 ? 0.0 : 1e-2);
        const double rl[] = {T, -1e9}, ru[] = {T, -1e9};
        int64_t nz = 0;

        for (int64_t j = 0; j < NC; j++) {
            as[j] = nz;
            c[j] = 0.0;
            if (j == 0) {
                cl[j] = cu[j] = 1e9;
                ai[nz] = 0; av[nz++] = 1.0;
            } else if (j == 1) {
                cl[j] = -1e9 - 1.0; cu[j] = -1e9 + 1.0;
                ai[nz] = 0; av[nz++] = 1.0;
                ai[nz] = 1; av[nz++] = 1.0;
            } else if (j < KS + 2) {
                cl[j] = cu[j] = t;
                ai[nz] = 0; av[nz++] = 1.0;
            } else if (j < KS + 4) {
                cl[j] = 0.0; cu[j] = 2e-7; c[j] = 1.0;
                ai[nz] = 0; av[nz++] = 1.0;
            } else {
                cl[j] = cu[j] = 0.0;
                ai[nz] = 1; av[nz++] = 1.0;
            }
        }
        as[NC] = nz;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         nz, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

static void test_the_shift_count_scales_by_the_end_it_is_testing(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    enum { KS = 256, NC = KS + 3 };
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double small = ldexp(1.0, -25);
    const double lost  = ldexp(1.0, -17);
    const double wcap  = ldexp(1.0, -23);

    for (int half = 0; half < 2; half++) {
        const double rl[] = {1e9}, ru[] = {1e9};
        for (int64_t j = 0; j < NC; j++) {
            as[j] = j; ai[j] = 0; av[j] = 1.0; c[j] = 0.0;
            if (j == 0) {
                c[j] = 1.0; cl[j] = 0.0;
                cu[j] = 1e9 - lost - (half == 0 ? 0.0 : 2e-4);
            } else if (j < KS + 1) {
                cl[j] = cu[j] = small;
            } else {
                c[j] = 1.0; cl[j] = 0.0; cu[j] = wcap;
            }
        }
        as[NC] = NC;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         NC, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);
        jaos_model_free(m);
    }
#endif
}

static void test_the_singleton_fold_counts_the_shifts_too(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    enum { KS = 256, NC = KS + 1 };
    static double c[NC], cl[NC], cu[NC], av[NC];
    static int64_t as[NC + 1], ai[NC];
    const double small = ldexp(1.0, -25);
    const double lost  = ldexp(1.0, -17);

    for (int half = 0; half < 2; half++) {
        const double rl[] = {1e9}, ru[] = {1e9};
        for (int64_t j = 0; j < NC; j++) {
            as[j] = j; ai[j] = 0; av[j] = 1.0; c[j] = 0.0;
            if (j == 0) {
                c[j] = 1.0; cl[j] = 0.0;
                cu[j] = 1e9 - lost - (half == 0 ? 0.0 : 1e-3);
            } else {
                cl[j] = cu[j] = small;
            }
        }
        as[NC] = NC;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         NC, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));
        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(half == 0 ? JAOS_SOLVE_OPTIMAL
                                        : JAOS_SOLVE_INFEASIBLE,
                              jaos_status_of(m));
        jaos_model_free(m);
    }
#endif
}

static void test_a_folds_value_carries_its_rows_error_into_the_next(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("pinned change-detector — skipped under either fault "
                        "build");
#else
    enum { KS = 256, NC = KS + 3, NNZ = KS + 4 };
    static double c[NC], cl[NC], cu[NC], av[NNZ];
    static int64_t as[NC + 1], ai[NNZ];
    const double small = ldexp(1.0, -25);
    const double wcap  = ldexp(1.0, -23);

    for (int half = 0; half < 2; half++) {
        const double target = 1e9 - 63.0 * wcap + (half == 0 ? 0.0 : 1e-3);
        const double rl[] = {1e9, target}, ru[] = {1e9, target};
        int64_t nz = 0;
        for (int64_t j = 0; j < NC; j++) {
            as[j] = nz; c[j] = 0.0;
            if (j == 0) {
                cl[j] = 1e9 - 1.0; cu[j] = 1e9 + 1.0;
                ai[nz] = 0; av[nz++] = 1.0;
                ai[nz] = 1; av[nz++] = 1.0;
            } else if (j < KS + 1) {
                cl[j] = cu[j] = small;
                ai[nz] = 0; av[nz++] = 1.0;
            } else {
                c[j] = 1.0; cl[j] = 0.0; cu[j] = wcap;
                ai[nz] = 1; av[nz++] = 1.0;
            }
        }
        as[NC] = nz;

        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         nz, as, ai, av));
        jm_presolve p;
        jm_presolve_init(&p);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_presolve_run(m, &p, nullptr));

        if (half == 0)
            TEST_ASSERT_NOT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        else
            TEST_ASSERT_EQUAL_INT(JM_PRESOLVE_INFEASIBLE, p.outcome);
        jm_presolve_free(&p);

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(half == 0 ? JAOS_SOLVE_OPTIMAL
                                        : JAOS_SOLVE_INFEASIBLE,
                              jaos_status_of(m));
        if (half == 0) {
            double obj = 0.0, x[NC];
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, x, nullptr, nullptr, nullptr));
            TEST_ASSERT_EQUAL_DOUBLE(wcap, obj);
            TEST_ASSERT_EQUAL_DOUBLE(1e9 - ldexp(1.0, -17), x[0]);
            TEST_ASSERT_EQUAL_DOUBLE(wcap, x[KS + 1]);
            TEST_ASSERT_EQUAL_DOUBLE(0.0,  x[KS + 2]);
        }
        jaos_model_free(m);
    }
#endif
}

static void test_a_collapse_on_an_inverted_box_keeps_the_midpoint(void)
{
    const double c[]  = {1.0};
    const double cl[] = {1e9}, cu[] = {1e9 - 5e-7};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

#if defined(JAOS_NO_PRESOLVE)

    (void)0;
#else
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    TEST_ASSERT_TRUE(x[0] < 1e9);
    TEST_ASSERT_TRUE(x[0] > 1e9 - 5e-7);
#endif
    jaos_model_free(m);
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_maximised_singleton_row_model(void)
{
    const double c[]  = {3.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};
    const int64_t s[]  = {0, 2, 3};
    const int64_t ix[] = {0, 1, 0};
    const double v[]   = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, s, ix, v));
    return m;
}
#endif

static void test_a_maximised_singleton_row_is_owed_its_multiplier(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_maximised_singleton_row_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 10.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2], y[2], d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, d));

    const double expected_y1 = 1.0, expected_d0 = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y1, &y[1], sizeof y[1]);
    TEST_ASSERT_EQUAL_MEMORY(&expected_d0, &d[0], sizeof d[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
#endif
}

static jaos_model *make_maximised_empty_column(double lo, double hi)
{
    const double c[]  = {0.0, 1.0};
    const double cl[] = {0.0, lo}, cu[] = {10.0, hi};
    const double rl[] = {0.0}, ru[] = {10.0};
    const int64_t s[]  = {0, 1, 1};
    const int64_t ix[] = {0};
    const double v[]   = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

static jaos_model *empty_col_and_a_row(double c2_upper)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[3] = {0.0, -6.0, -6.0};
    const double cl[3] = {-2.0, -1.0, -2.0};
    const double cu[3] = {-1.0, c2_upper, INFINITY};
    const double rl[1] = {5.0}, ru[1] = {5.0};
    const int64_t as[4] = {0, 1, 2, 2};
    const int64_t ai[2] = {0, 0};
    const double av[2] = {-1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    return m;
}

static void test_an_empty_column_is_unbounded_only_where_a_point_exists(void)
{
    jaos_model *m = empty_col_and_a_row(1.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m),
        "the row leaves no point, so the empty column proves nothing");
    jaos_model_free(m);

    jaos_model *b = empty_col_and_a_row(4.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_UNBOUNDED, jaos_status_of(b),
        "the row holds now, so the empty column does make it unbounded");
    jaos_model_free(b);
}

static void test_a_maximised_empty_column_takes_its_upper_bound(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_maximised_empty_column(0.0, 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const double expected_obj = 5.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);

    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    const double expected_x1 = 5.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_x1, &x[1], sizeof x[1]);
    jaos_model_free(m);
#endif
}

static void test_a_maximised_empty_column_is_not_unbounded_downwards(void)
{

    jaos_model *m = make_maximised_empty_column(-INFINITY, 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    const double expected_obj = 5.0;
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_MEMORY(&expected_obj, &obj, sizeof obj);
#endif
    jaos_model_free(m);
}

static void test_a_maximised_forcing_row_is_owed_its_multiplier(void)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {0.0};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], y[1], d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, d));

    const double zero = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&zero, &x[0], sizeof x[0]);
    TEST_ASSERT_EQUAL_MEMORY(&zero, &x[1], sizeof x[1]);

    const double expected_y0 = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected_y0, &y[0], sizeof y[0]);

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);

    jaos_model_free(m);
}

static void test_a_frozen_row_missed_at_scale_is_refused(void)
{
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.5, 0.0}, cu[] = {0.5, 1e9};
    const double rl[] = {1e9 + 1.0}, ru[] = {1e9 + 1.0};
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
    jaos_model_free(m);
}

static void test_the_recovered_column_respects_the_bound_it_was_promised(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#elif defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("white-box presolve test — the reference build has "
                        "no replay to exercise");
#else
    enum { IFN = 2000 };
    const double V = 50000.667694415897;
    const double b = 100001335.38883179;
    const double L = -8.8819028033554831e-07;
    const int64_t ncol = IFN + 1;

    double  *c  = malloc((size_t)ncol * sizeof *c);
    double  *cl = malloc((size_t)ncol * sizeof *cl);
    double  *cu = malloc((size_t)ncol * sizeof *cu);
    int64_t *st = malloc(((size_t)ncol + 1) * sizeof *st);
    int64_t *ix = malloc((size_t)ncol * sizeof *ix);
    double  *v  = malloc((size_t)ncol * sizeof *v);
    double  *x  = malloc((size_t)ncol * sizeof *x);
    TEST_ASSERT_NOT_NULL(c);  TEST_ASSERT_NOT_NULL(cl);
    TEST_ASSERT_NOT_NULL(cu); TEST_ASSERT_NOT_NULL(st);
    TEST_ASSERT_NOT_NULL(ix); TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_NOT_NULL(x);

    c[0] = 1.0e-9; cl[0] = L; cu[0] = INFINITY;
    st[0] = 0; ix[0] = 0; v[0] = 1.0;
    for (int64_t k = 1; k < ncol; k++) {
        c[k] = -1.0; cl[k] = 0.0; cu[k] = 1.0;
        st[k] = k; ix[k] = 0; v[k] = V;
    }
    st[ncol] = ncol;
    const double rl[] = {b}, ru[] = {b};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ncol, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     ncol, st, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_EQUAL_INT64(0, m->presolve_num_col);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));

    TEST_ASSERT_TRUE_MESSAGE(x[0] >= L,
        "recovered column published outside its own lower bound");

    TEST_ASSERT_TRUE_MESSAGE(fabs(x[0] - (b - (double)IFN * V)) <= 1.0e-7,
        "recovered column carries the accumulation's drift");

    free(c); free(cl); free(cu); free(st); free(ix); free(v); free(x);
    jaos_model_free(m);
#endif
}

static void test_the_presolve_report_says_what_was_removed(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    jaos_presolve_report rep;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_presolve_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.rounds);
    TEST_ASSERT_EQUAL_INT64(0, rep.num_row);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_presolve_result(m, nullptr));

    const double cost[2] = { 1.0, 1.0 };
    const double cl[2] = { 2.0, 0.0 }, cu[2] = { 2.0, INFINITY };
    const double rl[2] = { 3.0, -INFINITY }, ru[2] = { INFINITY, 10.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 1.0, 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_presolve_result(m, &rep));

#if defined(JAOS_NO_PRESOLVE)

    TEST_ASSERT_EQUAL_INT64(0, rep.rounds);
    TEST_ASSERT_EQUAL_INT64(0, rep.fixed_col);
    TEST_ASSERT_EQUAL_INT64(0, rep.empty_row);
    TEST_ASSERT_EQUAL_INT64(2, rep.num_row);
    TEST_ASSERT_EQUAL_INT64(2, rep.num_col);
#else
    TEST_ASSERT_TRUE(rep.rounds > 0);
    TEST_ASSERT_EQUAL_INT64(1, rep.fixed_col);
    TEST_ASSERT_TRUE(rep.empty_row >= 1);

    TEST_ASSERT_TRUE(rep.num_row <= 2);
    TEST_ASSERT_TRUE(rep.num_col <= 2);

    TEST_ASSERT_EQUAL_INT64(0, rep.duplicate_row);
    TEST_ASSERT_EQUAL_INT64(0, rep.duplicate_col);
    TEST_ASSERT_EQUAL_INT64(0, rep.dominated_col);
    TEST_ASSERT_EQUAL_INT64(0, rep.tightened_bound);
#endif
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixed_column_round_trip);
    RUN_TEST(test_postsolved_basis_has_exactly_num_row_basic_entries);
    RUN_TEST(test_fixed_col_counter_is_exact);
    RUN_TEST(test_the_presolve_report_says_what_was_removed);
    RUN_TEST(test_all_columns_fixed_solves_with_no_iterations);
    RUN_TEST(test_original_arrays_survive_a_reducing_solve);
    RUN_TEST(test_fixed_column_index_map_off_by_one);
    RUN_TEST(test_original_index_invariant_across_all_six_arrays);
    RUN_TEST(test_presolve_bills_the_work_counter);
    RUN_TEST(test_presolve_work_is_deterministic_across_re_solves);

    RUN_TEST(test_empty_row_round_trip);
    RUN_TEST(test_empty_row_index_off_by_one);
    RUN_TEST(test_empty_col_round_trip);
    RUN_TEST(test_empty_col_index_off_by_one);
    RUN_TEST(test_empty_col_reports_unbounded);
    RUN_TEST(test_singleton_row_round_trip);
    RUN_TEST(test_singleton_row_wrong_dual);
    RUN_TEST(test_two_folds_the_owning_row_takes_the_multiplier);
    RUN_TEST(test_two_folds_wrong_dual);
    RUN_TEST(test_a_frozen_row_that_cannot_be_satisfied_is_infeasible);
    RUN_TEST(test_a_frozen_row_that_is_exactly_satisfiable_is_not_refused);
    RUN_TEST(test_the_frozen_row_model_agrees_with_the_reference_build);
    RUN_TEST(test_singleton_col_round_trip);
    RUN_TEST(test_singleton_col_index_off_by_one);
    RUN_TEST(test_singleton_col_after_fixed_col);
    RUN_TEST(test_singleton_col_between_two_removals_solved_path);
    RUN_TEST(test_an_exact_tie_the_division_rounds_inward_publishes_the_bound);
    RUN_TEST(test_an_interior_recovery_takes_the_row_out_whatever_the_ulps_say);
    RUN_TEST(test_a_column_a_row_fixed_inside_its_box_is_basic);
    RUN_TEST(test_the_basis_count_promise_breaks_on_a_declined_column);
    RUN_TEST(test_a_short_mapped_basis_is_repaired_and_warm_survives);
    RUN_TEST(test_the_warm_repair_stops_at_its_cap);
    RUN_TEST(test_a_long_mapped_basis_falls_back_cold);
    RUN_TEST(test_singleton_col_open_below_publishes_a_finite_point);
    RUN_TEST(test_two_singleton_cols_on_one_row);
    RUN_TEST(test_free_col_singleton_round_trip);
    RUN_TEST(test_free_col_singleton_index_off_by_one);

    RUN_TEST(test_an_implied_free_column_is_substituted_out);
    RUN_TEST(test_an_implied_bound_outside_the_box_is_refused);
    RUN_TEST(test_an_implied_bound_at_exact_equality_is_declined);
    RUN_TEST(test_a_removed_column_pays_every_row_it_touches);
    RUN_TEST(test_the_recovered_column_respects_the_bound_it_was_promised);
    RUN_TEST(test_implied_free_col_index_off_by_one);
#if !defined(JAOS_NO_PRESOLVE)
    RUN_TEST(test_the_implied_free_counter_reads_its_three_models);
    RUN_TEST(test_a_range_row_that_shifted_into_an_equality_is_declined);
#endif

    RUN_TEST(test_empty_row_reports_infeasible);
    RUN_TEST(test_zero_row_model_solves);
    RUN_TEST(test_zero_col_model_solves);
    RUN_TEST(test_all_five_counters_move_independently);

    RUN_TEST(test_forcing_row_round_trip);
    RUN_TEST(test_forcing_row_index_off_by_one);
    RUN_TEST(test_redundant_row_round_trip);
    RUN_TEST(test_redundant_row_index_off_by_one);
    RUN_TEST(test_a_fold_onto_the_opposite_bound_fixes_the_column);
    RUN_TEST(test_a_fold_one_step_past_the_opposite_bound_is_infeasible);
    RUN_TEST(test_a_fold_inside_the_epsilon_does_not_flip_the_verdict);
    RUN_TEST(test_an_optimum_on_the_tightening_boundary_survives);
    RUN_TEST(test_activity_range_counters_are_exact);
    RUN_TEST(test_the_round_cap_is_the_one_its_sweep_set);
    RUN_TEST(test_the_fold_window_is_rounding_and_nothing_more);

    RUN_TEST(test_an_emptied_row_missed_by_more_than_rounding_is_refused);
    RUN_TEST(test_an_emptied_row_missed_by_rounding_alone_is_kept);
    RUN_TEST(test_a_fold_past_the_box_at_scale_is_refused);
    RUN_TEST(test_a_fold_onto_the_box_at_scale_still_collapses);
    RUN_TEST(test_a_collapse_on_an_inverted_box_keeps_the_midpoint);
    RUN_TEST(test_a_frozen_row_is_not_refused_below_its_own_traffic);
    RUN_TEST(test_a_frozen_row_emptied_and_still_short_is_refused);
    RUN_TEST(test_the_activity_pass_is_not_refused_below_its_own_traffic);
    RUN_TEST(test_the_activity_pass_still_refuses_a_real_shortfall);
    RUN_TEST(test_the_window_counts_the_shifts_and_not_only_their_scale);
    RUN_TEST(test_the_shift_count_scales_by_the_end_it_is_testing);
    RUN_TEST(test_the_singleton_fold_counts_the_shifts_too);
    RUN_TEST(test_a_folds_value_carries_its_rows_error_into_the_next);
    RUN_TEST(test_a_frozen_rows_window_ignores_the_far_bound);

    RUN_TEST(test_a_frozen_row_missed_at_scale_is_refused);

    RUN_TEST(test_a_maximised_singleton_row_is_owed_its_multiplier);
    RUN_TEST(test_an_empty_column_is_unbounded_only_where_a_point_exists);
    RUN_TEST(test_a_maximised_empty_column_takes_its_upper_bound);
    RUN_TEST(test_a_maximised_empty_column_is_not_unbounded_downwards);
    RUN_TEST(test_a_maximised_forcing_row_is_owed_its_multiplier);
    return UNITY_END();
}
