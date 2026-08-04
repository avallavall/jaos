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
    return UNITY_END();
}
