/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const int64_t ex_start[] = {0, 2, 3, 5};
static const int64_t ex_index[] = {0, 1, 1, 1, 0};
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
    jaos_model_free(nullptr);
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
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));

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

    const int64_t oob_index[] = {0, 1, 1, 2, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, oob_index, ex_value));

    const int64_t dup_index[] = {0, 1, 1, 1, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, dup_index, ex_value));

    const double nan_value[] = {1.0, 0.0, NAN, 4.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, ex_start, ex_index, nan_value));

    const int64_t bad_start0[] = {1, 2, 3, 5};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, ex_cost, ex_cl, ex_cu,
                     ex_rl, ex_ru, 5, bad_start0, ex_index, ex_value));

    const int64_t bad_desc[] = {0, 3, 2, 5};
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

    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[1]);
    jaos_model_free(m);
}

static void test_reload_replaces(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

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

    const int64_t want_start[] = {0, 2, 4};
    const int64_t want_index[] = {0, 2, 1, 2};
    const double  want_value[] = {1.0, 2.0, 3.0, 4.0};
    for (int i = 0; i <= 2; i++)
        TEST_ASSERT_EQUAL_INT64(want_start[i], m->ar_start[i]);
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->ar_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->ar_value[k]);
    }

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_ensure_rowwise(m));
    TEST_ASSERT_TRUE(m->rowwise_valid);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_FALSE(m->rowwise_valid);

    jaos_model_free(m);
}

static jaos_model *bounded_model(void)
{

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

static void test_two_product_residue_gives_up_rather_than_overflow(void)
{
    const double big = ldexp(1.0, 997);
    const double small = ldexp(1.0, -997);

    const double p = big * small;
    TEST_ASSERT_TRUE(isfinite(p));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, p);

    TEST_ASSERT_FALSE(isfinite(134217729.0 * big));

    TEST_ASSERT_EQUAL_DOUBLE(0.0, jm_two_product_residue(big, small, p));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jm_two_product_residue(small, big, p));

    const double inf = jaos_infinity();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jm_two_product_residue(2.0, inf, inf));

    const double ok = ldexp(1.0, 27) + 1.0;
    TEST_ASSERT_TRUE(isfinite(134217729.0 * ok));
    const double q = ok * ok;
    TEST_ASSERT_EQUAL_DOUBLE(1.0, jm_two_product_residue(ok, ok, q));
}

static void test_the_objective_recovers_what_a_rounded_product_dropped(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double big  = ldexp(1.0, 27) + 1.0;
    const double prod = ldexp(1.0, 54) + ldexp(1.0, 28);

    const double rounded = big * big;
    TEST_ASSERT_EQUAL_MEMORY(&prod, &rounded, sizeof prod);

    const double c[]  = {big, -1.0};
    const double cl[] = {big, prod}, cu[] = {big, prod};
    const double rl[] = {-1e30}, ru[] = {1e30};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    const double expected = 1.0;
    TEST_ASSERT_EQUAL_MEMORY(&expected, &obj, sizeof obj);

    double x[2] = {0.0, 0.0}, y[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
    TEST_ASSERT_EQUAL_MEMORY(&expected, &rep.primal_objective, sizeof expected);
    jaos_model_free(m);
#endif
}

static void test_the_objective_is_finite_at_the_top_of_the_range(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double cost = 1.4521649423643159e+281;
    const double value = 1.237940034936653e+27;
    const double plain = cost * value;
    TEST_ASSERT_TRUE(isfinite(plain));

    const double c[]  = {cost};
    const double cl[] = {value}, cu[] = {value};
    const double rl[] = {-1e30}, ru[] = {1e30};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_TRUE(isfinite(obj));
    TEST_ASSERT_EQUAL_MEMORY(&plain, &obj, sizeof obj);
    jaos_model_free(m);
#endif
}

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

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(m, 0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_cost(m, 0, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_col_bounds(m, 0, NAN, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_row_bounds(m, 0, 0.0, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, -INFINITY, INFINITY));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 5.0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_changed_bound_reaches_the_solve(void)
{
    jaos_model *m = bounded_model();
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, solved_objective(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 7.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -7.0, solved_objective(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 0.0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 1, 0.0, 3.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -5.0, solved_objective(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 1, 1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, solved_objective(m));
    jaos_model_free(m);
}

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

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 2.0, 7.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 4.5));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, lo);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, hi);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 0, &c));
    TEST_ASSERT_EQUAL_DOUBLE(4.5, c);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_cost(m, 0, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_cost(m, 2, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_bounds(m, -1, &lo, &hi));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_row_bounds(m, 1, &lo, &hi));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_bounds(nullptr, 0, &lo, &hi));
    jaos_model_free(m);
}

static void test_the_basis_outlives_a_modification_and_not_a_load(void)
{
    jaos_model *m = bounded_model();
    TEST_ASSERT_NULL(m->start_col_status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, solved_objective(m));
    TEST_ASSERT_NOT_NULL(m->start_col_status);
    TEST_ASSERT_NOT_NULL(m->start_row_status);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 7.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_NULL(m->sol_col_status);
    TEST_ASSERT_NOT_NULL(m->start_col_status);
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

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 5.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));

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

static void test_a_coefficient_replaces_inserts_and_deletes(void)
{

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

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 2.0));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->a_value[0]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 5.0));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(0, m->a_start[0]);
    TEST_ASSERT_EQUAL_INT64(2, m->a_start[1]);
    TEST_ASSERT_EQUAL_INT64(3, m->a_start[2]);
    TEST_ASSERT_EQUAL_INT64(0, m->a_index[0]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[1]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->a_value[1]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 1, 7.0));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(0, m->a_index[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->a_value[2]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_index[3]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[3]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT64(1, m->a_start[1]);
    for (int64_t k = 0; k < m->num_nz; k++)
        TEST_ASSERT_TRUE(m->a_value[k] != 0.0);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));

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

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    const double c[]  = {5.0, 6.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 2.0};
    const int64_t as[] = {0, 1, 4};
    const int64_t ai[] = {1,   1, 0, 0};
    const double  av[] = {7.0, 8.0, 9.0, 0.0};

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 2, c, cl, cu, 4, as, ai, av));
    TEST_ASSERT_EQUAL_INT64(5, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4 + 3, jaos_num_nz(m));

    TEST_ASSERT_EQUAL_INT64(0, m->a_start[0]);
    TEST_ASSERT_EQUAL_INT64(1, m->a_start[1]);
    TEST_ASSERT_EQUAL_INT64(2, m->a_start[2]);
    TEST_ASSERT_EQUAL_INT64(4, m->a_start[3]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_cost[0]);

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

    const double rl[] = {-INFINITY}, ru[] = {1.0};
    const int64_t rs[] = {0, 1}, ri[] = {0};
    const double  rv[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_rows(m, 1, rl, ru, 1, rs, ri, rv));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_FALSE(m->rowwise_valid);
    TEST_ASSERT_FALSE(m->scale_valid);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, solved_objective(m));

    const double c[] = {1.0}, zl[] = {0.0}, zu[] = {10.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double  av[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cols(m, 1, c, zl, zu, 1, as, ai, av));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, solved_objective(m));

    const int64_t drop_col[] = {2};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, drop_col));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, solved_objective(m));

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

    const int64_t del[] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, del));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));
    for (int64_t k = 0; k < jaos_num_nz(m); k++)
        TEST_ASSERT_EQUAL_INT64(0, m->a_index[k]);

    TEST_ASSERT_EQUAL_INT64(0, m->a_start[1] - m->a_start[0]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->a_value[0]);
    jaos_model_free(m);
}

static void test_deleting_two_at_once_keeps_relative_order(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

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

    const int64_t bad_row[] = {2};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, cl, cu, 1, as, bad_row, av));

    const int64_t ok_row[] = {0};
    const double inf_v[] = {INFINITY};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, cl, cu, 1, as, ok_row, inf_v));

    const double nan_l[] = {NAN};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, nan_l, cu, 1, as, ok_row, av));

    const int64_t dup_as[] = {0, 2}, dup_ai[] = {1, 1};
    const double  dup_av[] = {1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 1, c, cl, cu, 2, dup_as, dup_ai, dup_av));

    const double rl[] = {0.0}, ru[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_rows(m, 1, rl, ru, 2, dup_as, dup_ai, dup_av));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_add_cols(m, 0, c, cl, cu, 1, as, ok_row, av));

    const int64_t oor[] = {3};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_delete_cols(m, 1, oor));
    const int64_t twice[] = {1, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_delete_cols(m, 2, twice));

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

static void test_a_basis_that_would_stop_being_one_is_dropped(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_two_var(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(m->start_col_status);

    const double c[] = {1.0}, cl[] = {-INFINITY}, cu[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double  av[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cols(m, 1, c, cl, cu, 1, as, ai, av));
    TEST_ASSERT_NULL(m->start_col_status);
    TEST_ASSERT_NULL(m->start_row_status);

    jaos_model *n = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&n));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_two_var(n));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(n));
    int64_t basic_col = -1;
    for (int64_t j = 0; j < n->num_col; j++)
        if (n->start_col_status[j] == JAOS_BASIS_BASIC)
            basic_col = j;

    TEST_ASSERT_EQUAL_INT64(1, basic_col);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(n, 1, &basic_col));
    TEST_ASSERT_NULL(n->start_col_status);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(n));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(n));
    jaos_model_free(m);
    jaos_model_free(n);
}

static void derive_both(jaos_model *m)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_ensure_rowwise(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_model_scale(m, JM_SCALE_CURTIS_REID));
    TEST_ASSERT_TRUE(m->rowwise_valid);
    TEST_ASSERT_TRUE(m->scale_valid);
}

static void assert_columns_ascending(const jaos_model *m, const char *where)
{
    for (int64_t j = 0; j < m->num_col; j++)
        for (int64_t k = m->a_start[j] + 1; k < m->a_start[j + 1]; k++)
            TEST_ASSERT_TRUE_MESSAGE(m->a_index[k - 1] < m->a_index[k], where);
}

static void test_the_objective_sense_and_constant_read_back_and_change(void)
{
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

    jaos_obj_sense sense = JAOS_MINIMIZE;
    double offset = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_sense(m, &sense));
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, sense);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_offset(m, &offset));
    TEST_ASSERT_EQUAL_DOUBLE(100.0, offset);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_objective_sense(m, (jaos_obj_sense)7));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_objective_offset(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_objective_offset(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_objective_sense(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_objective_offset(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_objective_sense(nullptr, &sense));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_objective_sense(nullptr, JAOS_MINIMIZE));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_sense(m, &sense));
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, sense);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_offset(m, &offset));
    TEST_ASSERT_EQUAL_DOUBLE(100.0, offset);

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    const double as_max = 114.0;
    TEST_ASSERT_EQUAL_MEMORY(&as_max, &obj, sizeof obj);

    derive_both(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_sense(m, JAOS_MINIMIZE));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_NOT_NULL(m->start_col_status);
    TEST_ASSERT_TRUE(m->rowwise_valid);
    TEST_ASSERT_TRUE(m->scale_valid);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    const double as_min = 105.0;
    TEST_ASSERT_EQUAL_MEMORY(&as_min, &obj, sizeof obj);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_offset(m, 0.0));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    const double no_constant = 5.0;
    TEST_ASSERT_EQUAL_MEMORY(&no_constant, &obj, sizeof obj);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_sense(m, &sense));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, sense);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_offset(m, &offset));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, offset);
#endif
    jaos_model_free(m);
}

static void test_the_matrix_reads_back_by_column_row_and_entry(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    int64_t count = -1;
    int64_t idx[3] = {-1, -1, -1};
    double val[3] = {0.0, 0.0, 0.0};

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_entries(m, 2, &count, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT64(2, count);
    TEST_ASSERT_EQUAL_INT64(-1, idx[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_entries(m, 2, &count, idx, val));
    TEST_ASSERT_EQUAL_INT64(2, count);
    TEST_ASSERT_EQUAL_INT64(0, idx[0]);
    TEST_ASSERT_EQUAL_INT64(1, idx[1]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, val[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, val[1]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_entries(m, 0, &count, idx, val));
    TEST_ASSERT_EQUAL_INT64(1, count);
    TEST_ASSERT_EQUAL_INT64(0, idx[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, val[0]);
    TEST_ASSERT_FALSE(m->rowwise_valid);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_entries(m, 1, &count, idx, val));
    TEST_ASSERT_TRUE(m->rowwise_valid);
    TEST_ASSERT_EQUAL_INT64(2, count);
    TEST_ASSERT_EQUAL_INT64(1, idx[0]);
    TEST_ASSERT_EQUAL_INT64(2, idx[1]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, val[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, val[1]);

    double v = -1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 1, 2, &v));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 0, 1, &v));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 1, 0, &v));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, v);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 7.0));
    TEST_ASSERT_FALSE(m->rowwise_valid);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_entries(m, 1, &count, idx, val));
    TEST_ASSERT_EQUAL_INT64(3, count);
    TEST_ASSERT_EQUAL_INT64(0, idx[0]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, val[0]);
    TEST_ASSERT_EQUAL_INT64(1, idx[1]);
    TEST_ASSERT_EQUAL_INT64(2, idx[2]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_entries(m, 0, &count, idx, val));
    TEST_ASSERT_EQUAL_INT64(2, count);
    TEST_ASSERT_EQUAL_INT64(1, idx[1]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, val[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 1, 0, &v));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, v);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 0.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 1, 0, 0.0));
    idx[0] = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_entries(m, 0, &count, idx, val));
    TEST_ASSERT_EQUAL_INT64(0, count);
    TEST_ASSERT_EQUAL_INT64(-1, idx[0]);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_entries(m, 3, &count, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_row_entries(m, -1, &count, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_entries(m, 0, nullptr, idx, val));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_coefficient(m, 2, 0, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_coefficient(m, 0, 3, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_coefficient(m, 0, 0, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_col_entries(nullptr, 0, &count, nullptr, nullptr));
    jaos_model_free(m);
}

static void test_only_a_matrix_change_discards_the_derived_copies(void)
{
    const double one_cost[] = {1.0};
    const double one_lo[] = {0.0}, one_hi[] = {1.0};
    const int64_t one_start[] = {0, 1};
    const int64_t one_index[] = {0};
    const double one_value[] = {1.0};
    const int64_t del0[] = {0};

    struct { const char *name; int which; } matrix_ops[] = {
        {"jaos_set_coefficient", 0},
        {"jaos_add_cols",        1},
        {"jaos_add_rows",        2},
        {"jaos_delete_cols",     3},
        {"jaos_delete_rows",     4},
    };

    for (size_t t = 0; t < sizeof matrix_ops / sizeof matrix_ops[0]; t++) {
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
        derive_both(m);

        switch (matrix_ops[t].which) {
        case 0:
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_coefficient(m, 1, 0, 7.0));
            break;
        case 1:
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_add_cols(m, 1, one_cost, one_lo, one_hi,
                              1, one_start, one_index, one_value));
            break;
        case 2:
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_add_rows(m, 1, one_lo, one_hi,
                              1, one_start, one_index, one_value));
            break;
        case 3:
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, del0));
            break;
        default:
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, del0));
            break;
        }

        TEST_ASSERT_FALSE_MESSAGE(m->rowwise_valid, matrix_ops[t].name);
        TEST_ASSERT_FALSE_MESSAGE(m->scale_valid, matrix_ops[t].name);
        assert_columns_ascending(m, matrix_ops[t].name);
        jaos_model_free(m);
    }

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    derive_both(m);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 3.5));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 1, -2.0, 9.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -1.0, 4.0));

    TEST_ASSERT_TRUE_MESSAGE(m->rowwise_valid,
        "a bound or a cost threw away the row-wise mirror");
    TEST_ASSERT_TRUE_MESSAGE(m->scale_valid,
        "a bound or a cost threw away the scaling");
    jaos_model_free(m);
}

static void test_the_scaling_does_not_read_a_bound_or_a_cost(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_model_scale(m, JM_SCALE_CURTIS_REID));

    double row_before[2], col_before[3];
    bool any_scaling = false;
    for (int64_t i = 0; i < m->num_row; i++) {
        row_before[i] = m->row_scale[i];
        any_scaling = any_scaling || row_before[i] != 1.0;
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        col_before[j] = m->col_scale[j];
        any_scaling = any_scaling || col_before[j] != 1.0;
    }

    TEST_ASSERT_TRUE_MESSAGE(any_scaling,
        "every scale factor is 1.0, so this test compares nothing");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 0.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 1, -1e7));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 2, 3.25));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_col_bounds(m, 0, -INFINITY, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 1, -1e5, 1e7));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 2, 4.0, 4.0));
    for (int64_t i = 0; i < m->num_row; i++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_row_bounds(m, i, -1e9, 1e9));

    m->scale_valid = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_model_scale(m, JM_SCALE_CURTIS_REID));

    for (int64_t i = 0; i < m->num_row; i++)
        TEST_ASSERT_TRUE_MESSAGE(row_before[i] == m->row_scale[i],
                                 "a row scale factor moved with a bound");
    for (int64_t j = 0; j < m->num_col; j++)
        TEST_ASSERT_TRUE_MESSAGE(col_before[j] == m->col_scale[j],
                                 "a column scale factor moved with a cost");
    jaos_model_free(m);
}

static void test_a_column_left_empty_by_delete_rows_is_not_an_error(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    const int64_t before_nz = jaos_num_nz(m);
    const int64_t row1[] = {1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, row1));

    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));

    TEST_ASSERT_EQUAL_INT64(m->a_start[1], m->a_start[2]);
    TEST_ASSERT_EQUAL_INT64(4, before_nz);
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));

    double cost = 0.0, lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 1, &cost));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 1, &lo, &hi));
    TEST_ASSERT_TRUE(cost == ex_cost[1] && lo == ex_cl[1] && hi == ex_cu[1]);

    assert_columns_ascending(m, "after jaos_delete_rows");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_ensure_rowwise(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_model_scale(m, JM_SCALE_CURTIS_REID));
    jaos_model_free(m);
}

static void test_column_order_survives_a_chain_of_mutations(void)
{
    const double one_cost[] = {2.0};
    const double one_lo[] = {0.0}, one_hi[] = {5.0};
    const int64_t two_start[] = {0, 2};
    const int64_t two_index[] = {1, 0};
    const double two_value[] = {6.0, 7.0};
    const int64_t del1[] = {1};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    assert_columns_ascending(m, "after load");

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 1, one_cost, one_lo, one_hi,
                      2, two_start, two_index, two_value));
    assert_columns_ascending(m, "after add_cols with a descending column");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 1, 9.0));
    assert_columns_ascending(m, "after inserting a coefficient");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 1, 0.0));
    assert_columns_ascending(m, "after deleting a coefficient");

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_rows(m, 1, one_lo, one_hi,
                      2, two_start, two_index, two_value));
    assert_columns_ascending(m, "after add_rows");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, del1));
    assert_columns_ascending(m, "after delete_cols");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, del1));
    assert_columns_ascending(m, "after delete_rows");

    jaos_model_free(m);
}

static void test_an_unnamed_row_or_column_is_called_by_its_position(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    char buf[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C1", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 2, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C3", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("R2", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("COST", buf);

    int64_t k = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "C3", &k));
    TEST_ASSERT_EQUAL_INT64(2, k);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_index(m, "R1", &k));
    TEST_ASSERT_EQUAL_INT64(0, k);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "C4", &k));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "C0", &k));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "C01", &k));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "R1", &k));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "", &k));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no column is named"));

    char tiny[2];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_name(m, 0, tiny, 2));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_name(m, 3, buf, sizeof buf));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_row_name(m, -1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_name(nullptr, 0, buf, sizeof buf));
    jaos_model_free(m);
}

static void test_a_name_set_reads_back_and_resolves(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "flow"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "cap"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_name(m, "profit"));

    char buf[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("flow", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C1", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("cap", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("profit", buf);

    int64_t k = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "flow", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_index(m, "cap", &k));
    TEST_ASSERT_EQUAL_INT64(0, k);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "C2", &k));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "C3", &k));
    TEST_ASSERT_EQUAL_INT64(2, k);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "flow2"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "flow", &k));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "flow2", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, ""));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C2", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "C2", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_name(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("COST", buf);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 2, "same"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "same"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "same", &k));
    TEST_ASSERT_EQUAL_INT64(0, k);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "R1"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_index(m, "R1", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);
    jaos_model_free(m);
}

static void test_a_name_the_formats_cannot_carry_is_refused(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_name(m, 0, "a b"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_name(m, 0, "a\tb"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_row_name(m, 0, "a\n"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_objective_name(m, "\x01"));
    char longname[JAOS_NAME_MAX + 2];
    memset(longname, 'x', sizeof longname - 1);
    longname[sizeof longname - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_name(m, 0, longname));
    longname[JAOS_NAME_MAX] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, longname));
    char buf[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING(longname, buf);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_name(m, 0, buf, JAOS_NAME_MAX));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_name(m, 3, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_row_name(m, 2, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_name(nullptr, 0, "x"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "demand"));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_names_ride_with_their_rows_and_columns(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "a"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 2, "c"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "second"));

    const int64_t del0[] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, del0));
    char buf[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C1", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("c", buf);
    int64_t k = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "a", &k));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "c", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);

    const double one = 1.0, zero = 0.0, inf = INFINITY;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 1, &one, &zero, &inf, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 2, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C3", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("c", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_rows(m, 1, &zero, &one, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 2, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("R3", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("second", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 2, "new"));

    const int64_t del1[] = {1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, del1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("R2", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_row_index(m, "second", &k));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C2", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_col_index(m, "c", &k));
    jaos_model_free(m);
}

static void test_a_copy_is_the_same_problem_and_not_the_same_answer(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "flow"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_model_name(m, "example"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 12345));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_INT64(m->num_col, c->num_col);
    TEST_ASSERT_EQUAL_INT64(m->num_row, c->num_row);
    TEST_ASSERT_EQUAL_INT64(m->num_nz, c->num_nz);
    for (int64_t k = 0; k < m->num_nz; k++) {
        TEST_ASSERT_EQUAL_INT64(m->a_index[k], c->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(m->a_value[k], c->a_value[k]);
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        TEST_ASSERT_EQUAL_DOUBLE(m->col_cost[j], c->col_cost[j]);
        TEST_ASSERT_EQUAL_DOUBLE(m->col_upper[j], c->col_upper[j]);
    }

    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(c, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("flow", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(c, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("example", nm);
    TEST_ASSERT_EQUAL_INT64(12345, c->cfg.work_limit);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(c));
    TEST_ASSERT_NOT_NULL(c->start_col_status);
    TEST_ASSERT_NOT_NULL(m->start_col_status);
    for (int64_t j = 0; j < m->num_col; j++)
        TEST_ASSERT_EQUAL_INT(m->start_col_status[j], c->start_col_status[j]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
    double a = 0.0, b = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(c, &b));
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(c, 0, 100.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(c, 0, "changed"));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("C1", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_model *e = nullptr, *ec = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&e));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(e, &ec));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(ec));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_model_copy(nullptr, &ec));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_model_copy(m, nullptr));
    jaos_model_free(ec);
    jaos_model_free(e);
    jaos_model_free(c);
    jaos_model_free(m);
}

static void test_the_model_name_is_jaos_until_given(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("JAOS", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_model_name(m, "plan.2026"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("plan.2026", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_model_name(m, "a b"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("plan.2026", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_model_name(m, ""));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("JAOS", nm);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_model_name(m, "gone"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, load_example(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("JAOS", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_model_name(m, nm, 2));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_model_name(nullptr, nm, sizeof nm));
    jaos_model_free(m);
}

static void test_the_statistics_count_what_the_model_is(void)
{

    const double cost[4] = { 0.0, -3.0, 0.25, 0.0 };
    const double cl[4] = { 2.0, 0.0, 0.0, -INFINITY };
    const double cu[4] = { 2.0, 1.0, INFINITY, INFINITY };
    const double rl[3] = { 5.0, 1.0, -INFINITY };
    const double ru[3] = { 5.0, 4.0, INFINITY };

    const int64_t as[5] = { 0, 1, 3, 4, 4 };
    const int64_t ai[4] = { 0, 0, 1, 1 };
    const double av[4] = { 1.0, -8.0, 0.5, 2.0 };
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 3, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 2, true));

    jaos_model_stats st;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_statistics(m, &st));
    TEST_ASSERT_EQUAL_INT64(3, st.num_row);
    TEST_ASSERT_EQUAL_INT64(4, st.num_col);
    TEST_ASSERT_EQUAL_INT64(4, st.num_nz);
    TEST_ASSERT_EQUAL_INT64(1, st.equality_row);
    TEST_ASSERT_EQUAL_INT64(1, st.ranged_row);
    TEST_ASSERT_EQUAL_INT64(0, st.one_sided_row);
    TEST_ASSERT_EQUAL_INT64(1, st.free_row);
    TEST_ASSERT_EQUAL_INT64(1, st.empty_row);
    TEST_ASSERT_EQUAL_INT64(1, st.fixed_col);
    TEST_ASSERT_EQUAL_INT64(1, st.ranged_col);
    TEST_ASSERT_EQUAL_INT64(1, st.one_sided_col);
    TEST_ASSERT_EQUAL_INT64(1, st.free_col);
    TEST_ASSERT_EQUAL_INT64(1, st.empty_col);
    TEST_ASSERT_EQUAL_INT64(2, st.integer_col);
    TEST_ASSERT_EQUAL_INT64(1, st.binary_col);
    TEST_ASSERT_EQUAL_INT64(2, st.obj_nz);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.5, st.min_abs);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 8.0, st.max_abs);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.25, st.obj_min_abs);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, st.obj_max_abs);

    TEST_ASSERT_EQUAL_INT64(st.num_row, st.equality_row + st.ranged_row +
                                        st.one_sided_row + st.free_row);
    TEST_ASSERT_EQUAL_INT64(st.num_col, st.fixed_col + st.ranged_col +
                                        st.one_sided_col + st.free_col);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_model_statistics(m, nullptr));
    jaos_model_free(m);

    jaos_model *e = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&e));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_statistics(e, &st));
    TEST_ASSERT_EQUAL_INT64(0, st.num_row);
    TEST_ASSERT_EQUAL_INT64(0, st.num_col);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, st.max_abs);
    jaos_model_free(e);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_new_free_roundtrip);
    RUN_TEST(test_the_statistics_count_what_the_model_is);
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
    RUN_TEST(test_two_product_residue_gives_up_rather_than_overflow);
    RUN_TEST(test_the_objective_recovers_what_a_rounded_product_dropped);
    RUN_TEST(test_the_objective_is_finite_at_the_top_of_the_range);
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
    RUN_TEST(test_only_a_matrix_change_discards_the_derived_copies);
    RUN_TEST(test_the_scaling_does_not_read_a_bound_or_a_cost);
    RUN_TEST(test_a_column_left_empty_by_delete_rows_is_not_an_error);
    RUN_TEST(test_column_order_survives_a_chain_of_mutations);
    RUN_TEST(test_the_objective_sense_and_constant_read_back_and_change);
    RUN_TEST(test_the_matrix_reads_back_by_column_row_and_entry);
    RUN_TEST(test_an_unnamed_row_or_column_is_called_by_its_position);
    RUN_TEST(test_a_name_set_reads_back_and_resolves);
    RUN_TEST(test_a_name_the_formats_cannot_carry_is_refused);
    RUN_TEST(test_names_ride_with_their_rows_and_columns);
    RUN_TEST(test_a_copy_is_the_same_problem_and_not_the_same_answer);
    RUN_TEST(test_the_model_name_is_jaos_until_given);
    return UNITY_END();
}
