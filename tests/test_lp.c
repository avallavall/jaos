/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
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

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);

    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);

    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("obj", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 2, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("c3", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 2, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("z", nm);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_cost[1]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_cost[2]);

    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-3.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, m->col_upper[1]);
    TEST_ASSERT_TRUE(isinf(m->col_lower[2]) && m->col_lower[2] < 0);
    TEST_ASSERT_TRUE(isinf(m->col_upper[2]));

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

    TEST_ASSERT_EQUAL_DOUBLE(26.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[1]);

    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));

    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, m->col_upper[1]);

    const int64_t want_start[] = {0, 2, 4};
    const int64_t want_index[] = {0, 1, 0, 1};
    const double  want_value[] = {1.0, 1.0, 1.0, -1.0};
    for (int j = 0; j <= 2; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }

    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("x1", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("x2", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("R1", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("c", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("COST", nm);
    int64_t k = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_index(m, "c", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, "x2", &k));
    TEST_ASSERT_EQUAL_INT64(1, k);
    jaos_model_free(m);
}

static void test_a_ranged_constraint_is_one_row_with_two_ends(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_lp(m, "tests/data/g_ranged.lp"));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[1]);

    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->row_lower[2]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[2]) && m->row_upper[2] > 0.0);
    jaos_model_free(m);
}

static void test_a_constant_in_a_constraint_folds_into_the_rhs(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_lp(m, "tests/data/g_const.lp"));

    TEST_ASSERT_EQUAL_INT64(4, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));

    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[0]);

    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]) && m->row_upper[1] > 0.0);

    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_lower[3]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[3]);

    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->obj_offset);
    jaos_model_free(m);
}

static void test_a_bound_can_be_written_value_first_either_way(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_lp(m, "tests/data/g_revbounds.lp"));

    TEST_ASSERT_EQUAL_INT64(4, jaos_num_col(m));

    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->col_upper[0]);

    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, m->col_upper[1]);

    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->col_upper[2]);

    TEST_ASSERT_TRUE(isinf(m->col_lower[3]) && m->col_lower[3] < 0.0);
    TEST_ASSERT_TRUE(isinf(m->col_upper[3]) && m->col_upper[3] > 0.0);
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
    expect_reject("tests/data/el_int_unknown.lp", "not a variable");
    expect_reject("tests/data/el_rangedir.lp", "same way");
    expect_reject("tests/data/el_bounddir.lp", "same way");
    expect_reject("tests/data/el_unkbound.lp", "unknown variable");
    expect_reject("tests/data/el_badchar.lp", "unexpected character");
    expect_reject("tests/data/el_noend.lp", "End");
}

static void test_rejections_carry_line_numbers(void)
{
    expect_reject("tests/data/el_int_unknown.lp", "line 6");
    expect_reject("tests/data/el_rangedir.lp", "line 4");

    expect_reject("tests/data/el_bounddir.lp", "line 8");
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
        jaos_read_lp(m, "tests/data/el_rangedir.lp"));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);
    jaos_model_free(m);
}

static void test_a_semicontinuous_section_marks_the_columns(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g_semi.lp"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    bool semi = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(m, 0, &semi));
    TEST_ASSERT_TRUE(semi);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(m, 1, &semi));
    TEST_ASSERT_FALSE(semi);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->col_upper[0]);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_g1_labels_relations_bounds);
    RUN_TEST(test_g2_maximize_exponents_summing_wrapping);
    RUN_TEST(test_a_ranged_constraint_is_one_row_with_two_ends);
    RUN_TEST(test_a_constant_in_a_constraint_folds_into_the_rhs);
    RUN_TEST(test_a_bound_can_be_written_value_first_either_way);
    RUN_TEST(test_rejection_reasons_are_specific);
    RUN_TEST(test_rejections_carry_line_numbers);
    RUN_TEST(test_missing_file_is_io_error);
    RUN_TEST(test_failed_read_preserves_previous_model);
    RUN_TEST(test_a_semicontinuous_section_marks_the_columns);
    return UNITY_END();
}
