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

constexpr double TOL = 1e-6;

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static jaos_model *paired_qp(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {-3.0, -3.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {3.0, 3.0};
    const double rl[1] = {-inf}, ru[1] = {4.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {2.0, 2.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    return m;
}

static void test_a_paired_q_reads_back_as_it_was_set(void)
{
    jaos_model *m = paired_qp();
    TEST_ASSERT_EQUAL_INT64(3, jaos_quadratic_nz(m));
    int64_t r[3], c[3];
    double v[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_quadratic(m, r, c, v));

    double diag[2] = {0.0, 0.0}, off = 0.0;
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_TRUE(r[k] >= c[k]);
        if (r[k] == c[k])
            diag[r[k]] = v[k];
        else
            off = v[k];
    }
    TEST_ASSERT_EQUAL_DOUBLE(2.0, diag[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, diag[1]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, off);

    double q = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    jaos_model_free(m);
}

static void test_zero_pairs_are_dropped_and_the_rest_stay_in_place(void)
{
    jaos_model *m = fresh();
    const double cost[4] = {-1.0, -1.0, -1.0, -1.0};
    const double cl[4] = {0.0, 0.0, 0.0, 0.0};
    const double cu[4] = {10.0, 10.0, 10.0, 10.0};
    const int64_t as[5] = {0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    const int64_t qr[10] = {0, 1, 2, 3, 1, 2, 3, 2, 3, 0};
    const int64_t qc[10] = {0, 1, 2, 3, 0, 0, 0, 1, 2, 1};
    const double qv[10] = {2.0, 2.0, 2.0, 2.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0};
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK,
        jaos_set_quadratic(m, 10, qr, qc, qv), jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_quadratic_nz(m));
    int64_t r[6], c[6];
    double v[6];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_quadratic(m, r, c, v));
    int pairs = 0;
    for (int k = 0; k < 6; k++) {
        if (r[k] == c[k]) {
            TEST_ASSERT_EQUAL_DOUBLE(2.0, v[k]);
            continue;
        }
        TEST_ASSERT_EQUAL_INT64(c[k] + 1, r[k]);
        TEST_ASSERT_TRUE(c[k] == 1 || c[k] == 2);
        TEST_ASSERT_EQUAL_DOUBLE(1.0, v[k]);
        pairs++;
    }
    TEST_ASSERT_EQUAL_INT(2, pairs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_OPTIMAL, jaos_status_of(m),
                                  jaos_model_error(m));
    jaos_model_free(m);
}

static void test_a_paired_q_solves_to_the_point_it_should(void)
{
    jaos_model *m = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, obj);
    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 1.0, x[1]);
    jaos_model_free(m);
}

static jaos_model *bound_qp(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {-3.0, -3.0};
    const double cl[2] = {1.5, 0.0}, cu[2] = {3.0, 3.0};
    const double rl[1] = {-inf}, ru[1] = {4.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {2.0, 2.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    return m;
}

static void test_the_push_puts_the_answer_on_its_bound(void)
{
    jaos_model *m = bound_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -2.8125, obj);
    double x[2], y[1], dj[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, dj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.5, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 0.75, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 0.75, dj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 0.0, dj[1]);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-8, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_without_the_push_the_point_stops_short_of_the_bound(void)
{
    jaos_model *m = bound_qp();
    m->cfg.barrier_no_crossover = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_TRUE(x[0] > 1.5);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-10, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_FALSE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_a_stalled_barrier_recentres_and_finishes(void)
{
    jaos_model *m = fresh();
    const double cost[4] = {-14.4375, -1.25, 3.75, -2.9375};
    const double cl[4] = {-1.0, 1.0, -4.0, -3.0}, cu[4] = {4.0, 9.0, -2.0, 2.0};
    const double rl[1] = {-6.0}, ru[1] = {-6.0};
    const int64_t as[5] = {0, 0, 1, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {-3.0, -4.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const double q[4] = {5.25, 2.25, 1.25, 1.25};
    for (int64_t j = 0; j < 4; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, j, q[j]));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, -27.453125, obj);
    double x[4], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 2.75, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, -3.0, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, 0.75, x[3]);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-8, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_the_checker_judges_a_paired_q(void)
{
    jaos_model *m = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double x[2], y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);

    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, rep.primal_objective);
    jaos_model_free(m);
}

static void test_the_cross_term_moves_the_answer(void)
{
    jaos_model *a = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    double oa = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));

    jaos_model *b = paired_qp();
    const int64_t qr[2] = {0, 1}, qc[2] = {0, 1};
    const double qv[2] = {2.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(b, 2, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT64(2, jaos_quadratic_nz(b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));

    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -4.5, ob);
    TEST_ASSERT_TRUE(fabs(oa - ob) > 1.0);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_a_non_convex_q_is_refused(void)
{

    jaos_model *m = paired_qp();
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {1.0, 1.0, 3.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not convex"));
    jaos_model_free(m);
}

static void test_a_semi_definite_q_is_taken(void)
{

    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {0.0, 0.0};
    const double cl[2] = {-inf, -inf}, cu[2] = {inf, inf};
    const double rl[1] = {2.0}, ru[1] = {2.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const int64_t qr[3] = {0, 1, 1};
    const int64_t qc[3] = {0, 1, 0};
    const double qv[3] = {1.0, 1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));

    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 0.0, obj);
    jaos_model_free(m);
}

static void test_a_paired_q_survives_a_copy(void)
{
    jaos_model *a = paired_qp();
    jaos_model *b = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(a, &b));
    TEST_ASSERT_EQUAL_INT64(jaos_quadratic_nz(a), jaos_quadratic_nz(b));
    TEST_ASSERT_EQUAL_INT64(a->q_nz, b->q_nz);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_TRUE(oa == ob);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_deleting_a_column_carries_the_pairs_with_it(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[3] = {-1.0, -1.0, -1.0};
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {5.0, 5.0, 5.0};
    const double rl[1] = {-inf}, ru[1] = {9.0};
    const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
    const double av[3] = {1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));

    const int64_t qr[5] = {0, 1, 2, 1, 2};
    const int64_t qc[5] = {0, 1, 2, 0, 1};
    const double qv[5] = {2.0, 2.0, 2.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 5, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT64(2, m->q_nz);

    const int64_t drop[1] = {1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, drop));
    TEST_ASSERT_EQUAL_INT64(2, jaos_num_col(m));

    TEST_ASSERT_EQUAL_INT64(0, m->q_nz);
    TEST_ASSERT_EQUAL_INT64(2, jaos_quadratic_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_set_quadratic_refuses_what_it_cannot_take(void)
{
    jaos_model *m = paired_qp();
    const int64_t bad_r[1] = {5}, bad_c[1] = {0};
    const double bad_v[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, 1, bad_r, bad_c, bad_v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "names columns"));

    const int64_t inf_r[1] = {0}, inf_c[1] = {0};
    const double inf_v[1] = {INFINITY};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, 1, inf_r, inf_c, inf_v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not finite"));

    const int64_t twice_r[2] = {1, 0}, twice_c[2] = {0, 1};
    const double twice_v[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, 2, twice_r, twice_c,
                                             twice_v));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "given twice"));

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(m, -1, nullptr, nullptr,
                                             nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_quadratic(nullptr, 0, nullptr, nullptr,
                                             nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_quadratic(m, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_quadratic_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    jaos_model_free(m);
}

static void test_a_paired_q_is_bit_identical_across_runs(void)
{
    double first[2];
    double obj = 0.0;
    for (int run = 0; run < 2; run++) {
        jaos_model *m = paired_qp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double x[2], o = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &o));
        if (run == 0) {
            memcpy(first, x, sizeof x);
            obj = o;
        } else {
            TEST_ASSERT_EQUAL_MEMORY(first, x, sizeof x);
            TEST_ASSERT_TRUE(obj == o);
        }
        jaos_model_free(m);
    }
}

static void test_the_other_algorithms_still_refuse_a_quadratic(void)
{
    static const jaos_algorithm no[] = {JAOS_ALGORITHM_PRIMAL,
                                        JAOS_ALGORITHM_PDLP,
                                        JAOS_ALGORITHM_CONCURRENT};
    for (size_t k = 0; k < sizeof no / sizeof *no; k++) {
        jaos_model *m = paired_qp();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_algorithm(m, no[k]));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic term"));
        jaos_model_free(m);
    }
}

static void test_mps_carries_a_paired_q_both_ways(void)
{
    jaos_model *a = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_mps(a, "build/tqq.mps"));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(b, "build/tqq.mps"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    TEST_ASSERT_EQUAL_INT64(a->q_nz, b->q_nz);
    TEST_ASSERT_EQUAL_INT64(jaos_quadratic_nz(a), jaos_quadratic_nz(b));
    for (int64_t j = 0; j < jaos_num_col(a); j++) {
        double qa = 0.0, qb = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(a, j, &qa));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, j, &qb));
        TEST_ASSERT_TRUE(qa == qb);
    }
    for (int64_t p = 0; p < a->q_nz; p++) {
        TEST_ASSERT_EQUAL_INT64(a->q_index[p], b->q_index[p]);
        TEST_ASSERT_TRUE(a->q_value[p] == b->q_value[p]);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_TRUE(oa == ob);
    jaos_model_free(a);
    jaos_model_free(b);
    remove("build/tqq.mps");
}

static void write_lines(const char *path, const char *const *lines)
{
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    for (int64_t k = 0; lines[k] != nullptr; k++)
        fprintf(f, "%s\n", lines[k]);
    fclose(f);
}

static void test_mps_reads_both_halves_when_the_file_gives_them(void)
{
    static const char *const lower[] = {
        "NAME          PAIR",
        "ROWS", " N  COST", " L  R1",
        "COLUMNS",
        "    X         COST      -3.0       R1        1.0",
        "    Y         COST      -3.0       R1        1.0",
        "QUADOBJ",
        "    X         X         2.0",
        "    Y         Y         2.0",
        "    Y         X         1.0",
        "RHS", "    RHS       R1        4.0",
        "BOUNDS",
        " UP BND       X         3.0",
        " UP BND       Y         3.0",
        "ENDATA", nullptr};
    static const char *const both[] = {
        "NAME          PAIR",
        "ROWS", " N  COST", " L  R1",
        "COLUMNS",
        "    X         COST      -3.0       R1        1.0",
        "    Y         COST      -3.0       R1        1.0",
        "QMATRIX",
        "    X         X         2.0",
        "    Y         Y         2.0",
        "    Y         X         1.0",
        "    X         Y         1.0",
        "RHS", "    RHS       R1        4.0",
        "BOUNDS",
        " UP BND       X         3.0",
        " UP BND       Y         3.0",
        "ENDATA", nullptr};

    write_lines("build/tq_lower.mps", lower);
    write_lines("build/tq_both.mps", both);

    double obj[2];
    static const char *const files[2] = {"build/tq_lower.mps",
                                         "build/tq_both.mps"};
    for (int k = 0; k < 2; k++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, files[k]));
        TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
        TEST_ASSERT_EQUAL_INT64(1, m->q_nz);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[k]));
        jaos_model_free(m);
        remove(files[k]);
    }
    TEST_ASSERT_TRUE(obj[0] == obj[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, obj[0]);
}

static void test_mps_refuses_two_halves_that_disagree(void)
{
    static const char *const bad[] = {
        "NAME          PAIR",
        "ROWS", " N  COST", " L  R1",
        "COLUMNS",
        "    X         COST      -3.0       R1        1.0",
        "    Y         COST      -3.0       R1        1.0",
        "QMATRIX",
        "    Y         X         1.0",
        "    X         Y         2.0",
        "RHS", "    RHS       R1        4.0",
        "ENDATA", nullptr};
    write_lines("build/tq_bad.mps", bad);
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_mps(m, "build/tq_bad.mps"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "have to agree"));
    jaos_model_free(m);
    remove("build/tq_bad.mps");
}

static void test_lp_carries_a_paired_q_both_ways(void)
{
    static const char *const lines[] = {
        "Minimize",
        " obj: - 3 x - 3 y + [ 2 x ^ 2 + 2 y ^ 2 + 2 x * y ] / 2",
        "Subject To",
        " c1: x + y <= 4",
        "Bounds",
        " 0 <= x <= 3",
        " 0 <= y <= 3",
        "End", nullptr};
    write_lines("build/tq_pair.lp", lines);

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "build/tq_pair.lp"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(m));
    TEST_ASSERT_EQUAL_INT64(1, m->q_nz);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->q_value[0]);
    double q = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, obj);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_lp(m, "build/tq_back.lp"));
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, "build/tq_back.lp"));
    TEST_ASSERT_EQUAL_INT64(m->q_nz, b->q_nz);
    TEST_ASSERT_TRUE(m->q_value[0] == b->q_value[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    double ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_TRUE(obj == ob);
    jaos_model_free(b);
    jaos_model_free(m);
    remove("build/tq_pair.lp");
    remove("build/tq_back.lp");
}

static void test_lp_takes_the_block_undivided(void)
{
    static const char *const halved[] = {
        "Minimize",
        " obj: - 3 x - 3 y + [ x ^ 2 + y ^ 2 + x * y ]",
        "Subject To",
        " c1: x + y <= 4",
        "Bounds", " 0 <= x <= 3", " 0 <= y <= 3",
        "End", nullptr};
    write_lines("build/tq_undiv.lp", halved);
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(m, "build/tq_undiv.lp"));
    TEST_ASSERT_EQUAL_INT64(1, m->q_nz);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->q_value[0]);
    double q = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(m, 0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, q);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, obj);
    jaos_model_free(m);
    remove("build/tq_undiv.lp");
}

static void round_trip_paired(jaos_status (*write)(jaos_model *,
                                                   const char *),
                              jaos_status (*read)(jaos_model *,
                                                  const char *),
                              const char *path)
{
    jaos_model *a = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, write(a, path));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(a));

    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, read(b, path));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    TEST_ASSERT_EQUAL_INT64(a->q_nz, b->q_nz);
    TEST_ASSERT_EQUAL_INT64(jaos_quadratic_nz(a), jaos_quadratic_nz(b));
    for (int64_t p = 0; p < a->q_nz; p++) {
        TEST_ASSERT_EQUAL_INT64(a->q_index[p], b->q_index[p]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, a->q_value[p], b->q_value[p]);
    }
    for (int64_t j = 0; j < jaos_num_col(a); j++) {
        double qa = 0.0, qb = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(a, j, &qa));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_quadratic(b, j, &qb));
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, qa, qb);
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(b));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(b));
    double oa = 0.0, ob = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(a, &oa));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(b, &ob));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, oa, ob);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, -3.0, ob);
    jaos_model_free(a);
    jaos_model_free(b);
    remove(path);
}

static void test_qplib_carries_a_paired_q_both_ways(void)
{
    round_trip_paired(jaos_write_qplib, jaos_read_qplib, "build/tqq.qplib");
}

static void test_osil_carries_a_paired_q_both_ways(void)
{
    round_trip_paired(jaos_write_osil, jaos_read_osil, "build/tqq.osil");
}

static void solve_convex(int64_t nc, int64_t nr, const double *cost,
                         const double *cl, const double *cu,
                         const double *rl, const double *ru,
                         const int64_t *as, const int64_t *ai,
                         const double *av, int64_t nq, const int64_t *qr,
                         const int64_t *qc, const double *qv)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, nc, nr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     as[nc], as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, nq, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_OPTIMAL, jaos_status_of(m),
        "a convex Q the barrier solves with the diagonal alone must not "
        "defeat it with the pairs in");
    jaos_model_free(m);
}

static void test_a_paired_q_reaches_the_optimum_the_diagonal_does(void)
{

    const double inf = jaos_infinity();
    {
        const double cost[3] = {1.0, -2.0, 0.0};
        const double cl[3] = {0.0, -2.0, -inf};
        const double cu[3] = {5.0, 5.0, inf};
        const double rl[2] = {-inf, 1.0}, ru[2] = {4.0, inf};
        const int64_t as[4] = {0, 2, 4, 5};
        const int64_t ai[5] = {0, 1, 0, 1, 0};
        const double av[5] = {1.0, 1.0, 2.0, -1.0, 1.0};

        const int64_t qr[5] = {0, 1, 1, 2, 2};
        const int64_t qc[5] = {0, 0, 1, 1, 2};
        const double qv[5] = {4.25, 2.0, 5.25, -1.5, 2.5};
        solve_convex(3, 2, cost, cl, cu, rl, ru, as, ai, av, 5, qr, qc, qv);
    }
    {

        const double cost[2] = {-1.0, 1.0};
        const double cl[2] = {-inf, 0.0};
        const double cu[2] = {inf, 3.0};
        const double rl[1] = {-inf}, ru[1] = {6.0};
        const int64_t as[3] = {0, 1, 2};
        const int64_t ai[2] = {0, 0};
        const double av[2] = {1.0, 1.0};
        const int64_t qr[3] = {0, 1, 1};
        const int64_t qc[3] = {0, 0, 1};
        const double qv[3] = {1.0, -1.0, 1.0};
        solve_convex(2, 1, cost, cl, cu, rl, ru, as, ai, av, 3, qr, qc, qv);
    }
}

static void test_a_paired_q_in_a_mip_does_not_cut_off_the_optimum(void)
{
    jaos_model *m = fresh();
    const double cost[5] = {1.0, -2.0, -5.0, 6.0, -3.0};
    const double cl[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    const double cu[5] = {2.0, 1.0, 1.0, 2.0, 1.0};
    const double rl[1] = {0.0}, ru[1] = {0.0};
    const int64_t as[6] = {0, 0, 0, 0, 0, 0}, ai[1] = {0};
    const double av[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     0, as, ai, av));
    const int64_t qr[7] = {0, 1, 1, 2, 3, 4, 4};
    const int64_t qc[7] = {0, 0, 1, 2, 3, 2, 4};
    const double qv[7] = {1.0, -2.0, 13.0, 4.0, 9.0, 2.0, 5.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 7, qr, qc, qv));
    for (int64_t j = 0; j < 5; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(TOL, -3.0, obj);

    double x[5];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    const double want[5] = {0.0, 0.0, 1.0, 0.0, 0.0};
    for (int j = 0; j < 5; j++)
        TEST_ASSERT_DOUBLE_WITHIN(TOL, want[j], x[j]);
    jaos_model_free(m);
}

typedef struct {
    double last;
    int lines;
} barrier_log;

static void catch_objective(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    barrier_log *b = user;
    if (strncmp(line, "barrier ", 8) != 0)
        return;
    const char *p = strstr(line, "objective ");
    if (p == nullptr)
        return;
    b->last = strtod(p + 10, nullptr);
    b->lines++;
}

static void test_the_barrier_counts_a_fixed_column_in_its_objective(void)
{
    const double inf = jaos_infinity();
    const double cost[4] = {-5.0, -2.0, -4.0, -1.0};
    const double cl[4] = {0.0, 0.0, 1.0, 1.0};
    const double cu[4] = {1.0, 1.0, 1.0, 1.0};
    const double rl[2] = {-inf, -inf}, ru[2] = {2.0, 4.0};
    const int64_t as[5] = {0, 2, 4, 5, 7};
    const int64_t ai[7] = {0, 1, 0, 1, 0, 0, 1};
    const double av[7] = {-1.0, 2.0, -2.0, 2.0, -2.0, 2.0, 1.0};
    const int64_t qr[9] = {0, 1, 2, 1, 2, 3, 2, 3, 3};
    const int64_t qc[9] = {0, 0, 0, 1, 1, 1, 2, 2, 3};
    const double qv[9] = {9.0, 6.0, 3.0, 8.0, 4.0, -4.0, 6.0, -2.0, 13.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 4, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     7, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 9, qr, qc, qv));

    barrier_log b = {0.0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, catch_objective, &b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_PROGRESS));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(TOL, 20.0 / 9.0, obj);
    TEST_ASSERT_TRUE(b.lines > 0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, obj, b.last);

    double x[4];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0 / 9.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0 / 6.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[3]);
    jaos_model_free(m);
}

static void test_a_paired_q_maximised(void)
{
    const double inf = jaos_infinity();
    const double cost[2] = {3.0, 3.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {3.0, 3.0};
    const double rl[1] = {-inf}, ru[1] = {4.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    const int64_t qr[3] = {0, 1, 1}, qc[3] = {0, 1, 0};

    {
        const double qv[3] = {-2.0, -2.0, -1.0};
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         2, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[2];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(TOL, 3.0, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_DOUBLE_WITHIN(1e-5, 1.0, x[0]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-5, 1.0, x[1]);
        jaos_model_free(m);
    }
    {
        const double negcost[2] = {-3.0, -3.0};
        const double qv[3] = {2.0, 2.0, 1.0};
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, negcost, cl, cu, rl, ru,
                         2, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(TOL, -3.0, obj);
        jaos_model_free(m);
    }
    {
        const double qv[3] = {-2.0, -2.0, -3.0};
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         2, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not convex"));
        jaos_model_free(m);
    }
    {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                         2, as, ai, av));
        const double qv[3] = {-2.0, -2.0, -1.0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 1.0));
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
        jaos_model_free(m);
    }
}

static void test_the_two_quadratic_calls_reach_the_same_matrix(void)
{
    const int64_t qr[5] = {0, 1, 1, 2, 2}, qc[5] = {0, 1, 0, 2, 1};
    const double qv[5] = {4.0, 5.0, 2.0, 6.0, -1.0};
    const int64_t off_r[2] = {1, 2}, off_c[2] = {0, 1};
    const double off_v[2] = {2.0, -1.0};

    jaos_model *a = paired_qp();
    jaos_model *b = paired_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(a, 1, (const double[]){0.0}, (const double[]){0.0},
                      (const double[]){3.0}, 0, (const int64_t[]){0, 0},
                      (const int64_t[]){0}, (const double[]){0.0}));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(b, 1, (const double[]){0.0}, (const double[]){0.0},
                      (const double[]){3.0}, 0, (const int64_t[]){0, 0},
                      (const int64_t[]){0}, (const double[]){0.0}));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(a, 5, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(b, 2, off_r, off_c, off_v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(b, 0, 4.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(b, 1, 5.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(b, 2, 6.0));

    const int64_t n = jaos_quadratic_nz(a);
    TEST_ASSERT_EQUAL_INT64(5, n);
    TEST_ASSERT_EQUAL_INT64(n, jaos_quadratic_nz(b));

    int64_t ra[5], ca[5], rb[5], cb[5];
    double va[5], vb[5];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_quadratic(a, ra, ca, va));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_quadratic(b, rb, cb, vb));
    for (int64_t k = 0; k < n; k++) {
        TEST_ASSERT_EQUAL_INT64(ra[k], rb[k]);
        TEST_ASSERT_EQUAL_INT64(ca[k], cb[k]);
        TEST_ASSERT_DOUBLE_WITHIN(0.0, va[k], vb[k]);
    }
    jaos_model_free(a);
    jaos_model_free(b);
}

static jaos_model *unbounded_qp(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {40.0, inf};
    const double rl[1] = {1.0}, ru[1] = {inf};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, -1.0));
    return m;
}

static void test_an_unbounded_qp_ends_unbounded_with_its_ray(void)
{
    jaos_model *m = unbounded_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    double d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 0.0, d[0]);
    TEST_ASSERT_TRUE(d[1] > 0.0);
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-9, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 0.0, rep.curvature);
    jaos_model_free(m);
}

static void test_a_bounded_qp_with_free_columns_is_not_called_unbounded(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {0.0, -1.0};
    const double cl[2] = {-inf, -inf}, cu[2] = {inf, inf};
    const double rl[1] = {-10.0}, ru[1] = {inf};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 1, 1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-8, -0.5, obj);
    jaos_model_free(m);
}

static jaos_model *paired_unbounded_qp(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {-1.0, -1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {inf, inf};
    const double rl[1] = {-inf}, ru[1] = {3.0};
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     2, as, ai, av));
    const int64_t qr[3] = {0, 1, 1}, qc[3] = {0, 1, 0};
    const double qv[3] = {1.0, 1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    return m;
}

static void test_a_paired_unbounded_qp_finds_the_ray_through_the_pair(void)
{
    jaos_model *m = paired_unbounded_qp();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    double d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_TRUE(d[0] > 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, d[0], d[1]);
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-9, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    jaos_model_free(m);
}

static void test_the_ray_checker_refuses_a_direction_the_curvature_turns(void)
{
    jaos_model *m = paired_unbounded_qp();
    const double d[2] = {0.0, 1.0};
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_DOUBLE_WITHIN(0.0, -1.0, rep.rate);
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 0.0, rep.max_col_escape);
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 0.0, rep.max_row_escape);
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 1.0, rep.curvature);
    TEST_ASSERT_FALSE(rep.certified);
    const double e[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, e, 1e-7, &rep));
    TEST_ASSERT_DOUBLE_WITHIN(0.0, 0.0, rep.curvature);
    TEST_ASSERT_TRUE(rep.certified);
    jaos_model_free(m);
}

static unsigned long long fac_seed;

static double fac_rnd(void)
{
    fac_seed = fac_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(fac_seed >> 11) / 9007199254740992.0;
}

static void test_a_qp_whose_dense_columns_lose_the_walk_still_solves(void)
{
    enum { F = 3, C = 35, NX = F * C, NC = NX + F, NR = C + NX };
    static double cost[NC], cl[NC], cu[NC], rl[NR], ru[NR], q[NX], av[3 * NX];
    static int64_t as[NC + 1], ai[3 * NX];
    fac_seed = 1;
    int64_t nz = 0;
    for (int i = 0; i < C; i++)
        for (int j = 0; j < F; j++) {
            const int k = i * F + j;
            as[k] = nz;
            ai[nz] = i;
            av[nz++] = 1.0;
            ai[nz] = C + k;
            av[nz++] = 1.0;
            cost[k] = 1.0 + 9.0 * fac_rnd();
            q[k] = 0.5 + 2.0 * fac_rnd();
            cl[k] = 0.0;
            cu[k] = 1.0;
        }
    for (int j = 0; j < F; j++) {
        as[NX + j] = nz;
        for (int i = 0; i < C; i++) {
            ai[nz] = C + i * F + j;
            av[nz++] = -1.0;
        }
        cost[NX + j] = 20.0 + 40.0 * fac_rnd();
        cl[NX + j] = 0.0;
        cu[NX + j] = 1.0;
    }
    as[NC] = nz;
    for (int i = 0; i < C; i++)
        rl[i] = ru[i] = 1.0;
    for (int k = 0; k < NX; k++) {
        rl[C + k] = -jaos_infinity();
        ru[C + k] = 0.0;
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, NC, NR, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, nz,
                     as, ai, av));
    for (int k = 0; k < NX; k++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, k, q[k]));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 234.269676822, obj);
    static double x[NC], y[NR];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void catch_retry(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    if (strstr(line, "the conic interior point solves the model again") !=
        nullptr)
        *(bool *)user = true;
}

static void test_a_qp_the_barrier_cannot_finish_goes_to_the_conic_walk(void)
{
    enum { P = 101, N = 20 };
    static double cost[N], cl[N], cu[N], rl[P], ru[P], av[P * N];
    static int64_t as[N + 1], ai[P * N];
    int64_t nz = 0;
    for (int j = 0; j < N; j++) {
        as[j] = nz;
        for (int i = 0; i < P; i++) {
            const double t = i / (double)(P - 1);
            double a = 1.0;
            for (int k = 0; k < j; k++)
                a *= t;
            if (a != 0.0) {
                ai[nz] = i;
                av[nz++] = a;
            }
        }
        cost[j] = 1.0 / (j + 1);
        cl[j] = -jaos_infinity();
        cu[j] = jaos_infinity();
    }
    as[N] = nz;
    for (int i = 0; i < P; i++) {
        rl[i] = sin(i / (double)(P - 1));
        ru[i] = jaos_infinity();
    }
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, N, P, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, nz,
                     as, ai, av));
    for (int j = 0; j < N; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_col_quadratic(m, j, 1.0 / (j + 1)));
    bool retried = false;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_log_callback(m, catch_retry, &retried));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_TRUE(retried);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.575773803, obj);
    static double x[N], y[P];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void catch_rounded(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    if (strstr(line, "by the relaxation rounded and the rest solved") !=
        nullptr)
        *(bool *)user = true;
}

static jaos_model *mixed_rounding_qp(void)
{
    const double cost[] = {-0.4, -2.4, 0.0};
    const double cl[] = {0.0, 0.0, -5.0}, cu[] = {3.0, 3.0, 5.0};
    const double rl[] = {1.7}, ru[] = {1.7};
    const int64_t as[] = {0, 1, 2, 3}, ai[] = {0, 0, 0};
    const double av[] = {1.0, 1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 1.48, cost, cl, cu, rl, ru, 3,
                     as, ai, av));
    for (int j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, j, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    return m;
}

static void test_a_miqp_root_rounds_and_solves_for_the_rest(void)
{
    for (int on = 1; on >= 0; on--) {
        jaos_model *m = mixed_rounding_qp();
        bool rounded = false;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_log_callback(m, catch_rounded, &rounded));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_set_log_level(m, JAOS_LOG_PROGRESS));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, on));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0, x[3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(TOL, 0.57, obj);
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(m, x, nullptr, nullptr, nullptr));
        TEST_ASSERT_DOUBLE_WITHIN(TOL, 0.0, x[0]);
        TEST_ASSERT_DOUBLE_WITHIN(TOL, 1.0, x[1]);
        TEST_ASSERT_DOUBLE_WITHIN(TOL, 0.7, x[2]);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        TEST_ASSERT_EQUAL(on != 0, rounded);
        if (on)
            TEST_ASSERT_EQUAL_INT64(1, rep.first_incumbent_node);
        jaos_model_free(m);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_miqp_root_rounds_and_solves_for_the_rest);
    RUN_TEST(test_a_qp_the_barrier_cannot_finish_goes_to_the_conic_walk);
    RUN_TEST(test_a_qp_whose_dense_columns_lose_the_walk_still_solves);
    RUN_TEST(test_a_paired_q_reads_back_as_it_was_set);
    RUN_TEST(test_a_paired_q_solves_to_the_point_it_should);
    RUN_TEST(test_the_checker_judges_a_paired_q);
    RUN_TEST(test_the_cross_term_moves_the_answer);
    RUN_TEST(test_a_non_convex_q_is_refused);
    RUN_TEST(test_a_semi_definite_q_is_taken);
    RUN_TEST(test_a_paired_q_survives_a_copy);
    RUN_TEST(test_deleting_a_column_carries_the_pairs_with_it);
    RUN_TEST(test_set_quadratic_refuses_what_it_cannot_take);
    RUN_TEST(test_a_paired_q_is_bit_identical_across_runs);
    RUN_TEST(test_the_other_algorithms_still_refuse_a_quadratic);
    RUN_TEST(test_mps_carries_a_paired_q_both_ways);
    RUN_TEST(test_mps_reads_both_halves_when_the_file_gives_them);
    RUN_TEST(test_mps_refuses_two_halves_that_disagree);
    RUN_TEST(test_lp_carries_a_paired_q_both_ways);
    RUN_TEST(test_lp_takes_the_block_undivided);
    RUN_TEST(test_qplib_carries_a_paired_q_both_ways);
    RUN_TEST(test_osil_carries_a_paired_q_both_ways);
    RUN_TEST(test_a_paired_q_reaches_the_optimum_the_diagonal_does);
    RUN_TEST(test_a_paired_q_in_a_mip_does_not_cut_off_the_optimum);
    RUN_TEST(test_the_barrier_counts_a_fixed_column_in_its_objective);
    RUN_TEST(test_a_paired_q_maximised);
    RUN_TEST(test_the_two_quadratic_calls_reach_the_same_matrix);
    RUN_TEST(test_the_push_puts_the_answer_on_its_bound);
    RUN_TEST(test_without_the_push_the_point_stops_short_of_the_bound);
    RUN_TEST(test_a_stalled_barrier_recentres_and_finishes);
    RUN_TEST(test_an_unbounded_qp_ends_unbounded_with_its_ray);
    RUN_TEST(test_a_bounded_qp_with_free_columns_is_not_called_unbounded);
    RUN_TEST(test_a_paired_unbounded_qp_finds_the_ray_through_the_pair);
    RUN_TEST(test_the_ray_checker_refuses_a_direction_the_curvature_turns);
    RUN_TEST(test_zero_pairs_are_dropped_and_the_rest_stay_in_place);
    return UNITY_END();
}

