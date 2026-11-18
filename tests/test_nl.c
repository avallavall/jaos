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

static void test_a_linear_nl_reads_with_its_names_bounds_and_integers(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
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
#endif
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

static void test_a_quadratic_body_reads_as_q(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_quad.nl"));
    TEST_ASSERT_EQUAL_INT64(3, jaos_quadratic_nz(m));
    int64_t qr[3], qc[3];
    double qv[3], cost = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_quadratic(m, qr, qc, qv));
    for (int64_t k = 0; k < 3; k++) {
        const bool diag = qr[k] == qc[k];
        TEST_ASSERT_TRUE(qr[k] >= qc[k]);
        TEST_ASSERT_EQUAL_DOUBLE(2.0, qv[k]);
        TEST_ASSERT_TRUE(diag || (qr[k] == 1 && qc[k] == 0));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 0, &cost));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, cost);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, 1, &cost));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, cost);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 0.0, obj);
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/e_nonlin.nl"));
    TEST_ASSERT_EQUAL_INT64(1, jaos_row_quadratic_nz(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not convex"));
    jaos_model_free(m);
}

static void test_a_row_sums_the_terms_it_gives_one_column(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_read_nl(m, "tests/data/t_quadrow.nl"));
    TEST_ASSERT_EQUAL_INT64(1, jaos_row_quadratic_nz(m, 0));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_nz(m));
    double a = 0.0, lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 0, 0, &a));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, a);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, hi);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 1, 0, &a));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, a);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, (-5.0 - sqrt(41.0)) / 2.0, obj);
    jaos_model_free(m);
}

static void test_what_the_nl_reader_refuses_is_named_by_line(void)
{
    const struct { const char *file, *word; } bad[] = {
        {"tests/data/e_nl_log.nl", "operator o43"},
        {"tests/data/e_nlcount.nl", "no 'b' segment"},
        {"tests/data/e_binary.nl", "binary .nl"},
        {"tests/data/e_compl.nl", "complementarity"},
        {"tests/data/e_nl_huge.nl", "only from a file at least that many"},
        {"tests/data/e_nl_intcount.nl", "9223372036854775807 binary"},
        {"tests/data/e_nl_exp.nl", "operator o44 (exp)"},
        {"tests/data/e_nl_cube.nl", "the power 3 of a variable"},
        {"tests/data/e_nl_side.nl", "positive coefficient on its lower side"},
        {"tests/data/e_nl_ratio.nl", "do not hold at or above 0"},
        {"tests/data/e_nl_sqrtq.nl", "neither a sum of squares nor a product"},
        {"tests/data/e_nl_range.nl", "two finite bounds"},
        {"tests/data/e_nl_varexp.nl", "exponent is not a constant"},
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

static void test_an_nl_without_an_objective_keeps_its_row_names(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_noobj.nl"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(m));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, 1, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("y", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("c1", nm);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("COST", nm);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_cost[1]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->row_lower[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
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
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
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
#endif
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

static const char *slurp_sol(const char *path);

static double solved(jaos_model *m)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_solve(m), jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = NAN;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    return obj;
}

static void test_bodies_above_degree_two_read_as_cones(void)
{
    const struct {
        const char *file;
        int64_t rows, cols, cones;
        double obj;
    } c[] = {
        {"tests/data/t_nl_norm.nl", 2, 4, 1, 1.4142135623730951},
        {"tests/data/t_nl_soc.nl", 1, 3, 1, 1.4142135623730951},
        {"tests/data/t_nl_headrow.nl", 2, 3, 1, 1.4142135623730951},
        {"tests/data/t_nl_rsoc.nl", 3, 4, 1, 4.0},
        {"tests/data/t_nl_ratio.nl", 3, 5, 1, 1.0},
        {"tests/data/t_nl_root.nl", 1, 4, 1, 6.0},
        {"tests/data/t_nl_objnorm.nl", 3, 8, 2, 2.1602468994692869},
    };
    for (size_t k = 0; k < sizeof c / sizeof *c; k++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_nl(m, c[k].file),
                                      jaos_model_error(m));
        TEST_ASSERT_EQUAL_INT64_MESSAGE(c[k].rows, jaos_num_row(m), c[k].file);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(c[k].cols, jaos_num_col(m), c[k].file);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(c[k].cones, jaos_num_cones(m),
                                        c[k].file);
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-7, c[k].obj, solved(m), c[k].file);
        jaos_model_free(m);
    }

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_nl_soc.nl"));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("l", nm);
    jaos_cone_type type = 0;
    int64_t n = 0, cols[3] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, 0, &type, &n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_CONE_QUADRATIC, type);
    TEST_ASSERT_EQUAL_INT64(3, n);
    TEST_ASSERT_EQUAL_INT64(0, cols[0]);
    TEST_ASSERT_EQUAL_INT64(1, cols[1]);
    TEST_ASSERT_EQUAL_INT64(2, cols[2]);
    TEST_ASSERT_EQUAL_INT64(0, jaos_row_quadratic_nz(m, 0));
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_nl_rsoc.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, 0, &type, &n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_CONE_ROTATED, type);
    TEST_ASSERT_EQUAL_INT64(0, cols[0]);
    TEST_ASSERT_EQUAL_INT64(2, cols[1]);
    TEST_ASSERT_EQUAL_INT64(3, cols[2]);
    double lo = 0.0, hi = 0.0, a = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, 2, &lo, &hi));
    TEST_ASSERT_TRUE(lo == 0.0 && hi == 0.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 2, 1, &a));
    TEST_ASSERT_TRUE(a == -sqrt(2.0));
    jaos_model_free(m);
}

static void test_a_polynomial_that_collapses_reads_at_degree_two(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
                                  jaos_read_nl(m, "tests/data/t_nl_poly.nl"),
                                  jaos_model_error(m));
    double a = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 0, 0, &a));
    TEST_ASSERT_TRUE(a == 0.5);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 0, 1, &a));
    TEST_ASSERT_TRUE(a == 1.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_coefficient(m, 0, 2, &a));
    TEST_ASSERT_TRUE(a == 8.0);
    TEST_ASSERT_EQUAL_INT64(0, jaos_row_quadratic_nz(m, 0));
    TEST_ASSERT_EQUAL_INT64(1, jaos_row_quadratic_nz(m, 1));
    int64_t qr = -1, qc = -1;
    double qv = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_quadratic(m, 1, &qr, &qc, &qv));
    TEST_ASSERT_TRUE(qr == 0 && qc == 0 && qv == 2.0);
    TEST_ASSERT_EQUAL_INT64(0, jaos_quadratic_nz(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_nz(m));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 13.125, solved(m));
    jaos_model_free(m);
}

static void test_integer_columns_in_nonlinear_terms_follow_the_header(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
                                  jaos_read_nl(m, "tests/data/t_nl_miqp.nl"),
                                  jaos_model_error(m));
    const bool want[4] = {false, true, true, true};
    for (int64_t j = 0; j < 4; j++) {
        bool isint = !want[j];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, j, &isint));
        TEST_ASSERT_EQUAL_INT(want[j], isint);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -9.84, solved(m));
    jaos_model_free(m);
}

static void cone_model(jaos_model *m)
{
    const double inf = jaos_infinity();
    const double cost[7] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    const double cl[7] = {-inf, -inf, -inf, -inf, -inf, -3.0, 0.0};
    const double cu[7] = {inf, inf, inf, inf, inf, 3.0, inf};
    const double rl[4] = {2.0, 1.0, 1.5, -inf};
    const double ru[4] = {inf, 1.0, inf, 0.0};
    const int64_t as[8] = {0, 0, 1, 3, 3, 4, 6, 7};
    const int64_t ai[7] = {0, 0, 3, 1, 2, 3, 3};
    const double av[7] = {1.0, 1.0, -0.5, 1.0, 1.0, 0.25, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 7, 4, JAOS_MINIMIZE, 0.5, cost, cl, cu, rl, ru, 7,
                     as, ai, av));
    const int64_t soc[3] = {0, 1, 2}, rot[3] = {3, 4, 5};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 3, soc));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_ROTATED, 3, rot));
    const int64_t qi[1] = {1}, qj[1] = {1};
    const double qv[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 1, qi, qj, qv));
    const int64_t ri[3] = {2, 5, 5}, rj[3] = {2, 5, 2};
    const double rv[3] = {2.0, 1.0, 0.5};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_quadratic(m, 3, 3, ri, rj, rv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 5, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 6, "spare"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 3, "bowl"));
}

static void test_a_quadratic_and_conic_model_writes_to_nl_and_reads_back(void)
{
    jaos_model *a = fresh();
    cone_model(a);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_write_nl(a, "build/tn_cone.nl"),
                                  jaos_model_error(a));
    const char *text = slurp_sol("build/tn_cone.nl");
    TEST_ASSERT_NOT_NULL(strstr(text, "\n 7 6 1 0 1 0\n 3 1\n 0 0\n 6 1 1\n"));
    TEST_ASSERT_NOT_NULL(strstr(text, "\n 0 0 0 1 0\n"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_nl(b, "build/tn_cone.nl"),
                                  jaos_model_error(b));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_col(b));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_row(b));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_cones(b));
    TEST_ASSERT_EQUAL_INT64(1, jaos_quadratic_nz(b));
    char nm[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, 0, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("bowl", nm);
    TEST_ASSERT_EQUAL_INT64(3, jaos_row_quadratic_nz(b, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, 6, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("spare", nm);
    bool isint = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(b, 5, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, solved(a), solved(b));
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/tn_cone.nl");
    remove("build/tn_cone.col");
    remove("build/tn_cone.row");

    a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(a, "tests/data/t_nl_soc.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_nl(a, "build/tn_soc.nl"));
    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(b, "build/tn_soc.nl"));
    assert_same_model(a, b);
    jaos_cone_type ta = 0, tb = 0;
    int64_t na = 0, nb = 0, ca[3] = {0}, cb[3] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(a, 0, &ta, &na, ca));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(b, 0, &tb, &nb, cb));
    TEST_ASSERT_TRUE(ta == tb && na == nb && ca[0] == cb[0] && ca[1] == cb[1] &&
                     ca[2] == cb[2]);
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/tn_soc.nl");
    remove("build/tn_soc.col");
    remove("build/tn_soc.row");
}

static int sol_values(const char *path, double *v, int max)
{
    const char *s = strstr(slurp_sol(path), "Options\n");
    TEST_ASSERT_NOT_NULL(s);
    s += 8;
    char *end;
    const long nopt = strtol(s, &end, 10);
    s = end;
    for (long k = 0; k < nopt + 4; k++)
        strtod(s, &end), s = end;
    int n = 0;
    while (n < max) {
        while (*s == '\n')
            s++;
        if (*s == 'o' || *s == '\0')
            break;
        v[n++] = strtod(s, &end);
        s = end;
    }
    return n;
}

static void test_an_ampl_sol_gives_a_cone_row_its_multiplier(void)
{
    const struct {
        const char *file;
        int n;
        double want[6];
    } c[] = {
        {"tests/data/t_nl_soc.nl", 5,
         {-0.35355339059327373, 0.70710678118654757, 1.4142135623730951,
          1.0, 1.0}},
        {"tests/data/t_nl_norm.nl", 5,
         {-1.0, 0.70710678118654757, 1.0, 1.0, 1.4142135623730951}},
        {"tests/data/t_nl_rsoc.nl", 6, {-1.0, 4.0, -4.0, 4.0, 2.0, 1.0}},
    };
    for (size_t k = 0; k < sizeof c / sizeof *c; k++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, c[k].file));
        solved(m);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_write_sol_ampl(m, "build/tn_cone.sol",
                                                  nullptr));
        double v[8];
        TEST_ASSERT_EQUAL_INT_MESSAGE(c[k].n, sol_values("build/tn_cone.sol",
                                                         v, 8), c[k].file);
        for (int t = 0; t < c[k].n; t++)
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, c[k].want[t], v[t],
                                              c[k].file);
        jaos_model_free(m);
    }
    remove("build/tn_cone.sol");
}

static const char *slurp_sol(const char *path)
{
    static char buf[8192];
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    const size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static void test_an_ampl_sol_carries_the_duals_and_values_of_an_lp(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_ampl_lp.nl"));
    TEST_ASSERT_NULL(m->mip_start);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_sol_ampl(m, "build/tn_ampl.sol", nullptr));
    char want[512];
    snprintf(want, sizeof want,
             "JAOS %s: optimal; objective -7\n\nOptions\n3\n1\n1\n0\n"
             "3\n3\n2\n2\n0\n0\n-2\n4\n1\nobjno 0 0\n", JAOS_VERSION_STRING);
    TEST_ASSERT_EQUAL_STRING(want, slurp_sol("build/tn_ampl.sol"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_write_sol_ampl(m, "build/tn_ampl.sol", "hello\n\nworld\n\n"));
    const char *got = slurp_sol("build/tn_ampl.sol");
    TEST_ASSERT_EQUAL_INT(0, strncmp(got, "hello\nworld\n\nOptions\n3\n", 23));
    remove("build/tn_ampl.sol");
    jaos_model_free(m);
}

static void test_the_nl_bodies_decide_and_a_refusal_keeps_the_counts(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_jump_lp.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -7.0, obj);
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_nl(m, "tests/data/e_nl_log.nl"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_sol_ampl(m, "build/tn_ampl.sol", "refused"));
    TEST_ASSERT_EQUAL_STRING("refused\n\nOptions\n3\n1\n1\n0\n1\n0\n2\n0\n"
                             "objno 0 500\n", slurp_sol("build/tn_ampl.sol"));
    remove("build/tn_ampl.sol");
    jaos_model_free(m);
}

static jaos_callback_action stop_at_first(const jaos_incumbent *inc,
                                          void *user)
{
    (void)inc;
    (void)user;
    return JAOS_CALLBACK_STOP;
}

static const char *sol_tail(const char *s, int lines)
{
    const char *e = s + strlen(s);
    int seen = 0;
    for (const char *p = e - 1; p > s; p--)
        if (p[-1] == '\n' && ++seen == lines)
            return p;
    return s;
}

static void test_an_ampl_sol_codes_the_verdicts(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_nl(m, "tests/data/t_lin.nl"));
    TEST_ASSERT_NOT_NULL(m->mip_start);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, m->mip_start[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->mip_start[2]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_sol_ampl(m, "build/tn_ampl.sol", nullptr));
    const char *got = slurp_sol("build/tn_ampl.sol");
    TEST_ASSERT_NOT_NULL(strstr(got, "\nOptions\n3\n1\n1\n0\n3\n0\n3\n3\n"));
    TEST_ASSERT_EQUAL_STRING("objno 0 0\n", sol_tail(got, 1));
    jaos_model_free(m);

    const double inf = jaos_infinity();
    const double cost[1] = {1.0}, cl[1] = {0.0}, cu[1] = {1.0};
    const double rl[1] = {2.0}, ru[1] = {inf};
    const int64_t as[2] = {0, 1}, ai[1] = {0};
    const double av[1] = {1.0};
    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 1,
                     as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_sol_ampl(m, "build/tn_ampl.sol", nullptr));
    got = slurp_sol("build/tn_ampl.sol");
    TEST_ASSERT_NOT_NULL(strstr(got, ": infeasible\n\nOptions\n3\n1\n1\n0\n"
                                     "1\n0\n1\n0\nobjno 0 200\n"));
    jaos_model_free(m);

    const double kc[2] = {1.0, 1.0}, kl[2] = {0.0, 0.0}, ku[2] = {1.0, 1.0};
    const double krl[1] = {-inf}, kru[1] = {3.0};
    const int64_t kas[3] = {0, 1, 2}, kai[2] = {0, 0};
    const double kav[2] = {2.0, 2.0}, start[2] = {1.0, 0.0};
    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, kc, kl, ku, krl, kru, 2,
                     kas, kai, kav));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, start));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_incumbent_callback(m, stop_at_first, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_sol_ampl(m, "build/tn_ampl.sol", nullptr));
    got = slurp_sol("build/tn_ampl.sol");
    TEST_ASSERT_NOT_NULL(strstr(got, "; objective 1\n"));
    TEST_ASSERT_NOT_NULL(strstr(got, "\n1\n0\n2\n2\n1\n0\nobjno 0 400\n"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_incumbent_callback(m, nullptr,
                                                               nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_write_sol_ampl(m, "build/tn_ampl.sol", nullptr));
    got = slurp_sol("build/tn_ampl.sol");
    TEST_ASSERT_NOT_NULL(strstr(got, "\n1\n0\n2\n0\nobjno 0 401\n"));
    remove("build/tn_ampl.sol");
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_sol_ampl(nullptr, "x", nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_sol_ampl(m, nullptr, nullptr));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_an_ampl_sol_carries_the_duals_and_values_of_an_lp);
    RUN_TEST(test_an_ampl_sol_codes_the_verdicts);
    RUN_TEST(test_the_nl_bodies_decide_and_a_refusal_keeps_the_counts);
    RUN_TEST(test_a_linear_nl_reads_with_its_names_bounds_and_integers);
    RUN_TEST(test_a_binary_nl_without_name_files_gets_positional_names);
    RUN_TEST(test_a_quadratic_body_reads_as_q);
    RUN_TEST(test_a_row_sums_the_terms_it_gives_one_column);
    RUN_TEST(test_what_the_nl_reader_refuses_is_named_by_line);
    RUN_TEST(test_an_nl_without_an_objective_keeps_its_row_names);
    RUN_TEST(test_a_written_nl_reads_back_as_the_same_model);
    RUN_TEST(test_the_nl_writer_puts_the_integer_columns_last);
    RUN_TEST(test_the_nl_writer_refuses_what_the_format_has_no_place_for);
    RUN_TEST(test_bodies_above_degree_two_read_as_cones);
    RUN_TEST(test_a_polynomial_that_collapses_reads_at_degree_two);
    RUN_TEST(test_integer_columns_in_nonlinear_terms_follow_the_header);
    RUN_TEST(test_a_quadratic_and_conic_model_writes_to_nl_and_reads_back);
    RUN_TEST(test_an_ampl_sol_gives_a_cone_row_its_multiplier);
    return UNITY_END();
}
