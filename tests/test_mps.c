/* MPS reader tests: two golden instances verified field by field against
 * hand-computed models, plus one rejection per failure class. Paths are
 * relative to the repository root, where make runs.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h" /* white-box: the assembled model is inspected */
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

static void test_t1_fixed_layout_full_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    /* Columns X1 X2 X3; rows LIM1(L) LIM2(G) EQ1(E); COST is the objective. */
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);

    /* RHS on the COST row is the negated objective constant. */
    TEST_ASSERT_EQUAL_DOUBLE(-3.5, m->obj_offset);

    const double want_cost[] = {1.0, 2.0, -1.0}; /* -1.0D0 read as -1.0 */
    for (int j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_DOUBLE(want_cost[j], m->col_cost[j]);

    /* Column bounds: X1 [0,4] (UP), X2 [-1,inf) (LO), X3 [0,inf) default. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->col_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[2]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[2]));

    /* Row bounds: LIM1 (-inf,4]; LIM2 [1,3.5] (G with range 2.5); EQ1 [7,7]. */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(3.5, m->row_upper[1]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    /* Matrix, CSC, columns sorted by row index. */
    const int64_t want_start[] = {0, 2, 4, 6};
    const int64_t want_index[] = {0, 1, 0, 2, 1, 2};
    const double  want_value[] = {1.0, 1.0, 1.0, -1.0, 1.0, 1.0};
    for (int j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 6; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
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

    /* E row, rhs 4, range -1  ->  [3, 4]. */
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);

    /* x: UP -2 with no explicit lower -> the wart drops it to -inf. */
    TEST_ASSERT_TRUE(isinf(m->col_lower[0]) && m->col_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, m->col_upper[0]);
    /* y: MI then UP 5 -> (-inf, 5]. */
    TEST_ASSERT_TRUE(isinf(m->col_lower[1]) && m->col_lower[1] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->col_upper[1]);

    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->a_value[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->a_value[1]);
    jaos_model_free(m);
}

/* One rejection per failure class; the message must carry a line number. */
static void expect_reject(const char *path, const char *needle)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_mps(m, path));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), needle));
    jaos_model_free(m);
}

static void test_rejections_carry_line_numbers(void)
{
    expect_reject("tests/data/e_intorg.mps", "line 6");
    expect_reject("tests/data/e_badnum.mps", "line 6");
    expect_reject("tests/data/e_unknown_row.mps", "line 5");
    expect_reject("tests/data/e_recol.mps", "line 9");
    expect_reject("tests/data/e_dupcoef.mps", "line 6");
    expect_reject("tests/data/e_bv.mps", "line 8");
}

static void test_rejection_reasons_are_specific(void)
{
    expect_reject("tests/data/e_intorg.mps", "integer");
    expect_reject("tests/data/e_badnum.mps", "bad number");
    expect_reject("tests/data/e_unknown_row.mps", "unknown row");
    expect_reject("tests/data/e_recol.mps", "contiguous");
    expect_reject("tests/data/e_dupcoef.mps", "duplicate coefficient");
    expect_reject("tests/data/e_noendata.mps", "ENDATA");
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

    /* The t1 model must still be fully there. */
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(-3.5, m->obj_offset);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_t1_fixed_layout_full_model);
    RUN_TEST(test_t2_free_layout_objsense_ranges_wart);
    RUN_TEST(test_rejections_carry_line_numbers);
    RUN_TEST(test_rejection_reasons_are_specific);
    RUN_TEST(test_missing_file_is_io_error);
    RUN_TEST(test_failed_read_preserves_previous_model);
    return UNITY_END();
}
