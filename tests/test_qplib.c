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

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr)
        return false;
    fclose(f);
    return true;
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(n, (long)fread(s, 1, (size_t)n, f));
    s[n] = '\0';
    fclose(f);
    return s;
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
        double qa = 0.0, qb = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(a, j, &qa));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, j, &qb));
        TEST_ASSERT_TRUE(qa == qb);
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
}

static void round_trip(const char *src, jaos_status (*reader)(jaos_model *, const char *))
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, reader(a, src));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_qplib(a, "build/tq_tmp.qplib"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_qplib(b, "build/tq_tmp.qplib"));
    assert_same_model(a, b);
    if (!jm_model_has_integer(a) || true) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
        TEST_ASSERT_EQUAL_INT(jaos_status_of(a), jaos_status_of(b));
        if (jaos_status_of(a) == JAOS_SOLVE_OPTIMAL) {
            double oa = 0.0, ob = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
            TEST_ASSERT_TRUE(oa == ob);
        }
    }
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/tq_tmp.qplib");
}

static void test_qplib_round_trips_an_lp_a_qp_and_a_mip(void)
{
    round_trip("tests/data/g1.lp", jaos_read_lp);
    round_trip("tests/data/g_quad.lp", jaos_read_lp);
    round_trip("tests/data/t4_int.mps", jaos_read_mps);
    round_trip("tests/data/g_ranged.lp", jaos_read_lp);
}

static void test_qplib_reads_the_published_layout(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_qplib(m, "tests/data/g_quad.qplib"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("gquad", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("y", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("c1", nm);
    double q = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->col_upper[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->row_lower[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, obj);
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_qplib(m, "tests/data/e_quad_offdiag.qplib"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "off the diagonal"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "line 8"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_qplib(m, "tests/data/e_qcon.qplib"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic constraints"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO, jaos_read_qplib(m, "tests/data/no.qplib"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_qplib(nullptr, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_qplib(m, nullptr));
    jaos_model_free(m);
}

static void test_qplib_refuses_what_it_cannot_express(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g_sos.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_qplib(m, "build/tq_bad.qplib"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "SOS"));
    TEST_ASSERT_FALSE(file_exists("build/tq_bad.qplib"));
    jaos_model_free(m);
    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g_ind.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_qplib(m, "build/tq_bad.qplib"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "indicator"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_osil(m, "build/tq_bad.osil"));
    TEST_ASSERT_FALSE(file_exists("build/tq_bad.osil"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_qplib(nullptr, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_osil(m, nullptr));
    jaos_model_free(m);
}

static void test_osil_carries_every_part_of_the_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g_quad.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(m, "build/tq_tmp.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    char *s = slurp("build/tq_tmp.osil");
    TEST_ASSERT_NOT_NULL(strstr(s, "<osil xmlns=\"os.optimizationservices.org\">"));
    TEST_ASSERT_NOT_NULL(strstr(s, "<variables numberOfVariables=\"2\">"));
    TEST_ASSERT_NOT_NULL(strstr(s, "<var name=\"x\" type=\"C\" ub=\"10\"/>"));
    TEST_ASSERT_NOT_NULL(strstr(s, "<var name=\"y\" type=\"I\" ub=\"10\"/>"));
    TEST_ASSERT_NOT_NULL(strstr(s, "maxOrMin=\"min\" numberOfObjCoef=\"2\" constant=\"0\""));
    TEST_ASSERT_NOT_NULL(strstr(s, "<coef idx=\"0\">1</coef>"));
    TEST_ASSERT_NOT_NULL(strstr(s, "<con name=\"c1\" lb=\"2\"/>"));
    TEST_ASSERT_NOT_NULL(strstr(s, "<linearConstraintCoefficients numberOfValues=\"2\">"));
    TEST_ASSERT_NOT_NULL(strstr(s, "<qTerm idx=\"-1\" idxOne=\"0\" idxTwo=\"0\" coef=\"1\"/>"));
    TEST_ASSERT_NOT_NULL(strstr(s, "</osil>"));
    free(s);
    remove("build/tq_tmp.osil");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(m, "build/tq_tmp.osil.gz"));
    TEST_ASSERT_TRUE(file_exists("build/tq_tmp.osil.gz"));
    remove("build/tq_tmp.osil.gz");
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_qplib_round_trips_an_lp_a_qp_and_a_mip);
    RUN_TEST(test_qplib_reads_the_published_layout);
    RUN_TEST(test_qplib_refuses_what_it_cannot_express);
    RUN_TEST(test_osil_carries_every_part_of_the_model);
    return UNITY_END();
}
