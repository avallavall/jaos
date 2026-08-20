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

/* Solves and reads the objective back.
 *
 * Every caller is a positive test, so all of them are skipped under either
 * fault build — the same rule and the same reason as test_simplex.c's
 * `solve_and_verify`, and guarding here rather than at nine call sites.
 *
 * **The guard is newer than the helper, and D169 is why.** Until then the
 * objective was the number the REDUCED model carried, so a fault that
 * corrupted a postsolved value never reached it and these tests passed under
 * builds designed to break them. It is summed from the published values now,
 * so it fails there, which is the fault doing its job. */
static double solved_objective(jaos_model *m)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    (void)m;
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
    return 0.0;
#else
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    return obj;
#endif
}

/* ---- The objective is the objective of the point that is published ------ *
 *
 * `jaos.h` says "objective value of the solution held by the model". Two
 * things had to change for that to be true (D169): the sum is compensated, and
 * it is taken over the model that publishes it rather than over a reduced one.
 *
 * The model here is the first half:
 *
 *   costs in column order:  +1e16,  1 (k times),  -1e16
 *   every column fixed at 1, one row holding them all and binding nothing
 *
 * The answer is k. Summed in column order the running total is 1e16 while the
 * k ones arrive, and one ulp of 1e16 is 2, so every one of them is below half
 * an ulp and the total does not move; the -1e16 then brings it to zero. The
 * parent publishes 0 for k = 64 and for k = 256, on both the shipping and the
 * reference build, while `jaos_check_solution` — which accumulates in
 * `long double` — reads k. The library disagreed with itself by 100%. */
#define OBJ_K 256
#define OBJ_NC (OBJ_K + 2)

static void test_the_objective_is_summed_from_the_values_it_publishes(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    static double c[OBJ_NC], cl[OBJ_NC], cu[OBJ_NC], av[OBJ_NC];
    static int64_t as[OBJ_NC + 1], ai[OBJ_NC];
    const double rl[] = {-1e30}, ru[] = {1e30};

    for (int64_t j = 0; j < OBJ_NC; j++) {
        as[j] = j;
        cl[j] = cu[j] = 1.0;
        ai[j] = 0; av[j] = 1.0;
        c[j] = (j == 0) ? 1e16 : (j == OBJ_NC - 1) ? -1e16 : 1.0;
    }
    as[OBJ_NC] = OBJ_NC;

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, OBJ_NC, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     OBJ_NC, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    const double expected = (double)OBJ_K;
    TEST_ASSERT_EQUAL_MEMORY(&expected, &obj, sizeof obj);
    jaos_model_free(m);
#endif
}

/* The control the repair must not break, and it is the one a compensated sum
 * can get wrong: the constant term and the sense. A MAXIMIZE model reports
 * `obj_offset + c'x` in its own sense, so dropping the offset or minimising by
 * accident both show up here and neither would move any instance in the gate.
 *
 *   max 2*x0 + 3*x1 + 5*x2 + 100   s.t.  x0 + x1 + x2 <= 4
 *   0 <= x0, x1 <= 4     x2 fixed at 1
 *
 * **x2 is fixed and that is the point of it** (`numerics-reviewer`, D169).
 * Without it presolve reports NONE on this model — no fixed column, no
 * singleton row, no cost-0 singleton column, x0 is not implied free because
 * the row has no lower bound, and the row is neither forcing nor redundant —
 * so the test exercised `publish()` alone and never the two postsolve paths
 * the change touched most. A fixed column with a cost sends its own term
 * through the reduced model's `obj_offset` and back out through
 * `jm_postsolve_expand`'s replay, which is where a double count or a dropped
 * offset would appear.
 *
 * x2 takes 1 of the row, so x0 + x1 <= 3 and x1 is worth more: x1 = 3,
 * x0 = 0, objective 9 + 5 + 100 = 114. */
static void test_the_objective_keeps_its_constant_term_and_its_sense(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[] = {2.0, 3.0, 5.0};
    const double cl[] = {0.0, 0.0, 1.0}, cu[] = {4.0, 4.0, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t as[] = {0, 1, 2, 3}, ai[] = {0, 0, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MAXIMIZE, 100.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    const double expected = 114.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected, &obj, sizeof obj);

    /* And it is the objective of the point that came with it, including the
     * column presolve took out and put back. */
    double x[3] = {0.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    const double one = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&one, &x[2], sizeof one);
    const double from_x = 100.0 + c[0] * x[0] + c[1] * x[1] + c[2] * x[2];
    TEST_ASSERT_EQUAL_MEMORY(&from_x, &obj, sizeof obj);
    jaos_model_free(m);
#endif
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

/* A setter without a getter is a trap. A caller who built the model knows
 * what is in it; one who read it from a file does not, and telling that
 * caller they may change a bound while giving them no way to see the bound
 * they are changing is not an API. The values are the model's own — no
 * scaling, no substituted default, an absent bound reading as an infinity. */
static void test_bounds_and_costs_read_back(void)
{
    jaos_model *m = bounded_model();
    double lo = 0.0, hi = 0.0, c = 0.0;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 0, &c));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, c);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 1, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, lo);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, hi);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_TRUE(lo == -INFINITY);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, hi);

    /* What was set is what comes back. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 2.0, 7.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 4.5));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, lo);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, hi);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 0, &c));
    TEST_ASSERT_EQUAL_DOUBLE(4.5, c);

    /* Either bound may be left out; a cost has nowhere to go without one. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_cost(m, 0, nullptr));

    /* Out of range is refused rather than answered with a default. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_cost(m, 2, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_bounds(m, -1, &lo, &hi));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_row_bounds(m, 1, &lo, &hi));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_bounds(nullptr, 0, &lo, &hi));
    jaos_model_free(m);
}

/* Which of the two things a solve leaves behind survives a change, and that
 * difference is the whole of warm re-solve: one array outliving the other by
 * exactly one modification.
 *
 * The answer does not survive — it described the problem as it stood. The
 * basis does, because a bound moving does not stop a basis being a basis, and
 * for a small change it is usually near the new problem's. A load ends both,
 * since after it the indices name a different model. */
static void test_the_basis_outlives_a_modification_and_not_a_load(void)
{
    jaos_model *m = bounded_model();
    TEST_ASSERT_NULL(m->start_col_status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, solved_objective(m));
    TEST_ASSERT_NOT_NULL(m->start_col_status);
    TEST_ASSERT_NOT_NULL(m->start_row_status);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 7.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_NULL(m->sol_col_status);          /* the answer went */
    TEST_ASSERT_NOT_NULL(m->start_col_status);    /* the basis stayed */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -7.0, solved_objective(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_NULL(m->start_col_status);
    TEST_ASSERT_NULL(m->start_row_status);
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
    TEST_ASSERT_EQUAL_INT64(999, m->cfg.work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-5, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(1e-4, m->cfg.dual_tol);
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

/* ---------------------------------------------------------------------- */
/* Adding and deleting rows and columns                                    */
/* ---------------------------------------------------------------------- */

/* min 2x + 3y  s.t.  x + y >= 2,  0 <= x <= 1.5,  0 <= y <= 10.
 * Spend on x first because it is cheaper, and x runs out at 1.5:
 * the optimum is x = 1.5, y = 0.5, objective 4.5. Every case below moves
 * that number to somewhere else this comment can name. */
static jaos_status load_two_var(jaos_model *m)
{
    static const double c[]  = {2.0, 3.0};
    static const double cl[] = {0.0, 0.0}, cu[] = {1.5, 10.0};
    static const double rl[] = {2.0}, ru[] = {INFINITY};
    static const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    static const double av[] = {1.0, 1.0};
    return jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                        2, as, ai, av);
}

static void test_added_columns_append_and_leave_the_rest_alone(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));   /* 3 cols, 2 rows, 4 nz */

    /* Two columns, the second deliberately unsorted and carrying an explicit
     * zero, so the same invariant the loader keeps is checked on this path. */
    const double c[]  = {5.0, 6.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 2.0};
    const int64_t as[] = {0, 1, 4};
    const int64_t ai[] = {1,   1, 0, 0};
    const double  av[] = {7.0, 8.0, 9.0, 0.0};

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 2, c, cl, cu, 4, as, ai, av));
    TEST_ASSERT_EQUAL_INT64(5, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4 + 3, jaos_num_nz(m));   /* the zero was dropped */

    /* The three original columns are bit-for-bit where they were: appending
     * moves no index below num_col, which is the whole promise. */
    TEST_ASSERT_EQUAL_INT64(0, m->a_start[0]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_start[1]);
    TEST_ASSERT_EQUAL_INT64(2, m->a_start[2]);
    TEST_ASSERT_EQUAL_INT64(4, m->a_start[3]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_cost[0]);

    /* The new ones, sorted by row index. */
    TEST_ASSERT_EQUAL_INT64(5, m->a_start[4]);
    TEST_ASSERT_EQUAL_INT64(7, m->a_start[5]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[4]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->a_value[4]);
    TEST_ASSERT_EQUAL_INT64(0, m->a_index[5]);
    TEST_ASSERT_EQUAL_DOUBLE(9.0, m->a_value[5]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[6]);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, m->a_value[6]);
    TEST_ASSERT_EQUAL_DOUBLE(6.0, m->col_cost[4]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_upper[4]);
    jaos_model_free(m);
}

static void test_added_rows_land_after_every_column_s_own_entries(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    /* One row across columns 2 and 0, given in that order: the CSC copy must
     * still come out ascending, and it does without a sort because every new
     * row index is above every old one. */
    const double rl[] = {1.0}, ru[] = {4.0};
    const int64_t rs[] = {0, 2}, ri[] = {2, 0};
    const double  rv[] = {11.0, 12.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_rows(m, 1, rl, ru, 2, rs, ri, rv));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4 + 2, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[2]);

    for (int64_t j = 0; j < jaos_num_col(m); j++)
        for (int64_t k = m->a_start[j] + 1; k < m->a_start[j + 1]; k++)
            TEST_ASSERT_TRUE(m->a_index[k - 1] < m->a_index[k]);

    /* Column 0 was (row 0, 1.0); it is now that plus the new row. */
    TEST_ASSERT_EQUAL_INT64(2, m->a_start[1] - m->a_start[0]);
    TEST_ASSERT_EQUAL_INT64(2, m->a_index[m->a_start[1] - 1]);
    TEST_ASSERT_EQUAL_DOUBLE(12.0, m->a_value[m->a_start[1] - 1]);
    jaos_model_free(m);
}

static void test_a_dimension_change_the_solve_can_see(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_two_var(m));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.5, solved_objective(m));

    /* A cut: x <= 1. Now x = 1, y = 1 and the objective is 5. */
    const double rl[] = {-INFINITY}, ru[] = {1.0};
    const int64_t rs[] = {0, 1}, ri[] = {0};
    const double  rv[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_rows(m, 1, rl, ru, 1, rs, ri, rv));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_FALSE(m->rowwise_valid);
    TEST_ASSERT_FALSE(m->scale_valid);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, solved_objective(m));

    /* A cheaper way to satisfy the first row: z at cost 1. z = 2, and both
     * of the others go to zero, so the objective is 2. */
    const double c[] = {1.0}, zl[] = {0.0}, zu[] = {10.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double  av[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cols(m, 1, c, zl, zu, 1, as, ai, av));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, solved_objective(m));

    /* Take the cheap column away again and the cut is binding once more. */
    const int64_t drop_col[] = {2};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, drop_col));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, solved_objective(m));

    /* And take the cut away: back to where the model started. */
    const int64_t drop_row[] = {1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, drop_row));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.5, solved_objective(m));
    jaos_model_free(m);
}

static void test_deleting_renumbers_what_survives(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    /* Row 0 goes; row 1 becomes row 0, and every entry that named it has to
     * be rewritten. The entries that named row 0 go with it. */
    const int64_t del[] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, del));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));   /* was 4; two named row 0 */
    for (int64_t k = 0; k < jaos_num_nz(m); k++)
        TEST_ASSERT_EQUAL_INT64(0, m->a_index[k]);
    /* Column 0 held only (row 0), so it is empty now — legal, not an error. */
    TEST_ASSERT_EQUAL_INT64(0, m->a_start[1] - m->a_start[0]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[0]);   /* column 1 kept its 3 */
    jaos_model_free(m);
}

static void test_deleting_two_at_once_keeps_relative_order(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    /* Named out of order on purpose: a set is a set. Columns 0 and 2 go, so
     * the old column 1 — cost 1, one entry of 3.0 in row 1 — becomes 0. */
    const int64_t del[] = {2, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 2, del));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[0]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[0]);
    jaos_model_free(m);
}

static void test_a_dimension_change_refuses_what_it_must(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    const double c[] = {1.0}, cl[] = {0.0}, cu[] = {1.0};
    const int64_t as[] = {0, 1};
    const double  av[] = {1.0};

    /* A row index that is not a row of this model. */
    const int64_t bad_row[] = {2};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, cl, cu, 1, as, bad_row, av));
    /* A coefficient that is not a number. */
    const int64_t ok_row[] = {0};
    const double inf_v[] = {INFINITY};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, cl, cu, 1, as, ok_row, inf_v));
    /* A bound that is not a number. */
    const double nan_l[] = {NAN};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, nan_l, cu, 1, as, ok_row, av));
    /* One column naming one row twice. */
    const int64_t dup_as[] = {0, 2}, dup_ai[] = {1, 1};
    const double  dup_av[] = {1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, cl, cu, 2, dup_as, dup_ai, dup_av));
    /* One new row naming one column twice. */
    const double rl[] = {0.0}, ru[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_rows(m, 1, rl, ru, 2, dup_as, dup_ai, dup_av));
    /* Entries offered for no columns at all. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 0, c, cl, cu, 1, as, ok_row, av));

    /* Deletion: out of range, and the same index twice. */
    const int64_t oor[] = {3};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_delete_cols(m, 1, oor));
    const int64_t twice[] = {1, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_delete_cols(m, 2, twice));

    /* Every one of those left the model exactly as it was. */
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_model_free(m);
}

static int64_t count_basic(const jaos_model *m)
{
    int64_t n = 0;
    for (int64_t j = 0; j < m->num_col; j++)
        n += m->start_col_status[j] == JAOS_BASIS_BASIC;
    for (int64_t i = 0; i < m->num_row; i++)
        n += m->start_row_status[i] == JAOS_BASIS_BASIC;
    return n;
}

static void test_the_basis_survives_an_addition_and_still_counts(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_two_var(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(m->start_row_status);

    const double rl[] = {-INFINITY}, ru[] = {1.0};
    const int64_t rs[] = {0, 1}, ri[] = {0};
    const double  rv[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_rows(m, 1, rl, ru, 1, rs, ri, rv));
    TEST_ASSERT_NOT_NULL(m->start_row_status);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, m->start_row_status[1]);
    TEST_ASSERT_EQUAL_INT64(m->num_row, count_basic(m));

    const double c[] = {1.0}, cl[] = {0.0}, cu[] = {10.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double  av[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cols(m, 1, c, cl, cu, 1, as, ai, av));
    TEST_ASSERT_NOT_NULL(m->start_col_status);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, m->start_col_status[2]);
    TEST_ASSERT_EQUAL_INT64(m->num_row, count_basic(m));
    jaos_model_free(m);
}

/* The two cases the rule has to reject, built rather than hoped for. */
static void test_a_basis_that_would_stop_being_one_is_dropped(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_two_var(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(m->start_col_status);

    /* A column with no finite bound has nowhere to rest nonbasic, and a
     * nonbasic free variable is the one this solver cannot always price back
     * off — so the whole basis goes rather than one being created. */
    const double c[] = {1.0}, cl[] = {-INFINITY}, cu[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double  av[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cols(m, 1, c, cl, cu, 1, as, ai, av));
    TEST_ASSERT_NULL(m->start_col_status);
    TEST_ASSERT_NULL(m->start_row_status);

    /* And deleting a basic column leaves a count that is no longer a basis. */
    jaos_model *n = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&n));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_two_var(n));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(n));
    int64_t basic_col = -1;
    for (int64_t j = 0; j < n->num_col; j++)
        if (n->start_col_status[j] == JAOS_BASIS_BASIC)
            basic_col = j;
    /* Which one it is, is decided and not incidental: x rests at its upper
     * bound of 1.5 so it is nonbasic, y sits strictly inside [0, 10] so it is
     * basic, and one row means exactly one basic variable. Asserting it here
     * makes the rest of this test a statement rather than a guess. */
    TEST_ASSERT_EQUAL_INT64(1, basic_col);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(n, 1, &basic_col));
    TEST_ASSERT_NULL(n->start_col_status);

    /* The solve after it is cold and still right — and right here means
     * infeasible: all that is left is min 2x with x >= 2 and x <= 1.5. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(n));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(n));
    jaos_model_free(m);
    jaos_model_free(n);
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
    RUN_TEST(test_the_objective_is_summed_from_the_values_it_publishes);
    RUN_TEST(test_the_objective_keeps_its_constant_term_and_its_sense);
    RUN_TEST(test_bounds_and_costs_read_back);
    RUN_TEST(test_the_basis_outlives_a_modification_and_not_a_load);
    RUN_TEST(test_a_modification_discards_the_answer);
    RUN_TEST(test_configuration_survives_a_modification);
    RUN_TEST(test_a_coefficient_replaces_inserts_and_deletes);
    RUN_TEST(test_a_changed_coefficient_reaches_the_solve);
    RUN_TEST(test_added_columns_append_and_leave_the_rest_alone);
    RUN_TEST(test_added_rows_land_after_every_column_s_own_entries);
    RUN_TEST(test_a_dimension_change_the_solve_can_see);
    RUN_TEST(test_deleting_renumbers_what_survives);
    RUN_TEST(test_deleting_two_at_once_keeps_relative_order);
    RUN_TEST(test_a_dimension_change_refuses_what_it_must);
    RUN_TEST(test_the_basis_survives_an_addition_and_still_counts);
    RUN_TEST(test_a_basis_that_would_stop_being_one_is_dropped);
    return UNITY_END();
}
