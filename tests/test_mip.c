/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
    return m;
}

static jaos_model *knapsack(void)
{
    const double cost[3] = { 5.0, 4.0, 3.0 }, cl[3] = { 0, 0, 0 };
    const double cu[3] = { 1.0, 1.0, 1.0 };
    const double rl[1] = { -INFINITY }, ru[1] = { 5.0 };
    const int64_t as[4] = { 0, 1, 2, 3 }, ai[3] = { 0, 0, 0 };
    const double av[3] = { 2.0, 3.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    return m;
}

static jaos_model *miqp(bool with_row)
{
    jaos_model *m = fresh();
    const double cost[2] = {-5.2, -2.6}, cl[2] = {0.0, 0.0}, cu[2] = {5.0, 5.0};
    const double rl[1] = {-INFINITY}, ru[1] = {3.0};
    const int64_t as[3] = {0, 1, 2}, as0[3] = {0, 0, 0}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, with_row ? 1 : 0, JAOS_MINIMIZE, 0.0, cost, cl, cu,
                     rl, ru, with_row ? 2 : 0, with_row ? as : as0, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 1, 2.0));
    return m;
}

static void test_a_quadratic_objective_branches_on_barrier_relaxations(void)
{
    jaos_model *m = miqp(true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -8.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, x[1]);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 1);
    jaos_check_report chk;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, 1e-6, &chk));
    TEST_ASSERT_TRUE(chk.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -8.0, chk.primal_objective);

    jaos_model *b = miqp(true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT64(jaos_work_units(m), jaos_work_units(b));
    double xb[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(b, xb, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(x, xb, sizeof x);
    jaos_model_free(b);
    jaos_model_free(m);

    m = miqp(false);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -8.2, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, x[1]);
    jaos_model_free(m);

    m = miqp(true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, JAOS_ALGORITHM_PRIMAL));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    jaos_model_free(m);
}

static uint64_t miqp_next(uint64_t *s)
{
    *s = *s * 6364136223846793005u + 1442695040888963407u;
    return *s >> 33;
}

static void test_a_quadratic_objective_matches_enumeration(void)
{
    constexpr int64_t N = 6;
    constexpr int64_t M = 3;
    constexpr int64_t POINTS = 729;
    uint64_t seed = 20260909u;
    for (int inst = 0; inst < 24; inst++) {
        const double scale = inst % 2 == 0 ? 1.0 : 1000.0;
        double cost[N], quad[N], cl[N], cu[N], rl[M], ru[M], dense[M][N];
        double av[M * N];
        int64_t ai[M * N], as[N + 1], pt[N];
        for (int64_t j = 0; j < N; j++) {
            cost[j] = scale * ((double)(int64_t)(miqp_next(&seed) % 21) - 10.0);
            quad[j] = scale * (double)(1 + (int64_t)(miqp_next(&seed) % 6));
            cl[j] = 0.0;
            cu[j] = 2.0;
        }
        for (int64_t i = 0; i < M; i++)
            for (int64_t j = 0; j < N; j++)
                dense[i][j] = (double)(int64_t)(miqp_next(&seed) % 7) - 3.0;
        for (int64_t i = 0; i < M; i++) {
            rl[i] = -INFINITY;
            ru[i] = (double)(int64_t)(miqp_next(&seed) % 8);
        }
        int64_t nz = 0;
        as[0] = 0;
        for (int64_t j = 0; j < N; j++) {
            for (int64_t i = 0; i < M; i++)
                if (dense[i][j] != 0.0) {
                    ai[nz] = i;
                    av[nz] = dense[i][j];
                    nz++;
                }
            as[j + 1] = nz;
        }
        double best = INFINITY;
        for (int64_t code = 0; code < POINTS; code++) {
            int64_t c = code;
            for (int64_t j = 0; j < N; j++) {
                pt[j] = c % 3;
                c /= 3;
            }
            bool ok = true;
            for (int64_t i = 0; ok && i < M; i++) {
                double a = 0.0;
                for (int64_t j = 0; j < N; j++)
                    a += dense[i][j] * (double)pt[j];
                if (a > ru[i])
                    ok = false;
            }
            if (!ok)
                continue;
            double v = 0.0;
            for (int64_t j = 0; j < N; j++) {
                const double xj = (double)pt[j];
                v += cost[j] * xj + 0.5 * quad[j] * xj * xj;
            }
            if (v < best)
                best = v;
        }
        TEST_ASSERT_TRUE(best < INFINITY);

        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, N, M, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                         nz, as, ai, av));
        for (int64_t j = 0; j < N; j++) {
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                                  jaos_set_col_quadratic(m, j, quad[j]));
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[N];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(m, x, nullptr, nullptr, nullptr));
        const double slack = 1e-6 * (1.0 + fabs(best));
        TEST_ASSERT_TRUE(obj <= best + slack);
        TEST_ASSERT_TRUE(obj >= best - slack);
        for (int64_t j = 0; j < N; j++) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, floor(x[j] + 0.5), x[j]);
            TEST_ASSERT_TRUE(x[j] >= -1e-6 && x[j] <= 2.0 + 1e-6);
        }
        for (int64_t i = 0; i < M; i++) {
            double a = 0.0;
            for (int64_t j = 0; j < N; j++)
                a += dense[i][j] * x[j];
            TEST_ASSERT_TRUE(a <= ru[i] + 1e-6);
        }
        jaos_model *b = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(b, N, M, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                         nz, as, ai, av));
        for (int64_t j = 0; j < N; j++) {
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(b, j, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                                  jaos_set_col_quadratic(b, j, quad[j]));
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
        TEST_ASSERT_EQUAL_INT64(jaos_work_units(m), jaos_work_units(b));
        double xb[N];
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(b, xb, nullptr, nullptr, nullptr));
        TEST_ASSERT_EQUAL_MEMORY(x, xb, sizeof x);
        jaos_model_free(b);
        jaos_model_free(m);
    }
}

static void test_a_quadratic_objective_breaks_the_symmetry(void)
{
    constexpr int64_t N = 6;
    const double cost[N] = {-10.0, -10.0, -10.0, -10.0, -10.0, -10.0};
    const double q[N] = {2.0, 4.0, 6.0, 8.0, 10.0, 12.0};
    double lo[N], hi[N], av[N];
    int64_t as[N + 1], ai[N];
    for (int64_t j = 0; j < N; j++) {
        lo[j] = 0.0;
        hi[j] = 1.0;
        as[j] = j;
        ai[j] = 0;
        av[j] = 1.0;
    }
    as[N] = N;
    const double rl[1] = {-INFINITY}, ru[1] = {3.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, 1, JAOS_MINIMIZE, 0.0, cost, lo, hi, rl, ru, N, as,
                     ai, av));
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_mip_report sym;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &sym));
    TEST_ASSERT_TRUE(sym.symmetry_generators > 0);
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, 1, JAOS_MINIMIZE, 0.0, cost, lo, hi, rl, ru, N, as,
                     ai, av));
    for (int64_t j = 0; j < N; j++) {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, j, q[j]));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.symmetry_generators);
    double obj = 0.0, x[N];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -24.0, obj);
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, j < 3 ? 1.0 : 0.0, x[j]);
    jaos_model_free(m);
}

static void test_an_infeasible_quadratic_model_says_so(void)
{
    const double cost[2] = {1.0, 1.0}, lo[2] = {0.0, 0.0}, hi[2] = {1.0, 1.0};
    const double rl[1] = {3.0}, ru[1] = {INFINITY};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, lo, hi, rl, ru, 2, as,
                     ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 1, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);

    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, lo, hi, rl, ru, 2, as,
                     ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 1, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_cut_never_shuts_out_every_point(void)
{
    constexpr int64_t N = 5;
    constexpr int64_t M = 3;
    const double cost[N] = {6.0, -1.0, 9.0, 8.0, 3.0};
    const double dense[M][N] = {
        {-3.0,  2.0,  2.0,  0.0,  2.0},
        { 3.0,  0.0,  2.0,  1.0,  3.0},
        { 0.0, -1.0, -1.0,  3.0,  3.0},
    };
    const double rl[M] = {-3.0, -INFINITY, 2.0};
    const double ru[M] = {-1.0, 4.0, 2.0};
    double lo[N], hi[N], av[M * N];
    int64_t ai[M * N], as[N + 1], nz = 0;
    for (int64_t j = 0; j < N; j++) {
        lo[j] = 0.0;
        hi[j] = 1.0;
    }
    as[0] = 0;
    for (int64_t j = 0; j < N; j++) {
        for (int64_t i = 0; i < M; i++)
            if (dense[i][j] != 0.0) {
                ai[nz] = i;
                av[nz] = dense[i][j];
                nz++;
            }
        as[j + 1] = nz;
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, M, JAOS_MINIMIZE, 0.0, cost, lo, hi, rl, ru, nz, as,
                     ai, av));
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[N];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 13.0, obj);
    const double want[N] = {1.0, 1.0, 0.0, 1.0, 0.0};
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, want[j], x[j]);
    jaos_model_free(m);
}

static void test_a_conflict_rests_only_on_rows_that_always_hold(void)
{
    constexpr int64_t N = 9;
    constexpr int64_t M = 3;
    const double cost[N] = {0.0, -8.0, 9.0, -1.0, 3.0, -2.0, -10.0, 10.0, -5.0};
    const double dense[M][N] = {
        { 2.0, -1.0,  3.0,  2.0,  0.0,  1.0, -3.0,  3.0,  0.0},
        { 1.0, -3.0,  2.0,  3.0,  2.0,  2.0,  0.0,  3.0, -1.0},
        {-1.0, -3.0,  1.0, -1.0, -2.0, -1.0, -3.0, -3.0,  1.0},
    };
    const double rl[M] = {3.0, 3.0, -7.0};
    const double ru[M] = {3.0, 4.0, -7.0};
    const double want[N] = {1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    double lo[N], hi[N], av[M * N];
    int64_t ai[M * N], as[N + 1], nz = 0;
    for (int64_t j = 0; j < N; j++) {
        lo[j] = 0.0;
        hi[j] = 1.0;
    }
    as[0] = 0;
    for (int64_t j = 0; j < N; j++) {
        for (int64_t i = 0; i < M; i++)
            if (dense[i][j] != 0.0) {
                ai[nz] = i;
                av[nz] = dense[i][j];
                nz++;
            }
        as[j + 1] = nz;
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, M, JAOS_MAXIMIZE, 0.0, cost, lo, hi, rl, ru, nz, as,
                     ai, av));
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 4));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[N];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -6.0, obj);
    for (int64_t j = 0; j < N; j++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, want[j], x[j]);
    jaos_model_free(m);
}

static void test_the_knapsack_finds_the_integer_optimum(void)
{
    jaos_model *m = knapsack();
    bool isint = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[2]);

    TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));

    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    TEST_ASSERT_TRUE(rep.has_incumbent);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, rep.incumbent);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, rep.bound);

    jaos_check_report ck;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, 1e-7, &ck));
    TEST_ASSERT_TRUE(ck.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ck.max_integrality_violation);
    x[2] = 0.5;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, 1e-7, &ck));
    TEST_ASSERT_FALSE(ck.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, ck.max_integrality_violation);

    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, false));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_TRUE(obj > 9.0 + 1e-6);
    jaos_model_free(m);
}

static void test_a_fractional_root_branches_to_the_integer_answer(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);

    double x1[2], x2[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x1, nullptr, nullptr, nullptr));
    const int64_t nodes = rep.nodes, work = jaos_work_units(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(nodes, rep.nodes);
    TEST_ASSERT_EQUAL_INT64(work, jaos_work_units(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x2, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    jaos_model_free(m);
}

static void test_an_integer_model_with_no_integer_point_is_infeasible(void)
{

    const double cost[1] = { 1.0 }, cl[1] = { 0.2 }, cu[1] = { 0.8 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_FALSE(rep.has_incumbent);

    TEST_ASSERT_EQUAL_INT64(0, rep.nodes);
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_mip_incumbent(m, x, nullptr));
    jaos_model_free(m);
}

static void test_a_work_limit_stops_the_tree_and_keeps_the_incumbent(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const int64_t full = jaos_work_units(m);
    TEST_ASSERT_TRUE(full > 0);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    double x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 4 * full + 1000));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_incumbent(m, x, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    jaos_model_free(m);
}

static void test_the_marks_ride_with_their_columns_and_copy(void)
{
    jaos_model *m = knapsack();
    const int64_t del[1] = { 0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, del));
    bool isint = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &isint));
    TEST_ASSERT_FALSE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    const double one = 1.0, zero = 0.0, inf = INFINITY;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 1, &one, &zero, &inf, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 2, &isint));
    TEST_ASSERT_FALSE(isint);
    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(c, 1, &isint));
    TEST_ASSERT_TRUE(isint);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_col_integer(m, 3, true));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_gap(m, -1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_gap(m, 0.0));
    jaos_model_free(c);
    jaos_model_free(m);
}

static void test_the_readers_and_writers_carry_the_marks(void)
{

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_mps(m, "tests/data/t4_int.mps"),
                                  jaos_model_error(m));
    bool a = false, b = false, c = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 0, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 1, &b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, 2, &c));
    TEST_ASSERT_TRUE(a && !b && c);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->col_upper[2]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.5, obj);
    double xz[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, xz, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(xz[0] == floor(xz[0]) && xz[2] == 1.0);

    const char *paths[2] = { "build/tm_tmp.mps", "build/tm_tmp.lp" };
    for (int k = 0; k < 2; k++) {
        jaos_model *back = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, k == 0 ? jaos_write_mps(m, paths[k])
                                              : jaos_write_lp(m, paths[k]));
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, k == 0 ? jaos_read_mps(back, paths[k])
                                                      : jaos_read_lp(back, paths[k]),
                                      jaos_model_error(back));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(back, 0, &a));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(back, 1, &b));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(back, 2, &c));
        TEST_ASSERT_TRUE(a && !b && c);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(back));
        double o2 = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(back, &o2));
        TEST_ASSERT_EQUAL_MEMORY(&obj, &o2, sizeof obj);
        jaos_model_free(back);
        remove(paths[k]);
    }

    jaos_model *l = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_lp(l, "tests/data/g_int.lp"),
                                  jaos_model_error(l));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(l, 0, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(l, 1, &b));
    TEST_ASSERT_TRUE(a && b);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, l->col_upper[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(l));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(l, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, obj);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_lp(l, "tests/data/el_int_unknown.lp"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(l), "not a variable"));
    jaos_model_free(l);
    jaos_model_free(m);
}

static void test_without_cuts_or_dive_the_tree_branches_to_the_same_answer(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_TRUE(rep.nodes >= 2);
    const int64_t dived = rep.nodes;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 2);
    (void)dived;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_cut_rounds_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    jaos_model_free(m);

    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 3);
    jaos_model_free(m);
}

static void test_a_cut_over_continuous_columns_keeps_the_integer_optimum(void)
{
    const double cost[3] = { 3.0, 2.0, 1.0 }, cl[3] = { 0, 0, 0 };
    const double cu[3] = { 3.0, 3.0, 1.0 };
    const double rl[2] = { -INFINITY, -INFINITY }, ru[2] = { 5.5, 4.5 };
    const int64_t as[4] = { 0, 2, 4, 6 }, ai[6] = { 0, 1, 0, 1, 0, 1 };
    const double av[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 2, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         6, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
        if (pass == 1)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 8.5, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, x[2]);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0)
            TEST_ASSERT_TRUE(rep.cuts >= 1);
        else
            TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
        jaos_check_report ck;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_check_solution(m, x, nullptr, 1e-7, &ck));
        TEST_ASSERT_TRUE(ck.primal_feasible);
        jaos_model_free(m);
    }
}

static void test_the_rounding_heuristic_takes_the_relaxations_neighbour(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[3] = { -INFINITY, -INFINITY, -INFINITY };
    const double ru[3] = { 3.6, 2.2, 1.4 };
    const int64_t as[3] = { 0, 2, 4 }, ai[4] = { 0, 1, 0, 2 };
    const double av[4] = { 1.0, 1.0, 1.0, 1.0 };
    for (int on = 1; on >= 0; on--) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 3, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         4, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, on != 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[2], ra[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, ra, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);

        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, ra[0]);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (on) {
            TEST_ASSERT_TRUE(rep.heuristic_points >= 1);
            TEST_ASSERT_EQUAL_INT64(1, rep.first_incumbent_node);
        } else {
            TEST_ASSERT_EQUAL_INT64(0, rep.heuristic_points);
            TEST_ASSERT_TRUE(rep.first_incumbent_node >= 2);
        }
        jaos_check_report ck;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_check_solution(m, x, nullptr, 1e-7, &ck));
        TEST_ASSERT_TRUE(ck.primal_feasible);
        jaos_model_free(m);
    }
}

static void test_an_infeasible_rounding_is_not_taken(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.heuristic_points);
    jaos_model_free(m);
}

static char g_log[4096];
static void capture_log(void *user, jaos_log_level level, const char *line)
{
    (void)level; (void)user;
    const size_t have = strlen(g_log);
    snprintf(g_log + have, sizeof g_log - have, "%s\n", line);
}

static void test_the_tree_logs_its_start_root_and_end(void)
{
    jaos_model *m = knapsack();
    double x1[3], x2[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x1, nullptr, nullptr, nullptr));
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x2, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    TEST_ASSERT_NOT_NULL(strstr(g_log, "branch and bound: 3 integer columns of 3"));
    TEST_ASSERT_NOT_NULL(strstr(g_log, "root: relaxation"));
    TEST_ASSERT_NOT_NULL(strstr(g_log, "branch and bound: optimal after"));
    jaos_model_free(m);
}

typedef struct {
    int calls;
    jaos_incumbent last;
    double x[2];
    jaos_callback_action answer;
} seen_t;

static jaos_callback_action see_incumbent(const jaos_incumbent *inc, void *user)
{
    seen_t *s = user;
    s->calls++;
    s->last = *inc;
    s->x[0] = inc->col_value[0];
    s->x[1] = inc->col_value[1];
    return s->answer;
}


typedef struct {
    int calls, integral_calls, rejected;
    int64_t steer_to, solver_choice;
    int depth1_calls, depth1_integral;
    jaos_callback_action answer;
    jaos_status add_status;
    bool bad_row;
} node_seen;

static jaos_callback_action lazy_pair(jaos_node *ev, void *user)
{
    node_seen *s = user;
    s->calls++;
    if (ev->integral) {
        s->integral_calls++;
        if (ev->col_value[0] + ev->col_value[1] > 1.5) {
            const int64_t idx[2] = {0, 1};
            const double val[2] = {1.0, 1.0};
            s->add_status = jaos_node_add_row(ev, 2, idx, val, -INFINITY, 1.0);
            s->rejected++;
        }
    }
    return s->answer;
}

static void test_a_lazy_row_from_the_node_callback_rejects_the_point(void)
{
    const double c[3] = {-2.0, -2.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[1] = {-INFINITY}, ru[1] = {2.0};
    const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
    const double av[3] = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    node_seen seen = { .answer = JAOS_CALLBACK_CONTINUE, .add_status = JAOS_OK };
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_node_callback(nullptr, lazy_pair, &seen));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_node_callback(m, lazy_pair, &seen));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] + x[1] <= 1.0 + 1e-9);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[2]);
    TEST_ASSERT_TRUE(seen.calls >= 1);
    TEST_ASSERT_TRUE(seen.integral_calls >= 1);
    TEST_ASSERT_TRUE(seen.rejected >= 1);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, seen.add_status);

    seen.answer = JAOS_CALLBACK_STOP;
    seen.calls = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, seen.calls);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_node_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -4.0, obj);
    jaos_model_free(m);
}

static jaos_callback_action cover_cut(jaos_node *ev, void *user)
{
    node_seen *s = user;
    s->calls++;
    if (ev->node == 1 && !ev->integral && !s->bad_row) {
        const int64_t idx[3] = {0, 1, 2};
        const double val[3] = {1.0, 1.0, 1.0};
        s->add_status = jaos_node_add_row(ev, 3, idx, val, -INFINITY, 1.0);
    }
    if (s->bad_row) {
        const int64_t idx[1] = {7};
        const double val[1] = {1.0};
        if (jaos_node_add_row(ev, 1, idx, val, 0.0, 1.0) != JAOS_ERR_INVALID_INPUT ||
            jaos_node_add_row(ev, 1, idx, val, 2.0, 1.0) != JAOS_ERR_INVALID_INPUT ||
            jaos_node_add_row(nullptr, 0, nullptr, nullptr, 0.0, 1.0) != JAOS_ERR_INVALID_INPUT)
            return JAOS_CALLBACK_STOP;
    }
    return JAOS_CALLBACK_CONTINUE;
}

static jaos_model *cover_model(void)
{
    const double c[3] = {-3.0, -2.5, -2.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[1] = {-INFINITY}, ru[1] = {3.0};
    const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
    const double av[3] = {2.0, 2.0, 2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
    return m;
}

static void test_a_user_cut_from_the_node_callback_closes_the_root(void)
{
    int64_t nodes[2] = {0, 0};
    for (int on = 0; on < 2; on++) {
        jaos_model *m = cover_model();
        node_seen seen = { .add_status = JAOS_OK };
        if (on)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_node_callback(m, cover_cut, &seen));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        nodes[on] = rep.nodes;
        if (on) {
            TEST_ASSERT_EQUAL_INT(JAOS_OK, seen.add_status);
            TEST_ASSERT_TRUE(seen.calls >= 2);
        }
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(nodes[0] > 1);
    TEST_ASSERT_EQUAL_INT64(1, nodes[1]);

    jaos_model *m = cover_model();
    node_seen seen = { .bad_row = true };
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_node_callback(m, cover_cut, &seen));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static jaos_model *steer_model(void)
{
    const double c[3] = {-3.0, -1.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[2] = {-INFINITY, -INFINITY}, ru[2] = {3.0, 3.0};
    const int64_t as[4] = {0, 2, 3, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {2.0, 2.0, 2.0, 2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
    return m;
}

static jaos_callback_action steer_branch(jaos_node *ev, void *user)
{
    node_seen *s = user;
    s->calls++;
    if (ev->depth == 0 && !ev->integral) {
        s->solver_choice = ev->branch_col;
        s->steer_to = ev->branch_col == 1 ? 2 : 1;
        ev->branch_col = s->steer_to;
    }
    if (ev->depth == 1) {
        s->depth1_calls++;
        const double v = ev->col_value[s->steer_to];
        if (fabs(v - floor(v + 0.5)) <= 1e-9)
            s->depth1_integral++;
    }
    return JAOS_CALLBACK_CONTINUE;
}

static void test_the_node_callback_chooses_the_branching_column(void)
{
    jaos_model *m = steer_model();
    node_seen seen = { .steer_to = -1, .solver_choice = -1 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_node_callback(m, steer_branch, &seen));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_TRUE(seen.solver_choice == 1 || seen.solver_choice == 2);
    TEST_ASSERT_TRUE(seen.steer_to != seen.solver_choice);
    TEST_ASSERT_TRUE(seen.depth1_calls >= 1);
    TEST_ASSERT_EQUAL_INT(seen.depth1_calls, seen.depth1_integral);
    jaos_model_free(m);
}

static jaos_model *neighbour_model(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[3] = { -INFINITY, -INFINITY, -INFINITY };
    const double ru[3] = { 3.6, 2.2, 1.4 };
    const int64_t as[3] = { 0, 2, 4 }, ai[4] = { 0, 1, 0, 2 };
    const double av[4] = { 1.0, 1.0, 1.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    return m;
}

static void test_a_node_limit_stops_with_the_incumbent_the_callback_saw(void)
{
    jaos_model *m = neighbour_model();
    seen_t seen = { .answer = JAOS_CALLBACK_CONTINUE };
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_node_limit(m, -1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_incumbent_callback(m, see_incumbent, &seen));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NODE_LIMIT, jaos_status_of(m));
    TEST_ASSERT_EQUAL_STRING("node limit reached",
                             jaos_solve_status_str(JAOS_SOLVE_NODE_LIMIT));
    double x[2], obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_incumbent(m, x, &obj));
    TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
    TEST_ASSERT_EQUAL_INT(1, seen.calls);
    TEST_ASSERT_EQUAL_INT64(1, seen.last.node);
    TEST_ASSERT_TRUE(seen.last.by_rounding);
    TEST_ASSERT_EQUAL_INT64(2, seen.last.num_col);
    TEST_ASSERT_TRUE(seen.x[0] == 2.0 && seen.x[1] == 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, seen.last.objective);
    TEST_ASSERT_TRUE(seen.last.bound >= 3.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    TEST_ASSERT_TRUE(rep.has_incumbent);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(m, 0));
    seen.answer = JAOS_CALLBACK_STOP;
    seen.calls = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, seen.calls);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.has_incumbent);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_incumbent_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
    jaos_model_free(m);
}

static void test_both_branching_rules_reach_the_same_optimum(void)
{
    const jaos_branching rules[2] = { JAOS_BRANCH_MOST_FRACTIONAL,
                                      JAOS_BRANCH_PSEUDOCOST };
    for (int r = 0; r < 2; r++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_branching(m, rules[r]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_model_free(m);

        m = neighbour_model();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_branching(m, rules[r]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 1.0);
        jaos_model_free(m);
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_branching(m, (jaos_branching)7));
    jaos_model_free(m);
}

static void test_a_fractional_bound_on_an_integer_column_is_rounded_inward(void)
{
    const double cost[1] = { -1.0 }, cl[1] = { 1.2 }, cu[1] = { 2.8 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] == 2.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);

    double lo = 0.0, hi = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, 0, &lo, &hi));
    TEST_ASSERT_EQUAL_DOUBLE(1.2, lo);
    TEST_ASSERT_EQUAL_DOUBLE(2.8, hi);
    jaos_model_free(m);
}

static void test_strong_branching_probes_are_counted_and_change_no_answer(void)
{
    int64_t solves0 = 0, nodes0 = 0, work0 = 0;
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, pass == 0 ? 0 : 8));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            TEST_ASSERT_EQUAL_INT64(rep.nodes, rep.lp_solves);
            solves0 = rep.lp_solves;
            nodes0 = rep.nodes;
            work0 = jaos_work_units(m);
        } else {
            TEST_ASSERT_TRUE(rep.lp_solves > rep.nodes);
            TEST_ASSERT_TRUE(rep.lp_solves > solves0);
            TEST_ASSERT_TRUE(rep.nodes <= nodes0);
            TEST_ASSERT_TRUE(jaos_work_units(m) > work0 || rep.nodes < nodes0);
        }

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_reliability_set);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        jaos_model_free(m);
    }
}

static void test_a_capped_probe_reaches_the_same_optimum(void)
{
    const double caps[3] = { 0.5, 0.0, 4.0 };
    for (int c = 0; c < 3; c++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, 8));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_cap(m, caps[c]));
        TEST_ASSERT_TRUE(m->cfg.mip_probe_cap_set);
        TEST_ASSERT_EQUAL_DOUBLE(caps[c], m->cfg.mip_probe_cap);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.lp_solves > rep.nodes);
        jaos_model_free(m);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_cap(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_probe_cap_set);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_probe_cap(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_probe_cap(m, INFINITY));
    TEST_ASSERT_FALSE(m->cfg.mip_probe_cap_set);
    jaos_model_free(m);
}

static void test_every_dive_child_rule_reaches_the_same_optimum(void)
{
    const jaos_dive_child rules[4] = { JAOS_DIVE_NEARER, JAOS_DIVE_UP,
                                       JAOS_DIVE_DOWN, JAOS_DIVE_PSEUDOCOST };
    for (int r = 0; r < 4; r++) {
        jaos_model *m = knapsack();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_child(m, rules[r]));
        TEST_ASSERT_EQUAL_INT((int)rules[r], m->cfg.mip_dive_child);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.nodes >= 2);
        if (r == 3) {
            TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                                  jaos_set_mip_dive_child(m, (jaos_dive_child)7));
            TEST_ASSERT_EQUAL_INT((int)JAOS_DIVE_PSEUDOCOST, m->cfg.mip_dive_child);
        }
        jaos_model_free(m);
    }
}

static void test_cuts_below_the_root_reach_the_same_optimum(void)
{
    const double cost[5] = { 10.0, 13.0, 7.0, 9.0, 5.0 };
    const double cl[5] = { 0, 0, 0, 0, 0 }, cu[5] = { 1, 1, 1, 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 8.0 };
    const int64_t as[6] = { 0, 1, 2, 3, 4, 5 }, ai[5] = { 0, 0, 0, 0, 0 };
    const double av[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 };
    const int64_t depths[3] = { 0, 1, 100 };
    for (int d = 0; d < 3; d++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0, cuts1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = fresh();
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_load_lp(m, 5, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                             5, as, ai, av));
            for (int64_t j = 0; j < 5; j++)
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, depths[d]));
            TEST_ASSERT_TRUE(m->cfg.mip_cut_depth_set);
            TEST_ASSERT_EQUAL_INT64(depths[d], m->cfg.mip_cut_depth);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            TEST_ASSERT_TRUE(rep.nodes >= 2);
            if (depths[d] == 0)
                TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
            if (pass == 0) {
                nodes1 = rep.nodes;
                cuts1 = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0 && x1[2] == 0.0 &&
                         x1[3] == 0.0 && x1[4] == 0.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_cut_depth_set);
    jaos_model_free(m);
}

static jaos_model *knapsack5(void)
{
    const double cost[5] = { 10.0, 13.0, 7.0, 9.0, 5.0 };
    const double cl[5] = { 0, 0, 0, 0, 0 }, cu[5] = { 1, 1, 1, 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 8.0 };
    const int64_t as[6] = { 0, 1, 2, 3, 4, 5 }, ai[5] = { 0, 0, 0, 0, 0 };
    const double av[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     5, as, ai, av));
    for (int64_t j = 0; j < 5; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_dropping_slack_cuts_keeps_the_optimum(void)
{
    for (int drop = 0; drop < 2; drop++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_drop(m, drop == 1));
            TEST_ASSERT_EQUAL_INT(drop == 0, m->cfg.mip_no_cut_drop);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
}

static void test_probing_at_the_root_only_reaches_the_same_optimum(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_reliability(m, 8));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_depth(m, 0));
    TEST_ASSERT_TRUE(m->cfg.mip_probe_depth_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.lp_solves > rep.nodes);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probe_depth(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_probe_depth_set);
    jaos_model_free(m);
}

static void test_the_solution_pool_holds_the_best_points_best_first(void)
{
    const double w[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 }, c[5] = { 10, 13, 7, 9, 5 };
    for (int size = 3; size >= 1; size -= 2) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pool_size(m, size));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        int64_t held = -1;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_pool_count(m, &held));
        TEST_ASSERT_TRUE(held >= 1 && held <= size);
        double best[5], inc[5], incobj = 0.0, prev = INFINITY;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_incumbent(m, inc, &incobj));
        for (int64_t k = 0; k < held; k++) {
            double x[5], obj = 0.0, weight = 0.0, value = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_pool_solution(m, k, x, &obj));
            for (int j = 0; j < 5; j++) {
                TEST_ASSERT_TRUE(x[j] == 0.0 || x[j] == 1.0);
                weight += w[j] * x[j];
                value += c[j] * x[j];
            }
            TEST_ASSERT_TRUE(weight <= 8.0);
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, value, obj);
            TEST_ASSERT_TRUE(obj <= prev);
            prev = obj;
            if (k == 0) {
                memcpy(best, x, sizeof best);
                TEST_ASSERT_EQUAL_MEMORY(inc, x, sizeof inc);
                TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            } else {
                TEST_ASSERT_TRUE(memcmp(best, x, sizeof best) != 0);
            }
        }
        if (size == 1)
            TEST_ASSERT_EQUAL_INT64(1, held);
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                              jaos_mip_pool_solution(m, held, nullptr, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_pool_size(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pool_size(m, -1));
        TEST_ASSERT_EQUAL_INT64(0, m->cfg.mip_pool_size);
        jaos_model_free(m);
    }
}

static void test_a_cover_cut_closes_the_knapsack_at_the_root(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
    TEST_ASSERT_TRUE(m->cfg.mip_cover_rounds_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0, x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0);
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    TEST_ASSERT_TRUE(rep.cuts >= 1);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.nodes >= 2);
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_cover_rounds_set);
    jaos_model_free(m);

    m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 2, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    jaos_model_free(m);
}

static void test_a_cap_on_a_nodes_cuts_keeps_the_optimum(void)
{
    int64_t cuts_uncapped = -1;
    for (int cap = 0; cap < 2; cap++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0, cuts1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, cap));
            TEST_ASSERT_TRUE(m->cfg.mip_node_cut_cap_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes1 = rep.nodes;
                cuts1 = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
        if (cap == 0)
            cuts_uncapped = cuts1;
        else
            TEST_ASSERT_TRUE(cuts1 <= cuts_uncapped);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_node_cut_cap_set);
    jaos_model_free(m);
}

static void test_a_stalled_root_round_is_the_last(void)
{
    const double stall[3] = { 0.0, 1.0, 0.001 };
    int64_t cuts[3] = { 0 }, nodes[3] = { 0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 5));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_stall(m, stall[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_cut_stall_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                cuts[arm] = rep.cuts;
                nodes[arm] = rep.nodes;
            } else {
                TEST_ASSERT_EQUAL_INT64(cuts[arm], rep.cuts);
                TEST_ASSERT_EQUAL_INT64(nodes[arm], rep.nodes);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_EQUAL_INT64(3, cuts[0]);
    TEST_ASSERT_EQUAL_INT64(1, nodes[0]);
    TEST_ASSERT_EQUAL_INT64(1, cuts[1]);
    TEST_ASSERT_TRUE(nodes[1] > 1);
    TEST_ASSERT_EQUAL_INT64(3, cuts[2]);
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_stall(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_cut_stall_set);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_cut_stall(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_cut_stall(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_cut_stall(m, -INFINITY));
    jaos_model_free(m);
}

static void test_a_stalled_round_ends_the_cuts_under_it(void)
{
    int64_t cuts[2] = { 0 };
    for (int arm = 0; arm < 2; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_node_cut_stall(m, arm == 0 ? 0.0 : 1.0));
            TEST_ASSERT_TRUE(m->cfg.mip_node_cut_stall_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                cuts[arm] = rep.cuts;
                nodes1 = rep.nodes;
            } else {
                TEST_ASSERT_EQUAL_INT64(cuts[arm], rep.cuts);
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_TRUE(cuts[1] < cuts[0]);
    jaos_model *m = knapsack5();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_stall(m, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_TRUE(rep.cuts > 0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_stall(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_node_cut_stall_set);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_node_cut_stall(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_node_cut_stall(m, INFINITY));
    jaos_model_free(m);
}

static void test_root_cuts_may_leave_below_a_node(void)
{
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0, cuts1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 1));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_cut_depth(m, arm == 1 ? 100 : 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_root_cut_drop(m, 1));
            TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop_set);
            TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop);
            if (arm == 2)
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_drop(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            TEST_ASSERT_TRUE(rep.cuts >= 1);
            if (pass == 0) {
                nodes1 = rep.nodes;
                cuts1 = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_root_cut_drop(m, 0));
    TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop_set);
    TEST_ASSERT_FALSE(m->cfg.mip_root_cut_drop);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_root_cut_drop(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_root_cut_drop_set);
    jaos_model_free(m);
}

static jaos_model *knapsack_lift(void)
{
    const double cost[4] = { 10.0, 10.0, 6.0, 15.0 };
    const double cl[4] = { 0, 0, 0, 0 }, cu[4] = { 1, 1, 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 10.0 };
    const int64_t as[5] = { 0, 1, 2, 3, 4 }, ai[4] = { 0, 0, 0, 0 };
    const double av[4] = { 4.0, 4.0, 3.0, 8.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     4, as, ai, av));
    for (int64_t j = 0; j < 4; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_a_lifted_cover_closes_what_the_extended_cover_leaves(void)
{
    for (int lift = 0; lift < 2; lift++) {
        jaos_model *m = knapsack_lift();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 1));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_lift(m, lift));
        TEST_ASSERT_TRUE(m->cfg.mip_cover_lift_set);
        TEST_ASSERT_EQUAL_INT(lift == 1, m->cfg.mip_cover_lift);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[4];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_TRUE(x[0] == 1.0 && x[1] == 1.0 && x[2] == 0.0 && x[3] == 0.0);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.cuts >= 1);
        if (lift)
            TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
        else
            TEST_ASSERT_TRUE(rep.nodes >= 2);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_lift(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_cover_lift_set);
        jaos_model_free(m);
    }
}

static jaos_model *halved_row(void)
{
    const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
    const double cu[2] = { INFINITY, INFINITY };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_a_mir_cut_closes_the_halved_row_at_the_root(void)
{
    for (int rounds = 1; rounds >= 0; rounds--) {
        jaos_model *m = halved_row();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, rounds));
        TEST_ASSERT_TRUE(m->cfg.mip_mir_rounds_set);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (rounds == 1) {
            TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
            TEST_ASSERT_TRUE(rep.cuts >= 1);
        } else {
            TEST_ASSERT_TRUE(rep.nodes >= 2);
            TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
        }
        jaos_model_free(m);
    }
    double x1[5], x2[5];
    int64_t nodes1 = 0, cuts1 = 0;
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 2));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            nodes1 = rep.nodes;
            cuts1 = rep.cuts;
        } else {
            TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_mir_rounds_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
}

static void test_a_backtracking_dive_reaches_the_same_optimum(void)
{
    const int64_t times[3] = { 0, 1, 1000 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, times[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_backtrack_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, arm * 1000));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_limit(m, 2));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NODE_LIMIT, jaos_status_of(m));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_TRUE(rep.bound <= 24.8 + 1e-9 && rep.bound >= 23.0 - 1e-9);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_dive_backtrack_set);
        jaos_model_free(m);
    }
}

static void test_mir_cuts_at_the_nodes_keep_the_optimum(void)
{
    int64_t cuts_off = 0, cuts_on = 0;
    for (int on = 0; on < 2; on++) {
        jaos_model *m = halved_row();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_cut_cap(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_mir(m, on));
        TEST_ASSERT_TRUE(m->cfg.mip_node_mir_set);
        TEST_ASSERT_EQUAL_INT(on == 1, m->cfg.mip_node_mir);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (on == 0)
            cuts_off = rep.cuts;
        else
            cuts_on = rep.cuts;
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(cuts_on >= cuts_off);
    double x1[5], x2[5];
    int64_t nodes1 = 0, cuts1 = 0;
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 100));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_mir(m, 1));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            nodes1 = rep.nodes;
            cuts1 = rep.cuts;
        } else {
            TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            TEST_ASSERT_EQUAL_INT64(cuts1, rep.cuts);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_node_mir(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_node_mir_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
    TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
}

static void test_a_dive_bounded_by_the_gap_reaches_the_same_optimum(void)
{
    const double frac[3] = { 0.01, 1.0, 0.01 };
    const int64_t count[3] = { 0, 0, 2 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(m, count[arm]));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_gap(m, frac[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_gap_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }

    int64_t canary[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *c = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(c, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(c, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_backtrack(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_fix(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_mip_dive_gap(c, arm == 0 ? 1e-12 : 1e12));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(c));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(c, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(c, &rep));
        canary[arm] = rep.nodes;
        jaos_model_free(c);
    }
    TEST_ASSERT_TRUE(canary[0] != canary[1]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_dive_gap(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_dive_gap(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_dive_gap(m, -INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_gap(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_dive_gap_set);
    jaos_model_free(m);
}

static jaos_model *aggregate_pair(void)
{
    const double cost[3] = { 3.0, 2.0, 0.0 };
    const double cl[3] = { 0, 0, 0 }, cu[3] = { 3.0, 3.0, 10.0 };
    const double rl[3] = { -INFINITY, -INFINITY, -INFINITY };
    const double ru[3] = { 0.0, 3.0, 4.0 };

    const int64_t as[4] = { 0, 2, 4, 6 };
    const int64_t ai[6] = { 0, 2, 1, 2, 0, 1 };
    const double av[6] = { 2.0, 1.0, 2.0, 1.0, -1.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     6, as, ai, av));
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_an_aggregated_mir_cut_closes_what_one_row_leaves(void)
{
    int64_t nodes[2] = { 0, 0 }, cuts[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        double x1[3], x2[3];
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = aggregate_pair();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 4));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_aggregate(m, arm));
            TEST_ASSERT_TRUE(m->cfg.mip_mir_aggregate_set);
            TEST_ASSERT_EQUAL_INT64(arm, m->cfg.mip_mir_aggregate);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes[arm] = rep.nodes;
                cuts[arm] = rep.cuts;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes[arm], rep.nodes);
                TEST_ASSERT_EQUAL_INT64(cuts[arm], rep.cuts);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_EQUAL_INT64(0, cuts[0]);
    TEST_ASSERT_TRUE(nodes[0] > 1);
    TEST_ASSERT_TRUE(cuts[1] >= 1);
    TEST_ASSERT_EQUAL_INT64(1, nodes[1]);

    jaos_model *m = aggregate_pair();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_aggregate(m, 3));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_aggregate(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_mir_aggregate_set);
    jaos_model_free(m);
}

static void test_the_dive_heuristic_finds_the_first_incumbent(void)
{
    const int64_t solves[3] = { 0, 1, 5 };
    int64_t first[3] = { 0, 0, 0 }, points[3] = { 0, 0, 0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[2], x2[2];
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = neighbour_model();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_dive_heuristic(m, solves[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_heuristic_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr, nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                first[arm] = rep.first_incumbent_node;
                points[arm] = rep.heuristic_points;
            } else {
                TEST_ASSERT_EQUAL_INT64(first[arm], rep.first_incumbent_node);
                TEST_ASSERT_EQUAL_INT64(points[arm], rep.heuristic_points);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 2.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_EQUAL_INT64(0, points[0]);
    TEST_ASSERT_TRUE(first[0] > 1);
    TEST_ASSERT_EQUAL_INT64(points[1], points[0]);
    TEST_ASSERT_EQUAL_INT64(1, points[2]);
    TEST_ASSERT_EQUAL_INT64(1, first[2]);

    jaos_model *m = knapsack5();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 20));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_dive_heuristic_set);
    jaos_model_free(m);
}

static void test_the_dive_heuristic_runs_below_the_root(void)
{
    const int64_t depth[3] = { 0, 1, 20 };
    int64_t solves[3] = { 0, 0, 0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 20));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_dive_heuristic_depth(m, depth[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_heuristic_depth_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr,
                              nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes1 = rep.nodes;
                solves[arm] = rep.lp_solves;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(solves[arm], rep.lp_solves);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }

    TEST_ASSERT_TRUE(solves[2] > solves[0]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic_depth(m, 3));
    TEST_ASSERT_TRUE(m->cfg.mip_dive_heuristic_depth_set);
    TEST_ASSERT_EQUAL_INT64(3, m->cfg.mip_dive_heuristic_depth);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic_depth(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_dive_heuristic_depth_set);
    jaos_model_free(m);
}

static void test_rins_searches_the_incumbents_neighbourhood(void)
{
    const int64_t budget[2] = { 0, 30 };
    int64_t solves[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_rins(m, budget[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_rins_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr,
                              nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes1 = rep.nodes;
                solves[arm] = rep.lp_solves;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(solves[arm], rep.lp_solves);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }

    TEST_ASSERT_TRUE(solves[1] > solves[0]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_rins(m, 5));
    TEST_ASSERT_TRUE(m->cfg.mip_rins_set);
    TEST_ASSERT_EQUAL_INT64(5, m->cfg.mip_rins);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_rins(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_rins_set);
    jaos_model_free(m);
}

static void test_a_dive_bounded_by_the_degradation_keeps_the_optimum(void)
{
    const double frac[3] = { 0.0, 0.01, 1.0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(m, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_mip_dive_degrade(m, frac[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_dive_degrade_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr,
                              nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                nodes1 = rep.nodes;
            else
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    int64_t canary[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *c = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(c, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(c, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive(c, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_mip_dive_degrade(c, arm == 0 ? 1e-12 : 1e12));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(c));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(c, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(c, &rep));
        canary[arm] = rep.nodes;
        jaos_model_free(c);
    }
    TEST_ASSERT_TRUE(canary[0] != canary[1]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_mip_dive_degrade(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_mip_dive_degrade(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_degrade(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_dive_degrade_set);
    jaos_model_free(m);
}

static void test_the_feasibility_pump_finds_a_point_at_the_root(void)
{
    const int64_t rounds[3] = { 0, 5, 20 };
    int64_t first[3] = { 0, 0, 0 };
    for (int arm = 0; arm < 3; arm++) {
        double x1[5], x2[5];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = knapsack5();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, rounds[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_feaspump_set);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr,
                              nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes1 = rep.nodes;
                first[arm] = rep.first_incumbent_node;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(first[arm], rep.first_incumbent_node);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }

    TEST_ASSERT_TRUE(first[0] > 1);
    TEST_ASSERT_EQUAL_INT64(1, first[1]);
    TEST_ASSERT_EQUAL_INT64(1, first[2]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 7));
    TEST_ASSERT_TRUE(m->cfg.mip_feaspump_set);
    TEST_ASSERT_EQUAL_INT64(7, m->cfg.mip_feaspump);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_feaspump_set);
    jaos_model_free(m);
}

static jaos_model *interior_pair(void)
{
    const double cost[2] = { 1.0, 1.0 };
    const double cl[2] = { 0, 0 }, cu[2] = { 4, 4 };
    const double rl[4] = { 6.0, -INFINITY, -INFINITY, -INFINITY };
    const double ru[4] = { INFINITY, 8.0, 1.0, 1.0 };
    const int64_t as[3] = { 0, 4, 8 };
    const int64_t ai[8] = { 0, 1, 2, 3, 0, 1, 2, 3 };
    const double av[8] = { 4.0, 4.0, 1.0, -1.0, 4.0, 4.0, -1.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 4, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     8, as, ai, av));
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 20));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_obj(m, 0.0));
    return m;
}

static void test_the_general_pump_reaches_a_general_integer_point(void)
{
    int64_t first[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) {
        double x1[2], x2[2];
        int64_t nodes1 = 0;
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = interior_pair();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_general(m, arm));
            TEST_ASSERT_TRUE(m->cfg.mip_pump_general_set);
            TEST_ASSERT_EQUAL_INT(arm == 1, m->cfg.mip_pump_general);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr,
                              nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0) {
                nodes1 = rep.nodes;
                first[arm] = rep.first_incumbent_node;
            } else {
                TEST_ASSERT_EQUAL_INT64(nodes1, rep.nodes);
                TEST_ASSERT_EQUAL_INT64(first[arm], rep.first_incumbent_node);
            }
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(x1[0] == 1.0 && x1[1] == 1.0);
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_TRUE(first[0] > 1);
    TEST_ASSERT_EQUAL_INT64(1, first[1]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_general(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_pump_general_set);
    jaos_model_free(m);
}

static jaos_callback_action first_incumbent_objective(const jaos_incumbent *inc,
                                                      void *user)
{
    double *first = user;
    if (isnan(*first))
        *first = inc->objective;
    return JAOS_CALLBACK_CONTINUE;
}

static void test_the_objective_pump_finds_a_better_root_point(void)
{
    const double decay[2] = { 0.0, 0.9 };
    double root[2] = { NAN, NAN };
    for (int arm = 0; arm < 2; arm++) {
        double x1[5], x2[5];
        for (int pass = 0; pass < 2; pass++) {
            const double cost[5] = { 10.0, 13.0, 7.0, 6.0, 4.0 };
            const double cl[5] = { 0, 0, 0, 0, 0 }, cu[5] = { 1, 1, 1, 1, 1 };
            const double rl[1] = { -INFINITY }, ru[1] = { 8.0 };
            const int64_t as[6] = { 0, 1, 2, 3, 4, 5 };
            const int64_t ai[5] = { 0, 0, 0, 0, 0 };
            const double av[5] = { 3.0, 5.0, 2.0, 4.0, 2.0 };
            jaos_model *m = fresh();
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_load_lp(m, 5, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl,
                             ru, 5, as, ai, av));
            for (int64_t j = 0; j < 5; j++)
                TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 20));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_obj(m, decay[arm]));
            TEST_ASSERT_TRUE(m->cfg.mip_pump_obj_set);
            double seen = NAN;
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_set_incumbent_callback(m, first_incumbent_objective,
                                            &seen));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, pass == 0 ? x1 : x2, nullptr, nullptr,
                              nullptr));
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            TEST_ASSERT_EQUAL_INT64(1, rep.first_incumbent_node);
            TEST_ASSERT_FALSE(isnan(seen));
            if (pass == 0)
                root[arm] = seen;
            else
                TEST_ASSERT_TRUE(root[arm] == seen);
            jaos_model_free(m);
        }
        TEST_ASSERT_EQUAL_MEMORY(x1, x2, sizeof x1);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, root[0]);
    TEST_ASSERT_TRUE(root[1] > root[0]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_pump_obj(m, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_pump_obj(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_obj(m, 0.5));
    TEST_ASSERT_TRUE(m->cfg.mip_pump_obj_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_obj(m, -1.0));
    TEST_ASSERT_FALSE(m->cfg.mip_pump_obj_set);
    jaos_model_free(m);
}

static jaos_model *cycling_pair(void)
{
    const double cost[2] = { 1.0, 1.0 };
    const double cl[2] = { 0, 0 }, cu[2] = { 1, 1 };
    const double rl[1] = { -INFINITY }, ru[1] = { 3.0 };
    const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
    const double av[2] = { 2.0, 2.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    for (int64_t j = 0; j < 2; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    return m;
}

static void test_the_pump_perturbs_a_rounding_that_repeats(void)
{
    const int64_t rounds[3] = { 1, 4, 20 };
    int64_t solves[3] = { 0, 0, 0 };
    for (int arm = 0; arm < 3; arm++) {
        for (int pass = 0; pass < 2; pass++) {
            jaos_model *m = cycling_pair();
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, rounds[arm]));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);
            double x[2];
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_solution(m, x, nullptr, nullptr, nullptr));

            TEST_ASSERT_TRUE(2.0 * x[0] + 2.0 * x[1] <= 3.0 + 1e-9);
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            if (pass == 0)
                solves[arm] = rep.lp_solves;
            else
                TEST_ASSERT_EQUAL_INT64(solves[arm], rep.lp_solves);
            jaos_model_free(m);
        }
    }

    TEST_ASSERT_TRUE(solves[1] > solves[0]);
    TEST_ASSERT_EQUAL_INT64(solves[1], solves[2]);
}

static jaos_model *propagation_model(void)
{
    const double cost[4] = { 3.0, 2.4, 2.0, 1.0 };
    const double cl[4] = { 0, 0, 0, 0 };
    const double cu[4] = { 10.0, 10.0, 10.0, 10.0 };
    const double rl[2] = { -INFINITY, -INFINITY }, ru[2] = { 3.0, 3.0 };
    const int64_t as[5] = { 0, 2, 4, 5, 6 };
    const int64_t ai[6] = { 0, 1, 0, 1, 0, 0 };
    const double av[6] = { 1.0, 2.0, 1.0, 1.0, 1.0, 1.0 };
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     6, as, ai, av));
    for (int64_t j = 0; j < 4; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_propagation_tightens_bounds_and_keeps_the_optimum(void)
{
    double obj[3] = { 0.0, 0.0, 0.0 };
    int64_t moved[3] = { -1, -1, -1 };
    double x[3][4];

    for (int arm = 0; arm < 3; arm++) {
        jaos_model *m = propagation_model();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_mip_propagate(m, arm == 0 ? 0 : 4));
        TEST_ASSERT_TRUE(m->cfg.mip_propagate_set);
        if (arm == 2) {
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_propagate_depth(m, 0));
            TEST_ASSERT_TRUE(m->cfg.mip_propagate_depth_set);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[arm]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, x[arm], nullptr, nullptr, nullptr));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        moved[arm] = rep.tightened;
        jaos_model_free(m);
    }

    TEST_ASSERT_EQUAL_INT64(0, moved[0]);
    TEST_ASSERT_TRUE(moved[1] >= 5);
    TEST_ASSERT_EQUAL_INT64(moved[1], moved[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.4, obj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.4, obj[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.4, obj[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1][0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1][1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1][2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[1][3]);
}

static void test_propagation_proves_a_row_infeasible(void)
{
    for (int arm = 0; arm < 2; arm++) {
        const double cost[2] = { 1.0, 1.0 }, cl[2] = { 0, 0 };
        const double cu[2] = { 2.0, 2.0 };
        const double rl[1] = { 5.0 }, ru[1] = { INFINITY };
        const int64_t as[3] = { 0, 1, 2 }, ai[2] = { 0, 0 };
        const double av[2] = { 1.0, 1.0 };
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                         2, as, ai, av));
        for (int64_t j = 0; j < 2; j++)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_mip_propagate(m, arm == 0 ? 0 : 2));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));

        TEST_ASSERT_EQUAL_INT64(arm == 0 ? 1 : 0, rep.lp_solves);
        jaos_model_free(m);
    }
}

static void test_reduced_cost_fixing_keeps_the_optimum(void)
{
    double obj[2] = { 0.0, 0.0 };
    int64_t fixed[2] = { -1, -1 };
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_rcfix(m, arm));
        TEST_ASSERT_TRUE(m->cfg.mip_rcfix_set);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[arm]));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        fixed[arm] = rep.fixed_cols;
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_INT64(0, fixed[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, obj[0], obj[1]);

    jaos_model *m = knapsack();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_rcfix(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_rcfix_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_propagate(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_propagate_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_propagate_depth(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_propagate_depth_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_always(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_pump_always_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_always(m, 1));
    TEST_ASSERT_TRUE(m->cfg.mip_pump_always_set);
    TEST_ASSERT_TRUE(m->cfg.mip_pump_always);
    jaos_model_free(m);
}

static void test_the_pump_may_run_where_an_incumbent_exists(void)
{
    double obj[2] = { 0.0, 0.0 };
    for (int arm = 0; arm < 2; arm++) {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 20));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_pump_always(m, arm));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[arm]));
        jaos_model_free(m);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, obj[0], obj[1]);
}

static void test_a_starting_point_and_a_cutoff(void)
{
    double best = 0.0;
    int64_t plain_nodes = 0;
    {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &best));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        plain_nodes = rep.nodes;
        jaos_model_free(m);
    }

    {
        jaos_model *m = knapsack5();
        const double start[5] = { 1.0, 1.0, 0.0, 0.0, 0.0 };
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, start));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, best, obj);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 23.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_EQUAL_INT64(0, rep.first_incumbent_node);
        TEST_ASSERT_TRUE(rep.nodes <= plain_nodes);
        jaos_model_free(m);
    }

    {
        jaos_model *m = knapsack5();
        const double bad[5] = { 9.0, 9.0, 9.0, 9.0, 9.0 };
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, bad));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, best, obj);
        jaos_model_free(m);
    }

    for (int arm = 0; arm < 2; arm++) {
        jaos_model *m = knapsack5();
        const double start[5] = { 1.0, 1.0, 0.0, 0.0, 0.0 };
        if (arm == 1)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, start));

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cutoff(m, best + 1000.0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_model_free(m);
    }

    {
        jaos_model *m = knapsack5();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cutoff(m, best - 1.0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, best, obj);
        jaos_model_free(m);
    }

    jaos_model *m = knapsack5();
    const double nan_pt[5] = { 0.0, 0.0, NAN, 0.0, 0.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_start(m, nan_pt));
    TEST_ASSERT_NULL(m->mip_start);
    const double ok_pt[5] = { 1.0, 0.0, 0.0, 0.0, 0.0 };
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, ok_pt));
    TEST_ASSERT_NOT_NULL(m->mip_start);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, nullptr));
    TEST_ASSERT_NULL(m->mip_start);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_mip_cutoff(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cutoff(m, 3.0));
    TEST_ASSERT_TRUE(m->cfg.mip_cutoff_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cutoff(m, INFINITY));
    TEST_ASSERT_FALSE(m->cfg.mip_cutoff_set);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_start(m, ok_pt));
    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_NOT_NULL(c->mip_start);
    TEST_ASSERT_TRUE(c->mip_start != m->mip_start);
    TEST_ASSERT_EQUAL_MEMORY(m->mip_start, c->mip_start, sizeof ok_pt);
    jaos_model_free(c);
    jaos_model_free(m);
}

static void test_a_proved_incumbent_publishes_a_basis_of_the_model(void)
{
    for (int cuts = 1; cuts >= 0; cuts--) {
        jaos_model *m = knapsack5();
        if (cuts == 0) {
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

        const int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
        jaos_basis_status *cs = calloc((size_t)(nc + 1), sizeof *cs);
        jaos_basis_status *rs = calloc((size_t)(nr + 1), sizeof *rs);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(rs);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
        int64_t basic = 0;
        for (int64_t j = 0; j < nc; j++) basic += cs[j] == JAOS_BASIS_BASIC;
        for (int64_t i = 0; i < nr; i++) basic += rs[i] == JAOS_BASIS_BASIC;
        TEST_ASSERT_EQUAL_INT64(nr, basic);

        jaos_model *c = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(c, cs, rs));
        jaos_model_free(c);

        free(cs);
        free(rs);
        jaos_model_free(m);
    }
}

static void test_a_semicontinuous_column_rests_at_zero_or_above_its_floor(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test, skipped under either fault build");
#else
    const double cost[] = {1.0, 5.0};
    const double cl[]   = {2.0, 0.0};
    const double cu[]   = {10.0, 1.0};
    const double rl[]   = {1.0};
    const double ru[]   = {INFINITY};
    const int64_t as[]  = {0, 1, 2};
    const int64_t ai[]  = {0, 0};
    const double  av[]  = {1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_semicontinuous(m, 0, true));

    double obj = 0.0, x[2], y[1];
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_TRUE(x[0] == 2.0 && x[1] == 0.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 10.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_TRUE(x[0] == 0.0 && x[1] == 1.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.max_col_violation);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 0.0, 10.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, 0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, obj);

    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    bool semi = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_semicontinuous(c, 0, &semi));
    TEST_ASSERT_TRUE(semi);
    jaos_model_free(c);
    jaos_model_free(m);
#endif
}

static jaos_model *indicator_model(void)
{
    const double cost[] = {-1.0, 3.0};
    const double cl[]   = {0.0, 0.0};
    const double cu[]   = {10.0, 1.0};
    const double rl[]   = {-INFINITY};
    const double ru[]   = {2.0};
    const int64_t as[]  = {0, 1, 1};
    const int64_t ai[]  = {0};
    const double  av[]  = {1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    return m;
}

static void test_an_indicator_row_holds_only_while_its_column_says_so(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test, skipped under either fault build");
#else
    jaos_model *m = indicator_model();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_row_indicator(m, 0, 0, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_row_indicator(m, 0, 1, 2));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_row_indicator(m, 3, 1, 1));
    int64_t zc = 5;
    int zv = 5;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_indicator(m, 0, &zc, &zv));
    TEST_ASSERT_TRUE(zc == -1 && zv == 0);

    const double want_obj[3] = {-10.0, -7.0, -2.0};
    const double want_x[3] = {10.0, 10.0, 2.0};
    const double want_z[3] = {0.0, 1.0, 0.0};
    for (int pass = 0; pass < 3; pass++) {
        if (pass < 2)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_indicator(m, 0, 1, 1 - pass));
        else
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_indicator(m, 0, -1, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[2], y[1];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, want_obj[pass], obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, want_x[pass], x[0]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, want_z[pass], x[1]);
        jaos_check_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
        TEST_ASSERT_TRUE(rep.primal_feasible);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_indicator(m, 0, 1, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_indicator(m, 0, &zc, &zv));
    TEST_ASSERT_TRUE(zc == 1 && zv == 1);
    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_indicator(c, 0, &zc, &zv));
    TEST_ASSERT_TRUE(zc == 1 && zv == 1);
    const int64_t del[] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(c, 1, del));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_indicator(c, 0, &zc, &zv));
    TEST_ASSERT_TRUE(zc == 0 && zv == 1);
    jaos_model_free(c);
    jaos_model_free(m);
#endif
}

static void test_probing_fixes_a_binary_that_fits_one_way_only(void)
{
    const double c[3] = {-3.0, -1.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[3] = {-INFINITY, -INFINITY, 1.0};
    const double ru[3] = {1.0, 1.0, INFINITY};
    const int64_t as[4] = {0, 2, 4, 6}, ai[6] = {0, 1, 0, 2, 1, 2};
    const double av[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing_cap(m, 0.0));
    TEST_ASSERT_TRUE(m->cfg.mip_probing_cap_set);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.mip_probing_cap);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_probing_cap(m, INFINITY));
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, obj);
    TEST_ASSERT_NOT_NULL(strstr(g_log, "probing: 3 of 3 fractional binaries "
                                       "probed, 1 fixed, 0 other bounds "
                                       "implied"));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    double x[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[2]);

    TEST_ASSERT_TRUE(m->cfg.mip_tighten_set);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing(m, 0));
    TEST_ASSERT_TRUE(m->cfg.mip_probing_set);
    TEST_ASSERT_FALSE(m->cfg.mip_probing);
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_NULL(strstr(g_log, "probing:"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_probing_set);
    jaos_model_free(m);
}

static void test_probing_keeps_a_bound_both_settings_imply(void)
{
    const double c[2] = {-1.0, -1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {1.0, 5.0};
    const double rl[2] = {-INFINITY, -INFINITY};
    const double ru[2] = {2.0, 5.0};
    const int64_t as[3] = {0, 2, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {-3.0, 3.0, 1.0, 1.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing_cap(m, 0.0));
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3.0, obj);
    TEST_ASSERT_NOT_NULL(strstr(g_log, "probing: 1 of 1 fractional binaries "
                                       "probed, 0 fixed, 1 other bounds "
                                       "implied"));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    jaos_model_free(m);
}

static void test_clique_fixing_at_a_node_shortens_the_tree(void)
{
    const double c[3] = {-4.0, -4.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[1] = {-INFINITY}, ru[1] = {5.0};
    const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
    const double av[3] = {3.0, 3.0, 2.0};
    int64_t nodes[2] = {0, 0};
    for (int on = 0; on < 2; on++) {
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         3, as, ai, av));
        for (int64_t j = 0; j < 3; j++)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_fix(m, on));
        TEST_ASSERT_TRUE(m->cfg.mip_clique_fix_set);
        TEST_ASSERT_EQUAL_INT(on, m->cfg.mip_clique_fix);
        g_log[0] = '\0';
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, -5.0, obj);
        if (on) {
            TEST_ASSERT_NOT_NULL(strstr(g_log, "clique table: 1 conflicts over 2 literals"));
            TEST_ASSERT_NULL(strstr(g_log, " 0 columns fixed by cliques"));
        } else {
            TEST_ASSERT_NOT_NULL(strstr(g_log, " 0 columns fixed by cliques"));
        }
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        nodes[on] = rep.nodes;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_fix(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_clique_fix_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(nodes[1] < nodes[0]);
}


static jaos_model *zero_half_model(bool cycle)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    if (cycle) {
        const double c[3] = {-1.0, -1.0, -1.0};
        const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
        const double rl[3] = {-INFINITY, -INFINITY, -INFINITY};
        const double ru[3] = {1.0, 1.0, 1.0};
        const int64_t as[4] = {0, 2, 4, 6}, ai[6] = {0, 2, 0, 1, 1, 2};
        const double av[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         6, as, ai, av));
    } else {
        const double c[3] = {-1.0, -1.0, -1.0};
        const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
        const double rl[1] = {-INFINITY}, ru[1] = {3.0};
        const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
        const double av[3] = {2.0, 2.0, 2.0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         3, as, ai, av));
    }
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
    return m;
}

static void test_zero_half_cuts_close_an_odd_row_and_an_odd_cycle_at_the_root(void)
{
    for (int cycle = 0; cycle < 2; cycle++) {
        int64_t nodes[2] = {0, 0};
        for (int on = 0; on < 2; on++) {
            jaos_model *m = zero_half_model(cycle);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, on ? 1 : 0));
            TEST_ASSERT_TRUE(m->cfg.mip_zero_half_rounds_set);
            g_log[0] = '\0';
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            double obj = 0.0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.0, obj);
            jaos_mip_report rep;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
            nodes[on] = rep.nodes;
            if (on) {
                TEST_ASSERT_NOT_NULL(strstr(g_log, " 1 zero-half, "));
                TEST_ASSERT_EQUAL_INT64(1, rep.cuts);
            }
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, -1));
            TEST_ASSERT_FALSE(m->cfg.mip_zero_half_rounds_set);
            jaos_model_free(m);
        }
        TEST_ASSERT_TRUE(nodes[0] > 1);
        TEST_ASSERT_EQUAL_INT64(1, nodes[1]);
    }
}


static jaos_model *flow_model(void)
{
    const double c[4] = {-1.0, -1.0, 2.0, 2.0};
    const double cl[4] = {0.0, 0.0, 0.0, 0.0};
    const double cu[4] = {INFINITY, INFINITY, 1.0, 1.0};
    const double rl[3] = {-INFINITY, -INFINITY, -INFINITY};
    const double ru[3] = {5.0, 0.0, 0.0};
    const int64_t as[5] = {0, 2, 4, 5, 6}, ai[6] = {0, 1, 0, 2, 1, 2};
    const double av[6] = {1.0, 1.0, 1.0, 1.0, -4.0, -3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 2, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 3, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
    return m;
}

static void test_a_flow_cover_cut_closes_the_fixed_charge_row_at_the_root(void)
{
    int64_t nodes[2] = {0, 0};
    for (int on = 0; on < 2; on++) {
        jaos_model *m = flow_model();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, on ? 1 : 0));
        TEST_ASSERT_TRUE(m->cfg.mip_flow_cover_rounds_set);
        g_log[0] = '\0';
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        nodes[on] = rep.nodes;
        if (on) {
            TEST_ASSERT_NOT_NULL(strstr(g_log, " 1 flow covers and "));
            TEST_ASSERT_TRUE(rep.cuts >= 1);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_flow_cover_rounds_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(nodes[0] > 1);
    TEST_ASSERT_EQUAL_INT64(1, nodes[1]);
}


static void test_a_conflict_row_shortens_an_infeasible_tree(void)
{
    const double c[3] = {-1.0, -1.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[4] = {-INFINITY, -INFINITY, -INFINITY, 1.5};
    const double ru[4] = {1.0, 1.0, 1.0, INFINITY};
    const int64_t as[4] = {0, 3, 6, 9};
    const int64_t ai[9] = {0, 2, 3, 0, 1, 3, 1, 2, 3};
    const double av[9] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    int64_t nodes[2] = {0, 0};
    for (int on = 0; on < 2; on++) {
        jaos_model *m = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 4, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         9, as, ai, av));
        for (int64_t j = 0; j < 3; j++)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_zero_half_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_flow_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_orbital(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_conflicts(m, on));
        TEST_ASSERT_TRUE(m->cfg.mip_conflicts_set);
        TEST_ASSERT_EQUAL_INT(on, m->cfg.mip_conflicts);
        g_log[0] = '\0';
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        nodes[on] = rep.nodes;
        if (on)
            TEST_ASSERT_NOT_NULL(strstr(g_log, " 2 conflicts over 2 binaries"));
        else
            TEST_ASSERT_NOT_NULL(strstr(g_log, " 0 conflicts over 0 binaries"));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_conflicts(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_conflicts_set);
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(nodes[1] <= nodes[0]);
}

static void test_coefficient_tightening_is_a_switch(void)
{
    jaos_model *m = knapsack();
    TEST_ASSERT_TRUE(m->cfg.mip_tighten_set);
    TEST_ASSERT_FALSE(m->cfg.mip_tighten);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, 1));
    TEST_ASSERT_TRUE(m->cfg.mip_tighten);
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, obj);
    TEST_ASSERT_NOT_NULL(strstr(g_log,
        "coefficient tightening: 2 coefficients on 1 rows"));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
    TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
    TEST_ASSERT_EQUAL_INT64(0, rep.cuts);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tighten(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_tighten_set);
    jaos_model_free(m);
}

static void test_coefficient_tightening_closes_a_loose_binary_row(void)
{
    const double c[2] = {-1.0, -1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {1.0, 1.0};
    const double rl[1] = {-INFINITY}, ru[1] = {4.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {5.0, 3.0};

    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_probing(m, 0));
    g_log[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, capture_log, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.0, obj);
    TEST_ASSERT_NOT_NULL(strstr(g_log,
        "coefficient tightening: 1 coefficients on 1 rows"));

    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1]);
    jaos_model_free(m);
}

static void test_clique_cuts_close_a_pairwise_conflict_at_the_root(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test, skipped under either fault build");
#else
    const double cost[] = {-1.0, -1.0, -1.0};
    const double cl[]   = {0.0, 0.0, 0.0};
    const double cu[]   = {1.0, 1.0, 1.0};
    const double rl[]   = {-INFINITY, -INFINITY, -INFINITY};
    const double ru[]   = {1.0, 1.0, 1.0};
    const int64_t as[]  = {0, 2, 4, 6};
    const int64_t ai[]  = {0, 2, 0, 1, 1, 2};
    const double  av[]  = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    for (int pass = 0; pass < 2; pass++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                         6, as, ai, av));
        for (int64_t j = 0; j < 3; j++)
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cover_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_mir_rounds(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_cut_depth(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_feaspump(m, 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, pass == 0 ? 1 : 0));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.0, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (pass == 0) {
            TEST_ASSERT_EQUAL_INT64(1, rep.nodes);
            TEST_ASSERT_TRUE(rep.cuts >= 1);
        } else {
            TEST_ASSERT_TRUE(rep.nodes > 1);
        }
        jaos_model_free(m);
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, 2));
    TEST_ASSERT_TRUE(m->cfg.mip_clique_rounds_set && m->cfg.mip_clique_rounds == 2);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_clique_rounds(m, -1));
    TEST_ASSERT_FALSE(m->cfg.mip_clique_rounds_set);
    jaos_model_free(m);
#endif
}

static jaos_model *three_unit_columns(void)
{
    const double cost[] = {-1.0, -1.0, -1.0};
    const double cl[]   = {0.0, 0.0, 0.0};
    const double cu[]   = {1.0, 1.0, 1.0};
    const double rl[]   = {-INFINITY};
    const double ru[]   = {10.0};
    const int64_t as[]  = {0, 1, 2, 3};
    const int64_t ai[]  = {0, 0, 0};
    const double  av[]  = {1.0, 1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));
    return m;
}

static void test_special_ordered_sets_branch_to_their_optimum(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test, skipped under either fault build");
#else
    const int64_t cols[] = {2, 0, 1};
    const double w[] = {3.0, 1.0, 2.0};
    for (int type = 1; type <= 2; type++) {
        jaos_model *m = three_unit_columns();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(m, type, 3, cols, w));
        TEST_ASSERT_EQUAL_INT64(1, jaos_num_sos(m));
        int t = 0;
        int64_t n = 0, got[3];
        double gw[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_sos(m, 0, &t, &n, got, gw));
        TEST_ASSERT_EQUAL_INT(type, t);
        TEST_ASSERT_EQUAL_INT64(3, n);
        TEST_ASSERT_TRUE(got[0] == 0 && got[1] == 1 && got[2] == 2);
        TEST_ASSERT_TRUE(gw[0] == 1.0 && gw[1] == 2.0 && gw[2] == 3.0);

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3], y[1];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, type == 1 ? -1.0 : -2.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
        int nz = 0, first = -1, last = -1;
        for (int k = 0; k < 3; k++)
            if (x[k] != 0.0) {
                if (nz == 0)
                    first = k;
                last = k;
                nz++;
            }
        TEST_ASSERT_EQUAL_INT(type, nz);
        if (type == 2)
            TEST_ASSERT_EQUAL_INT(1, last - first);
        jaos_check_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
        TEST_ASSERT_TRUE(rep.primal_feasible);
        TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.max_integrality_violation);

        jaos_model *c = nullptr;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
        TEST_ASSERT_EQUAL_INT64(1, jaos_num_sos(c));
        const int64_t del[] = {1};
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(c, 1, del));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_sos(c, 0, &t, &n, got, gw));
        TEST_ASSERT_EQUAL_INT64(2, n);
        TEST_ASSERT_TRUE(got[0] == 0 && got[1] == 1);
        TEST_ASSERT_TRUE(gw[0] == 1.0 && gw[1] == 3.0);
        jaos_model_free(c);
        jaos_model_free(m);
    }
    jaos_model *m = three_unit_columns();
    const int64_t bad[] = {0, 7};
    const int64_t dup[] = {0, 0};
    const double dw[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_add_sos(m, 1, 2, bad, w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_add_sos(m, 1, 2, dup, w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_add_sos(m, 1, 2, cols, dw));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_add_sos(m, 3, 2, cols, w));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_sos(m));
    double x[3] = {1.0, 0.0, 1.0}, y[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(m, 1, 3, cols, w));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-9, &rep));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, rep.max_integrality_violation);
    jaos_model_free(m);
#endif
}

int main(void)
{
    UNITY_BEGIN();

#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    return UNITY_END();
#endif
    RUN_TEST(test_a_proved_incumbent_publishes_a_basis_of_the_model);
    RUN_TEST(test_the_knapsack_finds_the_integer_optimum);
    RUN_TEST(test_a_fractional_root_branches_to_the_integer_answer);
    RUN_TEST(test_an_integer_model_with_no_integer_point_is_infeasible);
    RUN_TEST(test_a_work_limit_stops_the_tree_and_keeps_the_incumbent);
    RUN_TEST(test_the_marks_ride_with_their_columns_and_copy);
    RUN_TEST(test_the_readers_and_writers_carry_the_marks);
    RUN_TEST(test_without_cuts_or_dive_the_tree_branches_to_the_same_answer);
    RUN_TEST(test_a_cut_over_continuous_columns_keeps_the_integer_optimum);
    RUN_TEST(test_the_rounding_heuristic_takes_the_relaxations_neighbour);
    RUN_TEST(test_an_infeasible_rounding_is_not_taken);
    RUN_TEST(test_the_tree_logs_its_start_root_and_end);
    RUN_TEST(test_a_node_limit_stops_with_the_incumbent_the_callback_saw);
    RUN_TEST(test_a_lazy_row_from_the_node_callback_rejects_the_point);
    RUN_TEST(test_a_user_cut_from_the_node_callback_closes_the_root);
    RUN_TEST(test_the_node_callback_chooses_the_branching_column);
    RUN_TEST(test_both_branching_rules_reach_the_same_optimum);
    RUN_TEST(test_a_fractional_bound_on_an_integer_column_is_rounded_inward);
    RUN_TEST(test_strong_branching_probes_are_counted_and_change_no_answer);
    RUN_TEST(test_a_capped_probe_reaches_the_same_optimum);
    RUN_TEST(test_every_dive_child_rule_reaches_the_same_optimum);
    RUN_TEST(test_cuts_below_the_root_reach_the_same_optimum);
    RUN_TEST(test_dropping_slack_cuts_keeps_the_optimum);
    RUN_TEST(test_probing_at_the_root_only_reaches_the_same_optimum);
    RUN_TEST(test_the_solution_pool_holds_the_best_points_best_first);
    RUN_TEST(test_a_cover_cut_closes_the_knapsack_at_the_root);
    RUN_TEST(test_a_cap_on_a_nodes_cuts_keeps_the_optimum);
    RUN_TEST(test_a_stalled_root_round_is_the_last);
    RUN_TEST(test_a_stalled_round_ends_the_cuts_under_it);
    RUN_TEST(test_root_cuts_may_leave_below_a_node);
    RUN_TEST(test_a_lifted_cover_closes_what_the_extended_cover_leaves);
    RUN_TEST(test_a_mir_cut_closes_the_halved_row_at_the_root);
    RUN_TEST(test_a_backtracking_dive_reaches_the_same_optimum);
    RUN_TEST(test_mir_cuts_at_the_nodes_keep_the_optimum);
    RUN_TEST(test_a_dive_bounded_by_the_gap_reaches_the_same_optimum);
    RUN_TEST(test_an_aggregated_mir_cut_closes_what_one_row_leaves);
    RUN_TEST(test_the_dive_heuristic_finds_the_first_incumbent);
    RUN_TEST(test_the_dive_heuristic_runs_below_the_root);
    RUN_TEST(test_the_feasibility_pump_finds_a_point_at_the_root);
    RUN_TEST(test_the_general_pump_reaches_a_general_integer_point);
    RUN_TEST(test_the_objective_pump_finds_a_better_root_point);
    RUN_TEST(test_the_pump_perturbs_a_rounding_that_repeats);
    RUN_TEST(test_rins_searches_the_incumbents_neighbourhood);
    RUN_TEST(test_a_dive_bounded_by_the_degradation_keeps_the_optimum);
    RUN_TEST(test_propagation_tightens_bounds_and_keeps_the_optimum);
    RUN_TEST(test_propagation_proves_a_row_infeasible);
    RUN_TEST(test_reduced_cost_fixing_keeps_the_optimum);
    RUN_TEST(test_the_pump_may_run_where_an_incumbent_exists);
    RUN_TEST(test_a_starting_point_and_a_cutoff);
    RUN_TEST(test_a_semicontinuous_column_rests_at_zero_or_above_its_floor);
    RUN_TEST(test_special_ordered_sets_branch_to_their_optimum);
    RUN_TEST(test_an_indicator_row_holds_only_while_its_column_says_so);
    RUN_TEST(test_clique_cuts_close_a_pairwise_conflict_at_the_root);
    RUN_TEST(test_coefficient_tightening_closes_a_loose_binary_row);
    RUN_TEST(test_coefficient_tightening_is_a_switch);
    RUN_TEST(test_probing_fixes_a_binary_that_fits_one_way_only);
    RUN_TEST(test_probing_keeps_a_bound_both_settings_imply);
    RUN_TEST(test_clique_fixing_at_a_node_shortens_the_tree);
    RUN_TEST(test_zero_half_cuts_close_an_odd_row_and_an_odd_cycle_at_the_root);
    RUN_TEST(test_a_flow_cover_cut_closes_the_fixed_charge_row_at_the_root);
    RUN_TEST(test_a_conflict_row_shortens_an_infeasible_tree);
    RUN_TEST(test_a_quadratic_objective_branches_on_barrier_relaxations);
    RUN_TEST(test_a_quadratic_objective_matches_enumeration);
    RUN_TEST(test_a_quadratic_objective_breaks_the_symmetry);
    RUN_TEST(test_an_infeasible_quadratic_model_says_so);
    RUN_TEST(test_a_cut_never_shuts_out_every_point);
    RUN_TEST(test_a_conflict_rests_only_on_rows_that_always_hold);
    return UNITY_END();
}
