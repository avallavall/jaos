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

static void test_a_linear_nl_reads_with_its_names_bounds_and_integers(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_lin.nl"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("x", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("z", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 2, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("y", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("c2", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("obj", nm);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_cost[1]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_cost[2]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_TRUE(isinf(m->col_lower[1]) && m->col_lower[1] < 0);
    TEST_ASSERT_TRUE(isinf(m->col_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, m->col_upper[2]);
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-5.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);
    bool isint = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &isint));
    TEST_ASSERT_FALSE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_FALSE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 2, &isint));
    TEST_ASSERT_TRUE(isint);
    const int64_t want_start[] = {0, 2, 4, 6};
    const int64_t want_index[] = {0, 1, 0, 2, 0, 1};
    const double want_value[] = {1.0, 1.0, 1.0, 1.0, 2.0, -1.0};
    for (int j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 6; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -4.0, obj);
    jaos_model_free(m);
}

static void test_a_binary_nl_without_name_files_gets_positional_names(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_bin.nl"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, m->sense);
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("C2", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("R1", nm);
    bool isint = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_upper[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    jaos_model_free(m);
}

static void test_what_the_nl_reader_refuses_is_named_by_line(void)
{
    const struct { const char *file, *word; } bad[] = {
        {"tests/data/e_nonlin.nl", "nonlinear expression"},
        {"tests/data/e_nlcount.nl", "linear models only"},
        {"tests/data/e_binary.nl", "binary .nl"},
        {"tests/data/e_compl.nl", "complementarity"},
    };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_INVALID_INPUT,
                                      jaos_read_nl(m, bad[k].file),
                                      bad[k].file);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(jaos_model_error(m), bad[k].word),
                                     jaos_model_error(m));
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "line "));
        jaos_model_free(m);
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_nl(nullptr, "x.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_nl(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO, jaos_read_nl(m, "tests/data/no_such.nl"));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_linear_nl_reads_with_its_names_bounds_and_integers);
    RUN_TEST(test_a_binary_nl_without_name_files_gets_positional_names);
    RUN_TEST(test_what_the_nl_reader_refuses_is_named_by_line);
    return UNITY_END();
}
