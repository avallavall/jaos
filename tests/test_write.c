/* Writer tests. The claim under test is one sentence: what JAOS writes,
 * JAOS reads back as the same model. So most of these read a model, write
 * it, read the file back and compare every field exactly — `==`, not a
 * tolerance, because "close" is not the claim.
 *
 * Two tests are controls rather than checks. `test_wart_control_shows_the
 * _round_trip_can_fail` writes by hand the file a naive writer would have
 * produced and asserts it reads back WRONG, which is what stops the
 * round-trip assertions above it from being vacuous. And the refusal tests
 * check that no file is left behind, because a writer that refuses after
 * opening the stream leaves a truncated file that looks like a model.
 *
 * Paths are relative to the repository root, where make runs.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h" /* white-box: the round trip compares the models */
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const char *TMP_MPS = "build/tw_tmp.mps";
static const char *TMP_LP  = "build/tw_tmp.lp";
static const char *TMP_SOL = "build/tw_tmp.sol";

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

/* Exact, because that is the claim. Unity's EQUAL_DOUBLE carries a
 * tolerance and would pass a round trip that lost digits. */
#define SAME_D(a, b) \
    TEST_ASSERT_TRUE_MESSAGE((a) == (b), "a value changed in the round trip")

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
    SAME_D(a->obj_offset, b->obj_offset);

    for (int64_t j = 0; j < a->num_col; j++) {
        SAME_D(a->col_cost[j], b->col_cost[j]);
        SAME_D(a->col_lower[j], b->col_lower[j]);
        SAME_D(a->col_upper[j], b->col_upper[j]);
        TEST_ASSERT_EQUAL_INT64(a->a_start[j + 1], b->a_start[j + 1]);
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        SAME_D(a->row_lower[i], b->row_lower[i]);
        SAME_D(a->row_upper[i], b->row_upper[i]);
    }
    for (int64_t k = 0; k < a->num_nz; k++) {
        TEST_ASSERT_EQUAL_INT64(a->a_index[k], b->a_index[k]);
        SAME_D(a->a_value[k], b->a_value[k]);
    }
    /* And the names, as the model gives them: a row nobody named is
     * called by its position on both sides, so it compares equal to a row
     * the file named that way (D284). */
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

/* The model's name travels through MPS, which has a line for it, and not
 * through LP, which has none: an LP round trip comes back as "JAOS". */
static void assert_same_model_name(const jaos_model *a, const jaos_model *b)
{
    char na[JAOS_NAME_MAX + 1], nb[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(a, na, sizeof na));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(b, nb, sizeof nb));
    TEST_ASSERT_EQUAL_STRING(na, nb);
}

/* --------------------------------------------------------------------- */
/* The models under test                                                 */
/* --------------------------------------------------------------------- */

/* Every shape the writer has a branch for, in one model: a maximize sense,
 * an objective constant, all four row types plus a ranged row and an empty
 * one, every bound form including the negative upper bound that triggers
 * the reader's classic wart, and a column with no coefficients at all.
 *
 * The value 1.0000000000000002 is `nextafter(1.0, 2.0)`: it needs all
 * seventeen significant digits, so it fails a writer that prints fifteen. */
static const double SEVENTEEN = 1.0000000000000002;

static jaos_model *build_every_shape(void)
{
    /* rows: R1 L, R2 G, R3 E, R4 free, R5 ranged, R6 empty E */
    const double rl[] = {-INFINITY, 2.0, 7.0, -INFINITY, 1.0, 0.0};
    const double ru[] = {10.0, INFINITY, 7.0, INFINITY, 4.0, 0.0};
    /* columns: default, free, fixed, MI+UP, LO+UP, and [0,-5] with no
     * entries — the wart case and the empty-column case in one. */
    const double cost[] = {1.5, 0.0, -2.0, 0.0, SEVENTEEN, 0.0};
    const double cl[]   = {0.0, -INFINITY, 3.0, -INFINITY, -1.0, 0.0};
    const double cu[]   = {INFINITY, INFINITY, 3.0, 4.0, 2.5, -5.0};

    const int64_t as[] = {0, 2, 3, 4, 5, 8, 8};
    const int64_t ai[] = {0, 1, 2, 4, 0, 1, 2, 4};
    const double  av[] = {1.0, 2.0, -1.0, 3.0, SEVENTEEN, -5.0, 6.0, 0.5};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 6, 6, JAOS_MAXIMIZE, -3.25, cost, cl, cu, rl, ru,
                     8, as, ai, av));
    return m;
}

/* Inside the LP dialect: no ranged row, no free row, no empty row, and
 * every column carries a cost so none of them is unnameable. */
static jaos_model *build_lp_shaped(void)
{
    const double rl[]   = {-INFINITY, -3.0, 7.0};
    const double ru[]   = {10.0, INFINITY, 7.0};
    const double cost[] = {3.0, 2.0, -1.0};
    const double cl[]   = {0.0, -1.0, -INFINITY};
    const double cu[]   = {INFINITY, 8.0, INFINITY};
    const int64_t as[]  = {0, 3, 6, 7};
    const int64_t ai[]  = {0, 1, 2, 0, 1, 2, 2};
    const double  av[]  = {1.0, 2.0, 1.0, 1.0, -1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 5.0, cost, cl, cu, rl, ru,
                     7, as, ai, av));
    return m;
}

/* --------------------------------------------------------------------- */
/* MPS                                                                   */
/* --------------------------------------------------------------------- */

static void test_mps_round_trip_golden_instance(void)
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(a, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(a, TMP_MPS));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, TMP_MPS));
    assert_same_model(a, b);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_MPS);
}

static void test_mps_round_trip_every_shape(void)
{
    jaos_model *a = build_every_shape();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(a, TMP_MPS));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, TMP_MPS));
    assert_same_model(a, b);

    /* Named individually, because assert_same_model would pass if both
     * sides were wrong in the same way and these are the four the writer
     * has to reason about rather than copy. */
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, b->sense);
    SAME_D(-3.25, b->obj_offset);
    SAME_D(0.0, b->col_lower[5]);      /* the negative-UP wart did not fire */
    SAME_D(-5.0, b->col_upper[5]);
    TEST_ASSERT_TRUE(isinf(b->row_lower[3]) && isinf(b->row_upper[3]));
    SAME_D(1.0, b->row_lower[4]);      /* the ranged row, both ends exact */
    SAME_D(4.0, b->row_upper[4]);
    SAME_D(SEVENTEEN, b->col_cost[4]); /* seventeen digits survived */
    SAME_D(SEVENTEEN, b->a_value[4]);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_MPS);
}

/* The control for the test above. A writer that emits `UP BND C6 -5` with
 * no explicit lower bound produces this file, and the reader's documented
 * wart then drops C6's lower bound to -inf. If this test ever stops seeing
 * -inf, the round-trip assertion above has stopped proving anything. */
static void test_wart_control_shows_the_round_trip_can_fail(void)
{
    FILE *f = fopen(TMP_MPS, "w");
    TEST_ASSERT_NOT_NULL(f);
    fprintf(f, "NAME          NAIVE\nROWS\n N  COST\n L  R1\n"
               "COLUMNS\n    C1        COST      1   R1        1\n"
               "    C6        COST      0\n"
               "RHS\n    RHS       R1        10\n"
               "BOUNDS\n UP BND       C6        -5\nENDATA\n");
    fclose(f);

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, TMP_MPS));
    TEST_ASSERT_TRUE_MESSAGE(m->col_lower[1] == -INFINITY,
        "the reader's negative-UP wart no longer fires, so the round-trip "
        "test above is no longer proving the writer avoids it");
    SAME_D(-5.0, m->col_upper[1]);

    jaos_model_free(m);
    remove(TMP_MPS);
}

static void test_mps_refuses_what_it_cannot_express(void)
{
    const double cost[] = {1.0};
    const double cl[]   = {0.0};
    const double cu[]   = {INFINITY};
    const int64_t as[]  = {0, 1};
    const int64_t ai[]  = {0};
    const double  av[]  = {1.0};

    /* A row whose lower bound sits above its upper one. Legitimate — the
     * solve calls it infeasible — and no RANGES form can say it. */
    double rl[] = {5.0}, ru[] = {2.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "R1"));
    TEST_ASSERT_FALSE_MESSAGE(file_exists(TMP_MPS),
        "a refused write left a partial file behind");

    /* A column bound at an infinity of the wrong sign. */
    double rl2[] = {0.0}, ru2[] = {1.0};
    double cl2[] = {INFINITY}, cu2[] = {INFINITY};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl2, cu2, rl2, ru2,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "C1"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));

    jaos_model_free(m);
}

/* --------------------------------------------------------------------- */
/* LP                                                                    */
/* --------------------------------------------------------------------- */

static void test_lp_round_trip(void)
{
    jaos_model *a = build_lp_shaped();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(a, b);
    SAME_D(5.0, b->obj_offset);
    TEST_ASSERT_TRUE(isinf(b->col_lower[2]) && isinf(b->col_upper[2]));

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_LP);
}

static void test_lp_round_trip_of_the_golden_lp(void)
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(a, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(a, b);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_LP);
}

/* Maximize, and a model wide enough that the expression has to wrap. The
 * reader wraps expressions freely, so a wrap must change nothing. */
static void test_lp_wraps_without_changing_the_model(void)
{
    constexpr int64_t N = 40;
    double cost[N], cl[N], cu[N], av[N];
    int64_t as[N + 1], ai[N];
    for (int64_t j = 0; j < N; j++) {
        cost[j] = (double)(j + 1);
        cl[j] = 0.0;
        cu[j] = INFINITY;
        as[j] = j;
        ai[j] = 0;
        av[j] = (double)(j + 1) * 0.5;
    }
    as[N] = N;
    double rl[] = {-INFINITY}, ru[] = {100.0};

    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, N, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     N, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(a, b);
    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, b->sense);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_LP);
}

/* The three things the dialect cannot say. Each must be refused by name and
 * must leave no file, because a truncated LP file still parses as a model. */
static void test_lp_refuses_what_the_dialect_cannot_say(void)
{
    jaos_model *m = build_every_shape();   /* free row, ranged row, empty row */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "MPS instead"));
    TEST_ASSERT_FALSE_MESSAGE(file_exists(TMP_LP),
        "a refused LP write left a partial file behind");

    /* And MPS takes that same model without complaint, which is what every
     * one of those messages promises. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, TMP_MPS));
    jaos_model *back = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(back, TMP_MPS));
    assert_same_model(m, back);

    jaos_model_free(m);
    jaos_model_free(back);
    remove(TMP_MPS);
}

/* LP format has no COLUMNS section, so the reader numbers a column where its
 * name FIRST appears in the token stream. A writer that lists only the
 * costed columns in the objective therefore renumbers every zero-cost column
 * by wherever its first coefficient happens to sit — silently, because the
 * file is valid and simply describes a different model.
 *
 * This is the case that fails against a writer whose objective skips a zero
 * cost, and it is the shape 83 of the 139 gate instances have (D226). Every
 * other LP test here happens to give all its columns a cost, which is why
 * none of them caught it and this one exists. */
static void test_lp_keeps_column_order_when_a_cost_is_zero(void)
{
    /* C1 costs nothing and appears only in R2; C2 costs 5 and is in R1. A
     * writer that starts the objective at C2 makes C2 column 0. */
    const double cost[] = {0.0, 5.0};
    const double cl[]   = {0.0, 0.0};
    const double cu[]   = {7.0, INFINITY};
    const double rl[]   = {-INFINITY, 4.0};
    const double ru[]   = {10.0, INFINITY};
    const int64_t as[]  = {0, 1, 2};
    const int64_t ai[]  = {1, 0};
    const double  av[]  = {3.0, 2.0};

    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 2, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(a, b);
    SAME_D(0.0, b->col_cost[0]);       /* still C1, not the costed column */
    SAME_D(7.0, b->col_upper[0]);
    SAME_D(5.0, b->col_cost[1]);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_LP);
}

/* A column in no row and with no cost: MPS has always taken it, and LP takes
 * it too now that the objective names every column. */
static void test_lp_takes_a_column_that_appears_in_no_row(void)
{
    const double cost[] = {1.0, 0.0};
    const double cl[]   = {0.0, -2.0};
    const double cu[]   = {INFINITY, 3.0};
    const double rl[]   = {-INFINITY};
    const double ru[]   = {10.0};
    const int64_t as[]  = {0, 1, 1};
    const int64_t ai[]  = {0};
    const double  av[]  = {1.0};

    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(a, b);
    SAME_D(-2.0, b->col_lower[1]);
    SAME_D(3.0, b->col_upper[1]);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_LP);
}

/* One column, one row: the smallest model that can carry a bad row or a bad
 * bound. `entry` decides whether the row gets its coefficient, which is how
 * an empty row is built. */
static jaos_model *one_by_one(double row_lo, double row_hi, double col_lo,
                              double col_hi, bool entry)
{
    const double cost[] = {1.0};
    const double cl[] = {col_lo}, cu[] = {col_hi};
    const double rl[] = {row_lo}, ru[] = {row_hi};
    const int64_t as_with[] = {0, 1}, as_none[] = {0, 0};
    const int64_t ai[] = {0};
    const double  av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     entry ? 1 : 0, entry ? as_with : as_none, ai, av));
    return m;
}

/* Every LP guard on its own, with the message it has to produce.
 *
 * `test_lp_refuses_what_the_dialect_cannot_say` above reaches only the
 * free-row branch: the check loop stops at the first failure and every
 * message contains "MPS instead", so four of the five guards could be
 * deleted and that test stayed green.
 *
 * The ranged-row guard is why this test is not optional. Without it a row
 * `1 <= r <= 4` is written as `R1: 1 C1 >= 1` — the upper bound is simply
 * gone, the file is valid, `jaos_read_lp` accepts it without a word, and the
 * model is silently different. That is the exact failure `src/write.c`
 * exists to prevent. */
static void test_each_lp_guard_fires_on_its_own(void)
{
    static const struct {
        double rl, ru, cl, cu;
        bool entry;
        const char *want;
    } cases[] = {
        /* A ranged row is NOT here: the LP dialect writes one now (D239).
         * Nor is a row with no coefficients: it is written as a zero term
         * since D276, and `test_a_row_with_no_coefficients_round_trips`
         * below is what replaced this line. */
        {-INFINITY, INFINITY,  0.0, INFINITY, true,  "is free"},
        {INFINITY, INFINITY,   0.0, INFINITY, true,  "at an infinity"},
        /* An equality row, so the row loop passes it and the column loop
         * is reached: the first refusal wins, so a row this loop rejects
         * would test that one twice instead of the column bound. */
        {0.0, 0.0,        INFINITY, INFINITY, true,  "at an infinity"},
    };

    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        jaos_model *m = one_by_one(cases[k].rl, cases[k].ru, cases[k].cl,
                                   cases[k].cu, cases[k].entry);
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                              jaos_write_lp(m, TMP_LP));
        TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(jaos_model_error(m), cases[k].want), cases[k].want);
        TEST_ASSERT_FALSE(file_exists(TMP_LP));
        jaos_model_free(m);
    }
}

/* A row with no coefficients survives the LP round trip, which is the case
 * the guard above used to refuse (D276).
 *
 * LP has no form for a constraint with an empty body, and that is what the
 * refusal said. It does have a form for a term whose coefficient is zero,
 * and the reader drops explicit zeros on the way back in, so the row comes
 * back empty. The assertions below are what make that a round trip rather
 * than a hope: the row must come back with **zero** entries, not one entry
 * of zero, and its bounds must be the ones it went out with.
 *
 * The negative half is `test_each_lp_guard_fires_on_its_own`, which still
 * refuses a free row and an infinite bound: this did not open the gate to
 * everything. */
static void test_a_row_with_no_coefficients_round_trips_through_lp(void)
{
    jaos_model *m = one_by_one(5.0, 5.0, 0.0, INFINITY, false);
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(m, TMP_LP));
    jaos_model_free(m);

    jaos_model *back = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(back, TMP_LP));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(back));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_col(back));
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, jaos_num_nz(back),
        "the zero term came back as an entry instead of being dropped");
    /* The column-wise store is the authoritative one and is always built;
     * `ar_start` is a mirror made on demand and is null on a model nothing
     * has asked for one. Reading it here segfaulted. */
    TEST_ASSERT_EQUAL_INT64(0, back->a_start[1] - back->a_start[0]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, back->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, back->row_upper[0]);
    jaos_model_free(back);
    remove(TMP_LP);
}

/* And a model with a row but no columns at all still refuses: there is no
 * variable to hang the zero term on. */
static void test_a_row_with_no_columns_is_still_refused(void)
{
    const double rl[] = {5.0}, ru[] = {5.0};
    const int64_t as[] = {0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 0, 1, JAOS_MINIMIZE, 0.0, nullptr, nullptr, nullptr,
                     rl, ru, 0, as, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no columns"));
    TEST_ASSERT_FALSE(file_exists(TMP_LP));
    jaos_model_free(m);
}

/* A ranged row survives the LP round trip as one row with both ends, which
 * is the case the guard above used to refuse (D239). */
static void test_a_ranged_row_round_trips_through_lp(void)
{
    jaos_model *m = one_by_one(1.0, 4.0, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(m, TMP_LP));
    jaos_model_free(m);

    jaos_model *back = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&back));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(back, TMP_LP));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_row(back));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, back->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, back->row_upper[0]);
    jaos_model_free(back);
    remove(TMP_LP);
}

/* The two MPS branches the refusal test above does not reach. */
static void test_each_mps_guard_fires_on_its_own(void)
{
    /* Both row bounds at an infinity: not a free row, and no row type. */
    jaos_model *m = one_by_one(INFINITY, INFINITY, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "both bounds"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    jaos_model_free(m);

    /* A ranged row no RANGES entry reproduces. 2^53 + 2 is representable and
     * 2^53 + 1 is not, so neither `1 + |r| == ru` nor `ru - |r| == 1` has a
     * solution: the refusal is right rather than cautious. */
    m = one_by_one(1.0, 9007199254740994.0, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "reproduces"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    jaos_model_free(m);
}

/* A refused write must not touch a file that was already at that path.
 *
 * `fopen(path, "w")` truncates and the failure path calls `remove`, so a
 * writer that opens before it checks destroys the caller's previous file and
 * hands back an error code. A caller re-saving `model.lp` after the model
 * acquired a free row would lose `model.lp`. */
static void test_a_refusal_leaves_an_existing_file_alone(void)
{
    static const char *KEEP = "kept by the caller\n";

    const char *paths[] = {TMP_LP, TMP_MPS, TMP_SOL};
    for (int k = 0; k < 3; k++) {
        FILE *f = fopen(paths[k], "w");
        TEST_ASSERT_NOT_NULL(f);
        fputs(KEEP, f);
        fclose(f);
    }

    jaos_model *m = one_by_one(-INFINITY, INFINITY, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    jaos_model_free(m);

    m = one_by_one(INFINITY, INFINITY, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    /* Never solved, so the solution writer refuses too. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_solution(m, TMP_SOL));
    jaos_model_free(m);

    for (int k = 0; k < 3; k++) {
        char line[64] = {0};
        FILE *f = fopen(paths[k], "r");
        TEST_ASSERT_NOT_NULL_MESSAGE(f, "a refused write deleted the file "
                                        "that was already there");
        TEST_ASSERT_NOT_NULL(fgets(line, sizeof line, f));
        fclose(f);
        TEST_ASSERT_EQUAL_STRING(KEEP, line);
        remove(paths[k]);
    }
}

/* --------------------------------------------------------------------- */
/* Solution                                                              */
/* --------------------------------------------------------------------- */

/* --------------------------------------------------------------------- */
/* The solution reader (D282)                                             */
/* --------------------------------------------------------------------- */

/* A model with three columns and two rows, solved, so there is an answer
 * with more than one of everything to round-trip. Free for the caller. */
static jaos_model *solved_model(void)
{
    /* minimize -x - 2y + z,  x + y <= 4,  y + z >= 1,  all in [0, 3] */
    const double cost[] = {-1.0, -2.0, 1.0};
    const double cl[]   = {0.0, 0.0, 0.0};
    const double cu[]   = {3.0, 3.0, 3.0};
    const double rl[]   = {-INFINITY, 1.0};
    const double ru[]   = {4.0, INFINITY};
    const int64_t as[]  = {0, 1, 3, 4};
    const int64_t ai[]  = {0, 0, 1, 1};
    const double  av[]  = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    return m;
}

/* Written and read back is the same answer, bit for bit. `%.17g` of a
 * finite double reads back as that double, so this is an equality test and
 * not a tolerance test -- a tolerance here would pass on a writer that
 * rounded. */
static void test_a_solution_file_reads_back_exactly(void)
{
    jaos_model *m = solved_model();

    double x[3], d[3], ra[2], rd[2], obj_w = 0.0;
    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, ra, rd, d));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj_w));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));

    double x2[3], d2[3], ra2[2], rd2[2], obj_r = 0.0;
    jaos_basis_status cs2[3], rs2[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, TMP_SOL, &obj_r, x2, d2, cs2, ra2, rd2, rs2));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    TEST_ASSERT_EQUAL_MEMORY(&obj_w, &obj_r, sizeof obj_w);
    for (int j = 0; j < 3; j++) {
        TEST_ASSERT_EQUAL_MEMORY(&x[j], &x2[j], sizeof x[j]);
        TEST_ASSERT_EQUAL_MEMORY(&d[j], &d2[j], sizeof d[j]);
        TEST_ASSERT_EQUAL_INT(cs[j], cs2[j]);
    }
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL_MEMORY(&ra[i], &ra2[i], sizeof ra[i]);
        TEST_ASSERT_EQUAL_MEMORY(&rd[i], &rd2[i], sizeof rd[i]);
        TEST_ASSERT_EQUAL_INT(rs[i], rs2[i]);
    }
    remove(TMP_SOL);
    jaos_model_free(m);
}

/* Every output pointer is optional, and asking for nothing must still
 * validate the whole file. The control is that a broken file passed with
 * all-NULL outputs is still refused. */
static void test_every_output_of_the_solution_reader_is_optional(void)
{
    jaos_model *m = solved_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, TMP_SOL, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, TMP_SOL, &obj, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(obj < 0.0);
    remove(TMP_SOL);
    jaos_model_free(m);
}

/* The basis comes back well enough to warm-start from, which is the point
 * of reading the statuses at all: set it, re-solve, and the answer is the
 * same one. */
static void test_a_read_basis_can_be_set_back(void)
{
    jaos_model *m = solved_model();
    double obj_first = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj_first));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));

    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, TMP_SOL, nullptr, nullptr, nullptr, cs,
                           nullptr, nullptr, rs));

    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj_again = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj_again));
    TEST_ASSERT_EQUAL_MEMORY(&obj_first, &obj_again, sizeof obj_first);
    remove(TMP_SOL);
    jaos_model_free(m);
}

/* Writes `text` to TMP_SOL, reads it against `m`, and requires the refusal
 * to carry `needle`. */
static void expect_sol_reject(jaos_model *m, const char *text,
                              const char *needle)
{
    FILE *f = fopen(TMP_SOL, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(text, f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_solution(m, TMP_SOL, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), needle));
    remove(TMP_SOL);
}

/* One refusal per failure class, each with its own message.
 *
 * The model here has one column and one row, so a file describing anything
 * else is the "different model" case. The first entry is the control: the
 * same text with the right counts is ACCEPTED, so every refusal below is
 * about the one thing it changes and not about the file being malformed in
 * general. */
static void test_the_solution_reader_refuses_by_name(void)
{
    const double cost[] = {1.0};
    const double cl[]   = {0.0};
    const double cu[]   = {INFINITY};
    const double rl[]   = {2.0};
    const double ru[]   = {INFINITY};
    const int64_t as[]  = {0, 1};
    const int64_t ai[]  = {0};
    const double  av[]  = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    /* The control: this exact text is a valid file for this model. */
    FILE *f = fopen(TMP_SOL, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("# JAOS solution file, format 1\n"
          "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
          "col C1 2 0 basic\nrow R1 2 1 lower\nend\n", f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, TMP_SOL, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    remove(TMP_SOL);

    /* A file from a model with a different shape. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 2\nrows 1\n"
        "col C1 2 0 basic\ncol C2 0 0 lower\nrow R1 2 1 lower\nend\n",
        "this model has");

    /* A name that is not the one this index gets. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col X1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "records are in index order");

    /* A status word that is not one of the four. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 superbasic\nrow R1 2 1 lower\nend\n",
        "not a basis status");

    /* A number the writer would never produce. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 inf 0 basic\nrow R1 2 1 lower\nend\n",
        "not a finite number");

    /* Trailing characters after a number: 1.5x is not 1.5. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 1.5x 0 basic\nrow R1 2 1 lower\nend\n",
        "not a finite number");

    /* A certificate is the other reader's (D285), and a status nobody
     * writes is nobody's. */
    expect_sol_reject(m,
        "status infeasible\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "jaos_read_certificate");
    expect_sol_reject(m,
        "status work_limit\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "only 'optimal'");

    /* Fewer records than the count promises. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nend\n",
        "carries");

    /* More records than the count allows. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\ncol C2 0 0 lower\nrow R1 2 1 lower\nend\n",
        "more 'col' records");

    /* A record before the counts, so nothing knows how many to expect. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\n"
        "col C1 2 0 basic\ncolumns 1\nrows 1\nrow R1 2 1 lower\nend\n",
        "before both counts");

    /* No end. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\n",
        "without 'end'");

    /* Content after end. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\nrow R2 0 0 lower\n",
        "after 'end'");

    /* A record nothing recognizes. */
    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nquux 1\nend\n",
        "unknown record");

    /* No objective line. */
    expect_sol_reject(m,
        "status optimal\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "no 'objective' line");

    /* No status line: refused at the first record that needs one, and at
     * the end when nothing did. */
    expect_sol_reject(m,
        "objective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "before 'status'");
    expect_sol_reject(m, "columns 1\nrows 1\nend\n", "no 'status' line");

    jaos_model_free(m);
}

static void test_the_solution_reader_rejects_bad_arguments(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_solution(nullptr, TMP_SOL, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_solution(m, nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_read_solution(m, "tests/data/does_not_exist.sol", nullptr,
                           nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr));
    jaos_model_free(m);
}

static void test_solution_file_carries_the_answer(void)
{
    /* minimize x, x >= 2. The optimum is 2. */
    const double cost[] = {1.0};
    const double cl[]   = {0.0};
    const double cu[]   = {INFINITY};
    const double rl[]   = {2.0};
    const double ru[]   = {INFINITY};
    const int64_t as[]  = {0, 1};
    const int64_t ai[]  = {0};
    const double  av[]  = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));

    FILE *f = fopen(TMP_SOL, "r");
    TEST_ASSERT_NOT_NULL(f);
    char line[256];
    bool saw_status = false, saw_obj = false, saw_col = false, saw_row = false;
    double obj = 0.0, colval = 0.0;
    char name[64], word[64];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "status optimal", 14) == 0)
            saw_status = true;
        else if (sscanf(line, "objective %lf", &obj) == 1)
            saw_obj = true;
        else if (sscanf(line, "col %63s %lf %*f %63s", name, &colval,
                        word) == 3) {
            saw_col = true;
            TEST_ASSERT_EQUAL_STRING("C1", name);
        } else if (strncmp(line, "row R1 ", 7) == 0)
            saw_row = true;
    }
    fclose(f);

    TEST_ASSERT_TRUE(saw_status && saw_obj && saw_col && saw_row);
    double want = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &want));
    SAME_D(want, obj);           /* the file's digits are the exact answer */
    SAME_D(2.0, colval);

    jaos_model_free(m);
    remove(TMP_SOL);
}

/* A solved model can hold a value no file can carry, and the writer must
 * say so instead of printing it.
 *
 * `wr_num` promises its caller a finite argument and asserts the seventeen
 * digit fallback reads back equal, which is false for a NaN. The two model
 * writers get that guarantee from the model's own setters, which reject a
 * non-finite cost or bound. `jaos_write_solution` gets it from nowhere: the
 * objective is a sum that may overflow, and `jm_objective_value` publishes
 * a non-finite one deliberately rather than lying about it.
 *
 * Two columns fixed at 1e300 with costs 1e300 and -1e300 give an objective
 * of `inf + -inf`. The solve is optimal and the answer is a NaN. Before the
 * guard this aborted on any build with asserts and wrote `objective -nan`
 * on any build without them, and the spelling of that word is the host
 * libc's, so the file was not reproducible either (D226). */
static void test_solution_refuses_a_value_no_file_can_carry(void)
{
    const double cost[] = {1e300, -1e300};
    const double cl[]   = {1e300, 1e300};
    const double cu[]   = {1e300, 1e300};
    const double rl[]   = {-INFINITY};
    const double ru[]   = {INFINITY};
    const int64_t as[]  = {0, 1, 2};
    const int64_t ai[]  = {0, 0};
    const double  av[]  = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    /* The setup has to keep reproducing, or this test passes on a model
     * that never reaches the guard. */
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_FALSE_MESSAGE(isfinite(obj),
                              "the objective is finite, so this model no "
                              "longer reaches the guard it was built for");

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not finite"));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));

    /* The two model writers take the same model: nothing in it is
     * non-finite, only the answer is. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, TMP_MPS));
    remove(TMP_MPS);

    jaos_model_free(m);
}

static void test_solution_refused_without_an_optimum(void)
{
    jaos_model *m = build_lp_shaped();     /* loaded, never solved */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not run"));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));
    jaos_model_free(m);
}

/* --------------------------------------------------------------------- */
/* Arguments                                                             */
/* --------------------------------------------------------------------- */

static void test_bad_arguments_and_unwritable_paths(void)
{
    jaos_model *m = build_lp_shaped();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_solution(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps(nullptr, TMP_MPS));

    const char *nowhere = "build/no_such_directory_here/x.mps";
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO, jaos_write_mps(m, nowhere));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "cannot open"));
    jaos_model_free(m);
}

/* An empty model has no rows, no columns and nothing to say, and it still
 * has to survive the trip: the section headers alone must read back. */
static void test_empty_model_round_trips(void)
{
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(a, TMP_MPS));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, TMP_MPS));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(b));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_row(b));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(b));

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_MPS);
}

/* --------------------------------------------------------------------- */
/* Names (D284)                                                          */
/* --------------------------------------------------------------------- */

/* A model with a name on some rows and columns and not others, so the
 * round trip has to carry both kinds: the stored ones as they are, the
 * unnamed ones as their positions. */
static jaos_model *build_named(void)
{
    jaos_model *m = build_lp_shaped();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "x.first"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 2, "z_3"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "supply"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_name(m, "profit"));
    return m;
}

static void test_names_round_trip_through_both_formats(void)
{
    jaos_model *a = build_named();
    char buf[JAOS_NAME_MAX + 1];

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_model_name(a, "named.model"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(a, TMP_MPS));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, TMP_MPS));
    assert_same_model(a, b);
    assert_same_model_name(a, b);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(b, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("named.model", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(b, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("profit", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_name(b, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("C2", buf);
    jaos_model_free(b);

    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(a, b);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(b, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("profit", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, 1, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("supply", buf);
    jaos_model_free(b);

    /* The golden MPS instance's own names survive a trip through LP. */
    jaos_model *g = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(g, "tests/data/t1.mps"));
    b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(g, TMP_LP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(g, b);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(b, 2, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("EQ1", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective_name(b, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("COST", buf);
    jaos_model_free(b);
    jaos_model_free(g);
    jaos_model_free(a);
    remove(TMP_MPS);
    remove(TMP_LP);
}

static void test_two_of_a_name_are_refused_by_every_writer(void)
{
    jaos_model *m = build_named();
    /* Column 1 is unnamed and called C2; a stored C2 elsewhere collides
     * with it, which is the positional case the header promises. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "C2"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "columns 0 and 1"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "'C2'"));
    TEST_ASSERT_FALSE(file_exists(TMP_LP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    remove(TMP_SOL);                 /* an earlier test may have left one */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "x.first"));

    /* The objective shares the rows' namespace in MPS, so it is one rule
     * for every format. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 2, "profit"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "the objective"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 2, "supply"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "rows 1 and 2"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 2, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(m, TMP_LP));
    remove(TMP_LP);
    jaos_model_free(m);
}

static void test_lp_refuses_a_name_its_scanner_would_not_read_back(void)
{
    jaos_model *m = build_named();
    /* A `-` is an operator; a leading digit is a number; a keyword is a
     * keyword whatever its case. MPS takes all three. */
    const char *bad[] = {"x-1", "2x", "Free", "a:b", "INF"};
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, bad[k]));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), bad[k]));
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "MPS"));
        TEST_ASSERT_FALSE(file_exists(TMP_LP));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, TMP_MPS));
        jaos_model *b = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, TMP_MPS));
        assert_same_model(m, b);
        jaos_model_free(b);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "r-0"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "row 'r-0'"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_name(m, "min"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "objective"));

    /* And what the CPLEX set allows, the scanner now reads: the symbols
     * other solvers' files carry, `.` first among them. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_name(m, "obj"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "y(2)/a$b#c!d"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(m, TMP_LP));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, TMP_LP));
    assert_same_model(m, b);
    jaos_model_free(b);
    remove(TMP_LP);
    remove(TMP_MPS);
    jaos_model_free(m);
}

static void test_mps_refuses_the_row_name_its_reader_takes_for_a_marker(void)
{
    jaos_model *m = build_named();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "'MARKER'"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "MARKER"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_objective_name(m, "'MARKER'"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    jaos_model_free(m);
}

static void test_a_solution_file_carries_the_names_and_is_checked_on_them(void)
{
    jaos_model *m = build_named();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    const jaos_status wst = jaos_write_solution(m, TMP_SOL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, wst, jaos_model_error(m));

    /* The file names the columns as the model does. */
    FILE *f = fopen(TMP_SOL, "r");
    TEST_ASSERT_NOT_NULL(f);
    char line[512];
    bool saw_named = false, saw_positional = false;
    while (fgets(line, sizeof line, f) != nullptr) {
        if (strncmp(line, "col x.first ", 12) == 0) saw_named = true;
        if (strncmp(line, "col C2 ", 7) == 0)       saw_positional = true;
    }
    fclose(f);
    TEST_ASSERT_TRUE(saw_named);
    TEST_ASSERT_TRUE(saw_positional);

    /* It reads back into the model that wrote it, and not into the same
     * model renamed. Not under the two presolve fault builds: they corrupt
     * the postsolved basis on purpose, so the file can carry a status word
     * the reader rightly refuses, the rule every test that reads a
     * postsolved answer follows. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    double x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, TMP_SOL, nullptr, x, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "renamed"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_solution(m, TMP_SOL, nullptr, x, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "'renamed'"));
#endif
    remove(TMP_SOL);
    jaos_model_free(m);
}


/* --------------------------------------------------------------------- */
/* Certificates in the solution file (D285)                              */
/* --------------------------------------------------------------------- */

/* x >= 3 and x <= 2 through one row: infeasible, with a certificate. */
static jaos_model *build_infeasible(void)
{
    const double cost[] = {1.0}, cl[] = {3.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {2.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "cap"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    return m;
}

/* min -x - y with x - y <= 1: unbounded along (1, 1). */
static jaos_model *build_unbounded(void)
{
    const double cost[] = {-1.0, -1.0}, cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {1.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, -1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "y"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    return m;
}

static void test_an_infeasibility_certificate_round_trips(void)
{
    jaos_model *m = build_infeasible();
    double want[1], got[1] = {NAN};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, want));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));

    /* The file says what it is, and names the row as the model does. */
    FILE *f = fopen(TMP_SOL, "r");
    TEST_ASSERT_NOT_NULL(f);
    char line[256];
    bool saw_status = false, saw_ray = false, saw_obj = false;
    while (fgets(line, sizeof line, f) != nullptr) {
        if (strcmp(line, "status infeasible\n") == 0) saw_status = true;
        if (strncmp(line, "ray cap ", 8) == 0)        saw_ray = true;
        if (strncmp(line, "objective", 9) == 0)       saw_obj = true;
    }
    fclose(f);
    TEST_ASSERT_TRUE(saw_status);
    TEST_ASSERT_TRUE(saw_ray);
    TEST_ASSERT_FALSE(saw_obj);

    /* It reads back bit for bit, says which kind it is, and the checker
     * accepts it from the model alone. */
    jaos_solve_status kind = JAOS_SOLVE_NOT_RUN;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution_file_status(m, TMP_SOL, &kind));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, kind);
    kind = JAOS_SOLVE_NOT_RUN;
    double untouched[2] = {7.0, 7.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_certificate(m, TMP_SOL, &kind, got, untouched));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, kind);
    SAME_D(want[0], got[0]);
    SAME_D(7.0, untouched[0]);
    jaos_certificate_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_certificate(m, got, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);

    /* The optimum reader refuses it and says where to go instead. */
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_solution(m, TMP_SOL, nullptr, x, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "jaos_read_certificate"));
    remove(TMP_SOL);
    jaos_model_free(m);
}

static void test_an_unbounded_ray_round_trips(void)
{
    jaos_model *m = build_unbounded();
    double want[2], got[2] = {NAN, NAN};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, want));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));

    FILE *f = fopen(TMP_SOL, "r");
    TEST_ASSERT_NOT_NULL(f);
    char line[256];
    bool saw_status = false, saw_c1 = false, saw_y = false;
    while (fgets(line, sizeof line, f) != nullptr) {
        if (strcmp(line, "status unbounded\n") == 0) saw_status = true;
        if (strncmp(line, "ray C1 ", 7) == 0)        saw_c1 = true;
        if (strncmp(line, "ray y ", 6) == 0)         saw_y = true;
    }
    fclose(f);
    TEST_ASSERT_TRUE(saw_status);
    TEST_ASSERT_TRUE(saw_c1);
    TEST_ASSERT_TRUE(saw_y);

    jaos_solve_status kind = JAOS_SOLVE_NOT_RUN;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_certificate(m, TMP_SOL, &kind, nullptr, got));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, kind);
    SAME_D(want[0], got[0]);
    SAME_D(want[1], got[1]);
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, got, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);

    /* A ray tampered with in the file still reads -- the reader judges
     * the format, not the mathematics -- and the checker then refuses it,
     * which is the division of labour the CLI's `check` rests on. */
    f = fopen(TMP_SOL, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("# JAOS solution file, format 1\nstatus unbounded\n"
          "columns 2\nrows 1\nray C1 1\nray y -1\nend\n", f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_certificate(m, TMP_SOL, &kind, nullptr, got));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, got, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    remove(TMP_SOL);
    jaos_model_free(m);
}

static void test_the_certificate_reader_refuses_what_is_not_one(void)
{
    jaos_model *m = build_unbounded();
    jaos_solve_status kind;
    double ray[2];
    /* `peek` cases go through jaos_solution_file_status, which takes any
     * of the three kinds, so a fault past an `optimal` status line is
     * reached rather than refused at the line itself. */
    struct { const char *text; const char *why; bool peek; } cases[] = {
        /* an optimum is the other reader's */
        {"status optimal\nobjective 1\ncolumns 2\nrows 1\n"
         "col C1 0 0 lower\ncol y 0 0 lower\nrow R1 0 0 basic\nend\n",
         "jaos_read_solution", false},
        /* a status nobody writes */
        {"status work_limit\ncolumns 2\nrows 1\nend\n", "only 'optimal'",
         false},
        /* a ray record where the status says an optimum */
        {"status optimal\nobjective 1\ncolumns 2\nrows 1\nray C1 1\nend\n",
         "'ray' record", true},
        /* an objective in a certificate */
        {"status unbounded\nobjective 1\ncolumns 2\nrows 1\nray C1 1\n"
         "ray y 1\nend\n", "'objective'", false},
        /* a col record in a certificate */
        {"status unbounded\ncolumns 2\nrows 1\ncol C1 0 0 lower\nend\n",
         "'col' record", false},
        /* the wrong name */
        {"status unbounded\ncolumns 2\nrows 1\nray C1 1\nray C2 1\nend\n",
         "expected 'y'", false},
        /* too few entries */
        {"status unbounded\ncolumns 2\nrows 1\nray C1 1\nend\n", "carries 1",
         false},
        /* too many */
        {"status unbounded\ncolumns 2\nrows 1\nray C1 1\nray y 1\nray y 1\n"
         "end\n", "more 'ray' records", false},
        /* a record before the status line */
        {"columns 2\nrows 1\nray C1 1\nstatus unbounded\nray y 1\nend\n",
         "before 'status'", false},
        /* an infeasible certificate over the wrong side */
        {"status infeasible\ncolumns 2\nrows 1\nray C1 1\nend\n",
         "expected 'R1'", false},
        /* not a number */
        {"status unbounded\ncolumns 2\nrows 1\nray C1 inf\nray y 1\nend\n",
         "finite", false},
    };
    for (size_t k = 0; k < sizeof cases / sizeof *cases; k++) {
        FILE *f = fopen(TMP_SOL, "w");
        TEST_ASSERT_NOT_NULL(f);
        fputs(cases[k].text, f);
        fclose(f);
        const jaos_status st = cases[k].peek
            ? jaos_solution_file_status(m, TMP_SOL, &kind)
            : jaos_read_certificate(m, TMP_SOL, &kind, nullptr, ray);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_INVALID_INPUT, st,
                                      cases[k].text);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(jaos_model_error(m), cases[k].why),
                                     jaos_model_error(m));
    }
    /* And the status peek refuses the same files, since it reads them
     * whole, but accepts every kind it does read. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution_file_status(m, TMP_SOL, &kind));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_certificate(m, TMP_SOL, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_certificate(nullptr, TMP_SOL, &kind, nullptr, ray));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution_file_status(m, TMP_SOL, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_solution_file_status(m, "build/no-such-dir/x.sol", &kind));
    remove(TMP_SOL);
    jaos_model_free(m);
}

static void test_a_solve_with_no_certificate_writes_nothing(void)
{
    /* An inverted box is infeasible with no ray to offer (jaos.h,
     * jaos_certificate), so there is nothing to write and the message says
     * so; a budget stop has no answer of any kind. */
    const double cost[] = {1.0}, cl[] = {5.0}, cu[] = {2.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    remove(TMP_SOL);                 /* an earlier test may have left one */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no certificate"));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));
    jaos_model_free(m);
}


int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mps_round_trip_golden_instance);
    RUN_TEST(test_mps_round_trip_every_shape);
    RUN_TEST(test_wart_control_shows_the_round_trip_can_fail);
    RUN_TEST(test_mps_refuses_what_it_cannot_express);
    RUN_TEST(test_lp_round_trip);
    RUN_TEST(test_lp_round_trip_of_the_golden_lp);
    RUN_TEST(test_lp_wraps_without_changing_the_model);
    RUN_TEST(test_lp_refuses_what_the_dialect_cannot_say);
    RUN_TEST(test_lp_keeps_column_order_when_a_cost_is_zero);
    RUN_TEST(test_lp_takes_a_column_that_appears_in_no_row);
    RUN_TEST(test_each_lp_guard_fires_on_its_own);
    RUN_TEST(test_a_row_with_no_coefficients_round_trips_through_lp);
    RUN_TEST(test_a_row_with_no_columns_is_still_refused);
    RUN_TEST(test_a_ranged_row_round_trips_through_lp);
    RUN_TEST(test_each_mps_guard_fires_on_its_own);
    RUN_TEST(test_a_refusal_leaves_an_existing_file_alone);
    RUN_TEST(test_solution_file_carries_the_answer);
    RUN_TEST(test_solution_refuses_a_value_no_file_can_carry);
    RUN_TEST(test_solution_refused_without_an_optimum);
    RUN_TEST(test_a_solution_file_reads_back_exactly);
    RUN_TEST(test_every_output_of_the_solution_reader_is_optional);
    RUN_TEST(test_a_read_basis_can_be_set_back);
    RUN_TEST(test_the_solution_reader_refuses_by_name);
    RUN_TEST(test_the_solution_reader_rejects_bad_arguments);
    RUN_TEST(test_bad_arguments_and_unwritable_paths);
    RUN_TEST(test_empty_model_round_trips);
    RUN_TEST(test_names_round_trip_through_both_formats);
    RUN_TEST(test_two_of_a_name_are_refused_by_every_writer);
    RUN_TEST(test_lp_refuses_a_name_its_scanner_would_not_read_back);
    RUN_TEST(test_mps_refuses_the_row_name_its_reader_takes_for_a_marker);
    RUN_TEST(test_a_solution_file_carries_the_names_and_is_checked_on_them);
    RUN_TEST(test_an_infeasibility_certificate_round_trips);
    RUN_TEST(test_an_unbounded_ray_round_trips);
    RUN_TEST(test_the_certificate_reader_refuses_what_is_not_one);
    RUN_TEST(test_a_solve_with_no_certificate_writes_nothing);
    return UNITY_END();
}
