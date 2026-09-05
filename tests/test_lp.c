/* LP-format reader tests: two golden instances verified field by field,
 * plus one rejection per failure class.
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

static void test_g1_labels_relations_bounds(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    /* Columns in order of appearance: x, y, z. Rows: c1, c2, c3. */
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);

    /* "+ 5" in the objective is a direct constant. */
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->col_cost[1]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_cost[2]);

    /* c1 (-inf,10]; c2 [-3,inf); c3 [7,7]. */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-3.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    /* x default [0,inf); y [-1,8]; z free. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, m->col_upper[1]);
    TEST_ASSERT_TRUE(isinf(m->col_lower[2]) && m->col_lower[2] < 0);
    TEST_ASSERT_TRUE(isinf(m->col_upper[2]));

    /* CSC: x hits c1,c2,c3 with 1,2,1; y hits c1,c2,c3 with 1,-1,1;
     * z hits c3 with 1. */
    const int64_t want_start[] = {0, 3, 6, 7};
    const int64_t want_index[] = {0, 1, 2, 0, 1, 2, 2};
    const double  want_value[] = {1.0, 2.0, 1.0, 1.0, -1.0, 1.0, 1.0};
    for (int j = 0; j <= 3; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 7; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
    jaos_model_free(m);
}

static void test_g2_maximize_exponents_summing_wrapping(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g2.lp"));

    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, m->sense);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->obj_offset);

    /* 2.5e1 x1 + x1 sums to 26; glued 3x2 reads as 3 * x2. */
    TEST_ASSERT_EQUAL_DOUBLE(26.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[1]);

    /* Wrapped constraint with =< gives (-inf,4]; second row [-1,inf). */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]));

    /* x1 default bounds; x2 [0, 1.5]. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[0]);
    TEST_ASSERT_TRUE(isinf(m->col_upper[0]));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->col_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, m->col_upper[1]);

    /* CSC: x1 in rows 0,1 (1,1); x2 in rows 0,1 (1,-1). */
    const int64_t want_start[] = {0, 2, 4};
    const int64_t want_index[] = {0, 1, 0, 1};
    const double  want_value[] = {1.0, 1.0, 1.0, -1.0};
    for (int j = 0; j <= 2; j++)
        TEST_ASSERT_EQUAL_INT64(want_start[j], m->a_start[j]);
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_EQUAL_INT64(want_index[k], m->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(want_value[k], m->a_value[k]);
    }
    jaos_model_free(m);
}

/* A two-sided constraint is one row with two finite ends, not two rows.
 * Both directions name the same interval: "1 <= e <= 5" and "5 >= e >= 1".
 * A leading number is only the left bound when an operator follows it, so
 * the third row is the case that would break a naive lookahead — its 3 is a
 * coefficient. */
static void test_a_ranged_constraint_is_one_row_with_two_ends(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_lp(m, "tests/data/g_ranged.lp"));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_row(m));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->row_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[1]);

    /* "3 x + y >= 2": the 3 is a coefficient and the row is one-sided. */
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->row_lower[2]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[2]) && m->row_upper[2] > 0.0);
    jaos_model_free(m);
}

/* A constant inside a constraint expression moves to the other side, with
 * its sign flipped (D278). Four shapes, because each reaches the fold by a
 * different route through the parser:
 *
 *   c1  after the terms, one-sided        x + 5 <= 10   ->  x <= 5
 *   c2  BEFORE them, and negative        -3 + 2y >= 7   ->  2y >= 10
 *   c3  inside a range           3 <= x + y + 1 <= 8    ->  2 <= x+y <= 7
 *   c4  two of them, on an equality    x + 2 + 3 = 10   ->  x = 5
 *
 * c2 is the one worth reading. A signed number at the head of a constraint
 * is a left-hand bound only when a relation follows it; here a `+` follows,
 * so the parser pushes it back with the sign folded in and it arrives as an
 * ordinary constant term. c3 is the other: the constant sits between the
 * two ends of the range, so BOTH ends shift by it, and shifting only one
 * would silently widen or narrow the row.
 *
 * The objective's constants are unchanged and still land in the offset;
 * asserting that here is what keeps the two paths apart. */
static void test_a_constant_in_a_constraint_folds_into_the_rhs(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_lp(m, "tests/data/g_const.lp"));

    TEST_ASSERT_EQUAL_INT64(4, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));

    /* c1: x + 5 <= 10 */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[0]);

    /* c2: -3 + 2 y >= 7 */
    TEST_ASSERT_EQUAL_DOUBLE(10.0, m->row_lower[1]);
    TEST_ASSERT_TRUE(isinf(m->row_upper[1]) && m->row_upper[1] > 0.0);

    /* c3: 3 <= x + y + 1 <= 8 -- both ends move by the same 1 */
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->row_lower[2]);
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->row_upper[2]);

    /* c4: x + 2 + 3 = 10 */
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_lower[3]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->row_upper[3]);

    /* The objective's constant is still the offset and not a row. */
    TEST_ASSERT_EQUAL_DOUBLE(7.0, m->obj_offset);
    jaos_model_free(m);
}

static void expect_reject(const char *path, const char *needle)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_lp(m, path));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), needle));
    jaos_model_free(m);
}

static void test_rejection_reasons_are_specific(void)
{
    expect_reject("tests/data/el_int.lp", "integer");
    expect_reject("tests/data/el_rangedir.lp", "same way");
    expect_reject("tests/data/el_unkbound.lp", "unknown variable");
    expect_reject("tests/data/el_badchar.lp", "unexpected character");
    expect_reject("tests/data/el_noend.lp", "End");
}

static void test_rejections_carry_line_numbers(void)
{
    expect_reject("tests/data/el_int.lp", "line 5");
    expect_reject("tests/data/el_rangedir.lp", "line 4");
    expect_reject("tests/data/el_unkbound.lp", "line 6");
    expect_reject("tests/data/el_badchar.lp", "line 4");
}

static void test_missing_file_is_io_error(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_read_lp(m, "tests/data/does_not_exist.lp"));
    jaos_model_free(m);
}

static void test_failed_read_preserves_previous_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_lp(m, "tests/data/el_rangedir.lp"));

    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(7, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, m->obj_offset);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_g1_labels_relations_bounds);
    RUN_TEST(test_g2_maximize_exponents_summing_wrapping);
    RUN_TEST(test_a_ranged_constraint_is_one_row_with_two_ends);
    RUN_TEST(test_a_constant_in_a_constraint_folds_into_the_rhs);
    RUN_TEST(test_rejection_reasons_are_specific);
    RUN_TEST(test_rejections_carry_line_numbers);
    RUN_TEST(test_missing_file_is_io_error);
    RUN_TEST(test_failed_read_preserves_previous_model);
    return UNITY_END();
}
