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

static void assert_same_model(const jaos_model *a, const jaos_model *b)
{
    TEST_ASSERT_EQUAL_INT64(a->num_col, b->num_col);
    TEST_ASSERT_EQUAL_INT64(a->num_row, b->num_row);
    TEST_ASSERT_EQUAL_INT64(a->num_nz, b->num_nz);
    TEST_ASSERT_EQUAL_INT(a->sense, b->sense);
    TEST_ASSERT_TRUE(a->obj_offset == b->obj_offset);
    char na[JAOS_NAME_MAX + 1], nb[JAOS_NAME_MAX + 1];
    for (int64_t j = 0; j < a->num_col; j++) {
        TEST_ASSERT_TRUE(a->col_cost[j] == b->col_cost[j]);
        TEST_ASSERT_TRUE(a->col_lower[j] == b->col_lower[j]);
        TEST_ASSERT_TRUE(a->col_upper[j] == b->col_upper[j]);
        TEST_ASSERT_EQUAL_INT64(a->a_start[j + 1], b->a_start[j + 1]);
        bool ia = false, ib = false;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(a, j, &ia));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(b, j, &ib));
        TEST_ASSERT_EQUAL_INT(ia, ib);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(a, j, &ia));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(b, j, &ib));
        TEST_ASSERT_EQUAL_INT(ia, ib);
        double qa = 0.0, qb = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(a, j, &qa));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, j, &qb));
        TEST_ASSERT_TRUE(qa == qb);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(a, j, na, sizeof na));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, j, nb, sizeof nb));
        TEST_ASSERT_EQUAL_STRING(na, nb);
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        TEST_ASSERT_TRUE(a->row_lower[i] == b->row_lower[i]);
        TEST_ASSERT_TRUE(a->row_upper[i] == b->row_upper[i]);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(a, i, na, sizeof na));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, i, nb, sizeof nb));
        TEST_ASSERT_EQUAL_STRING(na, nb);
    }
    for (int64_t k = 0; k < a->num_nz; k++) {
        TEST_ASSERT_EQUAL_INT64(a->a_index[k], b->a_index[k]);
        TEST_ASSERT_TRUE(a->a_value[k] == b->a_value[k]);
    }
}

static void round_trip(const char *src,
                       jaos_status (*reader)(jaos_model *, const char *))
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, reader(a, src));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(a, "build/to_tmp.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_osil(b, "build/to_tmp.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    assert_same_model(a, b);
    /* Under either presolve fault build the restored point lands on the
     * wrong column, so the tree on `g_semi.lp` never settles and the test
     * ran for an hour without a limit. The dearest of these models takes 3
     * nodes when presolve is right, so 1000 only stops that walk. Both
     * copies stop in the same place, because they are the same model. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(a, 1000));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(b, 1000));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT(jaos_status_of(a), jaos_status_of(b));
    if (jaos_status_of(a) == JAOS_SOLVE_OPTIMAL) {
        double oa = 0.0, ob = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
        TEST_ASSERT_TRUE(oa == ob);
    }
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/to_tmp.osil");
}

static void test_osil_round_trips_an_lp_a_qp_and_a_mip(void)
{
    round_trip("tests/data/g1.lp", jaos_read_lp);
    round_trip("tests/data/g_quad.lp", jaos_read_lp);
    round_trip("tests/data/t4_int.mps", jaos_read_mps);
    round_trip("tests/data/g_ranged.lp", jaos_read_lp);
    round_trip("tests/data/g_revbounds.lp", jaos_read_lp);
    round_trip("tests/data/g_semi.lp", jaos_read_lp);
    round_trip("tests/data/g_miqp.lp", jaos_read_lp);
}

static void test_a_semicontinuous_column_survives_a_model_with_no_integer_column(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test, skipped under either fault build");
#else
    /* min x + y/2 over x + y >= 1, with x in {0} u [2, 10] and y in [0, 1].
     * Taking the zero costs 0.5 and standing on the floor costs 2. The
     * reader used to install the semi-continuous marks and leave
     * `col_integer` null, and `jm_model_has_integer` reads that pointer
     * first, so the copy went to the LP, where x cannot leave [2, 10]. */
    const double cost[] = {1.0, 0.5};
    const double cl[] = {2.0, 0.0}, cu[] = {10.0, 1.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_semicontinuous(a, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(a, "build/semi_tmp.osil"));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_osil(b, "build/semi_tmp.osil"));
    assert_same_model(a, b);
    TEST_ASSERT_NOT_NULL_MESSAGE(b->col_integer,
        "a model that carries a semi-continuous column has to reach the "
        "tree, and the tree reads col_integer");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(a));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(b));

    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, oa);
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-9, 0.5, ob,
        "the copy read back from OSiL has to take the zero as well");

    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/semi_tmp.osil");
#endif
}

static void test_osil_round_trips_through_gzip(void)
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(a, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_osil(a, "build/to_tmp.osil.gz"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_osil(b, "build/to_tmp.osil.gz"));
    assert_same_model(a, b);
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/to_tmp.osil.gz");
}

static void test_osil_reads_the_row_wise_layout(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_osil(m, "tests/data/g_osil_rowwise.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(5, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);
    TEST_ASSERT_TRUE(m->obj_offset == 4.0);

    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("rowwise", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("x", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("r2", nm);

    const double cost[3] = {1.0, 2.0, 3.0};
    const double lower[3] = {0.0, 0.0, -2.0};
    const double upper[3] = {10.0, 1.0, 5.0};
    const bool integer[3] = {false, true, true};
    for (int64_t j = 0; j < 3; j++) {
        TEST_ASSERT_TRUE(m->col_cost[j] == cost[j]);
        TEST_ASSERT_TRUE(m->col_lower[j] == lower[j]);
        TEST_ASSERT_TRUE(m->col_upper[j] == upper[j]);
        bool is_int = false;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, j, &is_int));
        TEST_ASSERT_EQUAL_INT(integer[j], is_int);
    }
    TEST_ASSERT_TRUE(m->row_lower[0] == 1.0);
    TEST_ASSERT_TRUE(isinf(m->row_upper[0]));
    TEST_ASSERT_TRUE(m->row_lower[1] == 2.0);
    TEST_ASSERT_TRUE(m->row_upper[1] == 8.0);

    const int64_t start[4] = {0, 2, 4, 5};
    const int64_t index[5] = {0, 1, 0, 1, 0};
    const double value[5] = {1.0, 1.0, 1.0, 2.0, 1.0};
    for (int64_t j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(start[j], m->a_start[j]);
    for (int64_t k = 0; k < 5; k++) {
        TEST_ASSERT_EQUAL_INT64(index[k], m->a_index[k]);
        TEST_ASSERT_TRUE(m->a_value[k] == value[k]);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
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
    write_file("build/to_bad.osil", text);
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_osil(m, "build/to_bad.osil"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), want));
    jaos_model_free(m);
    remove("build/to_bad.osil");
}

static void test_osil_refuses_what_it_cannot_carry(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_osil(m, "tests/data/e_osil_nonlinear.osil"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "nonlinearExpressions"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "line 15"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_osil(m, "tests/data/e_osil_offdiag.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(1, m->q_nz);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO, jaos_read_osil(m, "tests/data/no.osil"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_osil(nullptr, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_osil(m, nullptr));
    jaos_model_free(m);

    refuses("NAME\nROWS\n", "no <osil> element");
    refuses("<osil><instanceData><variables numberOfVariables=\"2\">\n"
            "<var name=\"x\"/>\n</variables></instanceData></osil>\n",
            "declares 2 variables and carries 1");
    refuses("<osil><instanceData><constraints numberOfConstraints=\"0\"/>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"/></variables>\n"
            "<objectives><obj maxOrMin=\"up\"/></objectives>\n"
            "</instanceData></osil>\n",
            "neither \"max\" nor \"min\"");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"/></variables>\n"
            "<objectives><obj maxOrMin=\"min\"/><obj maxOrMin=\"min\"/>"
            "</objectives>\n</instanceData></osil>\n",
            "more than one <obj>");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\" type=\"Z\"/>"
            "</variables>\n</instanceData></osil>\n",
            "not one of C, B, I, S or D");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"/></variables>\n"
            "<constraints numberOfConstraints=\"1\"><con name=\"r\" lb=\"1\"/>"
            "</constraints>\n"
            "<linearConstraintCoefficients numberOfValues=\"1\">\n"
            "<start><el>0</el></start><rowIdx><el>0</el></rowIdx>"
            "<value><el>1</el></value>\n"
            "</linearConstraintCoefficients>\n</instanceData></osil>\n",
            "<start> block holds 1 entries, and 2 were wanted");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"/></variables>\n"
            "<constraints numberOfConstraints=\"1\"><con name=\"r\" lb=\"1\"/>"
            "</constraints>\n"
            "<linearConstraintCoefficients numberOfValues=\"1\">\n"
            "<start><el>0</el><el>1</el></start>"
            "<rowIdx><el>3</el></rowIdx><value><el>1</el></value>\n"
            "</linearConstraintCoefficients>\n</instanceData></osil>\n",
            "names constraint 3");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\" lb=\"lots\"/>"
            "</variables>\n</instanceData></osil>\n",
            "is not a number");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\" mult=\"3\"/>"
            "</variables>\n</instanceData></osil>\n",
            "repeated by mult");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"",
            "ends inside a tag");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_osil_round_trips_an_lp_a_qp_and_a_mip);
    RUN_TEST(test_a_semicontinuous_column_survives_a_model_with_no_integer_column);
    RUN_TEST(test_osil_round_trips_through_gzip);
    RUN_TEST(test_osil_reads_the_row_wise_layout);
    RUN_TEST(test_osil_refuses_what_it_cannot_carry);
    return UNITY_END();
}
