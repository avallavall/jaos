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

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr)
        return false;
    fclose(f);
    return true;
}

static void assert_same_model(const jaos_model *a, const jaos_model *b)
{
    TEST_ASSERT_EQUAL_INT64(a->num_col, b->num_col);
    TEST_ASSERT_EQUAL_INT64(a->num_row, b->num_row);
    TEST_ASSERT_EQUAL_INT64(a->num_nz, b->num_nz);
    TEST_ASSERT_EQUAL_INT(a->sense, b->sense);
    TEST_ASSERT_TRUE(a->obj_offset == b->obj_offset);
    for (int64_t j = 0; j < a->num_col; j++) {
        TEST_ASSERT_TRUE(a->col_cost[j] == b->col_cost[j]);
        TEST_ASSERT_TRUE(a->col_lower[j] == b->col_lower[j]);
        TEST_ASSERT_TRUE(a->col_upper[j] == b->col_upper[j]);
        TEST_ASSERT_EQUAL_INT64(a->a_start[j + 1], b->a_start[j + 1]);
        bool ia = false, ib = false;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(a, j, &ia));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(b, j, &ib));
        TEST_ASSERT_EQUAL_INT(ia, ib);
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        TEST_ASSERT_TRUE(a->row_lower[i] == b->row_lower[i]);
        TEST_ASSERT_TRUE(a->row_upper[i] == b->row_upper[i]);
    }
    for (int64_t k = 0; k < a->num_nz; k++) {
        TEST_ASSERT_EQUAL_INT64(a->a_index[k], b->a_index[k]);
        TEST_ASSERT_TRUE(a->a_value[k] == b->a_value[k]);
    }
    char na[JAOS_NAME_MAX + 1], nb[JAOS_NAME_MAX + 1];
    for (int64_t j = 0; j < a->num_col; j++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(a, j, na, sizeof na));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, j, nb, sizeof nb));
        TEST_ASSERT_EQUAL_STRING(na, nb);
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(a, i, na, sizeof na));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, i, nb, sizeof nb));
        TEST_ASSERT_EQUAL_STRING(na, nb);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(a, na, sizeof na));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(b, nb, sizeof nb));
    TEST_ASSERT_EQUAL_STRING(na, nb);
}

static void test_a_written_nl_reads_back_as_the_same_model(void)
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(a, "tests/data/t_lin.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_nl(a, "build/tn_tmp.nl"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));
    TEST_ASSERT_TRUE(file_exists("build/tn_tmp.col"));
    TEST_ASSERT_TRUE(file_exists("build/tn_tmp.row"));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(b, "build/tn_tmp.nl"));
    assert_same_model(a, b);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_TRUE(oa == ob);
    jaos_model_free(b);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_nl(a, "build/tn_tmp.nl.gz"));
    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(b, "build/tn_tmp.nl.gz"));
    assert_same_model(a, b);
    jaos_model_free(b);
    jaos_model_free(a);
    remove("build/tn_tmp.nl");
    remove("build/tn_tmp.nl.gz");
    remove("build/tn_tmp.col");
    remove("build/tn_tmp.row");
}

static void test_the_nl_writer_puts_the_integer_columns_last(void)
{
    const double rl[]   = {-INFINITY, 2.0};
    const double ru[]   = {10.0, 2.0};
    const double cost[] = {1.0, 2.0, 3.0};
    const double cl[]   = {0.0, 0.0, -1.0};
    const double cu[]   = {1.0, 5.0, INFINITY};
    const int64_t as[]  = {0, 2, 3, 4};
    const int64_t ai[]  = {0, 1, 0, 1};
    const double  av[]  = {1.0, 1.0, 2.0, 1.0};

    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 3, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(a, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(a, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(a, 0, "flag"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(a, 1, "count"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(a, 2, "level"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(a, 1, "fixed"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_nl(a, "build/tn_int.nl"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(b, "build/tn_int.nl"));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(b));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(b));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("level", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("flag", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, 2, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("count", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("fixed", nm);
    bool isint = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(b, 0, &isint));
    TEST_ASSERT_FALSE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(b, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(b, 2, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_TRUE(b->col_cost[0] == 3.0 && b->col_cost[1] == 1.0 &&
                     b->col_cost[2] == 2.0);
    TEST_ASSERT_TRUE(b->col_lower[0] == -1.0 && b->col_upper[1] == 1.0 &&
                     b->col_upper[2] == 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, oa, ob);
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/tn_int.nl");
    remove("build/tn_int.col");
    remove("build/tn_int.row");
}

static void test_the_nl_writer_refuses_what_the_format_has_no_place_for(void)
{
    const double rl[]   = {-INFINITY};
    const double ru[]   = {10.0};
    const double cost[] = {1.0, 1.0};
    const double cl[]   = {0.0, 0.0};
    const double cu[]   = {1.0, 1.0};
    const int64_t as[]  = {0, 1, 2};
    const int64_t ai[]  = {0, 0};
    const double  av[]  = {1.0, 1.0};
    const int64_t sos_cols[] = {0, 1};
    const double sos_w[] = {1.0, 2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(m, 1, 2, sos_cols, sos_w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_nl(m, "build/tn_bad.nl"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "SOS"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "MPS"));
    TEST_ASSERT_FALSE(file_exists("build/tn_bad.nl"));
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "amount"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_semicontinuous(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_nl(m, "build/tn_bad.nl"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "'amount'"));
    TEST_ASSERT_FALSE(file_exists("build/tn_bad.nl"));
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "gate"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_indicator(m, 0, 0, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_nl(m, "build/tn_bad.nl"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "'gate'"));
    TEST_ASSERT_FALSE(file_exists("build/tn_bad.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_nl(nullptr, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_nl(m, nullptr));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_linear_nl_reads_with_its_names_bounds_and_integers);
    RUN_TEST(test_a_binary_nl_without_name_files_gets_positional_names);
    RUN_TEST(test_what_the_nl_reader_refuses_is_named_by_line);
    RUN_TEST(test_a_written_nl_reads_back_as_the_same_model);
    RUN_TEST(test_the_nl_writer_puts_the_integer_columns_last);
    RUN_TEST(test_the_nl_writer_refuses_what_the_format_has_no_place_for);
    return UNITY_END();
}
