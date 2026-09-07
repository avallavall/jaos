/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

constexpr double CHECK_TOL = 1e-6;

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static void solve_and_verify(jaos_model *m, double expect_obj)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    (void)m;
    (void)expect_obj;
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, expect_obj, obj);

    int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof(double));
    double *y = calloc((size_t)(nr > 0 ? nr : 1), sizeof(double));
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, expect_obj, rep.primal_objective);

    free(x);
    free(y);
#endif
}

static void test_minimise_over_a_ge_row(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

static void test_maximise_with_two_rows(void)
{
    const double c[] = {3.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {2.0, 10.0};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};

    const int64_t as[] = {0, 2, 3};
    const int64_t ai[] = {0, 1, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    solve_and_verify(m, 10.0);
    jaos_model_free(m);
}

static void test_equality_row_with_a_capped_column(void)
{
    const double c[] = {1.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 5.0};
    const double rl[] = {3.0}, ru[] = {3.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, 5.0);
    jaos_model_free(m);
}

static void test_ranged_row(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {1.0}, ru[] = {3.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    solve_and_verify(m, -3.0);
    jaos_model_free(m);
}

static void test_objective_offset_is_carried(void)
{
    const double c[] = {1.0};
    const double cl[] = {2.0}, cu[] = {8.0};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 7.5, c, cl, cu, rl, ru,
                     1, as, ai, av));
    solve_and_verify(m, 9.5);
    jaos_model_free(m);
}

static void test_infeasible_model_is_reported(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    jaos_model_free(m);
}

static void test_three_by_three(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};

    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    solve_and_verify(m, 29.0);
    jaos_model_free(m);
}

static void test_solving_a_model_read_from_mps(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));

    solve_and_verify(m, 29.0);
    jaos_model_free(m);
}

static void test_unbounded_model_is_reported(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    double obj = 1234.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(1234.0, obj);
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    jaos_model_free(m);
}

static void test_missing_bound_that_a_row_restrains_solves_normally(void)
{
    const double c[] = {-1.0, -2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, -8.0);
    jaos_model_free(m);
}

static void test_a_ray_is_what_proves_unbounded(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_an_optimum_past_the_lent_bound_is_refused(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now solves this "
                        "model directly (02-03); runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {1e11};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NUMERICAL_ERROR, jaos_status_of(m));

    const char *err = jaos_model_error(m);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(strstr(err, "phase 1"));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "together"),
                                 "the refusal must say the combined "
                                 "direction was tried and blocked");
    jaos_model_free(m);
#endif
}

static void test_a_lent_bound_that_never_constrained_anything(void)
{
    const double c[] = {-1.0, -1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {3.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, -3.0);
    jaos_model_free(m);
}

static void test_optimality_is_rechecked_before_it_is_believed(void)
{
    const double c[] = {2.0, -3.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY};
    const double ru[] = {19.0, 13.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {4.0, -3.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solve_and_verify(m, -45.0);
    jaos_model_free(m);
}

static void test_maximise_without_column_upper_bounds(void)
{
    const double c[] = {3.0, 2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};
    const int64_t as[] = {0, 2, 3};
    const int64_t ai[] = {0, 1, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    solve_and_verify(m, 10.0);
    jaos_model_free(m);
}

static void test_t1_mps_is_infeasible_and_says_so(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_zero_objective_is_distinguishable_from_no_answer(void)
{
    const double c[] = {0.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {0.0}, ru[] = {1.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = -1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, obj);
    jaos_model_free(m);
}

static void test_a_long_solve_crosses_a_refactorization(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now folds every "
                        "per-column row into a bound (02-03); runs only "
                        "under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    enum { N = 100 };
    double c[N], cl[N], cu[N];
    double rl[N + 1], ru[N + 1];
    int64_t as[N + 1], ai[2 * N];
    double av[2 * N];

    for (int64_t j = 0; j < N; j++) {
        c[j] = 1.0;
        cl[j] = 0.0;
        cu[j] = 10.0;
        rl[j] = 1.0;
        ru[j] = INFINITY;
        as[j] = 2 * j;
        ai[2 * j] = j;
        av[2 * j] = 1.0;
        ai[2 * j + 1] = N;
        av[2 * j + 1] = 1.0;
    }
    as[N] = 2 * N;
    rl[N] = -INFINITY;
    ru[N] = 200.0;

    double obj[2];
    int64_t iters[2], work[2];
    for (int run = 0; run < 2; run++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, N, N + 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         2 * N, as, ai, av));
        solve_and_verify(m, 100.0);

        TEST_ASSERT_TRUE(jaos_iterations(m) > 64);

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[run]));
        iters[run] = jaos_iterations(m);
        work[run] = jaos_work_units(m);
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_MEMORY(&obj[0], &obj[1], sizeof(double));
    TEST_ASSERT_EQUAL_INT64(iters[0], iters[1]);
    TEST_ASSERT_EQUAL_INT64(work[0], work[1]);
#endif
}

static void test_free_variable_enters_and_settles(void)
{
    const double c[] = {2.0, 0.0};
    const double cl[] = {0.0, -INFINITY};
    const double cu[] = {10.0, INFINITY};
    const double rl[] = {4.0, -1.0};
    const double ru[] = {INFINITY, INFINITY};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, -1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solve_and_verify(m, 3.0);

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.5, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.5, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, y[1]);
    jaos_model_free(m);
}

#if defined(JAOS_NO_PRESOLVE)
constexpr int64_t WORK_PINNED = 8536;
#endif

static void test_work_accounting_is_pinned(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now folds two of "
                        "the three rows into bounds (02-03); runs only "
                        "under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    TEST_ASSERT_EQUAL_INT64(WORK_PINNED, jaos_work_units(m));
    jaos_model_free(m);
#endif
}

static void test_simultaneous_violations_of_wildly_different_size(void)
{
    const double c[] = {1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 10.0};
    const double rl[] = {2.0, 3000.0, 0.001};
    const double ru[] = {INFINITY, INFINITY, INFINITY};

    const int64_t as[] = {0, 2, 4, 6};
    const int64_t ai[] = {0, 2, 0, 1, 1, 2};
    const double av[] = {1.0, 0.001, 1.0, 1000.0, 1000.0, 0.001};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    solve_and_verify(m, 3.0);
    jaos_model_free(m);
}

constexpr int64_t DSE_N = 3;

typedef struct {
    int64_t start[DSE_N + 1];
    int64_t index[DSE_N * DSE_N];
    double  value[DSE_N * DSE_N];
} csc3;

static void pack3(double a[DSE_N][DSE_N], csc3 *out)
{
    int64_t nz = 0;
    for (int64_t j = 0; j < DSE_N; j++) {
        out->start[j] = nz;
        for (int64_t i = 0; i < DSE_N; i++)
            if (a[i][j] != 0.0) {
                out->index[nz] = i;
                out->value[nz] = a[i][j];
                nz++;
            }
    }
    out->start[DSE_N] = nz;
}

static void factor3(double a[DSE_N][DSE_N], csc3 *c, jm_lu *lu)
{
    pack3(a, c);
    jm_lu_init(lu);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(lu, DSE_N, c->start, c->index,
                                                c->value, 0.1, nullptr));
    TEST_ASSERT_EQUAL_INT64(DSE_N, lu->rank);
}

static void exact_weights(double b[DSE_N][DSE_N], double *w)
{
    csc3 c;
    jm_lu lu;
    factor3(b, &c, &lu);
    for (int64_t i = 0; i < DSE_N; i++) {
        double rho[DSE_N] = {0.0};
        rho[i] = 1.0;
        jm_lu_btran(&lu, rho, nullptr);
        double sum = 0.0;
        for (int64_t k = 0; k < DSE_N; k++)
            sum += rho[k] * rho[k];
        w[i] = sum;
    }
    jm_lu_free(&lu);
}

constexpr int64_t DSE_ROW = 1;

static void dse_pivot_case(double *w, double *expect,
                           double *alpha, double *tau)
{
    double b[DSE_N][DSE_N] = {
        {2.0, 1.0, 0.0},
        {0.0, 3.0, 1.0},
        {1.0, 0.0, 4.0},
    };
    const double aq[DSE_N] = {1.0, 2.0, 3.0};

    exact_weights(b, w);

    csc3 c;
    jm_lu lu;
    factor3(b, &c, &lu);

    memcpy(alpha, aq, DSE_N * sizeof *alpha);
    jm_lu_ftran(&lu, alpha, nullptr);

    memset(tau, 0, DSE_N * sizeof *tau);
    tau[DSE_ROW] = 1.0;
    jm_lu_btran(&lu, tau, nullptr);
    jm_lu_ftran(&lu, tau, nullptr);
    jm_lu_free(&lu);

    double after[DSE_N][DSE_N];
    memcpy(after, b, sizeof after);
    for (int64_t i = 0; i < DSE_N; i++)
        after[i][DSE_ROW] = aq[i];
    exact_weights(after, expect);
}

static void test_dse_weights_match_recomputed_norms(void)
{
    double w[DSE_N], expect[DSE_N], alpha[DSE_N], tau[DSE_N];
    dse_pivot_case(w, expect, alpha, tau);

    jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, w[DSE_ROW], 10.0, nullptr, 0);

    for (int64_t i = 0; i < DSE_N; i++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, expect[i], w[i]);
}

static void test_dse_repairs_a_carried_weight_that_slipped(void)
{
    double w[DSE_N], expect[DSE_N], alpha[DSE_N], tau[DSE_N];
    dse_pivot_case(w, expect, alpha, tau);

    double truth = w[DSE_ROW];
    w[DSE_ROW] = truth * 1.5;

    jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, truth, 10.0, nullptr, 0);

    for (int64_t i = 0; i < DSE_N; i++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, expect[i], w[i]);
}

static void test_dse_restarts_when_the_carried_weight_has_drifted(void)
{
    for (int trial = 0; trial < 2; trial++) {
        double w[DSE_N], expect[DSE_N], alpha[DSE_N], tau[DSE_N];
        dse_pivot_case(w, expect, alpha, tau);

        double truth = w[DSE_ROW];
        w[DSE_ROW] = trial == 0 ? truth * 1e6 : truth * 1e-6;

        jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, truth, 10.0, nullptr, 0);

        for (int64_t i = 0; i < DSE_N; i++)
            TEST_ASSERT_EQUAL_DOUBLE(1.0, w[i]);
    }
}

static jaos_model *badly_scaled_model(void)
{
    static const double c[] = {1e6, 1.5, 1e6};
    static const double cl[] = {0.0, 0.0, 0.0};
    static const double cu[] = {10.0, 10.0, 10.0};
    static const double rl[] = {2.0, 3.0};
    static const double ru[] = {INFINITY, INFINITY};
    static const int64_t as[] = {0, 2, 4, 6};
    static const int64_t ai[] = {0, 1, 0, 1, 0, 1};
    static const double av[] = {1e6, 1e6, 1.0, 2.0, 1e3, 1e3};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    return m;
}

static void test_scaling_changes_the_arithmetic_not_the_answer(void)
{
    jaos_model *plain = badly_scaled_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(plain, JM_SCALE_NONE));
    solve_and_verify(plain, 2.5);

    jaos_model *scaled = badly_scaled_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jm_model_scale(scaled, JM_SCALE_CURTIS_REID));

    bool rows_moved = false, cols_moved = false;
    for (int64_t i = 0; i < jaos_num_row(scaled); i++)
        if (scaled->row_scale[i] != 1.0)
            rows_moved = true;
    for (int64_t j = 0; j < jaos_num_col(scaled); j++)
        if (scaled->col_scale[j] != 1.0)
            cols_moved = true;
    TEST_ASSERT_TRUE(rows_moved);
    TEST_ASSERT_TRUE(cols_moved);

    solve_and_verify(scaled, 2.5);

    double a = 0.0, b = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(plain, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(scaled, &b));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, a, b);

    jaos_model_free(plain);
    jaos_model_free(scaled);
}

static void test_answers_come_back_in_the_models_units(void)
{
    jaos_model *m = badly_scaled_model();
    solve_and_verify(m, 2.5);

    double x[3], act[2], y[2], dj[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1e-6, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[2]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, act[1]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, y[1]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, dj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, dj[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 999000.0, dj[2]);

    jaos_model_free(m);
}

static void test_bound_flipping_fills_columns_in_one_step(void)
{
    const double c[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const double cu[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    const double rl[] = {5.5}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    const int64_t ai[] = {0, 0, 0, 0, 0, 0, 0, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 8, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     8, as, ai, av));
    solve_and_verify(m, 18.0);
    TEST_ASSERT_EQUAL_INT64(1, jaos_iterations(m));
    jaos_model_free(m);
}

static void test_settling_up_reaches_the_optimum_a_shifted_basis_hid(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[] = {0.0, 2e-8, 5e-7};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 0.001, 100.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2, 3};
    const int64_t ai[] = {0, 0, 0};
    const double av[] = {1.0, 1.0, 10.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_NONE));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], act[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, nullptr, nullptr));

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, act[0]);

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, obj);

    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, y[0]);

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, rep.objective_gap);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, rep.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, rep.gap_negative);

    jaos_model_free(m);
#endif
}

static void test_a_clean_up_pass_dispatches_every_column_it_identified(void)
{
    constexpr int64_t NR = 20;
    constexpr int64_t NC = 40;

    double c[NC], cl[NC], cu[NC], rl[NR], ru[NR], av[NC * 4];
    int64_t as[NC + 1], ai[NC * 4];

    uint64_t st = 236;
    #define NEXTU() (st = st * 6364136223846793005u + 1442695040888963407u, \
                     (double)((st >> 11) & 0x1FFFFFFFFFFFFFu) / 9007199254740992.0)

    int64_t nz = 0;
    for (int64_t j = 0; j < NC; j++) {
        as[j] = nz;
        int per = 2 + (int)(NEXTU() * 3.0);
        int64_t rows[8];
        int nrows = 0;
        for (int k = 0; k < per; k++) {
            int64_t r = (int64_t)(NEXTU() * (double)NR);
            bool dup = false;
            for (int t = 0; t < nrows; t++)
                if (rows[t] == r) dup = true;
            if (!dup) rows[nrows++] = r;
        }
        for (int a = 1; a < nrows; a++) {
            int64_t v = rows[a];
            int b = a - 1;
            while (b >= 0 && rows[b] > v) { rows[b + 1] = rows[b]; b--; }
            rows[b + 1] = v;
        }
        for (int k = 0; k < nrows; k++) {
            ai[nz] = rows[k];

            av[nz] = pow(10.0, NEXTU() * 2.0 - 1.0);
            nz++;
        }
        c[j] = pow(10.0, -6.0 - NEXTU() * 3.0);
        cl[j] = 0.0;
        cu[j] = (j < NC / 2) ? INFINITY : 1.0 + NEXTU() * 10.0;
    }
    as[NC] = nz;
    for (int64_t i = 0; i < NR; i++) { rl[i] = 1.0 + NEXTU(); ru[i] = INFINITY; }
    #undef NEXTU

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, NC, NR, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[NC], act[NR], y[NR], d[NC];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, d));

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);

    TEST_ASSERT_TRUE(jaos_work_units(m) < 62000);

    jaos_model_free(m);
}

constexpr double HARRIS_TOL = 1e-7;

static void test_harris_ignores_a_big_pivot_outside_the_window(void)
{
    const double num[] = {1.0, 500.0};
    const double den[] = {1.0, 100.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(2, num, den, HARRIS_TOL));
}

static void test_harris_prefers_the_larger_pivot_inside_the_window(void)
{
    const double num[] = {0.0, 1e-8};
    const double den[] = {1e-8, 1.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(2, num, den, HARRIS_TOL));
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(2, num, den, 0.0));
}

static void test_harris_on_a_degenerate_vertex_takes_the_best_pivot(void)
{
    const double num[] = {0.0, 0.0, 0.0};
    const double den[] = {1.0, 7.0, 3.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(3, num, den, HARRIS_TOL));
}

static void test_harris_edge_counts(void)
{
    const double num[] = {42.0};
    const double den[] = {0.5};
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(1, num, den, HARRIS_TOL));
    TEST_ASSERT_EQUAL_INT64(-1, jm_harris_pick(0, num, den, HARRIS_TOL));
}

static void test_bland_on_a_degenerate_vertex_takes_the_lowest_index(void)
{
    const int64_t var[] = {9, 4, 7};
    const double num[] = {0.0, 0.0, 0.0};
    const double den[] = {1.0, 7.0, 3.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_bland_pick(3, var, num, den));

    const int64_t var2[] = {2, 8};
    const double num2[] = {0.0, 0.0};
    const double den2[] = {1.0, 7.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(2, var2, num2, den2));
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(2, num2, den2, HARRIS_TOL));
}

static void test_bland_has_no_window_to_trade(void)
{
    const int64_t var[] = {5, 1};
    const double num[] = {0.0, 1e-8};
    const double den[] = {1e-8, 1.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(2, var, num, den));
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(2, num, den, HARRIS_TOL));
}

static void test_bland_does_not_let_the_index_beat_the_quotient(void)
{
    const int64_t var[] = {1, 9};
    const double num[] = {5.0, 1.0};
    const double den[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_bland_pick(2, var, num, den));
}

static void test_bland_edge_counts(void)
{
    const int64_t var[] = {3};
    const double num[] = {42.0};
    const double den[] = {0.5};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(1, var, num, den));
    TEST_ASSERT_EQUAL_INT64(-1, jm_bland_pick(0, var, num, den));
}

static void test_bland_compares_the_minimum_exactly_at_one_ulp(void)
{
    const int64_t var[] = {1, 9};
    const double q  = 1.0;
    const double qp = nextafter(q, 2.0);
    TEST_ASSERT_TRUE(qp > q);

    const double num[] = {qp, q};
    const double den[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_bland_pick(2, var, num, den));

    const double num2[] = {q, qp};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(2, var, num2, den));

    const double num3[] = {q, q};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(2, var, num3, den));
}

static void test_nonbasic_build_on_no_variables_counts_zero(void)
{
    uint64_t mark[2] = {0xdeadbeefULL, 0xfeedfaceULL};
    const jm_var_status status[] = {JM_AT_LOWER};
    TEST_ASSERT_EQUAL_INT64(0, jm_nonbasic_build(0, status, mark));
    TEST_ASSERT_EQUAL_INT64(0, jm_nonbasic_build(-1, status, mark));

    TEST_ASSERT_EQUAL_UINT64(0xdeadbeefULL, mark[0]);
    TEST_ASSERT_EQUAL_UINT64(0xfeedfaceULL, mark[1]);
}

static void test_alloc_array_of_zero_is_not_a_failure(void)
{
    void *p = jm_alloc_array(0, sizeof(double));
    TEST_ASSERT_NOT_NULL(p);
    free(p);
    void *c = jm_calloc_array(0, sizeof(double));
    TEST_ASSERT_NOT_NULL(c);
    free(c);
}

static void test_primal_bland_breaks_a_degenerate_tie_on_the_lowest_index(void)
{

    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 4, 0.0, 9, true));

    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 9, 0.0, 4, true));
}

static void test_primal_without_bland_a_tie_keeps_the_first_row(void)
{
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 4, 0.0, 9, false));
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 9, 0.0, 4, false));
}

static void test_primal_bland_does_not_let_the_index_beat_the_step(void)
{
    TEST_ASSERT_FALSE(jm_primal_row_wins(5.0, 1, 1.0, 9, true));
    TEST_ASSERT_TRUE(jm_primal_row_wins(1.0, 9, 5.0, 1, true));
}

static void test_primal_bland_has_no_window(void)
{
    const double a = 1.0;
    const double b = nextafter(1.0, 2.0);
    TEST_ASSERT_TRUE(b > a);
    TEST_ASSERT_TRUE(jm_primal_row_wins(a, 9, b, 1, true));
    TEST_ASSERT_FALSE(jm_primal_row_wins(b, 1, a, 9, true));
}

static void test_primal_bland_first_row_always_wins(void)
{
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 7, HUGE_VAL, -1, true));
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 7, HUGE_VAL, -1, false));

    TEST_ASSERT_FALSE(jm_primal_row_wins(HUGE_VAL, 7, HUGE_VAL, -1, true));
}

static void test_primal_bland_variable_zero_is_an_index(void)
{
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 0, 0.0, 1, true));
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 1, 0.0, 0, true));
}

static void test_primal_bland_takes_the_lowest_and_not_the_highest(void)
{
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 2, 0.0, 8, true));
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 8, 0.0, 2, true));
}

static int64_t greedy_winner(const double *step, const int64_t *var, int n,
                             unsigned mask, bool bland)
{
    int64_t best = -1;
    double best_step = HUGE_VAL;
    for (int i = 0; i < n; i++) {
        if ((mask & (1u << i)) == 0)
            continue;
        if (jm_primal_row_wins(step[i], var[i], best_step,
                               best >= 0 ? var[best] : -1, bland))
            best = i, best_step = step[i];
    }
    return best;
}

static void test_primal_row_wins_minimum_survives_every_subset(void)
{

    const double step[6] = { 0.0, 2.5, 0.0, 7.0, 2.5, 0.0 };
    const int64_t var[6] = {  9,   3,   4,   0,   1,   7 };

    for (int mode = 0; mode < 2; mode++) {
        const bool bland = mode == 1;
        const int64_t full = greedy_winner(step, var, 6, 0x3fu, bland);
        TEST_ASSERT_TRUE(full >= 0);
        for (unsigned mask = 1; mask < 64u; mask++) {
            if ((mask & (1u << full)) == 0)
                continue;
            TEST_ASSERT_EQUAL_INT64(full, greedy_winner(step, var, 6,
                                                        mask, bland));
        }
    }
}

static void test_primal_row_wins_dropping_the_winner_can_change_it(void)
{
    const double step[3] = { 1.0, 2.0, 3.0 };
    const int64_t var[3] = {  5,   6,   7  };
    const int64_t full = greedy_winner(step, var, 3, 0x7u, false);
    TEST_ASSERT_EQUAL_INT64(0, full);
    TEST_ASSERT_EQUAL_INT64(1, greedy_winner(step, var, 3, 0x6u, false));
}

#define PAT_WORDS 8
#define PAT_LIMIT (PAT_WORDS * 64)

static void assert_mark_clean(const uint64_t *mark)
{
    for (int i = 0; i < PAT_WORDS; i++)
        TEST_ASSERT_EQUAL_UINT64(0, mark[i]);
}

static void test_pattern_order_sorts_and_dedups(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;

    int64_t pos[] = {200, 5, 63, 5, 64, 130, 5, 0};
    int64_t k = jm_pattern_order(8, pos, mark, PAT_LIMIT, &words);

    TEST_ASSERT_EQUAL_INT64(6, k);
    const int64_t want[] = {0, 5, 63, 64, 130, 200};
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], pos[i]);
    TEST_ASSERT_EQUAL_INT64(4, words);
    assert_mark_clean(mark);
}

static void test_pattern_order_scans_only_the_touched_range(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {450, 449};
    TEST_ASSERT_EQUAL_INT64(2, jm_pattern_order(2, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(449, pos[0]);
    TEST_ASSERT_EQUAL_INT64(450, pos[1]);
    TEST_ASSERT_EQUAL_INT64(1, words);
    assert_mark_clean(mark);
}

static void test_pattern_order_keeps_a_full_pattern(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[PAT_LIMIT];
    for (int64_t i = 0; i < PAT_LIMIT; i++)
        pos[i] = PAT_LIMIT - 1 - i;

    TEST_ASSERT_EQUAL_INT64(PAT_LIMIT,
        jm_pattern_order(PAT_LIMIT, pos, mark, PAT_LIMIT, &words));
    for (int64_t i = 0; i < PAT_LIMIT; i++)
        TEST_ASSERT_EQUAL_INT64(i, pos[i]);
    TEST_ASSERT_EQUAL_INT64(PAT_WORDS, words);
    assert_mark_clean(mark);
}

static void test_pattern_order_drops_what_it_cannot_hold(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {PAT_LIMIT, -1, 7, PAT_LIMIT + 1000};
    TEST_ASSERT_EQUAL_INT64(1, jm_pattern_order(4, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(7, pos[0]);
    assert_mark_clean(mark);
}

static void test_pattern_order_edge_counts(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {3};

    TEST_ASSERT_EQUAL_INT64(0, jm_pattern_order(0, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(0, words);
    TEST_ASSERT_EQUAL_INT64(0, jm_pattern_order(1, pos, mark, 0, &words));
    TEST_ASSERT_EQUAL_INT64(0, words);
    assert_mark_clean(mark);
}

#define NB_WORDS 4
#define NB_VARS (NB_WORDS * 64)

static bool expansion_matches_status(int64_t nvar,
                                     const jm_var_status *status,
                                     const uint64_t *mark)
{
    uint64_t rebuilt[NB_WORDS] = {0};
    int64_t want[NB_VARS], got[NB_VARS];

    int64_t nwant = jm_nonbasic_build(nvar, status, rebuilt);
    if (jm_nonbasic_expand(nvar, rebuilt, want) != nwant)
        return false;
    if (jm_nonbasic_expand(nvar, mark, got) != nwant)
        return false;
    for (int64_t k = 0; k < nwant; k++)
        if (want[k] != got[k])
            return false;
    return true;
}

static void test_nonbasic_build_keeps_free_variables(void)
{
    jm_var_status status[NB_VARS];
    uint64_t mark[NB_WORDS] = {0};
    int64_t out[NB_VARS];

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = JM_BASIC;
    status[3]   = JM_AT_LOWER;
    status[70]  = JM_AT_UPPER;
    status[131] = JM_FREE;
    status[255] = JM_FREE;

    TEST_ASSERT_EQUAL_INT64(4, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(4, jm_nonbasic_expand(NB_VARS, mark, out));

    const int64_t want[] = {3, 70, 131, 255};
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], out[i]);

    for (int64_t v = 0; v < NB_VARS; v++) {
        bool set = ((mark[v >> 6] >> (v & 63)) & 1) != 0;
        if (status[v] == JM_BASIC)
            TEST_ASSERT_FALSE(set);
        else
            TEST_ASSERT_TRUE(set);
    }
}

static void test_nonbasic_expand_is_ascending_across_words(void)
{
    uint64_t mark[NB_WORDS] = {0};
    int64_t out[NB_VARS];

    const int64_t put[] = {255, 64, 192, 0, 63, 128, 191, 65};
    for (size_t i = 0; i < sizeof put / sizeof *put; i++)
        jm_nonbasic_insert(mark, put[i]);

    TEST_ASSERT_EQUAL_INT64(8, jm_nonbasic_expand(NB_VARS, mark, out));
    const int64_t want[] = {0, 63, 64, 65, 128, 191, 192, 255};
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], out[i]);
    for (int i = 1; i < 8; i++)
        TEST_ASSERT_TRUE(out[i] > out[i - 1]);
}

static void test_nonbasic_expand_handles_the_degenerate_counts(void)
{
    jm_var_status status[NB_VARS];

    uint64_t mark[NB_WORDS];
    int64_t out[NB_VARS];

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = JM_BASIC;
    out[0] = -7;
    TEST_ASSERT_EQUAL_INT64(0, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(0, jm_nonbasic_expand(NB_VARS, mark, out));
    TEST_ASSERT_EQUAL_INT64(-7, out[0]);

    status[NB_VARS - 1] = JM_AT_UPPER;
    TEST_ASSERT_EQUAL_INT64(1, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(1, jm_nonbasic_expand(NB_VARS, mark, out));
    TEST_ASSERT_EQUAL_INT64(NB_VARS - 1, out[0]);

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = JM_AT_LOWER;
    TEST_ASSERT_EQUAL_INT64(NB_VARS, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(NB_VARS, jm_nonbasic_expand(NB_VARS, mark, out));
    for (int64_t v = 0; v < NB_VARS; v++)
        TEST_ASSERT_EQUAL_INT64(v, out[v]);
}

static void test_nonbasic_survives_interleaved_eviction(void)
{
    jm_var_status status[NB_VARS];
    uint64_t mark[NB_WORDS] = {0};

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = (v % 3 == 0) ? JM_BASIC : JM_AT_LOWER;
    status[100] = JM_FREE;
    jm_nonbasic_build(NB_VARS, status, mark);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    status[98] = JM_BASIC;
    jm_nonbasic_remove(mark, 98);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    status[99] = JM_AT_UPPER;
    jm_nonbasic_insert(mark, 99);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    status[99] = JM_BASIC;
    jm_nonbasic_remove(mark, 99);
    status[98] = JM_AT_LOWER;
    jm_nonbasic_insert(mark, 98);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    uint64_t before[NB_WORDS];
    for (int i = 0; i < NB_WORDS; i++)
        before[i] = mark[i];
    jm_nonbasic_insert(mark, 42);
    jm_nonbasic_remove(mark, 42);
    for (int i = 0; i < NB_WORDS; i++)
        TEST_ASSERT_EQUAL_UINT64(before[i], mark[i]);
}

static void test_nonbasic_notices_a_missed_hook(void)
{
    jm_var_status status[NB_VARS];
    uint64_t mark[NB_WORDS] = {0};

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = (v % 3 == 0) ? JM_BASIC : JM_AT_LOWER;
    jm_nonbasic_build(NB_VARS, status, mark);

    status[98] = JM_BASIC;
    jm_nonbasic_remove(mark, 98);
    status[99] = JM_AT_UPPER;
    TEST_ASSERT_FALSE(expansion_matches_status(NB_VARS, status, mark));

    jm_nonbasic_insert(mark, 99);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));
}

static void test_solving_twice_is_bit_identical(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    double obj[2];
    int64_t iters[2], work[2];
    for (int run = 0; run < 2; run++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         5, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[run]));
        iters[run] = jaos_iterations(m);
        work[run] = jaos_work_units(m);
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_MEMORY(&obj[0], &obj[1], sizeof(double));
    TEST_ASSERT_EQUAL_INT64(iters[0], iters[1]);
    TEST_ASSERT_EQUAL_INT64(work[0], work[1]);
}

static void test_work_limit_stops_and_reports(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_budgets_survive_a_reload(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {0.0}, ru[] = {1.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 12345));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_time_limit(m, 42.0));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-4));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_dual_tolerance(m, 1e-5));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT64(12345, m->cfg.work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, m->cfg.time_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-4, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(1e-5, m->cfg.dual_tol);
    jaos_model_free(m);
}

static void test_a_tolerance_must_be_a_tolerance(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(m, -1e-9));
    TEST_ASSERT_TRUE(jaos_model_error(m)[0] != '\0');
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_dual_tolerance(m, -1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_dual_tolerance(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(nullptr, 1e-6));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_dual_tolerance(nullptr, 1e-6));
    jaos_model_free(m);
}

static void test_an_untouched_model_carries_no_tolerance_of_its_own(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.dual_tol);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-3));
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 0.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_dual_tolerance(m, 1e-3));
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, m->cfg.dual_tol);
    jaos_model_free(m);
}

static void test_a_wide_primal_tolerance_accepts_a_point_it_should_not(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now folds the "
                        "row into x's own bound directly (02-03); runs "
                        "only under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {5.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, obj);
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 10.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, obj);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    jaos_model_free(m);
#endif
}

static int64_t g_log_lines;
static jaos_log_level g_log_max;
static char g_log_last[256];

static void collect_log(void *user, jaos_log_level level, const char *line)
{
    *(int *)user += 1;
    g_log_lines++;
    if (level > g_log_max)
        g_log_max = level;
    snprintf(g_log_last, sizeof g_log_last, "%s", line);
}

static jaos_model *log_model(void)
{

    const double c[] = {-1.0, -2.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {4.0, 4.0, 4.0};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {5.0, 6.0};
    const int64_t as[] = {0, 2, 4, 6}, ai[] = {0, 1, 0, 1, 0, 1};
    const double av[] = {1.0, 2.0, 2.0, 1.0, 1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    return m;
}

static void test_logging_says_nothing_until_it_is_asked_to(void)
{
    int hits = 0;
    jaos_model *m = log_model();

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(0, hits);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_OFF));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(0, hits);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_TRUE(hits >= 2);
    TEST_ASSERT_TRUE(g_log_last[0] != '\0');

    int after = hits;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(after, hits);

    jaos_model_free(m);
}

static void test_a_level_outside_the_enum_is_refused(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_level(m, (jaos_log_level)-1));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_level(m, (jaos_log_level)99));
    TEST_ASSERT_TRUE(jaos_model_error(m)[0] != '\0');
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_level(nullptr, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_callback(nullptr, collect_log, nullptr));
    jaos_model_free(m);
}

static void test_watching_a_solve_does_not_change_it(void)
{
    jaos_model *quiet = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(quiet));
    double qobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(quiet, &qobj));
    const int64_t qiters = jaos_iterations(quiet);
    const int64_t qwork = jaos_work_units(quiet);
    double qx[3] = {0}, qy[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(quiet, qx, nullptr, qy, nullptr));

    int hits = 0;
    jaos_model *loud = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(loud, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(loud, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(loud));
    double lobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(loud, &lobj));
    double lx[3] = {0}, ly[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(loud, lx, nullptr, ly, nullptr));

    TEST_ASSERT_TRUE(hits > 0);

    TEST_ASSERT_EQUAL_MEMORY(&qobj, &lobj, sizeof qobj);
    TEST_ASSERT_EQUAL_MEMORY(qx, lx, sizeof qx);
    TEST_ASSERT_EQUAL_MEMORY(qy, ly, sizeof qy);
    TEST_ASSERT_EQUAL_INT64(qiters, jaos_iterations(loud));
    TEST_ASSERT_EQUAL_INT64(qwork, jaos_work_units(loud));

    jaos_model_free(quiet);
    jaos_model_free(loud);
}

typedef struct {
    int calls;
    int stop_after;
    int64_t last_iters;
    int64_t last_work;
    bool iters_on_the_beat;
    bool work_never_went_back;
} watcher;

static jaos_callback_action watch(const jaos_progress *p, void *user)
{
    watcher *w = user;
    if (p->work_units < w->last_work)
        w->work_never_went_back = false;
    if (p->iterations % 64 != 0)
        w->iters_on_the_beat = false;
    w->last_iters = p->iterations;
    w->last_work = p->work_units;
    w->calls++;
    return (w->stop_after >= 0 && w->calls > w->stop_after)
               ? JAOS_CALLBACK_STOP : JAOS_CALLBACK_CONTINUE;
}

static watcher fresh_watcher(int stop_after)
{
    return (watcher){.stop_after = stop_after, .iters_on_the_beat = true,
                     .work_never_went_back = true};
}

static void test_a_watcher_is_asked_and_changes_nothing(void)
{
    jaos_model *quiet = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(quiet));
    double qobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(quiet, &qobj));
    const int64_t qiters = jaos_iterations(quiet);
    const int64_t qwork = jaos_work_units(quiet);
    double qx[3] = {0}, qy[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(quiet, qx, nullptr, qy, nullptr));

    watcher w = fresh_watcher(-1);
    jaos_model *seen = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(seen, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(seen));
    double sobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(seen, &sobj));
    double sx_[3] = {0}, sy[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(seen, sx_, nullptr, sy, nullptr));

    TEST_ASSERT_TRUE(w.calls > 0);
    TEST_ASSERT_TRUE(w.iters_on_the_beat);
    TEST_ASSERT_TRUE(w.work_never_went_back);

    TEST_ASSERT_EQUAL_MEMORY(&qobj, &sobj, sizeof qobj);
    TEST_ASSERT_EQUAL_MEMORY(qx, sx_, sizeof qx);
    TEST_ASSERT_EQUAL_MEMORY(qy, sy, sizeof qy);
    TEST_ASSERT_EQUAL_INT64(qiters, jaos_iterations(seen));
    TEST_ASSERT_EQUAL_INT64(qwork, jaos_work_units(seen));

    jaos_model_free(quiet);
    jaos_model_free(seen);
}

static void test_a_watcher_can_stop_a_solve_and_it_resumes(void)
{
    watcher w = fresh_watcher(0);
    jaos_model *m = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, w.calls);

    double x[3] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));

    TEST_ASSERT_NOT_NULL(m->start_col_status);
    TEST_ASSERT_NOT_NULL(m->start_row_status);

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_progress_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_model *cold = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(cold));
    double cobj = 0.0, robj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(cold, &cobj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &robj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, cobj, robj);

    jaos_model_free(m);
    jaos_model_free(cold);
}

static void test_configuration_survives_a_load(void)
{
    int hits = 0;
    watcher w = fresh_watcher(-1);
    jaos_model *m = fresh();

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1000000));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-8));

    const double c[] = {-1.0, -2.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {4.0, 4.0, 4.0};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {5.0, 6.0};
    const int64_t as[] = {0, 2, 4, 6}, ai[] = {0, 1, 0, 1, 0, 1};
    const double av[] = {1.0, 2.0, 2.0, 1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));

    TEST_ASSERT_EQUAL_INT64(1000000, m->cfg.work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-8, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_LOG_DETAIL, m->cfg.log_level);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_TRUE(hits > 0);
    TEST_ASSERT_TRUE(w.calls > 0);
    jaos_model_free(m);
}

static void test_solve_time_is_reported_and_retired(void)
{
    jaos_model *m = log_model();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jaos_solve_time(m));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jaos_solve_time(nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const double t = jaos_solve_time(m);
    TEST_ASSERT_TRUE(t >= 0.0);
    TEST_ASSERT_TRUE(isfinite(t));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 0.0, 1.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jaos_solve_time(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));
    jaos_model_free(m);
}

static void test_queries_before_a_solve(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));

    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(nullptr));
    jaos_model_free(m);
}

static void test_duplicate_rows_reach_the_same_optimum(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, 2.0}, ru[] = {INFINITY, INFINITY};
    const int64_t as[] = {0, 2, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

static void test_a_row_that_is_the_sum_of_two_others(void)
{
    const double c[] = {1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {5.0, 5.0, 5.0};
    const double rl[] = {2.0, 2.0, 4.0};
    const double ru[] = {INFINITY, INFINITY, INFINITY};
    const int64_t as[] = {0, 2, 5, 7};
    const int64_t ai[] = {0, 2, 0, 1, 2, 1, 2};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     7, as, ai, av));
    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

static void test_dependent_rows_that_contradict_each_other(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {3.0, -INFINITY}, ru[] = {INFINITY, 1.0};
    const int64_t as[] = {0, 2, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_row_no_column_reaches(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {1.0}, ru[] = {2.0};
    const int64_t as[] = {0, 0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     0, as, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_row_no_column_reaches_but_that_holds_anyway(void)
{
    const double c[] = {1.0};
    const double cl[] = {-3.0}, cu[] = {1.0};
    const double rl[] = {-1.0}, ru[] = {2.0};
    const int64_t as[] = {0, 0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     0, as, nullptr, nullptr));
    solve_and_verify(m, -3.0);
    jaos_model_free(m);
}

static void test_the_basis_names_which_rows_hold_the_optimum(void)
{
    const double c[] = {2.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 100.0, 1.5};

    const int64_t as[] = {0, 3, 5};
    const int64_t ai[] = {0, 1, 2, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    solve_and_verify(m, 4.5);

    jaos_basis_status cs[2], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[2]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, nullptr));
    jaos_model_free(m);
}

static void test_the_basis_agrees_with_the_values_it_came_with(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    solve_and_verify(m, 29.0);

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)nc, sizeof *x);
    double *act = calloc((size_t)nr, sizeof *act);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(act);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, act, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    int64_t basic = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] == JAOS_BASIS_BASIC)
            basic++;
        else if (cs[j] == JAOS_BASIS_AT_LOWER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->col_lower[j], x[j]);
        else if (cs[j] == JAOS_BASIS_AT_UPPER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->col_upper[j], x[j]);
        else
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[j]);
    }
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i] == JAOS_BASIS_BASIC)
            basic++;
        else if (rs[i] == JAOS_BASIS_AT_LOWER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->row_lower[i], act[i]);
        else if (rs[i] == JAOS_BASIS_AT_UPPER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->row_upper[i], act[i]);
        else
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, act[i]);
    }
    TEST_ASSERT_EQUAL_INT64(nr, basic);

    free(x);
    free(act);
    free(cs);
    free(rs);
    jaos_model_free(m);
}

static void test_the_basis_is_refused_where_no_simplex_ran(void)
{
    jaos_basis_status cs[2], rs[1];
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(nullptr, cs, rs));

    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 1.0};
    const double rl[] = {5.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
#if defined(JAOS_NO_PRESOLVE)
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic = 0;
    for (int64_t j = 0; j < 2; j++) basic += cs[j] == JAOS_BASIS_BASIC;
    basic += rs[0] == JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT64(1, basic);
#else
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
#endif
    jaos_model_free(m);
}

static void load_warm_model(jaos_model *m, double r2_upper)
{
    const double c[] = {2.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 100.0, r2_upper};
    const int64_t as[] = {0, 3, 5};
    const int64_t ai[] = {0, 1, 2, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
}

static void test_re_solving_an_unchanged_model_costs_no_iterations(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    solve_and_verify(m, 4.5);
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);

    double first[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, first, nullptr, nullptr, nullptr));

    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    double again[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, again, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(first, again, sizeof first);
    jaos_model_free(m);
}

static void test_a_warm_re_solve_agrees_with_a_cold_one(void)
{
    jaos_model *warm = fresh();
    load_warm_model(warm, 1.5);
    solve_and_verify(warm, 4.5);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(warm, 2, -INFINITY, 1.0));
    solve_and_verify(warm, 5.0);

    jaos_model *cold = fresh();
    load_warm_model(cold, 1.0);
    solve_and_verify(cold, 5.0);

    double a[2], b[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(warm, a, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(cold, b, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, b[0], a[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, b[1], a[1]);

    jaos_model_free(warm);
    jaos_model_free(cold);
}

static void test_a_basis_handed_in_must_be_a_basis(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    jaos_basis_status cs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    jaos_basis_status rs[3] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_BASIC,
                               JAOS_BASIS_AT_UPPER};

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_basis(nullptr, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_basis(m, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_basis(m, cs, nullptr));

    jaos_basis_status bc[2], br[3];
    memcpy(bc, cs, sizeof cs);
    memcpy(br, rs, sizeof rs);
    bc[0] = JAOS_BASIS_AT_LOWER;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_basis(m, bc, br));

    memcpy(bc, cs, sizeof cs);
    br[0] = JAOS_BASIS_BASIC;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_basis(m, bc, br));

    memcpy(br, rs, sizeof rs);
    bc[1] = (jaos_basis_status)17;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_basis(m, bc, br));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));
    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    jaos_model_free(m);
}

static void load_unreducible_model(jaos_model *m)
{
    const double c[] = {1.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY};
    const double ru[] = {INFINITY, 10.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
}

static void test_the_primal_reaches_the_optimum_from_a_feasible_basis(void)
{
    int hits = 0;
    jaos_model *m = fresh();
    load_unreducible_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    g_log_last[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));

    m->cfg.force_primal = true;
    solve_and_verify(m, 2.0);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(g_log_last, "primal iterations"),
                                 "the summary must count them at all");
    TEST_ASSERT_NULL_MESSAGE(strstr(g_log_last, " 0 primal iterations"),
                             "the primal method did not take a single pivot");
    jaos_model_free(m);
}

static void test_the_primal_and_the_dual_agree_on_the_same_model(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    int hits = 0;
    char dual_line[256], primal_line[256];
    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};

    jaos_model *d = fresh();
    load_unreducible_model(d);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(d, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(d, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(d, JAOS_LOG_SUMMARY));
    g_log_last[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(d));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(d));
    snprintf(dual_line, sizeof dual_line, "%s", g_log_last);
    double obj_d = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(d, &obj_d));

    jaos_model *p = fresh();
    load_unreducible_model(p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(p, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(p, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(p, JAOS_LOG_SUMMARY));
    g_log_last[0] = '\0';
    p->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(p));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(p));
    snprintf(primal_line, sizeof primal_line, "%s", g_log_last);
    double obj_p = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(p, &obj_p));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, obj_d, obj_p);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(dual_line, " 0 primal iterations"),
                                 "the dual solve took primal iterations");
    TEST_ASSERT_NULL_MESSAGE(strstr(primal_line, " 0 primal iterations"),
                             "the primal solve took no primal iterations");
    jaos_model_free(d);
    jaos_model_free(p);
#endif
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && \
    !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static bool g_saw_phase1_finish;

static void watch_phase1_log(void *user, jaos_log_level level,
                             const char *line)
{
    (void)level;
    *(int *)user += 1;
    if (strstr(line, "phase 1 reached a feasible") != nullptr)
        g_saw_phase1_finish = true;
}
#endif

static void test_a_watcher_can_stop_the_primal_phase_1(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    int hits = 0;
    watcher w = fresh_watcher(0);
    jaos_model *m = fresh();
    load_unreducible_model(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_log_callback(m, watch_phase1_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    g_saw_phase1_finish = false;
    m->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m),
        "the watcher asked to stop and the solve did not");
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, w.calls,
        "the watcher was never called at all");
    TEST_ASSERT_FALSE_MESSAGE(g_saw_phase1_finish,
        "the stop came after phase 1 finished, so it was not phase 1's");
    jaos_model_free(m);
#endif
}

static void test_the_summary_separates_phase_1_from_phase_2(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test -- skipped under either fault build");
#else

    jaos_model *d = fresh();
    load_unreducible_model(d);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(d));
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, d->solve_primal_iters,
                                    "the dual took primal iterations");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, d->solve_phase1_iters,
                                    "the dual entered a primal phase 1");
    jaos_model_free(d);

    jaos_model *p = fresh();
    load_unreducible_model(p);
    p->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(p));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(p));

    TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(0, p->solve_phase1_iters,
        "phase 1 ran from a primal infeasible start and reported none");

    TEST_ASSERT_TRUE_MESSAGE(p->solve_phase1_iters <= p->solve_primal_iters,
        "more phase-1 iterations than primal iterations");

    TEST_ASSERT_TRUE_MESSAGE(p->solve_primal_iters <= p->solve_iters,
        "more primal iterations than iterations");
    jaos_model_free(p);
#endif
}

static void test_the_counts_belong_to_the_solve_that_just_ran(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test -- skipped under either fault build");
#else
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {10.0, -INFINITY};
    const double ru[] = {INFINITY, 3.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));

    (void)jaos_solve(m);
    const int64_t carried = m->solve_iters;
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, m->solve_primal_iters,
        "the dual took primal iterations");

    jaos_clear_basis(m);
    m->cfg.force_primal = true;
    const jaos_status st = jaos_solve(m);

    TEST_ASSERT_TRUE_MESSAGE(m->solve_phase1_iters >= 0,
        "a negative phase-1 count");
    TEST_ASSERT_TRUE_MESSAGE(
        m->solve_phase1_iters <= m->solve_primal_iters,
        "more phase-1 iterations than primal iterations");
    TEST_ASSERT_TRUE_MESSAGE(m->solve_primal_iters <= m->solve_iters,
        "more primal iterations than iterations");

    if (st != JAOS_OK) {
        TEST_ASSERT_TRUE_MESSAGE(m->solve_primal_iters > 0,
            "the primal was forced and reported no primal iterations");

        const char *why = jaos_model_error(m);
        if (why != nullptr &&
            strstr(why, "the primal phase 1 cannot reduce") != nullptr)
            TEST_ASSERT_EQUAL_INT64_MESSAGE(m->solve_primal_iters,
                m->solve_iters,
                "a refused primal solve reports a dual re-entry that never ran");
    }
    (void)carried;
    jaos_model_free(m);
#endif
}

static void load_boxed_model(jaos_model *m)
{
    const double c[]  = {-1.0, -0.5};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 10.0};
    const double rl[] = {-INFINITY, -INFINITY};
    const double ru[] = {10.0, 12.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
}

static void test_the_entering_column_stops_at_its_own_bound(void)
{
    jaos_model *m = fresh();
    load_boxed_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    m->cfg.force_primal = true;
    solve_and_verify(m, -3.75);

    double x[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE_MESSAGE(x[0] <= 1.0 + 1e-9,
                             "the entering column walked past its own bound");
    TEST_ASSERT_TRUE_MESSAGE(x[0] >= -1e-9, "and below its lower one");
    jaos_model_free(m);
}

static void test_the_primal_phase_1_repairs_an_infeasible_start(void)
{
    int hits = 0;
    jaos_model *m = fresh();
    load_unreducible_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    g_log_last[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));

    m->cfg.force_primal = true;
    solve_and_verify(m, 2.0);
    TEST_ASSERT_NULL_MESSAGE(strstr(g_log_last, " 0 primal iterations"),
                             "phase 1 did not take a single pivot");
    jaos_model_free(m);
}

static void test_the_primal_refuses_to_call_a_model_infeasible(void)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {10.0, -INFINITY};
    const double ru[] = {INFINITY, 3.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));

    m->cfg.force_primal = true;
    jaos_status st = jaos_solve(m);

    if (st == JAOS_OK) {
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    } else {
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL, st);
        const char *why = jaos_model_error(m);
        TEST_ASSERT_NOT_NULL(why);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(why, "D19"),
                                     "the refusal must say what it is");
    }
    jaos_model_free(m);
}

static void test_the_dual_agrees_from_the_same_infeasible_start(void)
{
    jaos_model *m = fresh();
    load_unreducible_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

static void test_a_hostile_basis_costs_iterations_and_not_the_answer(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[3] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC,
                               JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    solve_and_verify(m, 4.5);
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    jaos_model_free(m);
}

static void test_a_free_nonbasic_with_a_negative_reduced_cost_is_repaired(void)
{
    const double inf = jaos_infinity();
    const double cost[3] = {-1.0, 0.0, 0.0};
    const double lo[3]   = {-inf, 0.0, 0.0};
    const double up[3]   = { inf, 2.0, 1.0};
    const double rlo[2]  = { 2.0, 0.0};
    const double rup[2]  = { 6.0, 1.0};
    const int64_t st[4]  = {0, 1, 2, 3};
    const int64_t idx[3] = {0, 0, 1};
    const double val[3]  = {1.0, 2.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, cost, lo, up, rlo, rup,
                     3, st, idx, val));

    solve_and_verify(m, -6.0);

    jaos_basis_status cs[3] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC,
                               JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    solve_and_verify(m, -6.0);
    jaos_model_free(m);
}

static char g_start_line[80];

static void collect_start(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    if (strncmp(line, "starting from", 13) == 0)
        snprintf(g_start_line, sizeof g_start_line, "%s", line);
}

static void watch_the_start(jaos_model *m)
{
    g_start_line[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_log_callback(m, collect_start, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
}

static void test_a_status_whose_bound_was_retired(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);
    solve_and_verify(m, 4.5);

    jaos_basis_status rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[2]);

    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 2, 1.0, INFINITY));
    solve_and_verify(m, 4.0);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the basis on the model"));
    jaos_model_free(m);

    m = fresh();
    load_warm_model(m, 1.5);
    solve_and_verify(m, 4.5);

    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_row_bounds(m, 2, -INFINITY, INFINITY));
    solve_and_verify(m, 4.0);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the basis on the model"));
    jaos_model_free(m);
}

static void test_clearing_the_basis_makes_the_next_solve_cold(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    solve_and_verify(m, 4.5);
    const int64_t cold = jaos_iterations(m);
    TEST_ASSERT_TRUE(cold > 0);

    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    jaos_clear_basis(m);
    watch_the_start(m);
    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(cold, jaos_iterations(m));
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the slack basis"));

    jaos_clear_basis(m);
    jaos_clear_basis(nullptr);
    jaos_model_free(m);
}

static void test_a_budget_stop_can_be_resumed(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    solve_and_verify(m, 4.5);
    const int64_t whole_work = jaos_work_units(m);
    const int64_t whole_iters = jaos_iterations(m);

    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, whole_work / 2));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);

    double obj = 0.0;
    jaos_basis_status cs[2], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    {
        int64_t basic = 0;
        for (int64_t j = 0; j < jaos_num_col(m); j++)
            basic += cs[j] == JAOS_BASIS_BASIC;
        for (int64_t i = 0; i < jaos_num_row(m); i++)
            basic += rs[i] == JAOS_BASIS_BASIC;
        TEST_ASSERT_EQUAL_INT64(jaos_num_row(m), basic);
    }

    TEST_ASSERT_NOT_NULL(m->start_col_status);

    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 0));
    solve_and_verify(m, 4.5);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the basis on the model"));
    TEST_ASSERT_TRUE(jaos_iterations(m) < whole_iters);
    jaos_model_free(m);
}

static void test_a_warm_basis_of_empty_columns_factors_and_is_infeasible(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {6.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    solve_and_verify(m, 3.0);

    jaos_basis_status cs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

#define ACT_K 256
#define ACT_NC (ACT_K + 5)
#define ACT_NNZ (ACT_K + 6)

static jaos_model *make_lost_terms_model(double slack)
{
    static double c[ACT_NC], cl[ACT_NC], cu[ACT_NC], av[ACT_NNZ];
    static int64_t as[ACT_NC + 1], ai[ACT_NNZ];
    const double t = ldexp(1.0, -25);
    const double bound = (double)ACT_K * t + 1e-7 + slack;
    const double rl[] = {bound, -1e9}, ru[] = {bound, -1e9};
    int64_t nz = 0;

    for (int64_t j = 0; j < ACT_NC; j++) {
        as[j] = nz;
        c[j] = 0.0;
        if (j == 0) {
            cl[j] = cu[j] = 1e9;
            ai[nz] = 0; av[nz++] = 1.0;
        } else if (j == 1) {
            cl[j] = -1e9 - 1.0; cu[j] = -1e9 + 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
            ai[nz] = 1; av[nz++] = 1.0;
        } else if (j < ACT_K + 2) {
            cl[j] = cu[j] = t;
            ai[nz] = 0; av[nz++] = 1.0;
        } else if (j < ACT_K + 4) {
            cl[j] = 0.0; cu[j] = 2e-7; c[j] = 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
        } else {
            cl[j] = cu[j] = 0.0;
            ai[nz] = 1; av[nz++] = 1.0;
        }
    }
    as[ACT_NC] = nz;

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ACT_NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av));
    return m;
}

static void test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_lost_terms_model(0.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(5e-8, 1.1e-7, obj);
    jaos_model_free(m);
#endif
}

static void test_a_row_activity_still_refuses_a_real_shortfall(void)
{
    jaos_model *m = make_lost_terms_model(1e-2);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);

    m = make_lost_terms_model(5e-6);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_pattern_order_sorts_deduplicates_and_leaves_no_marks(void)
{
    constexpr int64_t limit = 10;
    constexpr int64_t words_needed = (limit + 63) / 64;
    uint64_t mark[words_needed];
    memset(mark, 0, sizeof mark);

    int64_t pos[] = {5, 2, 5, 0, 9, 2, -1, 10, 100, 9};
    int64_t words = -1;
    int64_t n = jm_pattern_order(10, pos, mark, limit, &words);

    TEST_ASSERT_EQUAL_INT64(4, n);
    TEST_ASSERT_EQUAL_INT64(0, pos[0]);
    TEST_ASSERT_EQUAL_INT64(2, pos[1]);
    TEST_ASSERT_EQUAL_INT64(5, pos[2]);
    TEST_ASSERT_EQUAL_INT64(9, pos[3]);
    TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(0, words,
        "no bitmap word was billed, so nothing was enumerated");

    for (int64_t w = 0; w < words_needed; w++)
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, mark[w],
            "a bit was left set, so the next caller starts dirty");

    int64_t again[] = {9, 9, 0, 2, 5};
    int64_t words2 = -1;
    int64_t n2 = jm_pattern_order(5, again, mark, limit, &words2);
    TEST_ASSERT_EQUAL_INT64(4, n2);
    TEST_ASSERT_EQUAL_INT64(0, again[0]);
    TEST_ASSERT_EQUAL_INT64(2, again[1]);
    TEST_ASSERT_EQUAL_INT64(5, again[2]);
    TEST_ASSERT_EQUAL_INT64(9, again[3]);
    for (int64_t w = 0; w < words_needed; w++)
        TEST_ASSERT_EQUAL_UINT64(0, mark[w]);

    int64_t none[1] = {0};
    int64_t words3 = -1;
    TEST_ASSERT_EQUAL_INT64(0, jm_pattern_order(0, none, mark, limit, &words3));
    for (int64_t w = 0; w < words_needed; w++)
        TEST_ASSERT_EQUAL_UINT64(0, mark[w]);
}

static void test_the_nonbasic_bitmap_matches_a_rebuild_after_a_basis_change(void)
{
    constexpr int64_t nvar = 130;
    constexpr int64_t words_needed = (nvar + 63) / 64;
    jm_var_status status[nvar];
    int64_t want_nonbasic = 0;
    for (int64_t v = 0; v < nvar; v++) {
        switch (v % 4) {
        case 0:  status[v] = JM_BASIC;    break;
        case 1:  status[v] = JM_AT_LOWER; break;
        case 2:  status[v] = JM_AT_UPPER; break;
        default: status[v] = JM_FREE;     break;
        }
        if (status[v] != JM_BASIC)
            want_nonbasic++;
    }

    uint64_t kept[words_needed], rebuilt[words_needed];
    memset(kept, 0, sizeof kept);
    memset(rebuilt, 0, sizeof rebuilt);

    const int64_t n0 = jm_nonbasic_build(nvar, status, kept);
    TEST_ASSERT_EQUAL_INT64(want_nonbasic, n0);

    const int64_t entering = 65;
    const int64_t leaving  = 64;
    TEST_ASSERT_EQUAL_INT(JM_AT_LOWER, status[entering]);
    TEST_ASSERT_EQUAL_INT(JM_BASIC, status[leaving]);

    status[entering] = JM_BASIC;
    jm_nonbasic_remove(kept, entering);
    status[leaving] = JM_AT_UPPER;
    jm_nonbasic_insert(kept, leaving);

    const int64_t n1 = jm_nonbasic_build(nvar, status, rebuilt);
    TEST_ASSERT_EQUAL_INT64(n0, n1);
    for (int64_t w = 0; w < words_needed; w++)
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(rebuilt[w], kept[w],
            "the hand-maintained bitmap drifted from a rebuild");

    int64_t a[nvar], b[nvar];
    const int64_t na = jm_nonbasic_expand(nvar, kept, a);
    const int64_t nb = jm_nonbasic_expand(nvar, rebuilt, b);
    TEST_ASSERT_EQUAL_INT64(n1, na);
    TEST_ASSERT_EQUAL_INT64(n1, nb);
    for (int64_t k = 0; k < na; k++) {
        TEST_ASSERT_EQUAL_INT64(b[k], a[k]);
        if (k > 0)
            TEST_ASSERT_TRUE_MESSAGE(a[k - 1] < a[k],
                "jm_nonbasic_expand did not come back ascending");
    }

    uint64_t stale[words_needed];
    memset(stale, 0, sizeof stale);
    jm_var_status before[nvar];
    memcpy(before, status, sizeof before);
    before[entering] = JM_AT_LOWER;
    before[leaving] = JM_BASIC;
    (void)jm_nonbasic_build(nvar, before, stale);
    bool differs = false;
    for (int64_t w = 0; w < words_needed; w++)
        differs = differs || stale[w] != rebuilt[w];
    TEST_ASSERT_TRUE_MESSAGE(differs,
        "the basis change moved no bit, so this test compares nothing");
}

static void test_the_iteration_split_is_written_on_an_interrupted_exit(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test -- skipped under either fault build");
#else

    jaos_model *full = fresh();
    load_unreducible_model(full);
    full->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(full));
    const int64_t full_primal = full->solve_primal_iters;
    TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(0, full_primal,
        "the forced primal ran no iterations, so there is nothing to split");

    watcher w = fresh_watcher(0);
    jaos_model *m = fresh();
    load_unreducible_model(m);
    m->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m),
        "the watcher asked to stop and the solve did not");
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, w.calls,
        "the watcher was never called, so nothing was interrupted");

    TEST_ASSERT_EQUAL_INT64_MESSAGE(jaos_iterations(m), m->solve_primal_iters,
        "an interrupted forced-primal solve did not write its primal count");
    TEST_ASSERT_TRUE_MESSAGE(
        m->solve_phase1_iters >= 0 &&
        m->solve_phase1_iters <= m->solve_primal_iters,
        "phase 1 claims more iterations than the primal ran");
    TEST_ASSERT_TRUE_MESSAGE(m->solve_primal_iters <= full_primal,
        "the interrupted solve ran past the complete one");

    jaos_model_free(m);
    jaos_model_free(full);
#endif
}

static void solved_both_ways_as_unbounded(jaos_model *m)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m),
                                  "the dual, which is the oracle here");
    m->cfg.force_primal = true;
    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_solve(m),
                                  jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m),
                                  "the primal must agree with the dual");
}

static void test_the_primal_declares_a_ray_it_meets_in_phase_2(void)
{
    const double c[] = {-1.0, 0.0, 0.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY, 1.0};
    const double rl[] = {0.0, 3.0}, ru[] = {0.0, INFINITY};
    const int64_t as[] = {0, 1, 3, 4};
    const int64_t ai[] = {0, 0, 1, 1};
    const double av[] = {1.0, -1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solved_both_ways_as_unbounded(m);
    jaos_model_free(m);
}

static void test_the_primal_declares_a_ray_through_a_free_column(void)
{
    const double c[] = {1.0, 0.0};
    const double cl[] = {-INFINITY, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {0.0}, ru[] = {0.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solved_both_ways_as_unbounded(m);
    jaos_model_free(m);
}

static void test_a_bounded_neighbour_of_that_model_is_not_a_ray(void)
{
    const double c[] = {-1.0, 0.0, 0.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY, 1.0};
    const double rl[] = {3.0, 3.0}, ru[] = {3.0, INFINITY};
    const int64_t as[] = {0, 1, 3, 4};
    const int64_t ai[] = {0, 0, 1, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.0, obj);
#endif

    m->cfg.force_primal = true;
    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_solve(m),
                                  jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.0, obj);
#endif
    (void)obj;
    jaos_model_free(m);
}

static void test_a_ray_needing_two_columns_is_answered(void)
{
    const double c[] = {-1.0, -1.0};
    const double cl[] = {0.0, 0.0};
    const double rl[] = {0.0, 2.0}, ru[] = {0.0, INFINITY};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, -1.0, 1.0};

    const double caps[] = {10.0, 1e3, 1e6, 1e9};
    for (size_t k = 0; k < sizeof caps / sizeof *caps; k++) {
        const double cu[] = {caps[k], INFINITY};
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         4, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-6 * caps[k], -2.0 * caps[k], obj);
        jaos_model_free(m);
    }

    const double cu[] = {INFINITY, INFINITY};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solved_both_ways_as_unbounded(m);
    jaos_model_free(m);
}

static void test_two_held_columns_whose_sum_is_still_blocked(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve folds the "
                        "singleton rows into bounds; runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {-1.0, -1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {1e11, 1e11};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 1};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NUMERICAL_ERROR, jaos_status_of(m));
    const char *err = jaos_model_error(m);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "together"),
                                 "the refusal must say the combined "
                                 "direction was tried and blocked");
    jaos_model_free(m);
#endif
}

static void test_a_sub_tolerance_flip_gap_is_repaired_not_infeasible(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve answers this "
                        "model first; runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {1.0, 2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {2.0, 2.0 - 0x1p-40};
    const double rl[] = {4.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_OPTIMAL, jaos_status_of(m),
                                  "a sub-tolerance flip gap must repair, "
                                  "not refuse");
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, obj);

    double cv[2], ra[1], rd[1], cd[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, cv, ra, rd, cd));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_solution(m, cv, rd, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE_MESSAGE(rep.dual_feasible,
                             "the repair must publish a clean certificate");
    jaos_model_free(m);
#endif
}

static void test_a_real_flip_gap_past_tolerance_stays_infeasible(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve answers this "
                        "model first; runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {1.0, 2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {2.0, 1.0};
    const double rl[] = {4.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

static void test_a_fixed_column_is_not_a_flip_candidate(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve answers this "
                        "model first; runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {1.0, 2.0, 2.0};
    const double cl[] = {0.0, 0.0, 1.0};
    const double cu[] = {2.0, 2.0, 1.0};
    const double rl[] = {4.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2, 3}, ai[] = {0, 0, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, obj);

    jaos_basis_status cs[3], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_BASIS_AT_UPPER, cs[0],
                                  "the walk must have flipped x1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_BASIS_AT_LOWER, cs[2],
                                  "a fixed column never joins the walk, so "
                                  "its label never toggles");
    jaos_model_free(m);
#endif
}

static void test_a_row_repairable_only_by_fixed_columns_is_infeasible(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve answers this "
                        "model first; runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
    const double c[] = {1.0, 0.0};
    const double cl[] = {1.0, 0.5};
    const double cu[] = {1.0, 0.5};
    const double rl[] = {4.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
#endif
}

static void test_every_work_limit_stops_honestly_and_resumes(void)
{
    const double c[] = {-1.0, -2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    for (int method = 0; method < 2; method++) {
        bool reached_optimal = false;
        for (int64_t lim = 1; lim < INT64_C(1) << 40; lim *= 4) {
            jaos_model *m = fresh();
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu,
                             rl, ru, 2, as, ai, av));
            m->cfg.force_primal = method == 1;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, lim));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            jaos_solve_status st = jaos_status_of(m);
            TEST_ASSERT_TRUE_MESSAGE(st == JAOS_SOLVE_WORK_LIMIT ||
                                     st == JAOS_SOLVE_OPTIMAL,
                                     "a budget stop must say so, on every "
                                     "limit value");
            if (st == JAOS_SOLVE_WORK_LIMIT) {
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 0));
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
                TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_OPTIMAL,
                                              jaos_status_of(m),
                                              "a stop must resume");
            }
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, -8.0, obj);
            bool done = st == JAOS_SOLVE_OPTIMAL;
            jaos_model_free(m);
            if (done) {
                reached_optimal = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(reached_optimal);
    }
}

static void test_a_ray_whose_direction_cancels_in_row_space(void)
{
    const double c[] = {-1.0, -1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {0.0, -INFINITY}, ru[] = {0.0, 5.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, -1.0, -1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solved_both_ways_as_unbounded(m);
    jaos_model_free(m);
}

static jaos_model *make_inverted(bool row)
{
    const double c[]  = {2.0, 3.0, 1.0, 4.0};
    const double cl[] = {0.0, 0.0, row ? 0.0 : 2.0, 0.0};
    const double cu[] = {5.0, 5.0, row ? 5.0 : 1.0, 5.0};
    const double rl[] = {2.0, -3.0, 1.0}, ru[] = {row ? 1.0 : 8.0, 3.0, 5.0};
    const int64_t s[]  = {0, 3, 5, 7, 10};
    const int64_t ix[] = {0, 1, 2, 0, 1, 0, 2, 0, 1, 2};
    const double v[]   = {1.0, 1.0, 2.0, 1.0, -1.0, 1.0, 1.0, 1.0, 2.0, -1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     10, s, ix, v));
    return m;
}

static void expect_trivially_infeasible(jaos_model *m)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));

    double y[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_certificate(m, y));
    jaos_basis_status cs[4], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
}

static void test_an_inverted_column_box_is_infeasible(void)
{
    jaos_model *m = make_inverted(false);
    expect_trivially_infeasible(m);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 2, 0.0, 5.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && \
    !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static void assert_basis_is_of_the_model(jaos_model *m, int64_t nc,
                                         int64_t nr, const double *cl,
                                         const double *cu, const double *rl,
                                         const double *ru)
{
    double *x = malloc((size_t)nc * sizeof *x);
    double *cd = malloc((size_t)nc * sizeof *cd);
    double *act = malloc((size_t)nr * sizeof *act);
    double *rd = malloc((size_t)nr * sizeof *rd);
    jaos_basis_status *cs = malloc((size_t)nc * sizeof *cs);
    jaos_basis_status *rs = malloc((size_t)nr * sizeof *rs);
    TEST_ASSERT_NOT_NULL(x); TEST_ASSERT_NOT_NULL(cd);
    TEST_ASSERT_NOT_NULL(act); TEST_ASSERT_NOT_NULL(rd);
    TEST_ASSERT_NOT_NULL(cs); TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, rd, cd));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    int64_t basics = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] == JAOS_BASIS_BASIC) basics++;
        TEST_ASSERT_FALSE_MESSAGE(
            cs[j] == JAOS_BASIS_AT_LOWER && cl[j] == -INFINITY,
            "a column is nonbasic on a lower bound the model has not got");
        TEST_ASSERT_FALSE_MESSAGE(
            cs[j] == JAOS_BASIS_AT_UPPER && cu[j] == INFINITY,
            "a column is nonbasic on an upper bound the model has not got");
        TEST_ASSERT_TRUE_MESSAGE(x[j] >= cl[j] - CHECK_TOL &&
                                 x[j] <= cu[j] + CHECK_TOL,
                                 "a published column value is outside its "
                                 "own box");
    }
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i] == JAOS_BASIS_BASIC) basics++;
        TEST_ASSERT_FALSE_MESSAGE(
            rs[i] == JAOS_BASIS_AT_LOWER && rl[i] == -INFINITY,
            "a row is nonbasic on a lower bound the model has not got");
        TEST_ASSERT_FALSE_MESSAGE(
            rs[i] == JAOS_BASIS_AT_UPPER && ru[i] == INFINITY,
            "a row is nonbasic on an upper bound the model has not got");
    }
    TEST_ASSERT_EQUAL_INT64_MESSAGE(nr, basics,
                                    "jaos_basis promises num_row basics");

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_check_solution(m, x, rd, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    free(x); free(cd); free(act); free(rd); free(cs); free(rs);
}
#endif

static void test_a_loan_nobody_holds_is_retired_before_publishing(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[] = {1.0, 0.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY, -INFINITY, -INFINITY};
    const double ru[] = {1.0, 3.0, 1.0, 4.0};
    const int64_t as[] = {0, 3, 5, 9};
    const int64_t ai[] = {0, 1, 2, 1, 2, 0, 1, 2, 3};
    const double av[] = {-2.0, 2.0, 1.0, 1.0, -2.0, 2.0, -2.0, -1.0, -1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 4, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     9, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -0.5, obj);
    assert_basis_is_of_the_model(m, 3, 4, cl, cu, rl, ru);

    double x[3], act[4], rd[4], cd[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, rd, cd));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, x[2]);
    jaos_model_free(m);
#endif
}

static void test_a_retired_loan_leaves_a_basis_of_the_model(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[] = {0.0, 1.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY, -INFINITY, -INFINITY};
    const double ru[] = {5.0, 5.0, 0.0, 5.0};
    const int64_t as[] = {0, 2, 5, 9};
    const int64_t ai[] = {0, 2, 0, 1, 2, 0, 1, 2, 3};
    const double av[] = {1.0, -1.0, -2.0, 2.0, 1.0, 2.0, -2.0, -2.0, -1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 4, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     9, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.5, obj);
    assert_basis_is_of_the_model(m, 3, 4, cl, cu, rl, ru);

    double x[3], act[4], rd[4], cd[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, rd, cd));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.5, x[2]);
    jaos_model_free(m);
#endif
}

static void test_an_inverted_row_box_is_infeasible(void)
{
    jaos_model *m = make_inverted(true);
    expect_trivially_infeasible(m);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, 2.0, 8.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, 2.0, 2.0 - 1e-4));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_the_algorithm_is_a_caller_option(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_DUAL, jaos_algorithm_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_algorithm(m, (jaos_algorithm)3));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PRIMAL));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_PRIMAL, jaos_algorithm_of(m));
    TEST_ASSERT_TRUE(m->cfg.force_primal);
    const double cost[2] = {1.0, 2.0}, cl[2] = {0.0, 0.0}, cu[2] = {5.0, 5.0};
    const double rl[1] = {2.0}, ru[1] = {5.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0,
                                                cost, cl, cu, rl, ru,
                                                2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_PRIMAL, jaos_algorithm_of(m));
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    for (int pass = 0; pass < 2; pass++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(
            m, pass ? JAOS_ALGORITHM_DUAL : JAOS_ALGORITHM_PRIMAL));
        jaos_clear_basis(m);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[2];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 0.0);
    }
#endif
    jaos_model_free(m);
}

static void test_devex_and_dantzig_pricing_reach_the_same_optimum(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test, skipped under either fault build");
#else
    const double cost[4] = {-1.0, -2.0, -3.0, 1.0};
    const double cl[4] = {0.0, 0.0, 0.0, 0.0}, cu[4] = {4.0, 4.0, 4.0, 4.0};
    const double rl[3] = {-INFINITY, -INFINITY, 1.0};
    const double ru[3] = {6.0, 7.0, INFINITY};
    const int64_t as[5] = {0, 3, 6, 8, 10};
    const int64_t ai[10] = {0, 1, 2, 0, 1, 2, 0, 1, 1, 2};
    const double av[10] = {1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 2.0, 1.0, 1.0, 1.0};
    double obj[2] = {0.0, 0.0};
    int64_t iters[2] = {0, 0};
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_load_lp(m, 4, 3, JAOS_MINIMIZE, 0.0,
                                                    cost, cl, cu, rl, ru,
                                                    10, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PRIMAL));
        m->cfg.primal_dantzig = pass == 1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[pass]));
        iters[pass] = jaos_iterations(m);
        double x[4], y[3];
        jaos_check_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
        TEST_ASSERT_TRUE(rep.primal_feasible && rep.dual_feasible);
        jaos_model_free(m);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, obj[1], obj[0]);
    TEST_ASSERT_TRUE(iters[0] > 0 && iters[1] > 0);
#endif
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_minimise_over_a_ge_row);
    RUN_TEST(test_maximise_with_two_rows);
    RUN_TEST(test_equality_row_with_a_capped_column);
    RUN_TEST(test_ranged_row);
    RUN_TEST(test_objective_offset_is_carried);
    RUN_TEST(test_infeasible_model_is_reported);
    RUN_TEST(test_three_by_three);
    RUN_TEST(test_solving_a_model_read_from_mps);
    RUN_TEST(test_unbounded_model_is_reported);
    RUN_TEST(test_missing_bound_that_a_row_restrains_solves_normally);
    RUN_TEST(test_a_ray_is_what_proves_unbounded);
    RUN_TEST(test_an_optimum_past_the_lent_bound_is_refused);
    RUN_TEST(test_a_lent_bound_that_never_constrained_anything);
    RUN_TEST(test_optimality_is_rechecked_before_it_is_believed);
    RUN_TEST(test_maximise_without_column_upper_bounds);
    RUN_TEST(test_t1_mps_is_infeasible_and_says_so);
    RUN_TEST(test_zero_objective_is_distinguishable_from_no_answer);
    RUN_TEST(test_a_long_solve_crosses_a_refactorization);
    RUN_TEST(test_free_variable_enters_and_settles);
    RUN_TEST(test_work_accounting_is_pinned);
    RUN_TEST(test_simultaneous_violations_of_wildly_different_size);
    RUN_TEST(test_dse_weights_match_recomputed_norms);
    RUN_TEST(test_dse_repairs_a_carried_weight_that_slipped);
    RUN_TEST(test_dse_restarts_when_the_carried_weight_has_drifted);
    RUN_TEST(test_scaling_changes_the_arithmetic_not_the_answer);
    RUN_TEST(test_answers_come_back_in_the_models_units);
    RUN_TEST(test_bound_flipping_fills_columns_in_one_step);
    RUN_TEST(test_settling_up_reaches_the_optimum_a_shifted_basis_hid);
    RUN_TEST(test_a_clean_up_pass_dispatches_every_column_it_identified);
    RUN_TEST(test_harris_ignores_a_big_pivot_outside_the_window);
    RUN_TEST(test_harris_prefers_the_larger_pivot_inside_the_window);
    RUN_TEST(test_harris_on_a_degenerate_vertex_takes_the_best_pivot);
    RUN_TEST(test_harris_edge_counts);
    RUN_TEST(test_bland_on_a_degenerate_vertex_takes_the_lowest_index);
    RUN_TEST(test_bland_has_no_window_to_trade);
    RUN_TEST(test_bland_does_not_let_the_index_beat_the_quotient);
    RUN_TEST(test_bland_edge_counts);
    RUN_TEST(test_bland_compares_the_minimum_exactly_at_one_ulp);
    RUN_TEST(test_nonbasic_build_on_no_variables_counts_zero);
    RUN_TEST(test_alloc_array_of_zero_is_not_a_failure);
    RUN_TEST(test_primal_bland_breaks_a_degenerate_tie_on_the_lowest_index);
    RUN_TEST(test_primal_without_bland_a_tie_keeps_the_first_row);
    RUN_TEST(test_primal_bland_does_not_let_the_index_beat_the_step);
    RUN_TEST(test_primal_bland_has_no_window);
    RUN_TEST(test_primal_bland_first_row_always_wins);
    RUN_TEST(test_primal_bland_variable_zero_is_an_index);
    RUN_TEST(test_primal_bland_takes_the_lowest_and_not_the_highest);
    RUN_TEST(test_primal_row_wins_minimum_survives_every_subset);
    RUN_TEST(test_primal_row_wins_dropping_the_winner_can_change_it);
    RUN_TEST(test_pattern_order_sorts_and_dedups);
    RUN_TEST(test_pattern_order_scans_only_the_touched_range);
    RUN_TEST(test_pattern_order_keeps_a_full_pattern);
    RUN_TEST(test_pattern_order_drops_what_it_cannot_hold);
    RUN_TEST(test_pattern_order_edge_counts);
    RUN_TEST(test_nonbasic_build_keeps_free_variables);
    RUN_TEST(test_nonbasic_expand_is_ascending_across_words);
    RUN_TEST(test_nonbasic_expand_handles_the_degenerate_counts);
    RUN_TEST(test_nonbasic_survives_interleaved_eviction);
    RUN_TEST(test_nonbasic_notices_a_missed_hook);
    RUN_TEST(test_solving_twice_is_bit_identical);
    RUN_TEST(test_work_limit_stops_and_reports);
    RUN_TEST(test_budgets_survive_a_reload);
    RUN_TEST(test_a_tolerance_must_be_a_tolerance);
    RUN_TEST(test_an_untouched_model_carries_no_tolerance_of_its_own);
    RUN_TEST(test_a_wide_primal_tolerance_accepts_a_point_it_should_not);
    RUN_TEST(test_logging_says_nothing_until_it_is_asked_to);
    RUN_TEST(test_a_level_outside_the_enum_is_refused);
    RUN_TEST(test_watching_a_solve_does_not_change_it);
    RUN_TEST(test_a_watcher_is_asked_and_changes_nothing);
    RUN_TEST(test_a_watcher_can_stop_a_solve_and_it_resumes);
    RUN_TEST(test_configuration_survives_a_load);
    RUN_TEST(test_solve_time_is_reported_and_retired);
    RUN_TEST(test_queries_before_a_solve);
    RUN_TEST(test_the_basis_names_which_rows_hold_the_optimum);
    RUN_TEST(test_the_basis_agrees_with_the_values_it_came_with);
    RUN_TEST(test_the_basis_is_refused_where_no_simplex_ran);
    RUN_TEST(test_re_solving_an_unchanged_model_costs_no_iterations);
    RUN_TEST(test_a_warm_re_solve_agrees_with_a_cold_one);
    RUN_TEST(test_a_basis_handed_in_must_be_a_basis);
    RUN_TEST(test_the_primal_reaches_the_optimum_from_a_feasible_basis);
    RUN_TEST(test_the_primal_and_the_dual_agree_on_the_same_model);
    RUN_TEST(test_a_watcher_can_stop_the_primal_phase_1);
    RUN_TEST(test_the_summary_separates_phase_1_from_phase_2);
    RUN_TEST(test_the_counts_belong_to_the_solve_that_just_ran);
    RUN_TEST(test_the_entering_column_stops_at_its_own_bound);
    RUN_TEST(test_the_primal_phase_1_repairs_an_infeasible_start);
    RUN_TEST(test_the_primal_refuses_to_call_a_model_infeasible);
    RUN_TEST(test_the_dual_agrees_from_the_same_infeasible_start);
    RUN_TEST(test_a_hostile_basis_costs_iterations_and_not_the_answer);
    RUN_TEST(test_a_free_nonbasic_with_a_negative_reduced_cost_is_repaired);
    RUN_TEST(test_a_status_whose_bound_was_retired);
    RUN_TEST(test_clearing_the_basis_makes_the_next_solve_cold);
    RUN_TEST(test_a_budget_stop_can_be_resumed);
    RUN_TEST(test_a_warm_basis_of_empty_columns_factors_and_is_infeasible);
    RUN_TEST(test_duplicate_rows_reach_the_same_optimum);
    RUN_TEST(test_a_row_that_is_the_sum_of_two_others);
    RUN_TEST(test_dependent_rows_that_contradict_each_other);
    RUN_TEST(test_a_row_no_column_reaches);
    RUN_TEST(test_a_row_no_column_reaches_but_that_holds_anyway);
    RUN_TEST(test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total);
    RUN_TEST(test_a_row_activity_still_refuses_a_real_shortfall);
    RUN_TEST(test_pattern_order_sorts_deduplicates_and_leaves_no_marks);
    RUN_TEST(test_the_nonbasic_bitmap_matches_a_rebuild_after_a_basis_change);
    RUN_TEST(test_the_iteration_split_is_written_on_an_interrupted_exit);
    RUN_TEST(test_the_primal_declares_a_ray_it_meets_in_phase_2);
    RUN_TEST(test_the_primal_declares_a_ray_through_a_free_column);
    RUN_TEST(test_a_bounded_neighbour_of_that_model_is_not_a_ray);
    RUN_TEST(test_a_ray_needing_two_columns_is_answered);
    RUN_TEST(test_two_held_columns_whose_sum_is_still_blocked);
    RUN_TEST(test_a_sub_tolerance_flip_gap_is_repaired_not_infeasible);
    RUN_TEST(test_a_real_flip_gap_past_tolerance_stays_infeasible);
    RUN_TEST(test_a_fixed_column_is_not_a_flip_candidate);
    RUN_TEST(test_a_row_repairable_only_by_fixed_columns_is_infeasible);
    RUN_TEST(test_every_work_limit_stops_honestly_and_resumes);
    RUN_TEST(test_a_ray_whose_direction_cancels_in_row_space);
    RUN_TEST(test_an_inverted_column_box_is_infeasible);
    RUN_TEST(test_an_inverted_row_box_is_infeasible);
    RUN_TEST(test_a_loan_nobody_holds_is_retired_before_publishing);
    RUN_TEST(test_a_retired_loan_leaves_a_basis_of_the_model);
    RUN_TEST(test_the_algorithm_is_a_caller_option);
    RUN_TEST(test_devex_and_dantzig_pricing_reach_the_same_optimum);
    return UNITY_END();
}
