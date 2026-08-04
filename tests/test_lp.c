/* LP-format reader tests: two golden instances verified field by field,
 * plus one rejection per failure class.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h" /* white-box: the assembled model is inspected */
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static void test_g1_labels_relations_bounds(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    /* Columns in order of appearance: x, y, z. Rows: c1, c2, c3. */
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);

    /* "+ 5" in the objective is a direct constant. */
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_cost[1]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_cost[2]);

    /* c1 (-inf,10]; c2 [-3,inf); c3 [7,7]. */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-3.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    /* x default [0,inf); y [-1,8]; z free. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, m->col_upper[1]);
    TEST_ASSERT_TRUE(isinf(m->col_lower[2]) && m->col_lower[2] < 0);
    TEST_ASSERT_TRUE(isinf(m->col_upper[2]));

    /* CSC: x hits c1,c2,c3 with 1,2,1; y hits c1,c2,c3 with 1,-1,1;
     * z hits c3 with 1. */
    const int64_t want_start[] = {0, 3, 6, 7};
    const int64_t want_index[] = {0, 1, 2, 0, 1, 2, 2};
    const double  want_value[] = {1.0, 2.0, 1.0, 1.0, -1.0, 1.0, 1.0};
    for (int j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 7; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
    jaos_model_free(m);
}

static void test_g2_maximize_exponents_summing_wrapping(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g2.lp"));

    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, m->sense);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->obj_offset);

    /* 2.5e1 x1 + x1 sums to 26; glued 3x2 reads as 3 * x2. */
    TEST_ASSERT_EQUAL_DOUBLE(26.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[1]);

    /* Wrapped constraint with =< gives (-inf,4]; second row [-1,inf). */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));

    /* x1 default bounds; x2 [0, 1.5]. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, m->col_upper[1]);

    /* CSC: x1 in rows 0,1 (1,1); x2 in rows 0,1 (1,-1). */
    const int64_t want_start[] = {0, 2, 4};
    const int64_t want_index[] = {0, 1, 0, 1};
    const double  want_value[] = {1.0, 1.0, 1.0, -1.0};
    for (int j = 0; j <= 2; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
    jaos_model_free(m);
}

static void expect_reject(const char *path, const char *needle)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_lp(m, path));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), needle));
    jaos_model_free(m);
}

static void test_rejection_reasons_are_specific(void)
{
    expect_reject("tests/data/el_int.lp", "integer");
    expect_reject("tests/data/el_ranged.lp", "ranged");
    expect_reject("tests/data/el_const.lp", "constant term");
    expect_reject("tests/data/el_unkbound.lp", "unknown variable");
    expect_reject("tests/data/el_badchar.lp", "unexpected character");
    expect_reject("tests/data/el_noend.lp", "End");
}

static void test_rejections_carry_line_numbers(void)
{
    expect_reject("tests/data/el_int.lp", "line 5");
    expect_reject("tests/data/el_ranged.lp", "line 4");
    expect_reject("tests/data/el_const.lp", "line 4");
    expect_reject("tests/data/el_unkbound.lp", "line 6");
    expect_reject("tests/data/el_badchar.lp", "line 4");
}

static void test_missing_file_is_io_error(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_read_lp(m, "tests/data/does_not_exist.lp"));
    jaos_model_free(m);
}

static void test_failed_read_preserves_previous_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_lp(m, "tests/data/el_ranged.lp"));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_g1_labels_relations_bounds);
    RUN_TEST(test_g2_maximize_exponents_summing_wrapping);
    RUN_TEST(test_rejection_reasons_are_specific);
    RUN_TEST(test_rejections_carry_line_numbers);
    RUN_TEST(test_missing_file_is_io_error);
    RUN_TEST(test_failed_read_preserves_previous_model);
    return UNITY_END();
}
