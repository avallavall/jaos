/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    fputs(text, f);
    fclose(f);
}

static void refuses(const char *text, const char *want)
{
    write_file("build/tc_bad.cbf", text);
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_cbf(m, "build/tc_bad.cbf"));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(jaos_model_error(m), want),
                                 jaos_model_error(m));
    jaos_model_free(m);
    remove("build/tc_bad.cbf");
}

static double solved(jaos_model *m)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    return obj;
}

static void test_the_manual_s_minimal_example_reads(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_cbf(m, "tests/data/g_cbf_min.cbf"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_cones(m));
    jaos_cone_type ty;
    int64_t n = 0, cols[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, 0, &ty, &n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_CONE_QUADRATIC, ty);
    TEST_ASSERT_EQUAL_INT64(3, n);
    bool is_int = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &is_int));
    TEST_ASSERT_TRUE(is_int);
    double lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(8.4, lo);
    TEST_ASSERT_EQUAL_DOUBLE(8.4, hi);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.1, solved(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, false));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.1 * 8.4 / sqrt(6.2 * 6.2 + 7.3 * 7.3),
                              solved(m));
    jaos_model_free(m);
}

static void test_a_cone_over_expressions_takes_new_columns(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_cbf(m, "tests/data/g_cbf_cone.cbf"));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_cones(m));
    int64_t n = 0, cols[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, 0, nullptr, &n, cols));
    TEST_ASSERT_EQUAL_INT64(4, cols[0]);
    TEST_ASSERT_EQUAL_INT64(6, cols[2]);
    double lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 1, &lo, &hi));
    TEST_ASSERT_TRUE(isinf(lo) && lo < 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, hi);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 3.0, solved(m));
    jaos_model_free(m);
}

static void test_a_block_of_single_variables_puts_the_cone_on_them(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_cbf(m, "tests/data/g_cbf_rquad.cbf"));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    jaos_cone_type ty;
    int64_t n = 0, cols[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, 0, &ty, &n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_CONE_ROTATED, ty);
    TEST_ASSERT_EQUAL_INT64(0, cols[0]);
    TEST_ASSERT_EQUAL_INT64(1, cols[1]);
    TEST_ASSERT_EQUAL_INT64(2, cols[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 2.0, solved(m));
    jaos_model_free(m);
}

static void test_the_lp_example_stops_at_change(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_cbf(m, "tests/data/g_cbf_lp.cbf"));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    double c = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 1, &c));
    TEST_ASSERT_EQUAL_DOUBLE(0.64, c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 984.0 / 193.0, solved(m));
    jaos_model_free(m);
}

static void test_a_model_goes_out_and_back_through_cbf(void)
{
    const char *files[] = {"tests/data/solve1.mps", "tests/data/g_cone.mps",
                           "tests/data/g_cbf_cone.cbf",
                           "tests/data/g_cbf_rquad.cbf"};
    for (size_t k = 0; k < sizeof files / sizeof *files; k++) {
        jaos_model *m = fresh();
        const char *f = files[k];
        const bool cbf = strstr(f, ".cbf") != nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, cbf ? jaos_read_cbf(m, f)
                                           : jaos_read_mps(m, f));
        const double want = solved(m);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_cbf(m, "build/tc_out.cbf"));
        jaos_model *b = fresh();
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
                                      jaos_read_cbf(b, "build/tc_out.cbf"),
                                      jaos_model_error(b));
        TEST_ASSERT_EQUAL_INT64(jaos_num_col(m), jaos_num_col(b));
        TEST_ASSERT_EQUAL_INT64(jaos_num_cones(m), jaos_num_cones(b));
        TEST_ASSERT_DOUBLE_WITHIN(1e-7 * (1.0 + fabs(want)), want, solved(b));
        jaos_model_free(b);
        jaos_model_free(m);
    }
    remove("build/tc_out.cbf");

    jaos_model *q = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(q, "tests/data/g_quad.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_cbf(q, "build/tc_quad.cbf"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(q), "quadratic objective"));
    jaos_model_free(q);
}

static void test_the_reader_refuses_what_jaos_does_not_carry(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_cbf(m, "tests/data/e_cbf_psd.cbf"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "semidefinite"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "line 8"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_cbf(m, "tests/data/e_cbf_exp.cbf"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "'EXP'"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO, jaos_read_cbf(m, "tests/data/no.cbf"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_cbf(nullptr, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_cbf(m, nullptr));
    jaos_model_free(m);

    refuses("OBJSENSE\nMIN\n", "starts with VER");
    refuses("VER\n4\nOBJSENSE\nMIN\n", "versions 1 to 3");
    refuses("VER\n3\nVAR\n1 1\nF 1\n", "no OBJSENSE");
    refuses("VER\n3\nOBJSENSE\nLOW\n", "MIN or MAX");
    refuses("VER\n3\nOBJSENSE\nMIN\nVAR\n2 1\nQR 1\n", "QR cone takes 2");
    refuses("VER\n3\nOBJSENSE\nMIN\nVAR\n3 1\nF 2\n", "hold 2 entries");
    refuses("VER\n3\nOBJSENSE\nMIN\nVAR\n1 1\nF 1\nCON\n1 1\nL+ 1\n"
            "ACOORD\n2\n0 0 1\n0 0 2\n", "a second coefficient");
    refuses("VER\n3\nOBJSENSE\nMIN\nVAR\n1 1\nF 1\nCON\n1 1\nL+ 1\n"
            "ACOORD\n1\n1 0 1\n", "names constraint '1'");
    refuses("VER\n3\nOBJSENSE\nMIN\nVAR\n1 1\nF 1\nOBJACOORD\n2\n0 1\n0 2\n",
            "a second coefficient");
    refuses("VER\n3\nOBJSENSE\nMIN\nBOUNDS\n", "not a CBF keyword");
    refuses("VER\n3\nOBJSENSE\nMIN\nPOWCONES\n1 2\n2\n1\n1\n", "power cones");
    refuses("VER\n3\nOBJSENSE\nMIN\nOBJSENSE\nMAX\n", "a second OBJSENSE");
    refuses("VER\n3\nOBJSENSE\nMIN\nCON\n1 1\nL+ 1\nVAR\n1 1\nF 1\n",
            "the variables come first");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_manual_s_minimal_example_reads);
    RUN_TEST(test_a_cone_over_expressions_takes_new_columns);
    RUN_TEST(test_a_block_of_single_variables_puts_the_cone_on_them);
    RUN_TEST(test_the_lp_example_stops_at_change);
    RUN_TEST(test_a_model_goes_out_and_back_through_cbf);
    RUN_TEST(test_the_reader_refuses_what_jaos_does_not_carry);
    return UNITY_END();
}
