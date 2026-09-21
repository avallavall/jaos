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

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(text, f);
    fclose(f);
}

static void test_a_bound_or_rhs_line_may_leave_the_set_name_out(void)
{
    write_text("build/tm_noset.mps",
               "NAME          NOSET\n"
               "ROWS\n"
               " N  obj\n"
               " L  r1\n"
               " G  r2\n"
               "COLUMNS\n"
               "    x         obj       1.0   r1        1.0\n"
               "    x         r2        1.0\n"
               "    y         obj       2.0   r1        1.0\n"
               "RHS\n"
               "              r1        4.0   r2        1.0\n"
               "BOUNDS\n"
               " UP           x         3.0\n"
               " LO           y        -1.0\n"
               " FR           y\n"
               "ENDATA\n");
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "build/tm_noset.mps"));
    double lo = 0.0, up = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &up));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, lo);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, up);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 1, &lo, &up));
    TEST_ASSERT_FALSE(isfinite(lo));
    TEST_ASSERT_FALSE(isfinite(up));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &up));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, up);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 1, &lo, &up));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, lo);
    jaos_model_free(m);
    remove("build/tm_noset.mps");
}

static void test_a_fixed_layout_name_with_a_space_reads_with_an_underscore(void)
{
    write_text("build/tm_fixed.mps",
               "NAME          FIXED\n"
               "ROWS\n"
               " N  OB1PNW20\n"
               " E  DEDO3 1R\n"
               " L  LC 123\n"
               "COLUMNS\n"
               "    DEDO3 11  OB1PNW20        .02466   DEDO3 1R           -1.\n"
               "    DEDO3 11  LC 123              1.\n"
               "    DEDO3 12  DEDO3 1R           -1.   LC 123              2.\n"
               "RHS\n"
               "    RHS 1     DEDO3 1R           -2.   LC 123             10.\n"
               "BOUNDS\n"
               " UP BND-1     DEDO3 11       200000.\n"
               "ENDATA\n");
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "build/tm_fixed.mps"));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    char name[64];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, name, sizeof name));
    TEST_ASSERT_EQUAL_STRING("DEDO3_1R", name);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, name, sizeof name));
    TEST_ASSERT_EQUAL_STRING("LC_123", name);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, name, sizeof name));
    TEST_ASSERT_EQUAL_STRING("DEDO3_11", name);
    double lo = 0.0, up = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &up));
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, lo);
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, up);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 1, &lo, &up));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, up);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &up));
    TEST_ASSERT_EQUAL_DOUBLE(200000.0, up);
    double c = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 0, &c));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.02466, c);
    jaos_model_free(m);
    remove("build/tm_fixed.mps");
}

static void test_a_quadobj_section_reads_and_writes_back(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/g_quad.mps"));
    double q = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 1, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    jaos_model_stats st;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_statistics(m, &st));
    TEST_ASSERT_EQUAL_INT64(2, st.quadratic_col);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, "build/tm_quad.mps"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, "build/tm_quad.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, 1, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    jaos_model_free(b);
    remove("build/tm_quad.mps");

    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, "tests/data/g_qmatrix.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    jaos_model_free(b);
    jaos_model_free(m);



    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_mps(b, "tests/data/e_quad_offdiag.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    TEST_ASSERT_EQUAL_INT64(1, b->q_nz);
    TEST_ASSERT_EQUAL_INT64(1, b->q_index[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, b->q_value[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(b, "build/tm_off.mps"));
    jaos_model *c = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(c, "build/tm_off.mps"));
    TEST_ASSERT_EQUAL_INT64(1, c->q_nz);
    TEST_ASSERT_EQUAL_INT64(b->q_index[0], c->q_index[0]);
    TEST_ASSERT_TRUE(b->q_value[0] == c->q_value[0]);
    jaos_model_free(c);
    jaos_model_free(b);
    remove("build/tm_off.mps");
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

static void test_an_indicators_section_marks_the_rows(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/g_ind.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    int64_t zc = -2;
    int zv = -2;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_indicator(m, 0, &zc, &zv));
    TEST_ASSERT_TRUE(zc == 1 && zv == 1);
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
    RUN_TEST(test_an_indicators_section_marks_the_rows);
    RUN_TEST(test_a_quadobj_section_reads_and_writes_back);
    RUN_TEST(test_a_bound_or_rhs_line_may_leave_the_set_name_out);
    RUN_TEST(test_a_fixed_layout_name_with_a_space_reads_with_an_underscore);
    return UNITY_END();
}
