/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static void test_every_option_reads_back_what_it_was_given(void)
{
    jaos_model *m = fresh();
    const int64_t n = jaos_num_options();
    TEST_ASSERT_TRUE(n > 40);
    TEST_ASSERT_NULL(jaos_option_name(n));
    for (int64_t k = 0; k < n; k++) {
        const char *name = jaos_option_name(k);
        TEST_ASSERT_NOT_NULL(name);
        char before[64], after[64];
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_get_option(m, name, before, 64), name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_set_option(m, name, before), name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_get_option(m, name, after, 64), name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(before, after, name);
    }
    jaos_model_free(m);
}

static void test_options_reach_the_setters(void)
{
    jaos_model *m = fresh();
    char buf[64];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_get_option(m, "mip_cut_rounds", buf, 64));
    TEST_ASSERT_EQUAL_STRING("1", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "mip_cut_rounds", "3"));
    TEST_ASSERT_TRUE(m->cfg.mip_cut_rounds_set && m->cfg.mip_cut_rounds == 3);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_get_option(m, "MIP_CUT_ROUNDS", buf, 64));
    TEST_ASSERT_EQUAL_STRING("3", buf);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "algorithm", "primal"));
    TEST_ASSERT_EQUAL_INT(JAOS_ALGORITHM_PRIMAL, jaos_algorithm_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "mip_heuristics", "off"));
    TEST_ASSERT_TRUE(m->cfg.mip_no_heuristics);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "mip_root_cut_drop", "0"));
    TEST_ASSERT_TRUE(m->cfg.mip_root_cut_drop_set && !m->cfg.mip_root_cut_drop);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "primal_tolerance", "1e-8"));
    TEST_ASSERT_EQUAL_DOUBLE(1e-8, jm_primal_tolerance(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "log_level", "detail"));
    TEST_ASSERT_EQUAL_INT(JAOS_LOG_DETAIL, m->cfg.log_level);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "mip_cutoff", "12.5"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_get_option(m, "mip_cutoff", buf, 64));
    TEST_ASSERT_EQUAL_STRING("12.5", buf);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_option(m, "mip_cutoff", "inf"));
    TEST_ASSERT_FALSE(m->cfg.mip_cutoff_set);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_option(m, "no_such", "1"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "no_such"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_option(m, "mip_cut_rounds", "three"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_option(m, "mip_cut_rounds", "3x"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_option(m, "mip_dive", "maybe"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_option(m, "algorithm", "barrier"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_option(m, "mip_gap", "-1"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_get_option(m, "no_such", buf, 64));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_get_option(m, "mip_gap", buf, 2));
    jaos_model_free(m);
}

static void test_an_options_file_is_read_line_by_line(void)
{
    const char *path = "build/to_opts.txt";
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("# options\nmip_cut_rounds 2\nalgorithm = primal\n\nmip_heuristics: false\n", f);
    fclose(f);
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_options(m, path));
    TEST_ASSERT_TRUE(m->cfg.mip_cut_rounds == 2 && m->cfg.force_primal &&
                     m->cfg.mip_no_heuristics);

    f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("mip_cut_rounds 2\nmip_cut_depth deep\n", f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_read_options(m, path));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), ":2:"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO, jaos_read_options(m, "build/no_such_file.txt"));
    remove(path);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_option_reads_back_what_it_was_given);
    RUN_TEST(test_options_reach_the_setters);
    RUN_TEST(test_an_options_file_is_read_line_by_line);
    return UNITY_END();
}
