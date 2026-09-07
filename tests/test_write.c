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

static const char *TMP_MPS = "build/tw_tmp.mps";
static const char *TMP_LP  = "build/tw_tmp.lp";
static const char *TMP_SOL = "build/tw_tmp.sol";

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

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

static void assert_same_model_name(const jaos_model *a, const jaos_model *b)
{
    char na[JAOS_NAME_MAX + 1], nb[JAOS_NAME_MAX + 1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(a, na, sizeof na));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_name(b, nb, sizeof nb));
    TEST_ASSERT_EQUAL_STRING(na, nb);
}

static const double SEVENTEEN = 1.0000000000000002;

static jaos_model *build_every_shape(void)
{

    const double rl[] = {-INFINITY, 2.0, 7.0, -INFINITY, 1.0, 0.0};
    const double ru[] = {10.0, INFINITY, 7.0, INFINITY, 4.0, 0.0};

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

    TEST_ASSERT_EQUAL_INT(JAOS_MAXIMIZE, b->sense);
    SAME_D(-3.25, b->obj_offset);
    SAME_D(0.0, b->col_lower[5]);
    SAME_D(-5.0, b->col_upper[5]);
    TEST_ASSERT_TRUE(isinf(b->row_lower[3]) && isinf(b->row_upper[3]));
    SAME_D(1.0, b->row_lower[4]);
    SAME_D(4.0, b->row_upper[4]);
    SAME_D(SEVENTEEN, b->col_cost[4]);
    SAME_D(SEVENTEEN, b->a_value[4]);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_MPS);
}

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

static void test_lp_refuses_what_the_dialect_cannot_say(void)
{
    jaos_model *m = build_every_shape();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "MPS instead"));
    TEST_ASSERT_FALSE_MESSAGE(file_exists(TMP_LP),
        "a refused LP write left a partial file behind");

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, TMP_MPS));
    jaos_model *back = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(back, TMP_MPS));
    assert_same_model(m, back);

    jaos_model_free(m);
    jaos_model_free(back);
    remove(TMP_MPS);
}

static void test_lp_keeps_column_order_when_a_cost_is_zero(void)
{

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
    SAME_D(0.0, b->col_cost[0]);
    SAME_D(7.0, b->col_upper[0]);
    SAME_D(5.0, b->col_cost[1]);

    jaos_model_free(a);
    jaos_model_free(b);
    remove(TMP_LP);
}

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

static void test_each_lp_guard_fires_on_its_own(void)
{
    static const struct {
        double rl, ru, cl, cu;
        bool entry;
        const char *want;
    } cases[] = {

        {-INFINITY, INFINITY,  0.0, INFINITY, true,  "is free"},
        {INFINITY, INFINITY,   0.0, INFINITY, true,  "at an infinity"},

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

    TEST_ASSERT_EQUAL_INT64(0, back->a_start[1] - back->a_start[0]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, back->row_lower[0]);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, back->row_upper[0]);
    jaos_model_free(back);
    remove(TMP_LP);
}

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

static void test_each_mps_guard_fires_on_its_own(void)
{

    jaos_model *m = one_by_one(INFINITY, INFINITY, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "both bounds"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    jaos_model_free(m);

    m = one_by_one(1.0, 9007199254740994.0, 0.0, INFINITY, true);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "reproduces"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    jaos_model_free(m);
}

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

static jaos_model *solved_model(void)
{

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

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 2\nrows 1\n"
        "col C1 2 0 basic\ncol C2 0 0 lower\nrow R1 2 1 lower\nend\n",
        "this model has");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col X1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "records are in index order");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 superbasic\nrow R1 2 1 lower\nend\n",
        "not a basis status");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 inf 0 basic\nrow R1 2 1 lower\nend\n",
        "not a finite number");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 1.5x 0 basic\nrow R1 2 1 lower\nend\n",
        "not a finite number");

    expect_sol_reject(m,
        "status infeasible\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "jaos_read_certificate");
    expect_sol_reject(m,
        "status work_limit\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "only 'optimal'");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nend\n",
        "carries");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\ncol C2 0 0 lower\nrow R1 2 1 lower\nend\n",
        "more 'col' records");

    expect_sol_reject(m,
        "status optimal\nobjective 2\n"
        "col C1 2 0 basic\ncolumns 1\nrows 1\nrow R1 2 1 lower\nend\n",
        "before both counts");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\n",
        "without 'end'");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\nrow R2 0 0 lower\n",
        "after 'end'");

    expect_sol_reject(m,
        "status optimal\nobjective 2\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nquux 1\nend\n",
        "unknown record");

    expect_sol_reject(m,
        "status optimal\ncolumns 1\nrows 1\n"
        "col C1 2 0 basic\nrow R1 2 1 lower\nend\n",
        "no 'objective' line");

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
    SAME_D(want, obj);
    SAME_D(2.0, colval);

    jaos_model_free(m);
    remove(TMP_SOL);
}

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

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, TMP_MPS));
    remove(TMP_MPS);

    jaos_model_free(m);
}

static void test_solution_refused_without_an_optimum(void)
{
    jaos_model *m = build_lp_shaped();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not run"));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));
    jaos_model_free(m);
}

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

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "C2"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "columns 0 and 1"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_lp(m, TMP_LP));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "'C2'"));
    TEST_ASSERT_FALSE(file_exists(TMP_LP));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    remove(TMP_SOL);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "x.first"));

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

    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_solution(m, TMP_SOL, nullptr, x, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "jaos_read_certificate"));
    remove(TMP_SOL);
    jaos_model_free(m);
}

static jaos_model *build_infeasible_by_simplex(void)
{
    const double cost[] = {1.0, 1.0}, cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, 2.0}, ru[] = {1.0, INFINITY};
    const int64_t as[] = {0, 2, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    return m;
}

static void test_a_certificate_file_carries_its_basis(void)
{
    jaos_model *m = build_infeasible_by_simplex();
    jaos_basis_status want_c[2], want_r[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, want_c, want_r));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));

    FILE *f = fopen(TMP_SOL, "r");
    TEST_ASSERT_NOT_NULL(f);
    char line[256];
    int basis_lines = 0;
    while (fgets(line, sizeof line, f) != nullptr)
        if (strncmp(line, "basis ", 6) == 0)
            basis_lines++;
    fclose(f);
    TEST_ASSERT_EQUAL_INT(4, basis_lines);

    jaos_basis_status got_c[2] = {(jaos_basis_status)-1,
                                  (jaos_basis_status)-1};
    jaos_basis_status got_r[2] = {(jaos_basis_status)-1,
                                  (jaos_basis_status)-1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_basis(m, TMP_SOL, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(want_c, got_c, sizeof want_c);
    TEST_ASSERT_EQUAL_MEMORY(want_r, got_r, sizeof want_r);

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_basis(m, TMP_SOL, nullptr, nullptr));

    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(c, got_c, got_r));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(c, 0, -INFINITY, 3.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(c));
    jaos_model_free(c);

    remove(TMP_SOL);
    jaos_model_free(m);
}

static void test_read_basis_takes_an_optimum_file_too(void)
{
    jaos_model *m = fresh();
    const double cost[] = {1.0}, cl[] = {0.0}, cu[] = {5.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_basis_status want_c[1], want_r[1], got_c[1], got_r[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, want_c, want_r));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_basis(m, TMP_SOL, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(want_c, got_c, sizeof want_c);
    TEST_ASSERT_EQUAL_MEMORY(want_r, got_r, sizeof want_r);
    remove(TMP_SOL);
    jaos_model_free(m);
}

static void test_read_basis_refuses_a_file_that_carries_none(void)
{
    jaos_model *m = build_infeasible();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, TMP_SOL));
    jaos_basis_status got_c[1], got_r[1];
    const jaos_status rd = jaos_read_basis(m, TMP_SOL, got_c, got_r);
#if defined(JAOS_NO_PRESOLVE)

    TEST_ASSERT_EQUAL_INT(JAOS_OK, rd);
#else
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, rd);
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no basis"));
#endif
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

    struct { const char *text; const char *why; bool peek; } cases[] = {

        {"status optimal\nobjective 1\ncolumns 2\nrows 1\n"
         "col C1 0 0 lower\ncol y 0 0 lower\nrow R1 0 0 basic\nend\n",
         "jaos_read_solution", false},

        {"status work_limit\ncolumns 2\nrows 1\nend\n", "only 'optimal'",
         false},

        {"status optimal\nobjective 1\ncolumns 2\nrows 1\nray C1 1\nend\n",
         "'ray' record", true},

        {"status unbounded\nobjective 1\ncolumns 2\nrows 1\nray C1 1\n"
         "ray y 1\nend\n", "'objective'", false},

        {"status unbounded\ncolumns 2\nrows 1\ncol C1 0 0 lower\nend\n",
         "'col' record", false},

        {"status unbounded\ncolumns 2\nrows 1\nray C1 1\nray C2 1\nend\n",
         "expected 'y'", false},

        {"status unbounded\ncolumns 2\nrows 1\nray C1 1\nend\n", "carries 1",
         false},

        {"status unbounded\ncolumns 2\nrows 1\nray C1 1\nray y 1\nray y 1\n"
         "end\n", "more 'ray' records", false},

        {"columns 2\nrows 1\nray C1 1\nstatus unbounded\nray y 1\nend\n",
         "before 'status'", false},

        {"status infeasible\ncolumns 2\nrows 1\nray C1 1\nend\n",
         "expected 'R1'", false},

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

    const double cost[] = {1.0}, cl[] = {5.0}, cu[] = {2.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    remove(TMP_SOL);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_solution(m, TMP_SOL));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no certificate"));
    TEST_ASSERT_FALSE(file_exists(TMP_SOL));
    jaos_model_free(m);
}

static const char *TMP_BAS = "build/tw_tmp.bas";

static const char *slurp(const char *path)
{
    static char buf[8192];
    FILE *f = fopen(path, "r");
    if (f == nullptr)
        return nullptr;
    const size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void put_bas(const char *text)
{
    FILE *f = fopen(TMP_BAS, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(text, f);
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
}

static jaos_model *build_bas_model(void)
{

    const double cost[] = {-1.0, -2.0, 0.0};
    const double cl[] = {0.0, 0.0, -INFINITY};
    const double cu[] = {3.0, 2.0, INFINITY};
    const double rl[] = {-INFINITY, 0.0}, ru[] = {4.0, INFINITY};
    const int64_t as[] = {0, 2, 4, 5}, ai[] = {0, 1, 0, 1, 0};
    const double av[] = {1.0, 1.0, 1.0, -1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     5, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "y"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 2, "z"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "cap"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "eq"));
    return m;
}

static void test_a_basis_file_round_trips(void)
{
    jaos_model *m = build_bas_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_basis_status want_c[3], want_r[2], got_c[3], got_r[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, want_c, want_r));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, TMP_BAS));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(want_c, got_c, sizeof want_c);
    TEST_ASSERT_EQUAL_MEMORY(want_r, got_r, sizeof want_r);
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_a_basis_file_warm_starts_a_second_solve(void)
{
    jaos_model *m = build_bas_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double first = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &first));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, TMP_BAS));

    jaos_model *n = build_bas_model();
    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps_basis(n, TMP_BAS, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(n, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(n));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(n));
    double second = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(n, &second));
    SAME_D(first, second);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(n));
    remove(TMP_BAS);
    jaos_model_free(n);
    jaos_model_free(m);
}

static void test_the_file_carries_the_cards_the_basis_asks_for(void)
{
    jaos_model *m = build_bas_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, TMP_BAS));
    const char *text = slurp(TMP_BAS);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "\nNAME "));
    TEST_ASSERT_NOT_NULL(strstr(text, "\nENDATA\n"));

    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    int64_t basic_cols = 0, upper_cols = 0;
    for (int64_t j = 0; j < 3; j++) {
        if (cs[j] == JAOS_BASIS_BASIC)
            basic_cols++;
        else if (cs[j] == JAOS_BASIS_AT_UPPER)
            upper_cols++;
    }
    int64_t two = 0, ul = 0;
    for (const char *p = text; (p = strstr(p, "\n X")) != nullptr; p += 3)
        two++;
    for (const char *p = text; (p = strstr(p, "\n UL ")) != nullptr; p += 5)
        ul++;
    TEST_ASSERT_EQUAL_INT64(basic_cols, two);
    TEST_ASSERT_EQUAL_INT64(upper_cols, ul);

    TEST_ASSERT_NULL(strstr(text, "\n LL "));
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_a_slack_basis_writes_no_cards(void)
{

    const double cost[] = {1.0}, cl[] = {0.0}, cu[] = {5.0};
    const double rl[] = {-INFINITY}, ru[] = {10.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_basis_status want_c[1], want_r[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, want_c, want_r));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, want_c[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, want_r[0]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, TMP_BAS));
    const char *text = slurp(TMP_BAS);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "\n X"));
    TEST_ASSERT_NULL(strstr(text, "\n UL "));
    TEST_ASSERT_NULL(strstr(text, "\n LL "));

    jaos_basis_status got_c[1], got_r[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(want_c, got_c, sizeof want_c);
    TEST_ASSERT_EQUAL_MEMORY(want_r, got_r, sizeof want_r);
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_free_round_trips_without_a_card_of_its_own(void)
{

    const double cost[] = {1.0, 0.0};
    const double cl[] = {0.0, -INFINITY}, cu[] = {5.0, INFINITY};
    const double rl[] = {-INFINITY, 0.0};
    const double ru[] = {INFINITY, 5.0};
    const int64_t as[] = {0, 2, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "w"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "z"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "loose"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 1, "cap"));

    const jaos_basis_status cs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_FREE};
    const jaos_basis_status rs[2] = {JAOS_BASIS_FREE, JAOS_BASIS_BASIC};
    put_bas("NAME          T\n XL w         loose\nENDATA\n");
    jaos_basis_status got_c[2], got_r[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(cs, got_c, sizeof cs);
    TEST_ASSERT_EQUAL_MEMORY(rs, got_r, sizeof rs);

    put_bas("NAME          T\n XU w         loose\nENDATA\n");
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no upper bound"));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_the_reader_takes_a_file_this_writer_would_not_write(void)
{
    jaos_model *m = build_bas_model();
    put_bas("* someone else's basis\n"
            " LL x\n"
            " XU y         cap\n"
            " XL z         eq\n"
            "ENDATA\n");
    const jaos_basis_status cs[3] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_BASIC,
                                     JAOS_BASIS_BASIC};
    const jaos_basis_status rs[2] = {JAOS_BASIS_AT_UPPER,
                                     JAOS_BASIS_AT_LOWER};
    jaos_basis_status got_c[3], got_r[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(cs, got_c, sizeof cs);
    TEST_ASSERT_EQUAL_MEMORY(rs, got_r, sizeof rs);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, got_c, got_r));
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_the_basis_reader_takes_a_positional_name(void)
{
    const double cost[] = {1.0}, cl[] = {0.0}, cu[] = {5.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    put_bas(" XL C1        R1\nENDATA\n");
    jaos_basis_status got_c[1], got_r[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, got_c[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, got_r[0]);
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_each_basis_reader_guard_fires_on_its_own(void)
{
    const struct { const char *text; const char *want; } bad[] = {
        {" XU nosuch    cap\nENDATA\n",         "no column"},
        {" XU x         nosuch\nENDATA\n",      "no row"},
        {" XU x         cap\n LL x\nENDATA\n",  "second card for column"},
        {" XU x         cap\n XU y         cap\nENDATA\n",
                                                "second card for row"},
        {" UL z\nENDATA\n",                     "no upper bound"},
        {" XU x         eq\nENDATA\n",          "no upper bound"},
        {" XL x         cap\nENDATA\n",         "no lower bound"},
        {" BS x\nENDATA\n",                     "not one of the four"},
        {" XL x\nENDATA\n",                     "takes a column and a row"},
        {" UL x         cap\nENDATA\n",         "takes a column"},
        {" XU x         cap\n",                 "without 'ENDATA'"},
        {"ENDATA\n XU x         cap\n",         "after 'ENDATA'"},
    };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        jaos_model *m = build_bas_model();
        put_bas(bad[k].text);
        jaos_basis_status cs[3], rs[2];
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_INVALID_INPUT,
            jaos_read_mps_basis(m, TMP_BAS, cs, rs), bad[k].text);
        TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(jaos_model_error(m), bad[k].want), jaos_model_error(m));
        remove(TMP_BAS);
        jaos_model_free(m);
    }
}

static void test_the_basis_reader_takes_what_the_guards_leave(void)
{
    jaos_model *m = build_bas_model();
    const char *good[] = {
        " XU x         cap\nENDATA\n",
        " XL x         eq\nENDATA\n",
        " UL x\nENDATA\n",
        " LL x\nENDATA\n",
        " XU x         cap\n LL y\nENDATA\n",
        " XU x         cap\n XL y         eq\nENDATA\n",
        "ENDATA\n",
    };
    for (size_t k = 0; k < sizeof good / sizeof good[0]; k++) {
        put_bas(good[k]);
        jaos_basis_status cs[3], rs[2];
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
            jaos_read_mps_basis(m, TMP_BAS, cs, rs), good[k]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
            jaos_set_basis(m, cs, rs), good[k]);
    }
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_a_refused_basis_read_leaves_the_arrays_alone(void)
{
    jaos_model *m = build_bas_model();
    jaos_basis_status cs[3] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_AT_UPPER,
                               JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[2] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_AT_UPPER};
    put_bas(" XU x         cap\n XL nosuch    eq\nENDATA\n");
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_mps_basis(m, TMP_BAS, cs, rs));
    for (int k = 0; k < 3; k++)
        TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, cs[k]);
    for (int k = 0; k < 2; k++)
        TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[k]);
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_the_basis_writer_refuses_without_a_basis(void)
{
    jaos_model *m = build_bas_model();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps_basis(m, TMP_BAS));
    TEST_ASSERT_FALSE(file_exists(TMP_BAS));
    jaos_model_free(m);
}

static void test_the_basis_writer_refuses_two_columns_of_a_name(void)
{
    jaos_model *m = build_bas_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps_basis(m, TMP_BAS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "both named"));
    TEST_ASSERT_FALSE(file_exists(TMP_BAS));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "y"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_name(m, 0, "x"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, TMP_BAS));
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_a_refusals_basis_is_written_too(void)
{
    jaos_model *m = build_infeasible_by_simplex();
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_basis_status want_c[2], want_r[2], got_c[2], got_r[2];
    const jaos_status av = jaos_basis(m, want_c, want_r);
    if (av != JAOS_OK) {

        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                              jaos_write_mps_basis(m, TMP_BAS));
        TEST_ASSERT_FALSE(file_exists(TMP_BAS));
        jaos_model_free(m);
        return;
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, TMP_BAS));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, got_c, got_r));
    TEST_ASSERT_EQUAL_MEMORY(want_c, got_c, sizeof want_c);
    TEST_ASSERT_EQUAL_MEMORY(want_r, got_r, sizeof want_r);
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_both_outputs_of_the_basis_reader_are_optional(void)
{
    jaos_model *m = build_bas_model();
    put_bas(" XU x         cap\n XL y         eq\nENDATA\n");
    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps_basis(m, TMP_BAS, nullptr, nullptr));
    remove(TMP_BAS);
    jaos_model_free(m);
}

static void test_the_basis_file_calls_reject_bad_arguments(void)
{
    jaos_model *m = build_bas_model();
    jaos_basis_status cs[3], rs[2];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps_basis(nullptr, TMP_BAS));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_mps_basis(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_mps_basis(nullptr, TMP_BAS, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_mps_basis(m, nullptr, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_read_mps_basis(m, "build/no_such_dir/x.bas", cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_write_mps_basis(m, "build/no_such_dir/x.bas"));
    jaos_model_free(m);
}

static void test_both_model_writers_take_a_gz_name(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));

    const char *paths[2] = {"build/tw_tmp.mps.gz", "build/tw_tmp.lp.gz"};
    for (int k = 0; k < 2; k++) {
        const jaos_status st = k == 0 ? jaos_write_mps(m, paths[k])
                                      : jaos_write_lp(m, paths[k]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, st, paths[k]);

        FILE *f = fopen(paths[k], "rb");
        TEST_ASSERT_NOT_NULL(f);
        unsigned char magic[2] = {0, 0};
        TEST_ASSERT_EQUAL_size_t(2u, fread(magic, 1, 2, f));
        TEST_ASSERT_EQUAL_INT(0, fclose(f));
        TEST_ASSERT_EQUAL_UINT8(0x1fu, magic[0]);
        TEST_ASSERT_EQUAL_UINT8(0x8bu, magic[1]);

        jaos_model *b = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_read_mps(b, paths[k])
                                              : jaos_read_lp(b, paths[k]));
        assert_same_model(m, b);
        jaos_model_free(b);
        remove(paths[k]);
    }
    jaos_model_free(m);
}

static void test_a_plain_name_still_writes_text(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(m, TMP_MPS));
    FILE *f = fopen(TMP_MPS, "rb");
    TEST_ASSERT_NOT_NULL(f);
    unsigned char magic[2] = {0, 0};
    TEST_ASSERT_EQUAL_size_t(2u, fread(magic, 1, 2, f));
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
    TEST_ASSERT_TRUE(magic[0] != 0x1fu || magic[1] != 0x8bu);
    remove(TMP_MPS);
    jaos_model_free(m);
}

static void test_the_answer_writers_take_a_gz_name(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const char *sol = "build/tw_tmp.sol.gz";
    const char *bas = "build/tw_tmp.bas.gz";
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, sol));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps_basis(m, bas));

    for (int k = 0; k < 2; k++) {
        FILE *f = fopen(k == 0 ? sol : bas, "rb");
        TEST_ASSERT_NOT_NULL(f);
        unsigned char magic[2] = {0, 0};
        TEST_ASSERT_EQUAL_size_t(2u, fread(magic, 1, 2, f));
        TEST_ASSERT_EQUAL_INT(0, fclose(f));
        TEST_ASSERT_EQUAL_UINT8(0x1fu, magic[0]);
        TEST_ASSERT_EQUAL_UINT8(0x8bu, magic[1]);
    }
    remove(sol);
    remove(bas);
    jaos_model_free(m);
}

static void test_a_refused_compressed_write_leaves_nothing(void)
{
    jaos_model *m = fresh();
    const double cost[] = {1.0, 1.0}, cl[] = {0.0, 0.0};
    const double cu[] = {1.0, 1.0};
    const double rl[] = {-INFINITY}, ru[] = {1.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "same"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "same"));
    const char *path = "build/tw_refused.mps.gz";
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(m, path));
    TEST_ASSERT_FALSE(file_exists(path));
    jaos_model_free(m);
}

static const char *TMP_PT = "build/tw_tmp.pt";

static void put_pt(const char *text)
{
    FILE *f = fopen(TMP_PT, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(text, f);
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
}

static void test_a_point_file_round_trips(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const int64_t nc = m->num_col;
    double *want = jm_alloc_array(nc, sizeof *want);
    double *got = jm_alloc_array(nc, sizeof *got);
    TEST_ASSERT_NOT_NULL(want);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, want, nullptr, nullptr, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_point(m, TMP_PT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_point(m, TMP_PT, got));
    for (int64_t j = 0; j < nc; j++)
        SAME_D(want[j], got[j]);

    jaos_check_report rep;
    memset(&rep, 0, sizeof rep);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, got, nullptr, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_FALSE(rep.checked_duals);

    free(want);
    free(got);
    remove(TMP_PT);
    jaos_model_free(m);
}

static void test_the_point_reader_takes_a_file_written_by_hand(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    const int64_t nc = m->num_col;
    TEST_ASSERT_EQUAL_INT64(3, nc);

    put_pt("# somebody else's answer\n"
           "\n"
           "   X3   3   \n"
           "X1 4    # the first one\n"
           "\tX2\t3\n");
    double x[3] = {0.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_point(m, TMP_PT, x));
    SAME_D(4.0, x[0]);
    SAME_D(3.0, x[1]);
    SAME_D(3.0, x[2]);
    remove(TMP_PT);
    jaos_model_free(m);
}

static void test_each_point_reader_guard_fires_on_its_own(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    const struct { const char *text; const char *want; } bad[] = {
        {"X1 4\nX2 3\n",                  "does not name"},
        {"X1 4\nX2 3\nX3 3\nX3 1\n",      "a second value"},
        {"X1 4\nX2 3\nnosuch 1\n",        "no column is named"},
        {"X1 4\nX2 3\nX3\n",              "1 field"},
        {"X1 4\nX2 3\nX3 3 3\n",          "3 fields"},
        {"X1 4\nX2 3\nX3 nan\n",          "not a finite number"},
        {"X1 4\nX2 3\nX3 inf\n",          "not a finite number"},
        {"X1 4\nX2 3\nX3 three\n",        "not a finite number"},
    };
    double x[3];
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        put_pt(bad[k].text);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_INVALID_INPUT,
            jaos_read_point(m, TMP_PT, x), bad[k].text);
        TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(jaos_model_error(m), bad[k].want), jaos_model_error(m));
    }

    put_pt("X1 4\nX2 3\nX3 3\n");
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_point(m, TMP_PT, x));
    remove(TMP_PT);
    jaos_model_free(m);
}

static void test_the_dual_half_runs_only_with_a_duals_file(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const int64_t nc = m->num_col, nr = m->num_row;
    double *x = jm_alloc_array(nc, sizeof *x);
    double *y = jm_alloc_array(nr, sizeof *y);
    double *back = jm_alloc_array(nr, sizeof *back);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_NOT_NULL(back);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, nullptr, y, nullptr));

    {
        FILE *f = fopen(TMP_PT, "w");
        TEST_ASSERT_NOT_NULL(f);
        char nm[JAOS_NAME_MAX + 1];
        for (int64_t i = 0; i < nr; i++) {
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_name(m, i, nm, sizeof nm));
            fprintf(f, "%s %.17g\n", nm, y[i]);
        }
        TEST_ASSERT_EQUAL_INT(0, fclose(f));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_duals(m, TMP_PT, back));
    for (int64_t i = 0; i < nr; i++)
        SAME_D(y[i], back[i]);

    jaos_check_report with, without;
    memset(&with, 0, sizeof with);
    memset(&without, 0, sizeof without);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, back, 1e-7, &with));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, nullptr, 1e-7, &without));
    TEST_ASSERT_TRUE(with.checked_duals);
    TEST_ASSERT_FALSE(without.checked_duals);
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && \
    !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    TEST_ASSERT_TRUE(with.dual_feasible);
#endif

    put_pt("X1 1\nX2 1\nX3 1\n");
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_duals(m, TMP_PT, back));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no row is named"));

    free(x);
    free(y);
    free(back);
    remove(TMP_PT);
    jaos_model_free(m);
}

static void test_the_point_writer_refuses_what_it_cannot_write(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));

    remove(TMP_PT);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_point(m, TMP_PT));
    TEST_ASSERT_FALSE(file_exists(TMP_PT));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_point(m, TMP_PT));
    remove(TMP_PT);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 0, "dup"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_name(m, 1, "dup"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_point(m, TMP_PT));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "both named"));
    TEST_ASSERT_FALSE(file_exists(TMP_PT));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_point(nullptr, TMP_PT));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_point(m, nullptr));
    double x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_point(nullptr, TMP_PT, x));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_point(m, nullptr, x));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_point(m, TMP_PT, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
        jaos_read_point(m, "build/no_such_dir/x.pt", x));
    jaos_model_free(m);
}

static void test_a_point_file_is_written_from_given_values(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    const double x[3] = {1.5, -2.0, 0.0};

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_point(m, TMP_PT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_point_values(m, TMP_PT, x));

    double back[3] = {9.0, 9.0, 9.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_point(m, TMP_PT, back));
    for (int k = 0; k < 3; k++)
        SAME_D(x[k], back[k]);

    const double bad[3] = {1.0, INFINITY, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_point_values(m, TMP_PT, bad));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no file can carry"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_point_values(nullptr, TMP_PT, x));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_point_values(m, nullptr, x));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_point_values(m, TMP_PT, nullptr));
    remove(TMP_PT);
    jaos_model_free(m);
}

static void test_a_duals_file_round_trips(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    const int64_t nr = m->num_row;
    double *want = jm_alloc_array(nr, sizeof *want);
    double *got = jm_alloc_array(nr, sizeof *got);
    TEST_ASSERT_NOT_NULL(want);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, nullptr, nullptr, want, nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_duals(m, TMP_PT));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_duals(m, TMP_PT, got));
    for (int64_t i = 0; i < nr; i++)
        SAME_D(want[i], got[i]);

    for (int64_t i = 0; i < nr; i++)
        want[i] = (double)i - 1.5;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_dual_values(m, TMP_PT, want));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_duals(m, TMP_PT, got));
    for (int64_t i = 0; i < nr; i++)
        SAME_D(want[i], got[i]);

    want[0] = INFINITY;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_dual_values(m, TMP_PT, want));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no file can carry"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_duals(nullptr, TMP_PT));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_duals(m, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_write_dual_values(m, TMP_PT, nullptr));

    free(want);
    free(got);
    remove(TMP_PT);
    jaos_model_free(m);
}

static void test_a_point_file_uses_positional_names(void)
{
    const double cost[] = {1.0, 1.0}, cl[] = {0.0, 0.0};
    const double cu[] = {5.0, 5.0};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_point(m, TMP_PT));
    const char *text = slurp(TMP_PT);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "C1 "));
    TEST_ASSERT_NOT_NULL(strstr(text, "C2 "));

    double x[2] = {-1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_point(m, TMP_PT, x));
    double want[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, want, nullptr, nullptr, nullptr));
    SAME_D(want[0], x[0]);
    SAME_D(want[1], x[1]);
    remove(TMP_PT);
    jaos_model_free(m);
}
static void test_a_semicontinuous_column_round_trips_through_both_formats(void)
{
    const double cost[] = {1.0, 5.0};
    const double cl[]   = {2.0, 0.0};
    const double cu[]   = {10.0, 1.0};
    const double rl[]   = {1.0};
    const double ru[]   = {INFINITY};
    const int64_t as[]  = {0, 1, 2};
    const int64_t ai[]  = {0, 0};
    const double  av[]  = {1.0, 1.0};
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_semicontinuous(a, 0, true));

    const char *paths[2] = {TMP_MPS, TMP_LP};
    for (int k = 0; k < 2; k++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_write_mps(a, paths[k])
                                              : jaos_write_lp(a, paths[k]));
        jaos_model *b = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_read_mps(b, paths[k])
                                              : jaos_read_lp(b, paths[k]));
        assert_same_model(a, b);
        bool semi = false;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(b, 0, &semi));
        TEST_ASSERT_TRUE(semi);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(b, 1, &semi));
        TEST_ASSERT_FALSE(semi);
        jaos_model_free(b);
        remove(paths[k]);
    }

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(a, 0, 2.0, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_write_mps(a, TMP_MPS));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(a), "C1"));
    TEST_ASSERT_FALSE(file_exists(TMP_MPS));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(a, TMP_LP));
    remove(TMP_LP);
    jaos_model_free(a);
}

static void test_special_ordered_sets_round_trip_through_both_formats(void)
{
    const double cost[] = {-1.0, -1.0, -1.0};
    const double cl[]   = {0.0, 0.0, 0.0};
    const double cu[]   = {1.0, 1.0, 1.0};
    const double rl[]   = {-INFINITY};
    const double ru[]   = {10.0};
    const int64_t as[]  = {0, 1, 2, 3};
    const int64_t ai[]  = {0, 0, 0};
    const double  av[]  = {1.0, 1.0, 1.0};
    jaos_model *a = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(a, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));
    const int64_t s1[] = {0, 1};
    const double w1[] = {1.5, 2.5};
    const int64_t s2[] = {0, 1, 2};
    const double w2[] = {1.0, 2.0, 3.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(a, 1, 2, s1, w1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(a, 2, 3, s2, w2));

    const char *paths[2] = {TMP_MPS, TMP_LP};
    for (int k = 0; k < 2; k++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_write_mps(a, paths[k])
                                              : jaos_write_lp(a, paths[k]));
        jaos_model *b = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_read_mps(b, paths[k])
                                              : jaos_read_lp(b, paths[k]));
        TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
        assert_same_model(a, b);
        TEST_ASSERT_EQUAL_INT64(2, jaos_num_sos(b));
        for (int64_t s = 0; s < 2; s++) {
            int ta = 0, tb = 0;
            int64_t na = 0, nb = 0, ca[3], cb[3];
            double wa[3], wb[3];
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_sos(a, s, &ta, &na, ca, wa));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_sos(b, s, &tb, &nb, cb, wb));
            TEST_ASSERT_EQUAL_INT(ta, tb);
            TEST_ASSERT_EQUAL_INT64(na, nb);
            for (int64_t t = 0; t < na; t++) {
                TEST_ASSERT_EQUAL_INT64(ca[t], cb[t]);
                SAME_D(wa[t], wb[t]);
            }
        }
        jaos_model_free(b);
        remove(paths[k]);
    }
    jaos_model_free(a);
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
    RUN_TEST(test_a_certificate_file_carries_its_basis);
    RUN_TEST(test_read_basis_takes_an_optimum_file_too);
    RUN_TEST(test_read_basis_refuses_a_file_that_carries_none);
    RUN_TEST(test_an_unbounded_ray_round_trips);
    RUN_TEST(test_the_certificate_reader_refuses_what_is_not_one);
    RUN_TEST(test_a_solve_with_no_certificate_writes_nothing);
    RUN_TEST(test_a_basis_file_round_trips);
    RUN_TEST(test_a_basis_file_warm_starts_a_second_solve);
    RUN_TEST(test_the_file_carries_the_cards_the_basis_asks_for);
    RUN_TEST(test_a_slack_basis_writes_no_cards);
    RUN_TEST(test_free_round_trips_without_a_card_of_its_own);
    RUN_TEST(test_the_reader_takes_a_file_this_writer_would_not_write);
    RUN_TEST(test_the_basis_reader_takes_a_positional_name);
    RUN_TEST(test_each_basis_reader_guard_fires_on_its_own);
    RUN_TEST(test_the_basis_reader_takes_what_the_guards_leave);
    RUN_TEST(test_a_refused_basis_read_leaves_the_arrays_alone);
    RUN_TEST(test_the_basis_writer_refuses_without_a_basis);
    RUN_TEST(test_the_basis_writer_refuses_two_columns_of_a_name);
    RUN_TEST(test_a_refusals_basis_is_written_too);
    RUN_TEST(test_both_outputs_of_the_basis_reader_are_optional);
    RUN_TEST(test_the_basis_file_calls_reject_bad_arguments);
    RUN_TEST(test_both_model_writers_take_a_gz_name);
    RUN_TEST(test_a_plain_name_still_writes_text);
    RUN_TEST(test_the_answer_writers_take_a_gz_name);
    RUN_TEST(test_a_refused_compressed_write_leaves_nothing);
    RUN_TEST(test_a_point_file_round_trips);
    RUN_TEST(test_the_point_reader_takes_a_file_written_by_hand);
    RUN_TEST(test_each_point_reader_guard_fires_on_its_own);
    RUN_TEST(test_the_dual_half_runs_only_with_a_duals_file);
    RUN_TEST(test_the_point_writer_refuses_what_it_cannot_write);
    RUN_TEST(test_a_point_file_is_written_from_given_values);
    RUN_TEST(test_a_duals_file_round_trips);
    RUN_TEST(test_a_point_file_uses_positional_names);
    RUN_TEST(test_a_semicontinuous_column_round_trips_through_both_formats);
    RUN_TEST(test_special_ordered_sets_round_trip_through_both_formats);
    return UNITY_END();
}
