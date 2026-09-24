/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_version_matches_macros(void)
{
    TEST_ASSERT_EQUAL_STRING(JAOS_VERSION_STRING, jaos_version());
}

static void test_the_build_commit_is_a_hash_or_empty(void)
{
    const char *c = jaos_build_commit();
    TEST_ASSERT_NOT_NULL(c);
    const size_t n = strlen(c);
    TEST_ASSERT_TRUE(n == 0 || n == 12);
    for (size_t k = 0; k < n; k++)
        TEST_ASSERT_NOT_NULL(strchr("0123456789abcdef", c[k]));
}

static void test_status_zero_values_are_the_defaults(void)
{

    TEST_ASSERT_EQUAL_INT(0, JAOS_OK);
    TEST_ASSERT_EQUAL_INT(0, JAOS_SOLVE_NOT_RUN);
}

static void test_status_strings_never_null_even_out_of_range(void)
{
    for (int s = -4; s <= 64; s++) {
        TEST_ASSERT_NOT_NULL(jaos_status_str((jaos_status)s));
        TEST_ASSERT_NOT_NULL(jaos_solve_status_str((jaos_solve_status)s));
    }
}

static void test_valid_statuses_have_distinct_strings(void)
{
    TEST_ASSERT_NOT_EQUAL(0, strcmp(jaos_status_str(JAOS_OK),
                                    jaos_status_str(JAOS_ERR_INVALID_INPUT)));
    TEST_ASSERT_NOT_EQUAL(0, strcmp(jaos_solve_status_str(JAOS_SOLVE_OPTIMAL),
                                    jaos_solve_status_str(JAOS_SOLVE_INFEASIBLE)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_version_matches_macros);
    RUN_TEST(test_the_build_commit_is_a_hash_or_empty);
    RUN_TEST(test_status_zero_values_are_the_defaults);
    RUN_TEST(test_status_strings_never_null_even_out_of_range);
    RUN_TEST(test_valid_statuses_have_distinct_strings);
    return UNITY_END();
}
