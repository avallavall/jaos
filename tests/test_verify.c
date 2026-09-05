/* Exact verification of a final basis (src/verify.c).
 *
 * The subject has no tolerance in it, so a green result here means nothing
 * unless the same predicate is shown rejecting. `jaos-testing`'s rule is the
 * shape of this file: for every case the verifier must call OPTIMAL there is
 * one it must call BROKEN, built by corrupting exactly one thing.
 *
 * The corruption is done by writing the published basis directly. That is
 * what reaching `jaos_internal.h` is for: no public call can install a wrong
 * basis, because the solve owns the published one, and without a wrong basis
 * the BROKEN path is never exercised at all.
 *
 * The oracle for the OPTIMAL cases is arithmetic done by hand, not the
 * solver. A verifier judged by the thing it verifies proves nothing.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "unity.h"

#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ----------------------------------------------------------- the models */

/* min -x - y  s.t.  x + y <= 4,  x <= 3,  0 <= x, y.
 * The optimum is x = 3, y = 1, objective -4, and it is unique. */
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

/* A model whose data is not exactly representable, so every row needs the
 * power-of-two scale before the elimination can touch it.
 *
 *   min -x - y   s.t.   0.1 x + 0.3 y <= 1.2,  0.7 x <= 2.1,  x, y >= 0
 *
 * Both rows carry 53-bit mantissas with exponents near -55, which is the
 * shape D273 measured: the scale is what the bound has to pay for.
 *
 * `maybe_unused` because its one caller is compiled out under the off-by-one
 * fault build, and `-Werror` would otherwise refuse that configuration. */
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

/* A model whose verdict depends on the dual being scaled the right way.
 *
 *   min -2x - z   s.t.   0.1 x + 0.3 z <= 1,   0 <= x, z <= 100
 *
 * Per unit of the row x is worth 2/0.1 = 20 and z only 1/0.3 = 3.33, so the
 * optimum is x = 10, z = 0, with x basic and z resting on its lower bound.
 * The basis is [0.1] and the dual solves 0.1 y = -2, so y = -20, and z's
 * reduced cost is -1 - 0.3*(-20) = 5, which is the >= 0 a lower bound needs.
 *
 * **The whole point of this model is that it goes red when the dual scale is
 * inverted.** The row scales by 2^55 to become integral; taking that factor
 * the wrong way leaves y at 2^-110 of its value, which is zero for every
 * practical purpose, and then z's reduced cost reads -1 and the answer is
 * rejected. A model whose duals are all zero cannot show this: zero passes
 * a "<= 0" test and a ">= 0" test alike, which is exactly why the first
 * decimal model here proved while 19 of the 94 netlib bases were being
 * rejected for it (D274). */
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

/* A maximize model, to prove the sense is applied where the reduced costs
 * are judged. max 2x + 3y s.t. x + y <= 10, 0 <= x <= 4, y >= 0. y is worth
 * more than x per unit of the row, so the optimum is x = 0, y = 10 and the
 * objective is 30. */
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

/* A model with a dense 3 by 3 block, so the elimination runs rather than
 * the single-entry fast path. The three rows tie every variable to every
 * other, which is what makes the strongly connected component one block of
 * three instead of three blocks of one.
 *
 *   min x + y + z
 *   s.t.  2x +  y +  z = 7
 *          x + 3y +  z = 10
 *          x +  y + 4z = 13
 *   with x, y, z in [0, 100], so all three slacks are nonbasic at equality
 *   and the basis is the 3 by 3 itself. The system has the unique solution
 *   x = 1, y = 2, z = 3: 2+2+3 = 7, 1+6+3 = 10, 1+2+12 = 15. */
static jaos_model *model_block3(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));

    const double cost[3] = { 1.0, 1.0, 1.0 };
    const double cl[3]   = { 0.0, 0.0, 0.0 };
    const double cu[3]   = { 100.0, 100.0, 100.0 };
    const double rl[3]   = { 7.0, 10.0, 15.0 };
    const double ru[3]   = { 7.0, 10.0, 15.0 };
    /* column-wise: x in rows 0,1,2; y in 0,1,2; z in 0,1,2 */
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

/* ------------------------------------------------------------ the tests */

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
    /* The hand-computed optimum, so the test is not asking the solver. */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -4.0, obj);
    jaos_model_free(m);
}

static void test_proves_a_model_whose_rows_need_scaling(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    /* Positive test. The off-by-one build shuffles every postsolve restore
     * index, so the basis this model publishes is not a basis of it, and
     * asking for a proof of it is a claim about a correct replay.
     *
     * **The verifier notices.** It returns JAOS_ERR_NUMERICAL here, because
     * the shuffled statuses no longer name num_row basics and `vbasis_build`
     * checks that rather than assuming it. That is the fault build being
     * caught, which is what it is for.
     *
     * The wrong-dual build rewrites a singleton row's dual on an OPTIMAL
     * replay. Nothing here reads `sol_dual` -- the duals are recomputed from
     * the basis -- so these run under it. */
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
    /* Every value is integral and the elimination formed real products. */
    TEST_ASSERT_TRUE_MESSAGE(r.terms > 0, "no product was formed");
    jaos_model_free(m);
}

/* --- the cases it must reject ---------------------------------------- */

/* A basic value outside its own bound. The published basis is kept and the
 * bound is moved under it by hand, which is the smallest single change that
 * makes the answer wrong without touching the basis itself. */
static void test_rejects_a_basic_value_outside_its_bound(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    /* y is basic at 1. Forbid anything above 0.5 and keep the basis. */
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

/* A reduced cost pointing out of the model. The basis is kept and one cost
 * is moved, which leaves every basic value right and only the dual wrong.
 * A column resting on its lower bound needs a reduced cost at or above zero,
 * so a large negative cost breaks it; on its upper bound, the reverse. */
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

/* A basis that is not one. Two columns marked basic where the model has one
 * row leaves the matrix rectangular, and the call must say so rather than
 * factor it. */
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

/* A structurally singular basis. The count is right and the rank is not:
 * the one basic column has no matrix entry at all, so no transversal exists
 * and there is nothing to pivot on. The call must say BROKEN rather than
 * divide by nothing.
 *
 *   min -x  s.t.  x <= 4,  0 <= x <= 10,  0 <= z <= 5
 *
 * z appears in no row. Marking z basic and both the slack and x nonbasic
 * keeps exactly one basic for one row and leaves the basis empty. */
static void test_rejects_a_structurally_singular_basis(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE)
    /* The rejection is the point, but it rests on the model verifying first:
     * without that precondition the test cannot tell its own corruption from
     * the build's. Under the off-by-one replay the precondition is false. */
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
    const int64_t start[3] = { 0, 1, 1 };   /* column 1 is empty */
    const int64_t index[1] = { 0 };
    const double value[1]  = { 1.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, start, index, value));

    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "the model itself does not verify before it is corrupted");

    m->sol_col_status[0] = JAOS_BASIS_AT_UPPER;   /* x off the basis   */
    m->sol_col_status[1] = JAOS_BASIS_BASIC;      /* the empty column in */
    m->sol_row_status[0] = JAOS_BASIS_AT_UPPER;   /* the slack off      */

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_BROKEN, r.status,
        "a basis with no transversal is not rejected");
    jaos_model_free(m);
#endif
}

/* A basis that has a transversal and no rank. Two proportional columns pass
 * the matching -- every diagonal entry is still nonzero -- and cancel in the
 * elimination. The verdict must be BROKEN at the RANK stage: that is a proof
 * the basis is not one, and it is a different answer from running out of
 * limbs, which is what REFUSED means. */
static void test_rejects_a_basis_that_is_singular_with_a_transversal(void)
{
    jaos_model *m = model_block3();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_OPTIMAL, r.status,
        "the model does not verify before it is corrupted");

    /* Column 1 becomes twice column 0, in place. Both are still basic and
     * both still have an entry in every row. */
    for (int64_t k = 0; k < 3; k++)
        m->a_value[3 + k] = 2.0 * m->a_value[k];

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_verify(m, &r));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_BROKEN, r.status,
        "a singular basis is not rejected");
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_STAGE_RANK, r.stage,
        "a singular basis is not reported at the rank stage");
    jaos_model_free(m);
}

/* And the rank stage is set by the no-transversal path too. Without this the
 * `stage` field has no test that reads it on the empty-column case. */
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

/* REFUSED, the third verdict, reached for the reason the header gives: the
 * bound exceeds the limbs. A hundred rows of `0.1` need a 53-bit scale each
 * and the basis wants about 5300 bits where there are 4096, so the refusal
 * is a priori and nothing is allocated. Without this test one of the three
 * verdicts ships with no case that produces it. */
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
        value[j] = 0.1;          /* a 53-bit odd mantissa at 2^-55 */
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

/* A nonbasic variable resting on a bound the model does not have. `jaos.h`
 * does not promise that AT_LOWER and AT_UPPER name a finite bound -- the
 * simplex publishes a column on a bound it lent itself that way, four of
 * `finnis`'s among them -- so the basis names no point and the call must
 * refuse rather than return an error the header never describes. */
static void test_refuses_a_nonbasic_variable_on_an_infinite_bound(void)
{
    jaos_model *m = model_two();
    jaos_verify_report r = verify_of(m);
    TEST_ASSERT_EQUAL_INT(JAOS_PROOF_OPTIMAL, r.status);

    /* Column 1's upper bound is infinity. Publish it as resting there. */
    TEST_ASSERT_TRUE(!isfinite(m->col_upper[1]));
    m->sol_col_status[1] = JAOS_BASIS_AT_UPPER;
    /* Keep the count right by making the slack basic in its place. */
    m->sol_col_status[0] = JAOS_BASIS_AT_UPPER;
    m->sol_row_status[0] = JAOS_BASIS_BASIC;

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_verify(m, &r),
        "an infinite bound returns an error rather than a verdict");
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_PROOF_REFUSED, r.status,
        "a nonbasic variable on an infinite bound is not refused");
    jaos_model_free(m);
}

/* --- the report itself ------------------------------------------------ */

/* The bound is read before anything is attempted, and it is a real number.
 * A basis of unit columns has determinant one and costs zero bits, which is
 * the right answer and not a missing one, so the model here is the 3 by 3:
 * its determinant is 2*(12-1) - 1*(4-1) + 1*(1-3) = 22 - 3 - 2 = 17, so the
 * bound must be at least log2(17) and no less than four bits. */
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

/* And a basis of unit columns really does cost nothing. */
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

/* ------------------------------------------------------- the exact values */

/* min x  s.t.  3x >= 1, x >= 0: the optimum is x = 1/3, the row's dual
 * 1/3, the objective 1/3, none of them a double. The oracle is arithmetic
 * by hand. */
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
    /* Nothing before a proof, and nothing after a solve alone. */
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
    /* 1/3 + 1/2 = 5/6, the constant summed exactly. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_exact_objective(m, &v));
    TEST_ASSERT_EQUAL_STRING("5/6", v);
    /* And the floating answer agrees to a rounding, which is what the
     * exact one is for: the double is 0.8333..., the rational is 5/6. */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0 / 6.0, obj);

    /* Out of range, and a null out. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 1, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_row_dual(m, -1, &v));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, nullptr));

    /* A modification drops them: the proof was about the old model. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, &v));
    jaos_model_free(m);
}

static void test_exact_values_carry_the_model_s_own_sign(void)
{
    /* max -x with the same row: the same point, x = 1/3, objective
     * -1/3 + 1/2 = 1/6, and the dual in the model's convention. For a
     * maximum the checker's signs flip, so the row at its lower bound
     * carries y <= 0: -1/3. */
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
    /* And the published double dual carries the same sign -- except under
     * the wrong-dual fault build, whose whole purpose is a published dual
     * of the wrong sign; the exact one above comes from the proof and is
     * right either way. */
#if !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, nullptr, nullptr, y, nullptr));
    TEST_ASSERT_TRUE(y[0] < 0.0);
#endif
    jaos_model_free(m);
}

static void test_a_nonbasic_column_reads_its_bound_and_the_two_by_two_its_solve(void)
{
    /* min -x - 2y  s.t.  x + y <= 4,  x <= 3: the optimum is unique, y = 4
     * basic, x = 0 nonbasic at its lower bound, objective -8, the row's
     * dual -2. model_two's optimum is a whole edge and the solver may stop
     * at either end of it, which is why this is not model_two. */
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
    /* A second solve drops them until the next proof. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_exact_col_value(m, 0, &v));
    jaos_model_free(m);
}


int main(void)
{
    UNITY_BEGIN();
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
    RUN_TEST(test_a_proved_basis_gives_its_values_exactly);
    RUN_TEST(test_exact_values_carry_the_model_s_own_sign);
    RUN_TEST(test_a_nonbasic_column_reads_its_bound_and_the_two_by_two_its_solve);
    return UNITY_END();
}
