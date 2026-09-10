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

/* minimise  1/2 (2 x0^2 + 2 x1^2 + 2 x0 x1) - 3 x0 - 3 x1
   subject to x0 + x1 <= 4, both in [0, 3].

   Q = [[2, 1], [1, 2]] is positive definite, so the objective is convex.
   Its unconstrained minimum solves Q x = (3, 3), which is x = (1, 1),
   inside every bound; the objective there is 1/2 (2 + 2 + 2) - 6 = -3. */
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

/* The cross term is what a diagonal Q cannot say.  With the same
   diagonal and no pairing the optimum is elsewhere, so a solver that
   quietly dropped the off-diagonal entry would land there instead. */
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

/* QUADOBJ names the lower triangle once and QMATRIX names both halves.
   One rule reads either: a pair given twice has to agree. */
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

/* The LP dialect writes the objective as c'x + [ ... ] / 2, so a square
   carries Q[j][j] and a product carries twice Q[i][j]: the block is
   halved and the pair counts on both sides of the diagonal. */
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

/* Without the / 2 the block is not halved, so the same Q needs half the
   coefficients. Both spellings have to reach the same model. */
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

int main(void)
{
    UNITY_BEGIN();
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
    return UNITY_END();
}

