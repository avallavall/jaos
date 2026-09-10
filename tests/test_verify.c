/* SPDX-License-Identifier: Apache-2.0 */
#include "unity.h"

#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *model_two(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double cost[2] = { -1.0, -1.0 };
    const double cl[2]   = {  0.0,  0.0 };
    const double cu[2]   = {  3.0, INFINITY };
    const double rl[1]   = { -INFINITY };
    const double ru[1]   = {  4.0 };
    const int64_t start[3] = { 0, 1, 2 };
    const int64_t index[2] = { 0, 0 };
    const double value[2]  = { 1.0, 1.0 };

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, start, index, value));
    return m;
}

[[maybe_unused]] static jaos_model *model_decimal(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double cost[2] = { -1.0, -1.0 };
    const double cl[2]   = {  0.0,  0.0 };
    const double cu[2]   = { INFINITY, INFINITY };
    const double rl[2]   = { -INFINITY, -INFINITY };
    const double ru[2]   = {  1.2, 2.1 };
    const int64_t start[3] = { 0, 2, 3 };
    const int64_t index[3] = { 0, 1, 0 };
    const double value[3]  = { 0.1, 0.7, 0.3 };

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, start, index, value));
    return m;
}

static jaos_model *model_dual_scaled(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double cost[2] = { -2.0, -1.0 };
    const double cl[2]   = {  0.0,  0.0 };
    const double cu[2]   = { 100.0, 100.0 };
    const double rl[1]   = { -INFINITY };
    const double ru[1]   = {  1.0 };
    const int64_t start[3] = { 0, 1, 2 };
    const int64_t index[2] = { 0, 0 };
    const double value[2]  = { 0.1, 0.3 };

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, start, index, value));
    return m;
}

static jaos_model *model_max(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double cost[2] = { 2.0, 3.0 };
    const double cl[2]   = { 0.0, 0.0 };
    const double cu[2]   = { 4.0, INFINITY };
    const double rl[1]   = { -INFINITY };
    const double ru[1]   = { 10.0 };
    const int64_t start[3] = { 0, 1, 2 };
    const int64_t index[2] = { 0, 0 };
    const double value[2]  = { 1.0, 1.0 };

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, start, index, value));
    return m;
}

static jaos_model *model_block3(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double cost[3] = { 1.0, 1.0, 1.0 };
    const double cl[3]   = { 0.0, 0.0, 0.0 };
    const double cu[3]   = { 100.0, 100.0, 100.0 };
    const double rl[3]   = { 7.0, 10.0, 15.0 };
    const double ru[3]   = { 7.0, 10.0, 15.0 };

    const int64_t start[4] = { 0, 3, 6, 9 };
    const int64_t index[9] = { 0, 1, 2, 0, 1, 2, 0, 1, 2 };
    const double value[9]  = { 2.0, 1.0, 1.0,
                               1.0, 3.0, 1.0,
                               1.0, 1.0, 4.0 };

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     9, start, index, value));
    return m;
}

static jaos_verify_report verify_of(jaos_model *m)
{
    jaos_verify_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    return r;
}

static void test_proves_a_small_optimum(void)
{
    jaos_model *m = model_two();
    const jaos_verify_report r = verify_of(m);

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
                                  "a two-column optimum is not proved");
    TEST_ASSERT_EQUAL_INT(-1, r.at_row);
    TEST_ASSERT_EQUAL_INT(-1, r.at_col);

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -4.0, obj);
    jaos_model_free(m);
}

static void test_proves_a_model_whose_rows_need_scaling(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)

    TEST_IGNORE_MESSAGE("positive test — skipped under the fault-injection "
                        "build");
#else
    jaos_model *m = model_decimal();
    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "a model of decimal data is not proved");
    jaos_model_free(m);
#endif
}

static void test_proves_a_model_whose_dual_carries_the_scale(void)
{
    jaos_model *m = model_dual_scaled();
    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "the dual is not scaled back the way the rows were");
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_STAGE_NONE, r.stage);
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -20.0, obj);
    jaos_model_free(m);
}

static void test_proves_a_maximize_model(void)
{
    jaos_model *m = model_max();
    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "the objective sense is not applied where the duals are judged");
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 30.0, obj);
    jaos_model_free(m);
}

static void test_proves_a_model_with_a_real_block(void)
{
    jaos_model *m = model_block3();
    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "a model whose basis is one dense block is not proved");

    TEST_ASSERT_TRUE_MESSAGE(r.terms > 0, "no product was formed");
    jaos_model_free(m);
}

static void test_rejects_a_basic_value_outside_its_bound(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    int64_t basic_col = -1;
    for (int64_t j = 0; j < m->num_col; j++)
        if (m->sol_col_status[j] == JAOS_BASIS_BASIC)
            basic_col = j;
    TEST_ASSERT_TRUE_MESSAGE(basic_col >= 0, "no basic structural to move");
    m->col_upper[basic_col] = 0.5;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_BROKEN, r.status,
        "a basic value above its own bound is not rejected");
    TEST_ASSERT_EQUAL_INT_MESSAGE(basic_col, r.at_col,
        "the wrong column is named");
    jaos_model_free(m);
}

static void test_rejects_a_reduced_cost_of_the_wrong_sign(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    int64_t nb = -1;
    double wrong = 0.0;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (m->sol_col_status[j] == JAOS_BASIS_AT_LOWER) {
            nb = j; wrong = -1000.0; break;
        }
        if (m->sol_col_status[j] == JAOS_BASIS_AT_UPPER) {
            nb = j; wrong = 1000.0; break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(nb >= 0, "no column rests on a bound");
    m->col_cost[nb] = wrong;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_BROKEN, r.status,
        "a reduced cost of the wrong sign is not rejected");
    TEST_ASSERT_EQUAL_INT_MESSAGE(nb, r.at_col, "the wrong column is named");
    jaos_model_free(m);
}

static void test_rejects_a_basis_of_the_wrong_count(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col_status[j] = JAOS_BASIS_BASIC;

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_NUMERICAL, jaos_verify(m, &r),
        "a basis with the wrong count is accepted");
    jaos_model_free(m);
}

static void test_rejects_a_structurally_singular_basis(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)

    TEST_IGNORE_MESSAGE("its precondition is a positive test — skipped under "
                        "the fault-injection build");
#else
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[2] = { -1.0, 0.0 };
    const double cl[2]   = {  0.0, 0.0 };
    const double cu[2]   = { 10.0, 5.0 };
    const double rl[1]   = { -INFINITY };
    const double ru[1]   = {  4.0 };
    const int64_t start[3] = { 0, 1, 1 };
    const int64_t index[1] = { 0 };
    const double value[1]  = { 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, start, index, value));

    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "the model itself does not verify before it is corrupted");

    m->sol_col_status[0] = JAOS_BASIS_AT_UPPER;
    m->sol_col_status[1] = JAOS_BASIS_BASIC;
    m->sol_row_status[0] = JAOS_BASIS_AT_UPPER;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_BROKEN, r.status,
        "a basis with no transversal is not rejected");
    jaos_model_free(m);
#endif
}

static void test_rejects_a_basis_that_is_singular_with_a_transversal(void)
{
    jaos_model *m = model_block3();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "the model does not verify before it is corrupted");

    for (int64_t k = 0; k < 3; k++)
        m->a_value[3 + k] = 2.0 * m->a_value[k];

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_BROKEN, r.status,
        "a singular basis is not rejected");
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_STAGE_RANK, r.stage,
        "a singular basis is not reported at the rank stage");
    jaos_model_free(m);
}

static void test_names_the_rank_stage_when_there_is_no_transversal(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    TEST_IGNORE_MESSAGE("its precondition is a positive test — skipped under "
                        "the fault-injection build");
#else
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[2] = { -1.0, 0.0 };
    const double cl[2]   = {  0.0, 0.0 };
    const double cu[2]   = { 10.0, 5.0 };
    const double rl[1]   = { -INFINITY };
    const double ru[1]   = {  4.0 };
    const int64_t start[3] = { 0, 1, 1 };
    const int64_t index[1] = { 0 };
    const double value[1]  = { 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, start, index, value));
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    m->sol_col_status[0] = JAOS_BASIS_AT_UPPER;
    m->sol_col_status[1] = JAOS_BASIS_BASIC;
    m->sol_row_status[0] = JAOS_BASIS_AT_UPPER;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_BROKEN, r.status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_STAGE_RANK, r.stage,
        "no transversal is not reported at the rank stage");
    jaos_model_free(m);
#endif
}

static void test_refuses_a_basis_wider_than_the_limbs(void)
{
    enum { N = 100 };
    double cost[N], cl[N], cu[N], rl[N], ru[N], value[N];
    int64_t start[N + 1], index[N];
    for (int64_t j = 0; j < N; j++) {
        cost[j] = -1.0;
        cl[j] = 0.0;
        cu[j] = 100.0;
        rl[j] = -INFINITY;
        ru[j] = 1.0;
        start[j] = j;
        index[j] = j;
        value[j] = 0.1;
    }
    start[N] = N;

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, N, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     N, start, index, value));

    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_REFUSED, r.status,
        "a basis past the limb budget is not refused");
    TEST_ASSERT_TRUE_MESSAGE(r.bound_bits > r.capacity_bits,
        "the bound does not exceed the capacity it was refused for");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, r.terms,
        "the refusal did work before refusing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, r.bytes_held,
        "the refusal allocated a block table");
    jaos_model_free(m);
}

static void test_refuses_a_nonbasic_variable_on_an_infinite_bound(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    TEST_ASSERT_TRUE(!isfinite(m->col_upper[1]));
    m->sol_col_status[1] = JAOS_BASIS_AT_UPPER;

    m->sol_col_status[0] = JAOS_BASIS_AT_UPPER;
    m->sol_row_status[0] = JAOS_BASIS_BASIC;

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_verify(m, &r),
        "an infinite bound returns an error rather than a verdict");
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_REFUSED, r.status,
        "a nonbasic variable on an infinite bound is not refused");
    jaos_model_free(m);
}

static void test_reads_the_bound_before_it_allocates(void)
{
    jaos_model *m = model_block3();
    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_TRUE_MESSAGE(r.capacity_bits >= 4096.0,
        "the capacity is not the limb budget");
    TEST_ASSERT_TRUE_MESSAGE(r.bound_bits >= 4.0,
        "the bound is below log2 of the determinant it must cover");
    TEST_ASSERT_TRUE_MESSAGE(r.bound_bits < r.capacity_bits,
        "a three-column basis does not fit");
    TEST_ASSERT_TRUE_MESSAGE(r.bytes_held > 0,
        "a three-row block was solved without a table");
    jaos_model_free(m);
}

static void test_a_unit_basis_costs_no_bits(void)
{
    jaos_model *m = model_two();
    const jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);
    TEST_ASSERT_TRUE_MESSAGE(r.bound_bits >= 0.0, "the bound went negative");
    TEST_ASSERT_TRUE_MESSAGE(r.bound_bits < r.capacity_bits, "it does not fit");
    jaos_model_free(m);
}

static void test_is_reproducible(void)
{
    jaos_model *m = model_block3();
    const jaos_verify_report a = verify_of(m);
    jaos_verify_report b;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &b));
    TEST_ASSERT_EQUAL_INT(a.status, b.status);
    TEST_ASSERT_EQUAL_INT64(a.blocks, b.blocks);
    TEST_ASSERT_EQUAL_INT64(a.largest_block, b.largest_block);
    TEST_ASSERT_EQUAL_INT64(a.terms, b.terms);
    TEST_ASSERT_EQUAL_DOUBLE(a.bound_bits, b.bound_bits);
    jaos_model_free(m);
}

static void test_refuses_a_model_that_was_not_solved(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_verify(nullptr, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_verify(m, nullptr));
    jaos_model_free(m);
}

static void test_refuses_a_quadratic_objective_by_name(void)
{
    const int64_t qr[] = {0, 1, 1};
    const int64_t qc[] = {0, 0, 1};
    const double  qv[] = {2.0, 1.0, 2.0};
    jaos_verify_report r;

    jaos_model *m = model_two();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_REFUSED, r.status);
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic"));
    jaos_model_free(m);

    m = model_two();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_REFUSED, r.status);
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic"));
    jaos_model_free(m);
}

static jaos_model *model_third(jaos_obj_sense sense)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[1] = { sense == JAOS_MINIMIZE ? 1.0 : -1.0 };
    const double cl[1] = { 0.0 }, cu[1] = { INFINITY };
    const double rl[1] = { 1.0 }, ru[1] = { INFINITY };
    const int64_t start[2] = { 0, 1 }, index[1] = { 0 };
    const double value[1] = { 3.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, sense, 0.5, cost, cl, cu, rl, ru,
                     1, start, index, value));
    return m;
}

static void test_a_proved_basis_gives_its_values_exactly(void)
{
    jaos_model *m = model_third(JAOS_MINIMIZE);
    const char *v = nullptr;

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_objective(m, &v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "jaos_verify"));

    jaos_verify_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_value(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("1/3", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_row_dual(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("1/3", v);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_objective(m, &v));
    TEST_ASSERT_EQUAL_STRING("5/6", v);

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0 / 6.0, obj);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 1, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_row_dual(m, -1, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, &v));
    jaos_model_free(m);
}

static void test_exact_values_carry_the_model_s_own_sign(void)
{

    jaos_model *m = model_third(JAOS_MAXIMIZE);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_verify_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
    const char *v = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_value(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("1/3", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_objective(m, &v));
    TEST_ASSERT_EQUAL_STRING("1/6", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_row_dual(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("-1/3", v);

#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, nullptr, nullptr, y, nullptr));
    TEST_ASSERT_TRUE(y[0] < 0.0);
#endif
    jaos_model_free(m);
}

static void test_a_nonbasic_column_reads_its_bound_and_the_two_by_two_its_solve(void)
{

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[2] = { -1.0, -2.0 }, cl[2] = { 0.0, 0.0 };
    const double cu[2] = { 3.0, INFINITY }, rl[1] = { -INFINITY }, ru[1] = { 4.0 };
    const int64_t start[3] = { 0, 1, 2 }, index[2] = { 0, 0 };
    const double value[2] = { 1.0, 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, start, index, value));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_verify_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
    const char *v = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_value(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("0", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_value(m, 1, &v));
    TEST_ASSERT_EQUAL_STRING("4", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_objective(m, &v));
    TEST_ASSERT_EQUAL_STRING("-8", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_row_dual(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("-2", v);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, &v));
    jaos_model_free(m);
}

static const char *TMP_PROOF = "build/tv_tmp.proof";

static bool proof_edit(const char *from, const char *to)
{
    FILE *f = fopen(TMP_PROOF, "r");
    if (f == nullptr)
        return false;
    static char buf[65536];
    const size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    char *at = strstr(buf, from);
    if (at == nullptr)
        return false;
    static char out[65536];
    const size_t head = (size_t)(at - buf);
    memcpy(out, buf, head);
    const size_t tolen = strlen(to);
    memcpy(out + head, to, tolen);
    strcpy(out + head + tolen, at + strlen(from));
    f = fopen(TMP_PROOF, "w");
    if (f == nullptr)
        return false;
    fputs(out, f);
    return fclose(f) == 0;
}

static void test_a_proof_file_round_trips_and_is_checked_exactly(void)
{

    for (int arm = 0; arm < 2; arm++) {
        jaos_model *m = model_third(arm == 0 ? JAOS_MINIMIZE
                                             : JAOS_MAXIMIZE);

        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                              jaos_write_proof(m, TMP_PROOF));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        jaos_verify_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &rep));
        TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, TMP_PROOF));

        jaos_model *fresh = model_third(arm == 0 ? JAOS_MINIMIZE
                                                 : JAOS_MAXIMIZE);
        jaos_proof_report pr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_check_proof(fresh, TMP_PROOF, &pr));
        TEST_ASSERT_TRUE(pr.primal);
        TEST_ASSERT_TRUE(pr.dual);
        TEST_ASSERT_TRUE(pr.objective);
        TEST_ASSERT_EQUAL_INT64(-1, pr.bad_row);
        TEST_ASSERT_EQUAL_INT64(-1, pr.bad_col);
        TEST_ASSERT_TRUE(pr.terms > 0);
        jaos_model_free(fresh);
        jaos_model_free(m);
    }
    remove(TMP_PROOF);
}

static void test_the_proof_checker_rejects_what_is_not_optimal(void)
{
    jaos_model *m = model_third(JAOS_MINIMIZE);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_verify_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, TMP_PROOF));
    jaos_model_free(m);

    jaos_proof_report pr;

    jaos_model *a = model_third(JAOS_MINIMIZE);
    TEST_ASSERT_TRUE(proof_edit("col C1 1/3", "col C1 1/4"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(a, TMP_PROOF, &pr));
    TEST_ASSERT_FALSE(pr.primal);
    TEST_ASSERT_EQUAL_INT64(0, pr.bad_row);
    jaos_model_free(a);
    TEST_ASSERT_TRUE(proof_edit("col C1 1/4", "col C1 1/3"));

    jaos_model *b = model_third(JAOS_MINIMIZE);
    TEST_ASSERT_TRUE(proof_edit("row R1 1/3", "row R1 0"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(b, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(pr.primal);
    TEST_ASSERT_FALSE(pr.dual);
    TEST_ASSERT_EQUAL_INT64(0, pr.bad_col);
    jaos_model_free(b);
    TEST_ASSERT_TRUE(proof_edit("row R1 0", "row R1 1/3"));

    jaos_model *c = model_third(JAOS_MINIMIZE);
    TEST_ASSERT_TRUE(proof_edit("objective 5/6", "objective 4/6"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(c, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(pr.primal);
    TEST_ASSERT_TRUE(pr.dual);
    TEST_ASSERT_FALSE(pr.objective);
    jaos_model_free(c);
    TEST_ASSERT_TRUE(proof_edit("objective 4/6", "objective 5/6"));

    jaos_model *d = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&d));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(d, TMP_PROOF, &pr));
    jaos_model_free(d);

    jaos_model *e = model_third(JAOS_MINIMIZE);
    TEST_ASSERT_TRUE(proof_edit("col C1 1/3", "col C1 1/0"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(e, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(proof_edit("col C1 1/0", "col C1 1/3x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(e, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(proof_edit("col C1 1/3x", "col C1 1/3"));
    TEST_ASSERT_TRUE(proof_edit("proof optimal", "proof unbounded"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(e, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(proof_edit("proof unbounded", "proof optimal"));
    TEST_ASSERT_TRUE(proof_edit("sense min", "sense max"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(e, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(proof_edit("sense max", "sense min"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(e, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(pr.primal && pr.dual && pr.objective);
    jaos_model_free(e);

    remove(TMP_PROOF);
}

static jaos_model *model_infeasible(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[1] = { 1.0 };
    const double cl[1] = { -INFINITY }, cu[1] = { INFINITY };
    const double rl[2] = { 1.0, -INFINITY }, ru[2] = { INFINITY, 0.0 };
    const int64_t start[2] = { 0, 2 }, index[2] = { 0, 1 };
    const double value[2] = { 1.0, 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, start, index, value));
    return m;
}

static jaos_model *model_thirds(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[3] = {-2.0, 3.0, -5.0};
    const double cl[3] = {0.0, 0.0, -1.0};
    const double cu[3] = {INFINITY, 2.0, INFINITY};
    const double rl[3] = {3.0, 2.0, -1.0};
    const double ru[3] = {3.0, INFINITY, -1.0};
    const int64_t start[4] = {0, 3, 6, 9};
    const int64_t index[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
    const double value[9] = {-3.0, 3.0, -1.0, 3.0, 1.0, -3.0,
                             -3.0, 3.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     9, start, index, value));
    return m;
}

static void test_a_written_proof_is_one_its_own_checker_takes(void)
{
    jaos_model *m = model_thirds();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    const jaos_status w = jaos_write_proof(m, TMP_PROOF);
    jaos_model_free(m);
    if (w != JAOS_OK)
        return;

    jaos_model *a = model_thirds();
    jaos_proof_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(a, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_INFEASIBLE, pr.kind);
    TEST_ASSERT_TRUE_MESSAGE(pr.certified,
        "solve wrote a proof its own checker calls broken");
    jaos_model_free(a);
    remove(TMP_PROOF);
}

static void test_an_infeasibility_certificate_is_checked_exactly(void)
{
    jaos_model *m = model_infeasible();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, TMP_PROOF));
    jaos_model_free(m);

    jaos_proof_report pr;
    jaos_model *a = model_infeasible();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(a, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_INFEASIBLE, pr.kind);
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)

    TEST_ASSERT_FALSE_MESSAGE(pr.certified,
                              "a ray lifted onto the wrong row must not "
                              "certify, exactly or otherwise");
    jaos_model_free(a);
    remove(TMP_PROOF);
    return;
#else
    TEST_ASSERT_TRUE(pr.certified);

    TEST_ASSERT_FALSE(pr.primal);
    jaos_model_free(a);
#endif

    jaos_model *b = model_infeasible();
    TEST_ASSERT_TRUE(proof_edit("ray R2 -1", "ray R2 0"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(b, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_INFEASIBLE, pr.kind);
    TEST_ASSERT_FALSE(pr.certified);
    jaos_model_free(b);
    TEST_ASSERT_TRUE(proof_edit("ray R2 0", "ray R2 -1"));

    jaos_model *c = model_infeasible();
    TEST_ASSERT_TRUE(proof_edit("ray R1 1", "col C1 1"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(c, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(proof_edit("col C1 1", "ray R1 1"));
    TEST_ASSERT_TRUE(proof_edit("proof infeasible", "proof infeasible\nobjective 0"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_check_proof(c, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(proof_edit("proof infeasible\nobjective 0",
                                "proof infeasible"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(c, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(pr.certified);
    jaos_model_free(c);
    remove(TMP_PROOF);
}

static void test_an_unbounded_ray_is_checked_exactly(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double cost[1] = { -1.0 };
    const double cl[1] = { 0.0 }, cu[1] = { INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { INFINITY };
    const int64_t start[2] = { 0, 1 }, index[1] = { 0 };
    const double value[1] = { 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, start, index, value));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    if (jaos_status_of(m) != JAOS_SOLVE_UNBOUNDED) {

        jaos_model_free(m);
        TEST_IGNORE_MESSAGE("this build did not report UNBOUNDED here");
        return;
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, TMP_PROOF));
    jaos_model_free(m);

    jaos_model *a = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, start, index, value));
    jaos_proof_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(a, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_UNBOUNDED, pr.kind);
    TEST_ASSERT_TRUE(pr.certified);

    TEST_ASSERT_TRUE(proof_edit("ray C1 1", "ray C1 -1"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(a, TMP_PROOF, &pr));
    TEST_ASSERT_FALSE(pr.certified);
    TEST_ASSERT_EQUAL_INT64(0, pr.bad_col);
    jaos_model_free(a);
    remove(TMP_PROOF);
}

static jaos_model *model_two_row_conflict(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {INFINITY, INFINITY};
    const double rl[2] = {-INFINITY, 2.0}, ru[2] = {1.0, INFINITY};
    const int64_t s[3] = {0, 2, 4};
    const int64_t ix[4] = {0, 1, 0, 1};
    const double v[4] = {1.0, 1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, s, ix, v));
    return m;
}

static void test_the_exact_certificate_is_derived_and_certifies(void)
{
    jaos_model *m = model_two_row_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));

    jaos_exact_ray_report rr;
    memset(&rr, 0, sizeof rr);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_certificate(m, &rr));
    TEST_ASSERT_TRUE(rr.derived);
    TEST_ASSERT_TRUE(rr.bound_bits <= rr.capacity_bits);

    const char *y0 = nullptr, *y1 = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_row_multiplier(m, 0, &y0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_row_multiplier(m, 1, &y1));
    TEST_ASSERT_EQUAL_STRING("-1", y0);
    TEST_ASSERT_EQUAL_STRING("1", y1);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, TMP_PROOF));
    jaos_model_free(m);

    jaos_model *a = model_two_row_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    jaos_proof_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(a, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_INFEASIBLE, pr.kind);
    TEST_ASSERT_TRUE(pr.certified);
    jaos_model_free(a);

    jaos_model *b = model_two_row_conflict();
    TEST_ASSERT_TRUE(proof_edit("ray R1 -1", "ray R1 -2"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(b, TMP_PROOF, &pr));
    TEST_ASSERT_FALSE(pr.certified);
    TEST_ASSERT_TRUE(proof_edit("ray R1 -2", "ray R1 -1"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(b, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(pr.certified);
    jaos_model_free(b);
    remove(TMP_PROOF);
}

static void test_the_exact_certificate_refuses_what_has_no_basis(void)
{
    jaos_exact_ray_report rr;

    jaos_model *m = model_two_row_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_exact_certificate(m, &rr));
    TEST_ASSERT_FALSE(rr.derived);
    jaos_model_free(m);

    jaos_model *o = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&o));
    {
        const double c[1] = {1.0}, cl[1] = {1.0}, cu[1] = {5.0};
        const double rl[1] = {1.0}, ru[1] = {INFINITY};
        const int64_t s[2] = {0, 1};
        const int64_t ix[1] = {0};
        const double v[1] = {1.0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(o, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         1, s, ix, v));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(o));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(o));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_exact_certificate(o, &rr));
    jaos_model_free(o);

    jaos_model *p = model_infeasible();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(p));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(p));
    const jaos_status got = jaos_exact_certificate(p, &rr);
#if defined(JAOS_NO_PRESOLVE)
    TEST_ASSERT_EQUAL_INT(JAOS_OK, got);
    TEST_ASSERT_TRUE(rr.derived);
#else
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, got);
    TEST_ASSERT_FALSE(rr.derived);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(p, TMP_PROOF));
    jaos_proof_report pr;
    jaos_model *q = model_infeasible();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(q, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_INFEASIBLE, pr.kind);
    jaos_model_free(q);
    remove(TMP_PROOF);
#endif
    jaos_model_free(p);
}

static void test_the_exact_certificate_is_dropped_with_the_answer(void)
{
    jaos_model *m = model_two_row_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_exact_ray_report rr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_certificate(m, &rr));
    TEST_ASSERT_TRUE(rr.derived);
    const char *y = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_row_multiplier(m, 0, &y));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 0, -INFINITY, 3.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_exact_row_multiplier(m, 0, &y));
    jaos_model_free(m);
}

static void test_the_exact_unbounded_ray_is_derived_and_checks(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    const double c[2] = {-1.0, 0.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {INFINITY, INFINITY};
    const double rl[1] = {-INFINITY}, ru[1] = {1.0};
    const int64_t s[3] = {0, 1, 2};
    const int64_t ix[2] = {0, 0};
    const double v[2] = {1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    jaos_exact_ray_report rr;
    memset(&rr, 0, sizeof rr);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_unbounded_ray(m, &rr));
    TEST_ASSERT_TRUE(rr.derived);

    TEST_ASSERT_EQUAL_INT64(-1, rr.at_row);

    const char *dx = nullptr, *dy = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_direction(m, 0, &dx));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_direction(m, 1, &dy));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, TMP_PROOF));
    jaos_proof_report pr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(m, TMP_PROOF, &pr));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_FILE_UNBOUNDED, pr.kind);

#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)

    TEST_ASSERT_FALSE_MESSAGE(pr.certified,
                              "a direction derived from a basis lifted onto "
                              "the wrong row must not certify");
    remove(TMP_PROOF);
    jaos_model_free(m);
    return;
#else

    TEST_ASSERT_EQUAL_STRING("1", dx);
    TEST_ASSERT_EQUAL_STRING("1", dy);
    TEST_ASSERT_TRUE(pr.certified);
#endif

    TEST_ASSERT_TRUE(proof_edit("ray C2 1", "ray C2 0"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(m, TMP_PROOF, &pr));
    TEST_ASSERT_FALSE(pr.certified);
    TEST_ASSERT_TRUE(proof_edit("ray C2 0", "ray C2 1"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(m, TMP_PROOF, &pr));
    TEST_ASSERT_TRUE(pr.certified);

    remove(TMP_PROOF);
    jaos_model_free(m);
}

static void test_each_exact_derivation_refuses_the_other_s_answer(void)
{
    jaos_exact_ray_report rr;
    jaos_model *m = model_two_row_conflict();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_exact_unbounded_ray(m, &rr));
    jaos_model_free(m);
}

static void test_verify_basis_agrees_with_verify_on_the_same_basis(void)
{
    jaos_model *m = model_two();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_basis_status cs[2], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    jaos_verify_report a, b;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify_basis(m, cs, rs, &b));
    TEST_ASSERT_EQUAL_INT(a.status, b.status);
    TEST_ASSERT_EQUAL_INT(a.stage, b.stage);
    TEST_ASSERT_EQUAL_INT64(a.blocks, b.blocks);
    TEST_ASSERT_EQUAL_INT64(a.largest_block, b.largest_block);
    TEST_ASSERT_EQUAL_INT64(a.terms, b.terms);
    TEST_ASSERT_EQUAL_DOUBLE(a.bound_bits, b.bound_bits);
    jaos_model_free(m);
}

static void test_verify_basis_proves_a_basis_with_no_solve(void)
{
    jaos_model *m = model_two();

    const jaos_basis_status cs[2] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_BASIC};
    const jaos_basis_status rs[1] = {JAOS_BASIS_AT_UPPER};

    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify_basis(m, cs, rs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);

    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));

    const char *v = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_value(m, 0, &v));
    TEST_ASSERT_EQUAL_STRING("3", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_col_value(m, 1, &v));
    TEST_ASSERT_EQUAL_STRING("1", v);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_objective(m, &v));
    TEST_ASSERT_EQUAL_STRING("-4", v);
    jaos_model_free(m);
}

static void test_verify_basis_rejects_a_basis_that_is_not_optimal(void)
{
    jaos_model *m = model_two();

    const jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER,
                                     JAOS_BASIS_AT_LOWER};
    const jaos_basis_status rs[1] = {JAOS_BASIS_BASIC};

    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify_basis(m, cs, rs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_BROKEN, rep.status);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_STAGE_DUAL, rep.stage);

    const char *v = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_exact_col_value(m, 0, &v));
    jaos_model_free(m);
}

static void test_verify_basis_rejects_an_infeasible_basis(void)
{
    jaos_model *m = model_two();

    const jaos_basis_status cs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_AT_LOWER};
    const jaos_basis_status rs[1] = {JAOS_BASIS_AT_UPPER};

    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify_basis(m, cs, rs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_BROKEN, rep.status);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_STAGE_PRIMAL, rep.stage);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, rep.violation);
    jaos_model_free(m);
}

static void test_verify_basis_refuses_a_basis_that_is_not_one(void)
{
    jaos_model *m = model_two();
    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);

    const jaos_basis_status few_c[2] = {JAOS_BASIS_AT_LOWER,
                                        JAOS_BASIS_AT_LOWER};
    const jaos_basis_status few_r[1] = {JAOS_BASIS_AT_UPPER};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(m, few_c, few_r, &rep));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "basic variables"));

    const jaos_basis_status many_c[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    const jaos_basis_status many_r[1] = {JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(m, many_c, many_r, &rep));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "basic variables"));

    const jaos_basis_status junk_c[2] = {(jaos_basis_status)9,
                                         JAOS_BASIS_BASIC};
    const jaos_basis_status junk_r[1] = {JAOS_BASIS_AT_UPPER};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(m, junk_c, junk_r, &rep));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no such basis status"));

    const jaos_basis_status ok_c[2] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_BASIC};
    const jaos_basis_status ok_r[1] = {JAOS_BASIS_AT_UPPER};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(nullptr, ok_c, ok_r, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(m, nullptr, ok_r, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(m, ok_c, nullptr, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_verify_basis(m, ok_c, ok_r, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify_basis(m, ok_c, ok_r, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
    jaos_model_free(m);
}

static void test_verify_basis_leaves_the_model_alone(void)
{
    jaos_model *m = model_two();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_basis_status was_c[2], was_r[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, was_c, was_r));
    double was_obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &was_obj));

    const jaos_basis_status other_c[2] = {JAOS_BASIS_AT_LOWER,
                                          JAOS_BASIS_AT_LOWER};
    const jaos_basis_status other_r[1] = {JAOS_BASIS_BASIC};
    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_verify_basis(m, other_c, other_r, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_BROKEN, rep.status);

    jaos_basis_status now_c[2], now_r[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, now_c, now_r));
    TEST_ASSERT_EQUAL_MEMORY(was_c, now_c, sizeof was_c);
    TEST_ASSERT_EQUAL_MEMORY(was_r, now_r, sizeof was_r);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double now_obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &now_obj));
    TEST_ASSERT_EQUAL_DOUBLE(was_obj, now_obj);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);
    jaos_model_free(m);
}

static void test_a_basis_from_outside_gets_a_proof_file(void)
{
    jaos_model *m = model_two();
    const jaos_basis_status cs[2] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_BASIC};
    const jaos_basis_status rs[1] = {JAOS_BASIS_AT_UPPER};
    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify_basis(m, cs, rs, &rep));
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, rep.status);

    const char *path = "build/tv_outside.proof";
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_proof(m, path));
    jaos_proof_report pr;
    memset(&pr, 0, sizeof pr);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_proof(m, path, &pr));
    TEST_ASSERT_TRUE(pr.certified);
    TEST_ASSERT_TRUE(pr.primal);
    TEST_ASSERT_TRUE(pr.dual);
    TEST_ASSERT_TRUE(pr.objective);
    remove(path);
    jaos_model_free(m);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_exact_unbounded_ray_is_derived_and_checks);
    RUN_TEST(test_each_exact_derivation_refuses_the_other_s_answer);
    RUN_TEST(test_proves_a_small_optimum);
    RUN_TEST(test_proves_a_model_whose_rows_need_scaling);
    RUN_TEST(test_proves_a_model_whose_dual_carries_the_scale);
    RUN_TEST(test_proves_a_maximize_model);
    RUN_TEST(test_proves_a_model_with_a_real_block);
    RUN_TEST(test_rejects_a_basic_value_outside_its_bound);
    RUN_TEST(test_rejects_a_reduced_cost_of_the_wrong_sign);
    RUN_TEST(test_rejects_a_basis_of_the_wrong_count);
    RUN_TEST(test_rejects_a_structurally_singular_basis);
    RUN_TEST(test_rejects_a_basis_that_is_singular_with_a_transversal);
    RUN_TEST(test_names_the_rank_stage_when_there_is_no_transversal);
    RUN_TEST(test_refuses_a_basis_wider_than_the_limbs);
    RUN_TEST(test_refuses_a_nonbasic_variable_on_an_infinite_bound);
    RUN_TEST(test_reads_the_bound_before_it_allocates);
    RUN_TEST(test_a_unit_basis_costs_no_bits);
    RUN_TEST(test_is_reproducible);
    RUN_TEST(test_refuses_a_model_that_was_not_solved);
    RUN_TEST(test_refuses_a_quadratic_objective_by_name);
    RUN_TEST(test_a_proved_basis_gives_its_values_exactly);
    RUN_TEST(test_a_proof_file_round_trips_and_is_checked_exactly);
    RUN_TEST(test_the_proof_checker_rejects_what_is_not_optimal);
    RUN_TEST(test_a_written_proof_is_one_its_own_checker_takes);
    RUN_TEST(test_an_infeasibility_certificate_is_checked_exactly);
    RUN_TEST(test_an_unbounded_ray_is_checked_exactly);
    RUN_TEST(test_the_exact_certificate_is_derived_and_certifies);
    RUN_TEST(test_the_exact_certificate_refuses_what_has_no_basis);
    RUN_TEST(test_the_exact_certificate_is_dropped_with_the_answer);
    RUN_TEST(test_exact_values_carry_the_model_s_own_sign);
    RUN_TEST(test_a_nonbasic_column_reads_its_bound_and_the_two_by_two_its_solve);
    RUN_TEST(test_verify_basis_agrees_with_verify_on_the_same_basis);
    RUN_TEST(test_verify_basis_proves_a_basis_with_no_solve);
    RUN_TEST(test_verify_basis_rejects_a_basis_that_is_not_optimal);
    RUN_TEST(test_verify_basis_rejects_an_infeasible_basis);
    RUN_TEST(test_verify_basis_refuses_a_basis_that_is_not_one);
    RUN_TEST(test_verify_basis_leaves_the_model_alone);
    RUN_TEST(test_a_basis_from_outside_gets_a_proof_file);
    return UNITY_END();
}
