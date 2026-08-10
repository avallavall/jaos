/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h" /* white-box: CSC/CSR internals are asserted here */
#include "unity.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Shared example, 2 rows x 3 cols:
 *     A = [ 1  0  2 ]
 *         [ 0  3  4 ]
 * CSC with column 2 deliberately unsorted, plus one explicit zero in
 * column 0 that the load must drop. */
static const int64_t ex_start[] = {0, 2, 3, 5};
static const int64_t ex_index[] = {0, 1, 1, 1, 0};   /* col2 = (1,4),(0,2) */
static const double  ex_value[] = {1.0, 0.0, 3.0, 4.0, 2.0};
static const double  ex_cost[]  = {1.0, 1.0, 1.0};
static const double  ex_cl[3]   = {0.0, 0.0, 0.0};
static const double  ex_cu[]    = {10.0, 10.0, 10.0};
static const double  ex_rl[2]   = {0.0, 0.0};
static const double  ex_ru[]    = {5.0, 5.0};

static jaos_status load_example(jaos_model *m)
{
    return jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0,
                        ex_cost, ex_cl, ex_cu, ex_rl, ex_ru,
                        5, ex_start, ex_index, ex_value);
}

static void test_new_free_roundtrip(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(m));
    jaos_model_free(m);
    jaos_model_free(nullptr); /* must be harmless */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_model_new(nullptr));
}

static void test_null_model_queries_read_as_empty(void)
{
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_row(nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(nullptr));
}

static void test_infinity_is_ieee_infinity(void)
{
    TEST_ASSERT_TRUE(isinf(jaos_infinity()));
    TEST_ASSERT_TRUE(jaos_infinity() > 0.0);
}

static void test_load_drops_zeros_and_sorts_columns(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m)); /* the 0.0 was dropped */

    /* White-box: authoritative CSC is sorted, zero gone. */
    const int64_t want_start[] = {0, 1, 2, 4};
    const int64_t want_index[] = {0, 1, 0, 1};
    const double  want_value[] = {1.0, 3.0, 2.0, 4.0};
    for (int j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
    jaos_model_free(m);
}

static void test_load_rejects_bad_input(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double nan_cost[] = {NAN, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, nan_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, ex_index, ex_value));

    const double inf_cost[] = {1.0, INFINITY, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, inf_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, ex_index, ex_value));

    const double nan_bound[] = {0.0, NAN, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, nan_bound, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, ex_index, ex_value));

    const int64_t oob_index[] = {0, 1, 1, 2, 0}; /* row 2 of a 2-row model */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, oob_index, ex_value));

    const int64_t dup_index[] = {0, 1, 1, 1, 1}; /* col2 hits row 1 twice */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, dup_index, ex_value));

    const double nan_value[] = {1.0, 0.0, NAN, 4.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, ex_index, nan_value));

    const int64_t bad_start0[] = {1, 2, 3, 5};   /* must begin at 0 */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, bad_start0, ex_index, ex_value));

    const int64_t bad_desc[] = {0, 3, 2, 5};     /* not nondecreasing */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, bad_desc, ex_index, ex_value));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, nullptr, ex_value));

    jaos_model_free(m);
}

static void test_failed_load_leaves_model_untouched(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    const double nan_cost[] = {NAN, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, nan_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, ex_index, ex_value));

    /* Everything from the successful load is still there. */
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[1]);
    jaos_model_free(m);
}

static void test_reload_replaces(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    /* Now a 1x1 model. */
    const double c[] = {2.0}, l[] = {0.0}, u[] = {1.0};
    const double rl2[] = {0.0}, ru2[] = {1.0};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {7.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MAXIMIZE, 0.5, c, l, u, rl2, ru2,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_nz(m));
    jaos_model_free(m);
}

static void test_empty_model_loads(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 0, 0, JAOS_MINIMIZE, 3.0, nullptr, nullptr, nullptr,
                     nullptr, nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(m));
    jaos_model_free(m);
}

static void test_csr_mirror_matches_hand_transpose(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_ensure_rowwise(m));
    /* Row 0: (col0, 1), (col2, 2). Row 1: (col1, 3), (col2, 4). */
    const int64_t want_start[] = {0, 2, 4};
    const int64_t want_index[] = {0, 2, 1, 2};
    const double  want_value[] = {1.0, 2.0, 3.0, 4.0};
    for (int i = 0; i <= 2; i++)
        TEST_ASSERT_EQUAL_INT64(want_start[i], m->ar_start[i]);
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->ar_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->ar_value[k]);
    }

    /* Idempotent, and invalidated by a reload. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_ensure_rowwise(m));
    TEST_ASSERT_TRUE(m->rowwise_valid);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_FALSE(m->rowwise_valid);

    jaos_model_free(m);
}

/* Changing a loaded problem.
 *
 * Three properties, and the order matters. That the modification is refused
 * when it should be is the cheap one. That the new bound actually reaches
 * the solve is what a stored-and-never-read setting would fail. And that the
 * previous answer stops being readable is the one protecting against the
 * failure this project is built against: a wrong number returned with full
 * confidence, here because the caller changed the model and the model kept
 * answering about the old one. */
static jaos_model *bounded_model(void)
{
    /* min -x0 - x1  s.t.  x0 + x1 <= 3,  0 <= x <= 10.
     * Optimum: anything on x0 + x1 = 3, objective -3. */
    const double c[] = {-1.0, -1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {-INFINITY}, ru[] = {3.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    return m;
}

static double solved_objective(jaos_model *m)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    return obj;
}

static void test_a_modification_out_of_range_is_refused(void)
{
    jaos_model *m = bounded_model();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(m, -1, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(m, 2, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_col_bounds(m, 2, 0.0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_row_bounds(m, 1, 0.0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(nullptr, 0, 1.0));
    /* Costs must be finite; bounds may be infinite but not NaN. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(m, 0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(m, 0, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_col_bounds(m, 0, NAN, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_row_bounds(m, 0, 0.0, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, -INFINITY, INFINITY));
    /* lower > upper is an infeasible model, not a bad call. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 5.0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_changed_bound_reaches_the_solve(void)
{
    jaos_model *m = bounded_model();
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, solved_objective(m));

    /* Loosen the row: the optimum follows it. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 7.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -7.0, solved_objective(m));

    /* Cap a column: now the row cannot be filled by x0 alone. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 0.0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 1, 0.0, 3.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -5.0, solved_objective(m));

    /* Flip a cost: x1 stops being worth using. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 1, 1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, solved_objective(m));
    jaos_model_free(m);
}

static void test_a_modification_discards_the_answer(void)
{
    jaos_model *m = bounded_model();
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, solved_objective(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_iterations(m) > 0 ? 1 : 0);

    double x[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));

    /* One bound moves, and the model stops answering about the old problem. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 5.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));

    /* And the same for a cost and for a column bound. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, -2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 0.0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_configuration_survives_a_modification(void)
{
    /* Budgets and tolerances are configuration, not problem data. A caller
     * who set them before a change should not have to set them again. */
    jaos_model *m = bounded_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 999));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-5));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_dual_tolerance(m, 1e-4));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, -3.0));
    TEST_ASSERT_EQUAL_INT64(999, m->work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-5, m->primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(1e-4, m->dual_tol);
    jaos_model_free(m);
}

/* Changing a coefficient is three operations, and the invariant it must not
 * break is the one the whole library reads: within a column, entries ascend
 * by row index, no duplicates, no explicit zeros. A test that only checked
 * the solve would pass on a matrix that had quietly stopped being sorted. */
static void test_a_coefficient_replaces_inserts_and_deletes(void)
{
    /*  A = [ 1  0 ]     one entry in column 0, one in column 1.
     *      [ 0  3 ]                                              */
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {1.0, 3.0}, ru[] = {INFINITY, INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 1};
    const double av[] = {1.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));

    /* Replace: no structural change. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 2.0));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->a_value[0]);

    /* Insert into column 0, below the entry already there: it must land
     * after it, not before, or the column stops being sorted. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 5.0));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(0, m->a_start[0]);
    TEST_ASSERT_EQUAL_INT64(2, m->a_start[1]);
    TEST_ASSERT_EQUAL_INT64(3, m->a_start[2]);
    TEST_ASSERT_EQUAL_INT64(0, m->a_index[0]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[1]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->a_value[1]);

    /* Insert above an existing entry: this is the one that goes first. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 1, 7.0));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(0, m->a_index[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->a_value[2]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[3]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[3]);

    /* Zero deletes rather than storing a zero, because a loaded model has
     * none and this one must stay indistinguishable from a loaded one. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(1, m->a_start[1]);
    for (int64_t k = 0; k < m->num_nz; k++)
        TEST_ASSERT_TRUE(m->a_value[k] != 0.0);

    /* Zeroing an entry that is not there changes nothing at all. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));

    /* Every column still ascends, which is the invariant the readers, the
     * checker and the factorization all assume. */
    for (int64_t j = 0; j < m->num_col; j++)
        for (int64_t k = m->a_start[j] + 1; k < m->a_start[j + 1]; k++)
            TEST_ASSERT_TRUE(m->a_index[k - 1] < m->a_index[k]);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_coefficient(m, 0, 0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_coefficient(m, 2, 0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_coefficient(m, 0, 2, 1.0));
    jaos_model_free(m);
}

static void test_a_changed_coefficient_reaches_the_solve(void)
{
    /* min x  s.t.  a*x >= 6,  0 <= x <= 10. With a = 2 the answer is 3;
     * with a = 3 it is 2. Nothing but the new coefficient reaching the
     * factorization can move it. */
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {6.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, solved_objective(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 3.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_FALSE(m->rowwise_valid);
    TEST_ASSERT_FALSE(m->scale_valid);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, solved_objective(m));

    /* Deleting the only entry leaves a row nothing can satisfy. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_new_free_roundtrip);
    RUN_TEST(test_null_model_queries_read_as_empty);
    RUN_TEST(test_infinity_is_ieee_infinity);
    RUN_TEST(test_load_drops_zeros_and_sorts_columns);
    RUN_TEST(test_load_rejects_bad_input);
    RUN_TEST(test_failed_load_leaves_model_untouched);
    RUN_TEST(test_reload_replaces);
    RUN_TEST(test_empty_model_loads);
    RUN_TEST(test_csr_mirror_matches_hand_transpose);
    RUN_TEST(test_a_modification_out_of_range_is_refused);
    RUN_TEST(test_a_changed_bound_reaches_the_solve);
    RUN_TEST(test_a_modification_discards_the_answer);
    RUN_TEST(test_configuration_survives_a_modification);
    RUN_TEST(test_a_coefficient_replaces_inserts_and_deletes);
    RUN_TEST(test_a_changed_coefficient_reaches_the_solve);
    return UNITY_END();
}
