/* Scaling tests. The property that matters is the scaled magnitude
 * rho_i * |a_ij| * gamma_j, not the individual factors: the normal-equation
 * system is singular along (r+k, c-k), and that freedom cancels in the
 * product.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *build(int64_t nc, int64_t nr, const int64_t *as,
                         const int64_t *ai, const double *av, int64_t nnz)
{
    double *cost = calloc((size_t)nc, sizeof(double));
    double *cl = calloc((size_t)nc, sizeof(double));
    double *cu = malloc((size_t)nc * sizeof(double));
    double *rl = calloc((size_t)nr, sizeof(double));
    double *ru = malloc((size_t)nr * sizeof(double));
    TEST_ASSERT_NOT_NULL(cost);
    TEST_ASSERT_NOT_NULL(cu);
    TEST_ASSERT_NOT_NULL(ru);
    for (int64_t j = 0; j < nc; j++)
        cu[j] = INFINITY;
    for (int64_t i = 0; i < nr; i++)
        ru[i] = INFINITY;

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, nc, nr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     nnz, as, ai, av));
    free(cost); free(cl); free(cu); free(rl); free(ru);
    return m;
}

/* Worst ratio between scaled magnitudes across all nonzeros. */
static double spread(const jaos_model *m)
{
    double lo = HUGE_VAL, hi = 0.0;
    for (int64_t j = 0; j < m->num_col; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            double v = jm_scaled_abs(m, j, k);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    return hi == 0.0 ? 1.0 : hi / lo;
}

static void assert_all_powers_of_two(const jaos_model *m)
{
    for (int64_t i = 0; i < m->num_row; i++) {
        double l = log2(m->row_scale[i]);
        TEST_ASSERT_TRUE(isfinite(m->row_scale[i]) && m->row_scale[i] > 0.0);
        TEST_ASSERT_EQUAL_DOUBLE(floor(l), l);
    }
    for (int64_t j = 0; j < m->num_col; j++) {
        double l = log2(m->col_scale[j]);
        TEST_ASSERT_TRUE(isfinite(m->col_scale[j]) && m->col_scale[j] > 0.0);
        TEST_ASSERT_EQUAL_DOUBLE(floor(l), l);
    }
}

/* A = diag(2^10, 2^-5) * ones(2,2) * diag(1, 2^-3): an exactly recoverable
 * scaling, so the residual of the least-squares system reaches zero and
 * every scaled magnitude must come out at exactly 1. */
static void test_curtis_reid_recovers_exact_power_of_two_scaling(void)
{
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double  av[] = {1024.0, 0.03125, 128.0, 0.00390625};
    jaos_model *m = build(2, 2, as, ai, av, 4);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    TEST_ASSERT_TRUE(m->scale_valid);
    assert_all_powers_of_two(m);

    for (int64_t j = 0; j < 2; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            TEST_ASSERT_EQUAL_DOUBLE(1.0, jm_scaled_abs(m, j, k));

    jaos_model_free(m);
}

static void test_scaling_leaves_the_matrix_untouched(void)
{
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double  av[] = {1024.0, 0.03125, 128.0, 0.00390625};
    jaos_model *m = build(2, 2, as, ai, av, 4);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    /* The stored matrix is the authority the checker judges against. */
    TEST_ASSERT_EQUAL_DOUBLE(1024.0, m->a_value[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.03125, m->a_value[1]);
    TEST_ASSERT_EQUAL_DOUBLE(128.0, m->a_value[2]);
    TEST_ASSERT_EQUAL_DOUBLE(0.00390625, m->a_value[3]);
    jaos_model_free(m);
}

static void test_already_scaled_matrix_is_left_alone(void)
{
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double  av[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = build(2, 2, as, ai, av, 4);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    for (int64_t i = 0; i < 2; i++)
        TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_scale[i]);
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_scale[j]);
    jaos_model_free(m);
}

/* Badly conditioned and not exactly recoverable: both modes must still cut
 * the spread hard. */
static void test_both_modes_reduce_a_nasty_spread(void)
{
    const int64_t as[] = {0, 3, 6, 9};
    const int64_t ai[] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
    const double  av[] = {
        1.0e6, 3.0e-7, 5.0e2,
        2.0e5, 7.0e-6, 1.0e3,
        4.0e6, 1.0e-7, 9.0e1,
    };

    jaos_model *m = build(3, 3, as, ai, av, 9);
    double before = spread(m);
    TEST_ASSERT_TRUE(before > 1.0e12);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    double cr = spread(m);
    assert_all_powers_of_two(m);
    TEST_ASSERT_TRUE(cr < before / 1.0e6);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_GEOMETRIC));
    double geo = spread(m);
    assert_all_powers_of_two(m);
    TEST_ASSERT_TRUE(geo < before / 1.0e6);

    jaos_model_free(m);
}

static void test_none_mode_is_identity(void)
{
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double  av[] = {1024.0, 0.03125, 128.0, 0.00390625};
    jaos_model *m = build(2, 2, as, ai, av, 4);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_NONE));
    TEST_ASSERT_TRUE(m->scale_valid);
    for (int64_t i = 0; i < 2; i++)
        TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_scale[i]);
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_scale[j]);
    jaos_model_free(m);
}

/* An empty column and an empty row must not produce NaN or a zero factor;
 * they carry no information, so they stay at 1. */
static void test_empty_row_and_column_stay_at_one(void)
{
    const int64_t as[] = {0, 1, 1};       /* column 1 is empty */
    const int64_t ai[] = {0};             /* row 1 is empty    */
    const double  av[] = {8.0};
    jaos_model *m = build(2, 2, as, ai, av, 1);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    assert_all_powers_of_two(m);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_scale[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_scale[1]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_GEOMETRIC));
    assert_all_powers_of_two(m);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_scale[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_scale[1]);
    jaos_model_free(m);
}

static void test_repeated_scaling_is_bit_identical(void)
{
    const int64_t as[] = {0, 3, 6, 9};
    const int64_t ai[] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
    const double  av[] = {
        1.0e6, 3.0e-7, 5.0e2,
        2.0e5, 7.0e-6, 1.0e3,
        4.0e6, 1.0e-7, 9.0e1,
    };
    jaos_model *m = build(3, 3, as, ai, av, 9);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    double r0[3], c0[3];
    for (int i = 0; i < 3; i++) {
        r0[i] = m->row_scale[i];
        c0[i] = m->col_scale[i];
    }
    /* Recomputing from the same input must land on the same bits (D8). */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_MEMORY(&r0[i], &m->row_scale[i], sizeof(double));
        TEST_ASSERT_EQUAL_MEMORY(&c0[i], &m->col_scale[i], sizeof(double));
    }
    jaos_model_free(m);
}

static void test_load_invalidates_scaling(void)
{
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double  av[] = {1024.0, 0.03125, 128.0, 0.00390625};
    jaos_model *m = build(2, 2, as, ai, av, 4);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_CURTIS_REID));
    TEST_ASSERT_TRUE(m->scale_valid);

    const double one[] = {1.0, 1.0};
    const double zero[] = {0.0, 0.0};
    const double inf2[] = {INFINITY, INFINITY};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, one, zero, inf2, zero, inf2,
                     4, as, ai, av));
    TEST_ASSERT_FALSE(m->scale_valid);
    /* jm_scaled_abs must fall back to the raw magnitude. */
    TEST_ASSERT_EQUAL_DOUBLE(1024.0, jm_scaled_abs(m, 0, 0));
    jaos_model_free(m);
}

static void test_scale_rejects_bad_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_model_scale(nullptr, JM_SCALE_CURTIS_REID));

    const int64_t as[] = {0, 1};
    const int64_t ai[] = {0};
    const double  av[] = {2.0};
    jaos_model *m = build(1, 1, as, ai, av, 1);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_model_scale(m, (jm_scale_mode)99));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_curtis_reid_recovers_exact_power_of_two_scaling);
    RUN_TEST(test_scaling_leaves_the_matrix_untouched);
    RUN_TEST(test_already_scaled_matrix_is_left_alone);
    RUN_TEST(test_both_modes_reduce_a_nasty_spread);
    RUN_TEST(test_none_mode_is_identity);
    RUN_TEST(test_empty_row_and_column_stay_at_one);
    RUN_TEST(test_repeated_scaling_is_bit_identical);
    RUN_TEST(test_load_invalidates_scaling);
    RUN_TEST(test_scale_rejects_bad_arguments);
    return UNITY_END();
}
