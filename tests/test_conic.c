/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    double x[8], s[16], z[16];
    jm_cone_result r;
} answer;

static void run(const jm_cone_problem *pb, answer *a)
{
    memset(a, 0, sizeof *a);
    a->r.x = a->x;
    a->r.s = a->s;
    a->r.z = a->z;
    jm_work w = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_cone_solve(pb, nullptr, &w, &a->r));
}

static const int64_t no_p_start[4] = {0, 0, 0, 0};

static void test_an_lp_reaches_its_optimum(void)
{
    const int64_t as[3] = {0, 2, 4};
    const int64_t ai[4] = {0, 1, 0, 2};
    const double av[4] = {1.0, -1.0, 1.0, -1.0};
    const double q[2] = {-1.0, -1.0}, b[3] = {1.0, 0.0, 0.0};
    const jm_cone_problem pb = {
        .n = 2, .m = 3, .p_start = no_p_start, .p_index = nullptr,
        .p_value = nullptr, .q = q, .a_start = as, .a_index = ai,
        .a_value = av, .b = b, .nzero = 0, .nnonneg = 3, .nsoc = 0,
        .soc_dim = nullptr};
    answer a;
    run(&pb, &a);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, a.r.status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -1.0, a.r.pobj);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 1.0, a.x[0] + a.x[1]);
}

static void test_a_cone_gives_the_norm(void)
{
    const int64_t as[4] = {0, 1, 3, 5};
    const int64_t ai[5] = {2, 0, 3, 1, 4};
    const double av[5] = {-1.0, 1.0, -1.0, 1.0, -1.0};
    const double q[3] = {1.0, 0.0, 0.0}, b[5] = {3.0, 4.0, 0.0, 0.0, 0.0};
    const int64_t dims[1] = {3};
    const jm_cone_problem pb = {
        .n = 3, .m = 5, .p_start = no_p_start, .p_index = nullptr,
        .p_value = nullptr, .q = q, .a_start = as, .a_index = ai,
        .a_value = av, .b = b, .nzero = 2, .nnonneg = 0, .nsoc = 1,
        .soc_dim = dims};
    answer a;
    run(&pb, &a);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, a.r.status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.0, a.x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 3.0, a.x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 4.0, a.x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.0, a.r.pobj);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.0, a.r.dobj);
}

static void test_a_quadratic_objective_stops_at_its_bound(void)
{
    const int64_t ps[2] = {0, 1}, pi[1] = {0};
    const double pv[1] = {1.0};
    const int64_t as[2] = {0, 1}, ai[1] = {0};
    const double av[1] = {1.0};
    const double q[1] = {-3.0}, b[1] = {2.0};
    const jm_cone_problem pb = {
        .n = 1, .m = 1, .p_start = ps, .p_index = pi, .p_value = pv, .q = q,
        .a_start = as, .a_index = ai, .a_value = av, .b = b, .nzero = 0,
        .nnonneg = 1, .nsoc = 0, .soc_dim = nullptr};
    answer a;
    run(&pb, &a);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, a.r.status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 2.0, a.x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -4.0, a.r.pobj);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, a.z[0]);
}

static void test_an_infeasible_pair_of_rows_is_certified(void)
{
    const int64_t as[2] = {0, 2}, ai[2] = {0, 1};
    const double av[2] = {-1.0, 1.0};
    const double q[1] = {0.0}, b[2] = {-1.0, 0.0};
    const jm_cone_problem pb = {
        .n = 1, .m = 2, .p_start = no_p_start, .p_index = nullptr,
        .p_value = nullptr, .q = q, .a_start = as, .a_index = ai,
        .a_value = av, .b = b, .nzero = 0, .nnonneg = 2, .nsoc = 0,
        .soc_dim = nullptr};
    answer a;
    run(&pb, &a);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, a.r.status);
    TEST_ASSERT_TRUE(a.z[0] >= 0.0 && a.z[1] >= 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -1.0, b[0] * a.z[0] + b[1] * a.z[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 0.0, -a.z[0] + a.z[1]);
}

static void test_an_unbounded_objective_gives_its_ray(void)
{
    const int64_t as[2] = {0, 1}, ai[1] = {0};
    const double av[1] = {-1.0};
    const double q[1] = {-1.0}, b[1] = {0.0};
    const jm_cone_problem pb = {
        .n = 1, .m = 1, .p_start = no_p_start, .p_index = nullptr,
        .p_value = nullptr, .q = q, .a_start = as, .a_index = ai,
        .a_value = av, .b = b, .nzero = 0, .nnonneg = 1, .nsoc = 0,
        .soc_dim = nullptr};
    answer a;
    run(&pb, &a);
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, a.r.status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 1.0, a.x[0]);
}

static void test_a_cone_whose_infimum_is_not_attained_ends_near_it(void)
{
    const int64_t as[3] = {0, 1, 2}, ai[2] = {0, 2};
    const double av[2] = {-1.0, -1.0};
    const double q[2] = {1.0, 1.0}, b[3] = {0.0, 1.0, 0.0};
    const int64_t dims[1] = {3};
    const jm_cone_problem pb = {
        .n = 2, .m = 3, .p_start = no_p_start, .p_index = nullptr,
        .p_value = nullptr, .q = q, .a_start = as, .a_index = ai,
        .a_value = av, .b = b, .nzero = 0, .nnonneg = 0, .nsoc = 1,
        .soc_dim = dims};
    answer a;
    run(&pb, &a);
    TEST_ASSERT_TRUE(a.r.status == JAOS_SOLVE_OPTIMAL ||
                     a.r.status == JAOS_SOLVE_NUMERICAL_ERROR);
    if (a.r.status == JAOS_SOLVE_OPTIMAL)
        TEST_ASSERT_DOUBLE_WITHIN(1e-5, 0.0, a.r.pobj);
}

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static void assert_checked(jaos_model *m, bool with_cones)
{
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double x[16], y[16], z[16];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nr > 0 ? y : nullptr,
                                                 nullptr));
    jaos_check_report rep;
    if (with_cones) {
        int64_t at = 0;
        for (int64_t k = 0; k < jaos_num_cones(m); k++) {
            int64_t n = 0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, k, nullptr, &n, nullptr));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, k, z + at));
            at += n;
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_check_conic_solution(m, x, y, z, 1e-6, &rep));
    } else {
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &rep));
    }
    TEST_ASSERT_TRUE_MESSAGE(rep.primal_feasible, "primal side refused");
    TEST_ASSERT_TRUE_MESSAGE(rep.checked_duals, "dual side not checked");
    TEST_ASSERT_TRUE_MESSAGE(rep.dual_feasible, "dual side refused");
    (void)nc;
}

static jaos_model *norm_model(jaos_obj_sense sense)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double sg = sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double cost[3] = {sg, 0.0, 0.0};
    const double cl[3] = {-inf, -inf, -inf}, cu[3] = {inf, inf, inf};
    const double rl[2] = {3.0, 4.0}, ru[2] = {3.0, 4.0};
    const int64_t as[4] = {0, 0, 1, 2}, ai[2] = {0, 1};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, sense, 0.0, cost, cl, cu, rl, ru, 2, as, ai, av));
    const int64_t cols[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cone(m, JAOS_CONE_QUADRATIC, 3, cols));
    return m;
}

static void test_a_cone_on_the_model_gives_the_norm(void)
{
    jaos_model *m = norm_model(JAOS_MINIMIZE);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.0, obj);
    double z[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, z[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -0.6, z[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -0.8, z[2]);
    assert_checked(m, true);
    jaos_model_free(m);
}

static void test_a_maximised_cone_publishes_its_duals_the_right_way(void)
{
    jaos_model *m = norm_model(JAOS_MAXIMIZE);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -5.0, obj);
    assert_checked(m, true);
    jaos_model_free(m);
}

static void test_a_rotated_cone_halves_the_square(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[3] = {1.0, 0.0, 0.0};
    const double cl[3] = {0.0, 1.0, 3.0}, cu[3] = {inf, 1.0, 3.0};
    const int64_t as[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    const int64_t cols[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_ROTATED, 3, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 4.5, obj);
    assert_checked(m, true);
    jaos_model_free(m);
}

static void test_a_quadratic_row_bends_the_optimum(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {-1.0, -1.0};
    const double cl[2] = {-inf, -inf}, cu[2] = {inf, inf};
    const double rl[1] = {-inf}, ru[1] = {2.0};
    const int64_t as[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 0, as,
                     nullptr, nullptr));
    const int64_t qr[2] = {0, 1}, qc[2] = {0, 1};
    const double qv[2] = {2.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_quadratic(m, 0, 2, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT64(2, jaos_row_quadratic_nz(m, 0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double x[2], y[1], act[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 2.0, act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -0.5, y[0]);
    assert_checked(m, false);
    jaos_model_free(m);
}

static void test_a_concave_row_on_its_lower_side_is_convex(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[1] = {1.0}, cl[1] = {-inf}, cu[1] = {inf};
    const double rl[1] = {-4.0}, ru[1] = {inf};
    const int64_t as[2] = {0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru, 0, as,
                     nullptr, nullptr));
    const int64_t q0[1] = {0};
    const double qv[1] = {-2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_quadratic(m, 0, 1, q0, q0, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 2.0, obj);
    assert_checked(m, false);
    jaos_model_free(m);
}

static void test_a_non_convex_quadratic_row_is_refused(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[1] = {1.0}, cl[1] = {-inf}, cu[1] = {inf};
    const double rl[1] = {1.0}, ru[1] = {inf};
    const int64_t as[2] = {0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 0, as,
                     nullptr, nullptr));
    const int64_t q0[1] = {0};
    const double qv[1] = {2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_quadratic(m, 0, 1, q0, q0, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "not convex"));
    jaos_model_free(m);
}

static void test_a_refused_certificate_is_reweighted_until_it_holds(void)
{
    const char *path[2] = {"tests/data/g_cert_tilt.mps",
                           "tests/data/g_cert_climb.mps"};
    for (int k = 0; k < 2; k++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, path[k]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_INFEASIBLE,
                                      jaos_status_of(m), path[k]);
        const int64_t nr = jaos_num_row(m), nk = jaos_num_cones(m);
        double *y = jm_alloc_array(nr > 0 ? nr : 1, sizeof *y);
        TEST_ASSERT_NOT_NULL(y);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_certificate(m, y),
                                      path[k]);
        int64_t members = 0;
        for (int64_t c = 0; c < nk; c++) {
            jaos_cone_type ty;
            int64_t n = 0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_cone(m, c, &ty, &n, nullptr));
            members += n;
        }
        double *z = jm_alloc_array(members > 0 ? members : 1, sizeof *z);
        TEST_ASSERT_NOT_NULL(z);
        for (int64_t c = 0, at = 0; c < nk; c++) {
            jaos_cone_type ty;
            int64_t n = 0;
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jaos_cone(m, c, &ty, &n, nullptr));
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, c, z + at));
            at += n;
        }
        jaos_certificate_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_check_conic_certificate(m, y, nk > 0 ? z : nullptr, 1e-7,
                                         &rep));
        TEST_ASSERT_TRUE_MESSAGE(rep.certified, path[k]);
        free(y);
        free(z);
        jaos_model_free(m);
    }
}

static void test_an_infeasible_cone_is_certified(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {0.0, 0.0};
    const double cl[2] = {-inf, 2.0}, cu[2] = {1.0, 2.0};
    const int64_t as[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    const int64_t cols[2] = {0, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cone(m, JAOS_CONE_QUADRATIC, 2, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    double z[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    jaos_certificate_report rep;
    const double y[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_certificate(m, y, z, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    const double wrong[2] = {-1.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_certificate(m, y, wrong, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    jaos_model_free(m);
}

static void test_an_unbounded_cone_gives_a_ray_in_the_cone(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {-1.0, 0.0};
    const double cl[2] = {-inf, -inf}, cu[2] = {inf, inf};
    const int64_t as[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    const int64_t cols[2] = {0, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cone(m, JAOS_CONE_QUADRATIC, 2, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    double d[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    jaos_ray_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.certified);
    const double bad[2] = {0.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, bad, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.certified);
    jaos_model_free(m);
}

static void test_cones_survive_a_copy_and_guard_their_columns(void)
{
    jaos_model *m = norm_model(JAOS_MINIMIZE);
    jaos_model *c = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &c));
    TEST_ASSERT_EQUAL_INT64(1, jaos_num_cones(c));
    jaos_cone_type ty;
    int64_t n = 0, cols[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(c, 0, &ty, &n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_CONE_QUADRATIC, ty);
    TEST_ASSERT_EQUAL_INT64(3, n);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(c));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(c, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5.0, obj);

    const int64_t del[1] = {2};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_delete_cols(m, 1, del));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "cone"));
    const int64_t which[1] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cones(m, 1, which));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_cones(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, del));
    jaos_model_free(c);
    jaos_model_free(m);
}

static void test_a_quadratic_row_follows_deletes_and_copies(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[3] = {1.0, -1.0, -1.0};
    const double cl[3] = {0.0, -inf, -inf}, cu[3] = {1.0, inf, inf};
    const double rl[2] = {-inf, -inf}, ru[2] = {1.0, 2.0};
    const int64_t as[4] = {0, 1, 1, 1}, ai[1] = {0};
    const double av[1] = {1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 1, as,
                     ai, av));
    const int64_t qr[2] = {1, 2}, qc[2] = {1, 2};
    const double qv[2] = {2.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_quadratic(m, 1, 2, qr, qc, qv));
    const int64_t drow[1] = {0}, dcol[1] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_rows(m, 1, drow));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_delete_cols(m, 1, dcol));
    TEST_ASSERT_EQUAL_INT64(2, jaos_row_quadratic_nz(m, 0));
    int64_t r[2], c[2];
    double v[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_quadratic(m, 0, r, c, v));
    TEST_ASSERT_EQUAL_INT64(0, r[0]);
    TEST_ASSERT_EQUAL_INT64(1, r[1]);
    jaos_model *cp = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_copy(m, &cp));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(cp));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(cp));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(cp, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -2.0, obj);
    jaos_model_free(cp);
    jaos_model_free(m);
}

static void test_a_conic_answer_goes_through_the_solution_file(void)
{
    jaos_model *m = norm_model(JAOS_MINIMIZE);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, "build/tc_norm.sol"));
    double x[3], y[2], z[3], held[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_solution(m, "build/tc_norm.sol", nullptr, x, nullptr,
                           nullptr, nullptr, y, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_cone_duals(m, "build/tc_norm.sol", z));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, held));
    for (int t = 0; t < 3; t++)
        TEST_ASSERT_EQUAL_DOUBLE(held[t], z[t]);
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_solution(m, x, y, z, 1e-7, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    z[0] = -1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_solution(m, x, y, z, 1e-7, &rep));
    TEST_ASSERT_FALSE(rep.dual_feasible);

    FILE *f = fopen("build/tc_short.sol", "wb");
    TEST_ASSERT_NOT_NULL(f);
    fputs("status optimal\nobjective 5\ncolumns 3\nrows 2\ncones 1\n"
          "col C1 5 0 basic\ncol C2 3 0 basic\ncol C3 4 0 basic\n"
          "row R1 3 0 basic\nrow R2 4 0 basic\n"
          "cone 0 0 1\ncone 0 1 -0.6\nend\n", f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_cone_duals(m, "build/tc_short.sol", z));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "3 members"));
    f = fopen("build/tc_short.sol", "wb");
    TEST_ASSERT_NOT_NULL(f);
    fputs("status optimal\nobjective 5\ncolumns 3\nrows 2\ncones 1\n"
          "cone 0 1 1\nend\n", f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_read_cone_duals(m, "build/tc_short.sol", z));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "member 0"));
    remove("build/tc_short.sol");
    remove("build/tc_norm.sol");
    jaos_model_free(m);

    m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {0.0, 0.0};
    const double cl[2] = {-inf, 2.0}, cu[2] = {1.0, 2.0};
    const int64_t as[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    const int64_t cols[2] = {0, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cone(m, JAOS_CONE_QUADRATIC, 2, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_write_solution(m, "build/tc_inf.sol"));
    double ray[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_cone_duals(m, "build/tc_inf.sol", ray));
    jaos_certificate_report crep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_certificate(m, nullptr, ray, 1e-7, &crep));
    TEST_ASSERT_TRUE(crep.certified);
    jaos_iis_side rs[1], cs[2];
    jaos_iis_report irep;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_iis(m, rs, cs, &irep));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "cones"));
    remove("build/tc_inf.sol");
    jaos_model_free(m);
}

static void assert_integer_answer(jaos_model *m, double want)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, want, obj);
    jaos_mip_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &r));
    TEST_ASSERT_TRUE(r.has_incumbent);
    TEST_ASSERT_TRUE(r.nodes > 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, want, r.incumbent);
    const int64_t nc = jaos_num_col(m);
    double x[16], y[16], z[16];
    TEST_ASSERT_TRUE(nc <= 16 && jaos_num_row(m) <= 16);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    for (int64_t j = 0; j < nc; j++) {
        bool integer = false;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_integer(m, j, &integer));
        if (integer)
            TEST_ASSERT_EQUAL_DOUBLE(round(x[j]), x[j]);
    }
    int64_t at = 0;
    for (int64_t k = 0; k < jaos_num_cones(m); k++) {
        int64_t n = 0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone(m, k, nullptr, &n, nullptr));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, k, z + at));
        at += n;
    }
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_solution(m, x, y, at > 0 ? z : nullptr, 1e-7, &rep));
    TEST_ASSERT_TRUE_MESSAGE(rep.primal_feasible, "primal side refused");
    TEST_ASSERT_TRUE(rep.max_integrality_violation == 0.0);
}

static void test_integer_columns_in_a_cone_branch_to_the_optimum(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[5] = {1.0, 0.0, 0.0, 0.0, 0.0};
    const double cl[5] = {0.0, 0.0, 0.0, -inf, -inf};
    const double cu[5] = {inf, 5.0, 5.0, inf, inf};
    const double rl[2] = {1.6, 2.3}, ru[2] = {1.6, 2.3};
    const int64_t as[6] = {0, 0, 1, 2, 3, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {1.0, 1.0, -1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 4, as,
                     ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 2, true));
    const int64_t cols[3] = {0, 3, 4};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 3, cols));
    assert_integer_answer(m, 0.5);
    double x[5];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, x[1]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, x[2]);
    jaos_model_free(m);
}

static void test_integer_columns_under_a_quadratic_row(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[2] = {1.0, 1.0};
    const double cl[2] = {0.0, 0.0}, cu[2] = {inf, inf};
    const double rl[1] = {-inf}, ru[1] = {10.0};
    const int64_t as[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, cost, cl, cu, rl, ru, 0, as,
                     nullptr, nullptr));
    const int64_t qr[2] = {0, 1}, qc[2] = {0, 1};
    const double qv[2] = {2.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_quadratic(m, 0, 2, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 0, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    assert_integer_answer(m, 4.0);
    jaos_model_free(m);
}

static void test_an_integer_point_outside_every_cone_is_infeasible(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[3] = {1.0, 0.0, 0.0};
    const double cl[3] = {0.0, -3.0, -inf}, cu[3] = {0.4, 3.0, inf};
    const double rl[1] = {0.5}, ru[1] = {0.5};
    const int64_t as[4] = {0, 0, 1, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 2, as,
                     ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    const int64_t cols[2] = {0, 2};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 2, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_mip_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &r));
    TEST_ASSERT_FALSE(r.has_incumbent);
    TEST_ASSERT_TRUE(r.nodes >= 3);
    jaos_model_free(m);
}

static jaos_model *nearest_point_model(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[5] = {1.0, 0.0, 0.0, 0.0, 0.0};
    const double cl[5] = {0.0, 0.0, 0.0, -inf, -inf};
    const double cu[5] = {inf, 5.0, 5.0, inf, inf};
    const double rl[2] = {1.6, 2.3}, ru[2] = {1.6, 2.3};
    const int64_t as[6] = {0, 0, 1, 2, 3, 4}, ai[4] = {0, 1, 0, 1};
    const double av[4] = {1.0, 1.0, -1.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 5, 2, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 4, as,
                     ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 2, true));
    const int64_t cols[3] = {0, 3, 4};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 3, cols));
    return m;
}

static void test_the_rounded_root_is_an_incumbent_at_node_1(void)
{
    jaos_model *m = nearest_point_model();
    assert_integer_answer(m, 0.5);
    jaos_mip_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &r));
    TEST_ASSERT_EQUAL_INT64(1, r.first_incumbent_node);
    TEST_ASSERT_TRUE(r.heuristic_points >= 1);
    jaos_model_free(m);

    m = nearest_point_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    assert_integer_answer(m, 0.5);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &r));
    TEST_ASSERT_TRUE(r.first_incumbent_node > 1);
    TEST_ASSERT_EQUAL_INT64(0, r.heuristic_points);
    jaos_model_free(m);
}

static void test_the_root_dive_finds_a_cardinality_point(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cost[9] = {0, 0, 0, 0, 0, 0, 0, 0, 1.0};
    const double cl[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    const double cu[9] = {1, 1, 1, 1, 1, 1, 1, 1, inf};
    const double rl[6] = {-inf, -inf, -inf, -inf, 1.0, -inf};
    const double ru[6] = {0.0, 0.0, 0.0, 0.0, 1.0, 2.0};
    const int64_t as[10] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 16};
    const int64_t ai[16] = {0, 4, 1, 4, 2, 4, 3, 4, 0, 5, 1, 5, 2, 5, 3, 5};
    const double av[16] = {1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, 1, -1, 1, -1, 1};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 9, 6, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, 16,
                     as, ai, av));
    for (int64_t j = 4; j < 8; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    const int64_t cols[5] = {8, 0, 1, 2, 3};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 5, cols));
    assert_integer_answer(m, sqrt(0.5));
    jaos_mip_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &r));
    TEST_ASSERT_EQUAL_INT64(1, r.first_incumbent_node);
    TEST_ASSERT_TRUE(r.heuristic_points >= 1);
    jaos_model_free(m);
}

static void test_an_sos_set_beside_a_cone_is_refused(void)
{
    jaos_model *m = norm_model(JAOS_MINIMIZE);
    const int64_t sc[2] = {1, 2};
    const double sw[2] = {1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_sos(m, 1, 2, sc, sw));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(m));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "SOS"));
    jaos_model_free(m);
}

enum { WIDE = 500 };

static double wide_coef(int64_t i)
{
    return (i % 2 == 0 ? 1.0 : -1.0) * (double)(1 + i % 5);
}

static void assert_wide_checked(jaos_model *m, int64_t members)
{
    const int64_t nc = jaos_num_col(m);
    double *x = calloc((size_t)nc, sizeof *x);
    double *z = calloc((size_t)members, sizeof *z);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(z);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    jaos_check_report rep;
    double y[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_solution(m, x, y, z, 1e-9, &rep));
    TEST_ASSERT_TRUE_MESSAGE(rep.primal_feasible, "primal side refused");
    TEST_ASSERT_TRUE_MESSAGE(rep.dual_feasible, "dual side refused");
    free(x);
    free(z);
}

static void test_a_wide_cone_gives_the_norm(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const int64_t n = WIDE + 1;
    double *cost = calloc((size_t)n, sizeof *cost);
    double *cl = calloc((size_t)n, sizeof *cl);
    double *cu = calloc((size_t)n, sizeof *cu);
    int64_t *as = calloc((size_t)n + 1, sizeof *as);
    int64_t *cols = calloc((size_t)n, sizeof *cols);
    TEST_ASSERT_NOT_NULL(cols);
    double aa = 0.0;
    cl[0] = cu[0] = 1.0;
    for (int64_t j = 1; j < n; j++) {
        cost[j] = -wide_coef(j);
        cl[j] = -inf;
        cu[j] = inf;
        aa += wide_coef(j) * wide_coef(j);
    }
    for (int64_t j = 0; j < n; j++)
        cols[j] = j;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, n, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * sqrt(aa), -sqrt(aa), obj);
    assert_wide_checked(m, n);
    free(cost); free(cl); free(cu); free(as); free(cols);
    jaos_model_free(m);
}

static void test_a_wide_rotated_cone_gives_the_sum_of_squares(void)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const int64_t n = WIDE + 2;
    double *cost = calloc((size_t)n, sizeof *cost);
    double *cl = calloc((size_t)n, sizeof *cl);
    double *cu = calloc((size_t)n, sizeof *cu);
    int64_t *as = calloc((size_t)n + 1, sizeof *as);
    int64_t *cols = calloc((size_t)n, sizeof *cols);
    TEST_ASSERT_NOT_NULL(cols);
    double aa = 0.0;
    cost[0] = 1.0;
    cu[0] = inf;
    cl[1] = cu[1] = 0.5;
    for (int64_t j = 2; j < n; j++) {
        cl[j] = cu[j] = wide_coef(j);
        aa += wide_coef(j) * wide_coef(j);
    }
    for (int64_t j = 0; j < n; j++)
        cols[j] = j;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, n, 0, JAOS_MINIMIZE, 0.0, cost, cl, cu, nullptr,
                     nullptr, 0, as, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_ROTATED, n, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * aa, aa, obj);
    assert_wide_checked(m, n);
    free(cost); free(cl); free(cu); free(as); free(cols);
    jaos_model_free(m);
}

static void catch_left_out(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    if (strstr(line, "the walk leaves out") != nullptr ||
        strstr(line, "every cone is left out") != nullptr)
        (*(int *)user)++;
}

static jaos_model *badly_scaled_box(bool live)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_read_mps(m, live ? "tests/data/g_cone_badbox.mps"
                              : "tests/data/g_cone_dropbox.mps"));
    return m;
}

static void test_a_model_whose_cones_all_go_leaves_the_walk(void)
{
    int left_out = 0;
    jaos_model *m = badly_scaled_box(false);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, catch_left_out,
                                                         &left_out));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, left_out);
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6 * 73622257.83, 73622257.83, obj);
    double z[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, z[0]);
    assert_checked(m, true);
    jaos_model_free(m);
}

static void catch_alone(void *user, jaos_log_level level, const char *line)
{
    (void)level;
    if (strstr(line, "touch no row, no cone and no other column") != nullptr)
        (*(int *)user)++;
}

static void test_columns_that_touch_nothing_take_their_own_minimiser(void)
{
    int alone = 0;
    jaos_model *m = badly_scaled_box(true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, catch_alone,
                                                         &alone));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, alone);
    double obj = 0.0, x[18];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * 73622258.83, 73622258.83, obj);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr,
                                                 nullptr));
    TEST_ASSERT_EQUAL_DOUBLE(1736510.0, x[11]);
    TEST_ASSERT_EQUAL_DOUBLE(1838820000.0, x[13]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, x[1]);
    double y[1] = {0.0}, z[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 1, z + 1));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_conic_solution(m, x, y, z, 1e-9, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    jaos_model_free(m);
}

static void test_a_failed_leaf_is_set_aside_and_the_tree_goes_on(void)
{
    jaos_model *m = badly_scaled_box(true);
    const double one = 1.0, zero = 0.0, inf = jaos_infinity();
    const int64_t rs[2] = {0, 2}, ri[2] = {11, 13};
    const double rv[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_rows(m, 1, &zero, &inf, 2, rs, ri, rv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_add_cols(m, 1, &one, &zero, &one, 0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_set_col_integer(m, jaos_num_col(m) - 1, true));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NUMERICAL_ERROR, jaos_status_of(m));
    jaos_mip_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &r));
    TEST_ASSERT_EQUAL_INT64(3, r.nodes);
    TEST_ASSERT_FALSE(r.has_incumbent);
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "node 2: "));
    jaos_model_free(m);
}

static void test_a_refused_direction_is_replaced_by_a_solve_over_directions(void)
{
    const char *path[2] = {"tests/data/g_ray_probe.mps",
                           "tests/data/g_ray_stall.mps"};
    for (int k = 0; k < 2; k++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, path[k]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m),
                                      path[k]);
        const int64_t nc = jaos_num_col(m);
        double *d = jm_alloc_array(nc, sizeof *d);
        TEST_ASSERT_NOT_NULL(d);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
        jaos_ray_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-6, &rep));
        TEST_ASSERT_TRUE_MESSAGE(rep.certified, "the ray is not certified");
        TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.max_col_escape);
        TEST_ASSERT_EQUAL_DOUBLE(0.0, rep.max_row_escape);
        free(d);
        jaos_model_free(m);
    }
}

static void test_a_refused_certificate_is_no_verdict_without_quadratic_rows(void)
{
    jaos_model *m = badly_scaled_box(true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const jaos_solve_status st = jaos_status_of(m);
    TEST_ASSERT_NOT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, st);
    if (st == JAOS_SOLVE_OPTIMAL) {
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        TEST_ASSERT_DOUBLE_WITHIN(1e-6 * 73622258.83, 73622258.83, obj);
    } else {
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NUMERICAL_ERROR, st);
        TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "does not confirm"));
    }
    jaos_model_free(m);
}

static jaos_model *dead_cone_model(jaos_obj_sense sense, double head_cost,
                                   int *left_out)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double sg = sense == JAOS_MAXIMIZE ? -1.0 : 1.0;
    const double cost[6] = {sg * head_cost, sg * 2.0, 0.0, sg * 1.0,
                            sg * 1.0, 0.0};
    const double cl[6] = {-1.0, -inf, -inf, 0.0, -inf, 1.0};
    const double cu[6] = {0.0, inf, inf, 2.0, inf, 1.0};
    const double rl[1] = {1.0}, ru[1] = {inf};
    const int64_t as[7] = {0, 0, 1, 1, 2, 2, 2}, ai[2] = {0, 0};
    const double av[2] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 6, 1, sense, 0.0, cost, cl, cu, rl, ru, 2, as, ai, av));
    const int64_t cols[3] = {0, 1, 2}, live[2] = {4, 5};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 3, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 2, live));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, catch_left_out,
                                                         left_out));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    return m;
}

static void test_a_cone_held_at_its_tip_is_left_out_of_the_walk(void)
{
    const jaos_obj_sense senses[2] = {JAOS_MINIMIZE, JAOS_MAXIMIZE};
    const double head_costs[2] = {0.0, 5.0};
    for (int s = 0; s < 2; s++)
        for (int h = 0; h < 2; h++) {
            int left_out = 0;
            jaos_model *m = dead_cone_model(senses[s], head_costs[h], &left_out);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
            TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
            TEST_ASSERT_EQUAL_INT(1, left_out);
            double obj = 0.0, x[6], z[3];
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, s == 0 ? 2.0 : -2.0, obj);
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                                  jaos_solution(m, x, nullptr, nullptr, nullptr));
            TEST_ASSERT_EQUAL_DOUBLE(0.0, x[0]);
            TEST_ASSERT_EQUAL_DOUBLE(0.0, x[1]);
            TEST_ASSERT_EQUAL_DOUBLE(0.0, x[2]);
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, x[3]);
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, x[4]);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
            const double sg = s == 0 ? 1.0 : -1.0;
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, h == 0 ? 1.0 : 5.0, sg * z[0]);
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, sg * z[1]);
            TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, z[2]);
            assert_checked(m, true);
            jaos_model_free(m);
        }
}

static jaos_model *idle_cone_model(const double *rl, const double *ru,
                                   int64_t nr, const int64_t *as,
                                   const int64_t *ai, const double *av,
                                   const double *cost, int *left_out)
{
    jaos_model *m = fresh();
    const double inf = jaos_infinity();
    const double cl[3] = {-inf, -inf, -inf}, cu[3] = {inf, inf, inf};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, nr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     as[3], as, ai, av));
    const int64_t cols[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_add_cone(m, JAOS_CONE_QUADRATIC, 3, cols));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, catch_left_out,
                                                         left_out));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    return m;
}

static void test_a_cone_whose_head_is_free_and_idle_is_left_out(void)
{
    const double inf = jaos_infinity();
    const int64_t as[4] = {0, 0, 1, 2}, ai[2] = {0, 1};
    const double av[2] = {1.0, 1.0}, cost[3] = {0.0, 1.0, 1.0};
    const double rl[2] = {1.0, 2.0}, ru[2] = {inf, inf};
    int left_out = 0;
    jaos_model *m = idle_cone_model(rl, ru, 2, as, ai, av, cost, &left_out);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, left_out);
    double x[3], z[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 2.0, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, sqrt(5.0), x[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    for (int t = 0; t < 3; t++)
        TEST_ASSERT_EQUAL_DOUBLE(0.0, z[t]);
    assert_checked(m, true);
    jaos_model_free(m);

    const int64_t bs[4] = {0, 0, 2, 4}, bi[4] = {0, 1, 0, 1};
    const double bv[4] = {1.0, 1.0, 1.0, 1.0};
    const double bl[2] = {2.0, -inf}, bu[2] = {inf, 1.0};
    left_out = 0;
    m = idle_cone_model(bl, bu, 2, bs, bi, bv, cost, &left_out);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, left_out);
    double y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_certificate(m, y));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cone_dual(m, 0, z));
    jaos_certificate_report cr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_conic_certificate(m, y, z, 1e-9,
                                                                &cr));
    TEST_ASSERT_TRUE(cr.certified);
    jaos_model_free(m);

    const int64_t es[4] = {0, 0, 0, 0};
    const double down[3] = {0.0, -1.0, 0.0};
    left_out = 0;
    m = idle_cone_model(nullptr, nullptr, 0, es, nullptr, nullptr, down,
                        &left_out);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, left_out);
    double d[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_unbounded_ray(m, d));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12 * fabs(d[1]), fabs(d[1]), d[0]);
    jaos_ray_report rr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_ray(m, d, 1e-9, &rr));
    TEST_ASSERT_TRUE(rr.certified);
    jaos_model_free(m);
}

static jaos_model *weighted_rounding(void)
{
    enum { N = 6, C = 1 + 2 * N };
    static const double w[N] = {1.0, 1.0, 1.0, 100.0, 100.0, 100.0};
    static const double a[N] = {0.5, 1.5, 2.5, 0.3, 1.3, 2.7};
    const double inf = jaos_infinity();
    double cost[C] = {0}, cl[C], cu[C], rl[N], ru[N], av[2 * N];
    int64_t as[C + 1], ai[2 * N];
    cl[0] = -inf;
    cu[0] = inf;
    cost[0] = 1.0;
    int64_t nz = 0;
    as[0] = 0;
    for (int j = 0; j < N; j++) {
        cl[1 + j] = 0.0;
        cu[1 + j] = 5.0;
        as[1 + j] = nz;
        ai[nz] = j;
        av[nz++] = -w[j];
        rl[j] = ru[j] = -w[j] * a[j];
    }
    for (int j = 0; j < N; j++) {
        cl[1 + N + j] = -inf;
        cu[1 + N + j] = inf;
        as[1 + N + j] = nz;
        ai[nz] = j;
        av[nz++] = 1.0;
    }
    as[C] = nz;
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, C, N, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru, nz,
                     as, ai, av));
    for (int j = 0; j < N; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, 1 + j, true));
    int64_t cone[1 + N];
    cone[0] = 0;
    for (int j = 0; j < N; j++)
        cone[1 + j] = 1 + N + j;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jaos_add_cone(m, JAOS_CONE_QUADRATIC, 1 + N, cone));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_heuristics(m, false));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_dive_heuristic(m, 0));
    return m;
}

static void test_the_conic_tree_learns_which_columns_move_the_bound(void)
{
    int64_t nodes[2];
    for (int rule = 0; rule < 2; rule++) {
        jaos_model *m = weighted_rounding();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_mip_branching(m, rule == 0 ? JAOS_BRANCH_PSEUDOCOST
                                                : JAOS_BRANCH_MOST_FRACTIONAL));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        double obj = 0.0;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
        const double want = sqrt(3 * 0.25 + 3 * 900.0);
        TEST_ASSERT_DOUBLE_WITHIN(1e-6 * want, want, obj);
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        nodes[rule] = rep.nodes;
        jaos_model_free(m);
    }
    TEST_ASSERT_TRUE(nodes[0] < nodes[1]);
}

static void test_the_conic_tree_answers_the_same_on_any_thread_count(void)
{
    int64_t nodes[2], work[2];
    double obj[2], x[2][64];
    const int64_t threads[2] = {1, 4};
    for (int t = 0; t < 2; t++) {
        jaos_model *m = weighted_rounding();
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_tree_batch(m, 4));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_threads(m, threads[t]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        TEST_ASSERT_TRUE(jaos_num_col(m) <= 64);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[t]));
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
                              jaos_solution(m, x[t], nullptr, nullptr, nullptr));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        nodes[t] = rep.nodes;
        work[t] = jaos_work_units(m);
        if (t == 1)
            TEST_ASSERT_EQUAL_MEMORY(x[0], x[1],
                                     (size_t)jaos_num_col(m) * sizeof x[0][0]);
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_INT64(nodes[0], nodes[1]);
    TEST_ASSERT_EQUAL_INT64(work[0], work[1]);
    TEST_ASSERT_EQUAL_MEMORY(&obj[0], &obj[1], sizeof obj[0]);

    jaos_model *one = weighted_rounding();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(one));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(one));
    jaos_mip_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(one, &rep));
    TEST_ASSERT_TRUE_MESSAGE(rep.nodes != nodes[0],
                             "a round of four took the same nodes as one");
    double lone = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(one, &lone));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * lone, lone, obj[0]);
    jaos_model_free(one);

    jaos_model *bad = weighted_rounding();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_set_mip_tree_batch(bad, 0));
    jaos_model_free(bad);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_refused_direction_is_replaced_by_a_solve_over_directions);
    RUN_TEST(test_the_conic_tree_answers_the_same_on_any_thread_count);
    RUN_TEST(test_a_model_whose_cones_all_go_leaves_the_walk);
    RUN_TEST(test_columns_that_touch_nothing_take_their_own_minimiser);
    RUN_TEST(test_a_failed_leaf_is_set_aside_and_the_tree_goes_on);
    RUN_TEST(test_a_cone_held_at_its_tip_is_left_out_of_the_walk);
    RUN_TEST(test_a_cone_whose_head_is_free_and_idle_is_left_out);
    RUN_TEST(test_the_conic_tree_learns_which_columns_move_the_bound);
    RUN_TEST(test_a_refused_certificate_is_no_verdict_without_quadratic_rows);
    RUN_TEST(test_integer_columns_in_a_cone_branch_to_the_optimum);
    RUN_TEST(test_integer_columns_under_a_quadratic_row);
    RUN_TEST(test_an_integer_point_outside_every_cone_is_infeasible);
    RUN_TEST(test_the_rounded_root_is_an_incumbent_at_node_1);
    RUN_TEST(test_the_root_dive_finds_a_cardinality_point);
    RUN_TEST(test_an_sos_set_beside_a_cone_is_refused);
    RUN_TEST(test_a_wide_cone_gives_the_norm);
    RUN_TEST(test_a_wide_rotated_cone_gives_the_sum_of_squares);
    RUN_TEST(test_a_cone_on_the_model_gives_the_norm);
    RUN_TEST(test_a_maximised_cone_publishes_its_duals_the_right_way);
    RUN_TEST(test_a_rotated_cone_halves_the_square);
    RUN_TEST(test_a_quadratic_row_bends_the_optimum);
    RUN_TEST(test_a_concave_row_on_its_lower_side_is_convex);
    RUN_TEST(test_a_non_convex_quadratic_row_is_refused);
    RUN_TEST(test_a_refused_certificate_is_reweighted_until_it_holds);
    RUN_TEST(test_an_infeasible_cone_is_certified);
    RUN_TEST(test_an_unbounded_cone_gives_a_ray_in_the_cone);
    RUN_TEST(test_cones_survive_a_copy_and_guard_their_columns);
    RUN_TEST(test_a_quadratic_row_follows_deletes_and_copies);
    RUN_TEST(test_a_conic_answer_goes_through_the_solution_file);
    RUN_TEST(test_an_lp_reaches_its_optimum);
    RUN_TEST(test_a_cone_gives_the_norm);
    RUN_TEST(test_a_quadratic_objective_stops_at_its_bound);
    RUN_TEST(test_an_infeasible_pair_of_rows_is_certified);
    RUN_TEST(test_an_unbounded_objective_gives_its_ray);
    RUN_TEST(test_a_cone_whose_infimum_is_not_attained_ends_near_it);
    return UNITY_END();
}
