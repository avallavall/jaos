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

/* minimise -x - y over x + y <= 1, x, y >= 0: the optimum is -1 on the
   whole edge; the slacks sit in one nonnegative block of three. */
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

/* minimise t over (t, x, y) in the second-order cone with x = 3 and
   y = 4 as two rows of the zero cone: t is the norm, 5. */
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

/* minimise 1/2 x^2 - 3 x over x <= 2: the free minimum 3 is cut off and
   the optimum is x = 2 at -4, the bound's multiplier 1. */
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

/* x >= 1 and x <= 0 as two nonnegative slacks: no x meets both, and the
   certificate adds the two rows to 0 <= -1. */
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

/* minimise -x over x >= 0: the ray is x itself. */
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

/* minimise x + y over the cone x >= ||(1, y)|| with x, y free: along the
   boundary x = sqrt(1 + y^2), so x + y falls to 0 without reaching it and
   the optimum is 0, not attained. The walk must end near it, not refuse. */
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

/* minimise t over (t, x, y) in the cone with the rows x = 3 and y = 4. */
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

/* minimise t with 2 t u >= x^2, u fixed at 1 and x fixed at 3: t = 4.5. */
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

/* minimise -x - y over x^2 + y^2 <= 2, a quadratic row 1/2 x'Qx with
   Q = 2I: the optimum is (1, 1) at -2, the row's dual -1/2. */
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

/* maximise x over -x^2 >= -4, a concave row on its lower side: x = 2. */
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

/* (t, x) in the cone with t <= 1 and x = 2: no point, and the certificate
   combines the two bounds with a cone multiplier. */
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

/* minimise -t over (t, x) in the cone: t runs away along (1, 0). */
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
    double x[8], y[8], z[8];
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

/* minimise t with t >= ||(x - 1.6, y - 2.3)|| over integer x and y in
   [0, 5]: the nearest integer point is (2, 2), at 0.5. */
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

/* maximise x + y over x^2 + y^2 <= 10 with x and y integer and at least
   0: the relaxation stops at sqrt(5) each, and 4 is the best integer sum. */
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

/* t >= |x - 0.5| with t <= 0.4 and x integer: the relaxation is feasible
   and every integer x is 0.5 away. */
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

/* an SOS set beside a cone is refused by name. */
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

/* minimise -a'x over ||x|| <= t with t fixed at 1, x of WIDE members: the
   optimum is x = a / ||a|| at -||a||. */
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

/* minimise t with 2 t u >= ||x||², u fixed at 1/2 and x fixed at a, WIDE
   members: t = ||a||². */
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_integer_columns_in_a_cone_branch_to_the_optimum);
    RUN_TEST(test_integer_columns_under_a_quadratic_row);
    RUN_TEST(test_an_integer_point_outside_every_cone_is_infeasible);
    RUN_TEST(test_an_sos_set_beside_a_cone_is_refused);
    RUN_TEST(test_a_wide_cone_gives_the_norm);
    RUN_TEST(test_a_wide_rotated_cone_gives_the_sum_of_squares);
    RUN_TEST(test_a_cone_on_the_model_gives_the_norm);
    RUN_TEST(test_a_maximised_cone_publishes_its_duals_the_right_way);
    RUN_TEST(test_a_rotated_cone_halves_the_square);
    RUN_TEST(test_a_quadratic_row_bends_the_optimum);
    RUN_TEST(test_a_concave_row_on_its_lower_side_is_convex);
    RUN_TEST(test_a_non_convex_quadratic_row_is_refused);
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
