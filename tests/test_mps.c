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

static void check_t3(jaos_model *m)
{
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));

    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0.0);
    TEST_ASSERT_TRUE(isinf(m->row_upper[0]) && m->row_upper[0] > 0.0);

    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[1]);

    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_cost[1]);

    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);
}

static void test_t3_objname_picks_the_second_free_row(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t3_objname.mps"));
    check_t3(m);
    jaos_model_free(m);
}

static void test_objname_on_the_next_line_is_the_same_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps(m, "tests/data/t3_objname_nextline.mps"));
    check_t3(m);
    jaos_model_free(m);
}

static void test_t1_fixed_layout_full_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);

    TEST_ASSERT_EQUAL_DOUBLE(-3.5, m->obj_offset);

    const double want_cost[] = {1.0, 2.0, -1.0};
    for (int j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_DOUBLE(want_cost[j], m->col_cost[j]);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->col_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[2]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[2]));

    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(3.5, m->row_upper[1]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    const int64_t want_start[] = {0, 2, 4, 6};
    const int64_t want_index[] = {0, 1, 0, 2, 1, 2};
    const double  want_value[] = {1.0, 1.0, 1.0, -1.0, 1.0, 1.0};
    for (int j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 6; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }

    char nm[JAOS_NAME_MAX + 1];
    const char *want_col[] = {"X1", "X2", "X3"};
    const char *want_row[] = {"LIM1", "LIM2", "EQ1"};
    for (int j = 0; j < 3; j++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, j, nm, sizeof nm));
        TEST_ASSERT_EQUAL_STRING(want_col[j], nm);
        int64_t k = -1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, want_col[j], &k));
        TEST_ASSERT_EQUAL_INT64(j, k);
    }
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, i, nm, sizeof nm));
        TEST_ASSERT_EQUAL_STRING(want_row[i], nm);
        int64_t k = -1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_index(m, want_row[i], &k));
        TEST_ASSERT_EQUAL_INT64(i, k);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("COST", nm);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("T1", nm);

    int64_t k = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_row_index(m, "COST", &k));
    jaos_model_free(m);
}

static void test_t2_free_layout_objsense_ranges_wart(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps(m, "tests/data/t2_objsense.mps"));

    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, m->sense);

    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_cost[1]);

    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);

    TEST_ASSERT_TRUE(isinf(m->col_lower[0]) && m->col_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, m->col_upper[0]);

    TEST_ASSERT_TRUE(isinf(m->col_lower[1]) && m->col_lower[1] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->col_upper[1]);

    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->a_value[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->a_value[1]);
    jaos_model_free(m);
}

static void expect_reject(const char *path, const char *needle)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_mps(m, path));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), needle));
    jaos_model_free(m);
}

static void test_rejections_carry_line_numbers(void)
{
    expect_reject("tests/data/e_badnum.mps", "line 6");
    expect_reject("tests/data/e_unknown_row.mps", "line 5");
    expect_reject("tests/data/e_recol.mps", "line 9");
    expect_reject("tests/data/e_dupcoef.mps", "line 6");
}

static void test_rejection_reasons_are_specific(void)
{
    expect_reject("tests/data/e_badnum.mps", "bad number");
    expect_reject("tests/data/e_unknown_row.mps", "unknown row");
    expect_reject("tests/data/e_recol.mps", "contiguous");
    expect_reject("tests/data/e_dupcoef.mps", "duplicate coefficient");
    expect_reject("tests/data/e_noendata.mps", "ENDATA");

    expect_reject("tests/data/e_objname_missing.mps", "no free row by that name");
    expect_reject("tests/data/e_objname_late.mps", "OBJNAME after ROWS");
    expect_reject("tests/data/e_objname_twice.mps", "a second OBJNAME");
    expect_reject("tests/data/e_objname_notfree.mps", "no free row by that name");
}

static void test_missing_file_is_io_error(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_read_mps(m, "tests/data/does_not_exist.mps"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "cannot open"));
    jaos_model_free(m);
}

static void test_failed_read_preserves_previous_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_mps(m, "tests/data/e_badnum.mps"));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(-3.5, m->obj_offset);
    jaos_model_free(m);
}

static void test_a_semicontinuous_bound_marks_the_column(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/e_sc.mps"));
    bool semi = false, integer = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(m, 0, &semi));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &integer));
    TEST_ASSERT_TRUE(semi);
    TEST_ASSERT_FALSE(integer);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->col_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    jaos_model_free(m);
}

static void test_an_sos_section_builds_the_sets(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/g_sos.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_sos(m));
    int t = 0;
    int64_t n = 0, cols[3];
    double w[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_sos(m, 0, &t, &n, cols, w));
    TEST_ASSERT_EQUAL_INT(1, t);
    TEST_ASSERT_EQUAL_INT64(3, n);
    TEST_ASSERT_TRUE(cols[0] == 0 && cols[1] == 1 && cols[2] == 2);
    TEST_ASSERT_TRUE(w[0] == 1.0 && w[1] == 2.0 && w[2] == 3.0);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_t1_fixed_layout_full_model);
    RUN_TEST(test_t3_objname_picks_the_second_free_row);
    RUN_TEST(test_objname_on_the_next_line_is_the_same_model);
    RUN_TEST(test_t2_free_layout_objsense_ranges_wart);
    RUN_TEST(test_rejections_carry_line_numbers);
    RUN_TEST(test_rejection_reasons_are_specific);
    RUN_TEST(test_missing_file_is_io_error);
    RUN_TEST(test_failed_read_preserves_previous_model);
    RUN_TEST(test_a_semicontinuous_bound_marks_the_column);
    RUN_TEST(test_an_sos_section_builds_the_sets);
    return UNITY_END();
}
