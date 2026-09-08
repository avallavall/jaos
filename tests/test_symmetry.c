/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include <math.h>
#include <string.h>
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static jaos_model *covering(const double *cost)
{
    const double cl[3] = {0.0, 0.0, 0.0}, cu[3] = {1.0, 1.0, 1.0};
    const double rl[1] = {1.0}, ru[1] = {INFINITY};
    const int64_t as[4] = {0, 1, 2, 3}, ai[3] = {0, 0, 0};
    const double av[3] = {1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     3, as, ai, av));
    for (int64_t j = 0; j < 3; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    return m;
}

static void test_three_alike_columns_are_one_orbit(void)
{
    const double c[3] = {1.0, 1.0, 1.0};
    jaos_model *m = covering(c);
    jm_symmetry s;
    int64_t work = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_symmetry_find(m, 100000, &s, &work));
    TEST_ASSERT_TRUE(s.ngen >= 1);
    TEST_ASSERT_EQUAL_INT64(1, s.norbit);
    TEST_ASSERT_EQUAL_INT64(3, s.largest);
    TEST_ASSERT_TRUE(work > 0);
    for (int64_t k = 0; k < s.ngen; k++) {
        bool seen[3] = {false, false, false};
        for (int64_t j = 0; j < 3; j++) {
            TEST_ASSERT_TRUE(s.gen[k * 3 + j] >= 0 && s.gen[k * 3 + j] < 3);
            seen[s.gen[k * 3 + j]] = true;
        }
        TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2]);
    }
    TEST_ASSERT_EQUAL_INT64(s.orbit[0], s.orbit[1]);
    TEST_ASSERT_EQUAL_INT64(s.orbit[0], s.orbit[2]);
    jm_symmetry_free(&s);
    jaos_model_free(m);
}

static void test_different_costs_leave_no_symmetry(void)
{
    const double c[3] = {1.0, 2.0, 3.0};
    jaos_model *m = covering(c);
    jm_symmetry s;
    int64_t work = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_symmetry_find(m, 100000, &s, &work));
    TEST_ASSERT_EQUAL_INT64(0, s.ngen);
    TEST_ASSERT_EQUAL_INT64(0, s.norbit);
    TEST_ASSERT_NULL(s.orbit);
    jm_symmetry_free(&s);
    jaos_model_free(m);
}

static void test_two_alike_columns_beside_a_third(void)
{
    const double c[3] = {1.0, 2.0, 1.0};
    jaos_model *m = covering(c);
    jm_symmetry s;
    int64_t work = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_symmetry_find(m, 100000, &s, &work));
    TEST_ASSERT_EQUAL_INT64(1, s.ngen);
    TEST_ASSERT_EQUAL_INT64(1, s.norbit);
    TEST_ASSERT_EQUAL_INT64(2, s.largest);
    TEST_ASSERT_EQUAL_INT64(2, s.gen[0]);
    TEST_ASSERT_EQUAL_INT64(1, s.gen[1]);
    TEST_ASSERT_EQUAL_INT64(0, s.gen[2]);
    jm_symmetry_free(&s);
    jaos_model_free(m);
}

static void test_the_tree_reports_the_orbits_and_the_switch_holds(void)
{
    const double c[3] = {1.0, 1.0, 1.0};
    for (int on = 1; on >= 0; on--) {
        jaos_model *m = covering(c);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_symmetry(m, on));
        TEST_ASSERT_TRUE(m->cfg.mip_symmetry_set);
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
        jaos_mip_report rep;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_mip_result(m, &rep));
        if (on) {
            TEST_ASSERT_TRUE(rep.symmetry_generators >= 1);
            TEST_ASSERT_EQUAL_INT64(1, rep.symmetry_orbits);
        } else {
            TEST_ASSERT_EQUAL_INT64(0, rep.symmetry_generators);
            TEST_ASSERT_EQUAL_INT64(0, rep.symmetry_orbits);
        }
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_mip_symmetry(m, -1));
        TEST_ASSERT_FALSE(m->cfg.mip_symmetry_set);
        jaos_model_free(m);
    }
}

static void test_a_six_cycle_is_one_orbit_with_a_rotation_and_a_reflection(void)
{
    const double c[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    const double cl[6] = {0, 0, 0, 0, 0, 0}, cu[6] = {1, 1, 1, 1, 1, 1};
    const double rl[6] = {-INFINITY, -INFINITY, -INFINITY, -INFINITY,
                          -INFINITY, -INFINITY};
    const double ru[6] = {1, 1, 1, 1, 1, 1};
    const int64_t as[7] = {0, 2, 4, 6, 8, 10, 12};
    const int64_t ai[12] = {5, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5};
    const double av[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 6, 6, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     12, as, ai, av));
    for (int64_t j = 0; j < 6; j++)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_integer(m, j, true));
    jm_symmetry s;
    int64_t work = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_symmetry_find(m, 1000000, &s, &work));
    TEST_ASSERT_TRUE(s.ngen >= 2);
    TEST_ASSERT_EQUAL_INT64(1, s.norbit);
    TEST_ASSERT_EQUAL_INT64(6, s.largest);
    for (int64_t k = 0; k < s.ngen; k++) {
        const int64_t *g = s.gen + k * 6;
        bool moves = false;
        for (int64_t j = 0; j < 6; j++) {
            const int64_t d = (g[(j + 1) % 6] - g[j] + 12) % 6;
            TEST_ASSERT_TRUE(d == 1 || d == 5);
            moves = moves || g[j] != j;
        }
        TEST_ASSERT_TRUE(moves);
    }
    jm_symmetry_free(&s);
    jaos_model_free(m);
}

static void test_a_work_cap_of_nothing_finds_nothing_and_says_so(void)
{
    const double c[3] = {1.0, 1.0, 1.0};
    jaos_model *m = covering(c);
    jm_symmetry s;
    int64_t work = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_symmetry_find(m, 0, &s, &work));
    TEST_ASSERT_EQUAL_INT64(0, s.ngen);
    jm_symmetry_free(&s);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_three_alike_columns_are_one_orbit);
    RUN_TEST(test_different_costs_leave_no_symmetry);
    RUN_TEST(test_two_alike_columns_beside_a_third);
    RUN_TEST(test_the_tree_reports_the_orbits_and_the_switch_holds);
    RUN_TEST(test_a_six_cycle_is_one_orbit_with_a_rotation_and_a_reflection);
    RUN_TEST(test_a_work_cap_of_nothing_finds_nothing_and_says_so);
    return UNITY_END();
}
