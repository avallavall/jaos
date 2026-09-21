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

static void test_a_name_with_spaces_reads_with_underscores_and_writes_mps(void)
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_osil(a, "tests/data/g_osil_spaces.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(a, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("Par,_Inc._Example", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(a, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("x_0", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(a, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("cut_and_dye", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(a, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("Par,_Inc._Objective_Function", nm);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(a, "build/to_spaces.mps"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, "build/to_spaces.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    assert_same_model(a, b);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(b, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("Par,_Inc._Objective_Function", nm);
    jaos_model_free(b);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_nl(a, "build/to_spaces.nl"));
    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(b, "build/to_spaces.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("cut_and_dye", nm);
    jaos_model_free(b);
    jaos_model_free(a);
    remove("build/to_spaces.mps");
    remove("build/to_spaces.nl");
    remove("build/to_spaces.col");
    remove("build/to_spaces.row");

    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\x01y\"/>"
            "</variables>\n</instanceData></osil>\n",
            "not a name JAOS accepts");
}

static void test_a_var_or_con_repeated_by_mult_reads_as_that_many(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_osil(m, "tests/data/g_osil_mult.osil"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("C1", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("C2", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 2, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("z", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("R2", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 2, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("cap", nm);
    bool isint = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_TRUE(m->col_upper[0] == 1.0 && m->col_upper[1] == 1.0);
    TEST_ASSERT_TRUE(m->col_upper[2] == 4.0);
    TEST_ASSERT_TRUE(m->row_upper[0] == 1.5 && m->row_upper[1] == 1.5);
    TEST_ASSERT_TRUE(m->row_upper[2] == 5.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 8.0, obj);
    jaos_model_free(m);

    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"2\"><var name=\"x\" mult=\"2\"/>"
            "</variables>\n</instanceData></osil>\n",
            "would give every copy that name");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"2\"><var mult=\"0\"/>"
            "</variables>\n</instanceData></osil>\n",
            "not a count of 1 or more");
}

static void test_a_quadratic_row_reads_solves_and_writes_back(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_osil(m, "tests/data/g_osil_qcon.osil"));
    TEST_ASSERT_EQUAL_INT64(2, jaos_row_quadratic_nz(m, 0));
    int64_t qi[2], qj[2];
    double qv[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_quadratic(m, 0, qi, qj, qv));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, qv[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, qv[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -2.0, obj);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(m, "build/to_qcon.osil"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_osil(b, "build/to_qcon.osil"));
    TEST_ASSERT_EQUAL_INT64(2, jaos_row_quadratic_nz(b, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_quadratic(b, 0, qi, qj, qv));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, qv[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, qv[1]);
    remove("build/to_qcon.osil");

    const int64_t cols[2] = {0, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(b, JAOS_CONE_QUADRATIC, 2,
                                                 cols));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_osil(b, "build/to_cone.osil"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(b), "second-order cones"));
    jaos_model_free(b);
    jaos_model_free(m);

    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"/></variables>\n"
            "<constraints numberOfConstraints=\"1\"><con ub=\"1\"/>"
            "</constraints>\n"
            "<quadraticCoefficients numberOfQuadraticTerms=\"1\">\n"
            "<qTerm idx=\"3\" idxOne=\"0\" idxTwo=\"0\" coef=\"1\"/>\n"
            "</quadraticCoefficients></instanceData></osil>\n",
            "constraint 3");
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
            "<variables numberOfVariables=\"1\"><var mult=\"3\"/>"
            "</variables>\n</instanceData></osil>\n",
            "declares 1 variables and carries 3");
    refuses("<osil><instanceData>\n"
            "<variables numberOfVariables=\"1\"><var name=\"x\"",
            "ends inside a tag");
}

static void test_a_cost_of_1e300_is_a_cost_and_not_an_infinity(void)
{
    const double cost[2] = {1e300, -1e300};
    const double cl[2] = {-1e6, -1e6}, cu[2] = {1e6, 1e6};
    const double rl[1] = {-1e6}, ru[1] = {1e6};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1e-300, 1e200};
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 2, 1, JAOS_MINIMIZE, 1e200, cost, cl, cu, rl, ru, 2,
                     as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(a, "build/to_huge.osil"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
        jaos_read_osil(b, "build/to_huge.osil"), jaos_model_error(b));
    double c0 = 0.0, c1 = 0.0, v = 0.0, lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(b, 0, &c0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(b, 1, &c1));
    TEST_ASSERT_EQUAL_DOUBLE(1e300, c0);
    TEST_ASSERT_EQUAL_DOUBLE(-1e300, c1);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(b, 0, 1, &v));
    TEST_ASSERT_EQUAL_DOUBLE(1e200, v);
    double off = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_offset(b, &off));
    TEST_ASSERT_EQUAL_DOUBLE(1e200, off);
    jaos_model *c = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(c, 1, 0, JAOS_MINIMIZE, 0.0, (double[]){1.0},
                     (double[]){-1e40}, (double[]){1e40}, nullptr, nullptr,
                     0, (int64_t[]){0, 0}, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_osil(c, "build/to_huge.osil"));
    jaos_model *d = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_osil(d, "build/to_huge.osil"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(d, 0, &lo, &hi));
    TEST_ASSERT_TRUE(lo == -jaos_infinity() && hi == jaos_infinity());
    jaos_model_free(a);
    jaos_model_free(b);
    jaos_model_free(c);
    jaos_model_free(d);
    remove("build/to_huge.osil");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_cost_of_1e300_is_a_cost_and_not_an_infinity);
    RUN_TEST(test_osil_round_trips_an_lp_a_qp_and_a_mip);
    RUN_TEST(test_a_semicontinuous_column_survives_a_model_with_no_integer_column);
    RUN_TEST(test_osil_round_trips_through_gzip);
    RUN_TEST(test_osil_reads_the_row_wise_layout);
    RUN_TEST(test_a_name_with_spaces_reads_with_underscores_and_writes_mps);
    RUN_TEST(test_a_var_or_con_repeated_by_mult_reads_as_that_many);
    RUN_TEST(test_osil_refuses_what_it_cannot_carry);
    RUN_TEST(test_a_quadratic_row_reads_solves_and_writes_back);
    return UNITY_END();
}
