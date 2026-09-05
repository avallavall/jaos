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

/* OBJNAME says which free row is the objective, and t3 has two of them so
 * the answer is not the first (D280). Read with the first-N-row rule the
 * same text is a DIFFERENT model: `IGNORED` would be the objective at
 * 10x + 20y and `PROFIT` would be a free row. Both are legal models, which
 * is what makes this worth pinning field by field rather than by a status.
 *
 * The `IGNORED` row survives as an ordinary free row with both bounds
 * infinite, which is what the standard says an extra N row is. Dropping it
 * would renumber every row after it. */
static void check_t3(jaos_model *m)
{
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    /* Two rows: IGNORED (free) and BALANCE (E). PROFIT is the objective and
     * is not a matrix row, exactly as the first N row is not in t1. */
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_row(m));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));

    /* Row 0 is IGNORED, and it is free rather than gone. */
    TEST_ASSERT_TRUE(isinf(m->row_lower[0]) && m->row_lower[0] < 0.0);
    TEST_ASSERT_TRUE(isinf(m->row_upper[0]) && m->row_upper[0] > 0.0);

    /* Row 1 is BALANCE, an equality at 4. */
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_lower[1]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, m->row_upper[1]);

    /* The costs come from PROFIT and not from IGNORED. This is the
     * assertion the whole feature is: against the first-N-row rule they
     * would be 10 and 20. */
    TEST_ASSERT_EQUAL_DOUBLE(3.0, m->col_cost[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_cost[1]);

    /* Both columns still carry their IGNORED entry, in the row it became. */
    TEST_ASSERT_EQUAL_INT64(4, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_MINIMIZE, m->sense);
}

static void test_t3_objname_picks_the_second_free_row(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t3_objname.mps"));
    check_t3(m);
    jaos_model_free(m);
}

/* The name on the line after the header rather than on it. OBJSENSE has the
 * same two spellings and t2 covers its second one; this covers this one. */
static void test_objname_on_the_next_line_is_the_same_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps(m, "tests/data/t3_objname_nextline.mps"));
    check_t3(m);
    jaos_model_free(m);
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

    /* RHS on the COST row is the negated objective constant, which is
     * CPLEX's documented convention and what docs/format-support.md records.
     *
     * This assertion is load-bearing against a specific mistake. The Netlib
     * reference optima do not include this constant — neither the readme's
     * nor Koch's — so on e226, the one instance of the standard set where
     * the two conventions are distinguishable, a correct answer looks wrong
     * against both published values by exactly the constant. The tempting
     * repair is to make the reader drop it. That would trade every model
     * whose author meant the constant for agreement with two reference sets
     * that predate the convention, and this test is here to make that
     * trade fail loudly. The gate handles it in bench/netlib.manifest
     * instead. */
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

    /* The file's names are the model's (D284), and resolve back. */
    char nm[JAOS_NAME_MAX + 1];
    const char *want_col[] = {"X1", "X2", "X3"};
    const char *want_row[] = {"LIM1", "LIM2", "EQ1"};
    for (int j = 0; j < 3; j++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(m, j, nm, sizeof nm));
        TEST_ASSERT_EQUAL_STRING(want_col[j], nm);
        int64_t k = -1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_index(m, want_col[j], &k));
        TEST_ASSERT_EQUAL_INT64(j, k);
    }
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, i, nm, sizeof nm));
        TEST_ASSERT_EQUAL_STRING(want_row[i], nm);
        int64_t k = -1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_index(m, want_row[i], &k));
        TEST_ASSERT_EQUAL_INT64(i, k);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("COST", nm);
    /* And the NAME line's word is the model's name. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(m, nm, sizeof nm));
    TEST_ASSERT_EQUAL_STRING("T1", nm);
    /* The objective is not a row, so its name is not a row's. */
    int64_t k = -1;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_row_index(m, "COST", &k));
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

    /* OBJNAME's own four, each with its own message. The last one is the
     * one worth reading: a G row named by OBJNAME never matches, because
     * the objective has to be a free row, so it reports the same "no free
     * row by that name" as a name that matches nothing at all. */
    expect_reject("tests/data/e_objname_missing.mps", "no free row by that name");
    expect_reject("tests/data/e_objname_late.mps", "OBJNAME after ROWS");
    expect_reject("tests/data/e_objname_twice.mps", "a second OBJNAME");
    expect_reject("tests/data/e_objname_notfree.mps", "no free row by that name");
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
    RUN_TEST(test_t3_objname_picks_the_second_free_row);
    RUN_TEST(test_objname_on_the_next_line_is_the_same_model);
    RUN_TEST(test_t2_free_layout_objsense_ranges_wart);
    RUN_TEST(test_rejections_carry_line_numbers);
    RUN_TEST(test_rejection_reasons_are_specific);
    RUN_TEST(test_missing_file_is_io_error);
    RUN_TEST(test_failed_read_preserves_previous_model);
    return UNITY_END();
}
