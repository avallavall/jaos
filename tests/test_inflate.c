/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

static unsigned char *slurp_bytes(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(f, 0, SEEK_END));
    *n = ftell(f);
    rewind(f);
    unsigned char *p = malloc((size_t)*n);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t((size_t)*n, fread(p, 1, (size_t)*n, f));
    fclose(f);
    return p;
}

static int first_block_type(const char *path)
{
    long n = 0;
    unsigned char *p = slurp_bytes(path, &n);
    TEST_ASSERT_TRUE(n > 18);
    TEST_ASSERT_EQUAL_UINT(0x1fu, p[0]);
    TEST_ASSERT_EQUAL_UINT(0x8bu, p[1]);
    unsigned flg = p[3];
    long at = 10;
    if (flg & 0x04u) {
        long xlen = (long)p[at] | ((long)p[at + 1] << 8);
        at += 2 + xlen;
    }
    if (flg & 0x08u) {
        while (at < n && p[at] != 0u)
            at++;
        at++;
    }
    if (flg & 0x10u) {
        while (at < n && p[at] != 0u)
            at++;
        at++;
    }
    if (flg & 0x02u)
        at += 2;
    TEST_ASSERT_TRUE(at < n);
    int type = (p[at] >> 1) & 3;
    free(p);
    return type;
}

static void test_the_fixtures_cover_all_three_block_types(void)
{
    TEST_ASSERT_EQUAL_INT(0, first_block_type("tests/data/t1_stored.mps.gz"));
    TEST_ASSERT_EQUAL_INT(1, first_block_type("tests/data/t1_fixed.mps.gz"));
    TEST_ASSERT_EQUAL_INT(2, first_block_type("tests/data/t1.mps.gz"));
}

static void same_model(const jaos_model *a, const jaos_model *b)
{
    TEST_ASSERT_EQUAL_INT64(a->num_col, b->num_col);
    TEST_ASSERT_EQUAL_INT64(a->num_row, b->num_row);
    TEST_ASSERT_EQUAL_INT64(a->num_nz, b->num_nz);
    TEST_ASSERT_EQUAL_INT(a->sense, b->sense);
    TEST_ASSERT_EQUAL_DOUBLE(a->obj_offset, b->obj_offset);
    for (int64_t j = 0; j < a->num_col; j++) {
        TEST_ASSERT_EQUAL_DOUBLE(a->col_cost[j], b->col_cost[j]);
        TEST_ASSERT_EQUAL_DOUBLE(a->col_lower[j], b->col_lower[j]);
        TEST_ASSERT_EQUAL_DOUBLE(a->col_upper[j], b->col_upper[j]);
        TEST_ASSERT_EQUAL_INT64(a->a_start[j], b->a_start[j]);
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        TEST_ASSERT_EQUAL_DOUBLE(a->row_lower[i], b->row_lower[i]);
        TEST_ASSERT_EQUAL_DOUBLE(a->row_upper[i], b->row_upper[i]);
    }
    for (int64_t k = 0; k < a->num_nz; k++) {
        TEST_ASSERT_EQUAL_INT64(a->a_index[k], b->a_index[k]);
        TEST_ASSERT_EQUAL_DOUBLE(a->a_value[k], b->a_value[k]);
    }
}

static void mps_matches(const char *plain, const char *gz)
{
    jaos_model *a = fresh();
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(a, plain));
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_OK, jaos_read_mps(b, gz), gz);
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    same_model(a, b);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_every_block_type_reads_the_same_model(void)
{
    mps_matches("tests/data/t1.mps", "tests/data/t1.mps.gz");
    mps_matches("tests/data/t1.mps", "tests/data/t1_stored.mps.gz");
    mps_matches("tests/data/t1.mps", "tests/data/t1_fixed.mps.gz");
}

static void test_all_the_optional_header_fields_are_skipped(void)
{
    long n = 0;
    unsigned char *p = slurp_bytes("tests/data/t1_fixed.mps.gz", &n);
    unsigned flg = p[3];
    free(p);
    TEST_ASSERT_EQUAL_UINT(0x02u | 0x04u | 0x08u | 0x10u, flg);
    mps_matches("tests/data/t1.mps", "tests/data/t1_fixed.mps.gz");
}

static void test_two_members_read_as_one_file(void)
{
    mps_matches("tests/data/t1.mps", "tests/data/t1_two.mps.gz");
}

static void test_zero_padding_after_the_member_is_ignored(void)
{
    mps_matches("tests/data/t1.mps", "tests/data/t1_padded.mps.gz");
}

static void test_a_longer_instance_round_trips(void)
{
    mps_matches("tests/data/solve1.mps", "tests/data/solve1.mps.gz");
}

static void test_the_lp_reader_takes_a_compressed_file(void)
{
    jaos_model *a = fresh();
    jaos_model *b = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(a, "tests/data/g1.lp"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_lp(b, "tests/data/g1.lp.gz"));
    TEST_ASSERT_EQUAL_STRING("", jaos_model_error(b));
    same_model(a, b);
    jaos_model_free(a);
    jaos_model_free(b);
}

static void test_a_block_with_no_distance_code_is_accepted(void)
{
    jaos_model *m = fresh();
    char *out = nullptr;
    int64_t n = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_slurp(m, "tests/data/gz_nodist.gz", &out, &n));
    TEST_ASSERT_EQUAL_INT64(3, n);
    TEST_ASSERT_EQUAL_STRING("AAA", out);
    free(out);
    jaos_model_free(m);
}

static void test_an_empty_member_decodes_to_nothing(void)
{
    jaos_model *m = fresh();
    char *out = nullptr;
    int64_t n = 0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_slurp(m, "tests/data/gz_empty.gz", &out, &n));
    TEST_ASSERT_EQUAL_INT64(0, n);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("", out);
    free(out);
    jaos_model_free(m);
}

static void expect_reject(const char *path, const char *needle)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_INVALID_INPUT,
                                  jaos_read_mps(m, path), path);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(jaos_model_error(m), needle),
                                 jaos_model_error(m));

    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(m));
    jaos_model_free(m);
}

static void test_a_broken_container_is_refused_with_a_reason(void)
{
    expect_reject("tests/data/eg_method.mps.gz", "not deflate");
    expect_reject("tests/data/eg_reserved.mps.gz", "reserved gzip header");
    expect_reject("tests/data/eg_trailing.mps.gz", "after the last gzip");
    expect_reject("tests/data/eg_badheadcrc.mps.gz", "header checksum");
}

static void test_a_wrong_checksum_is_refused(void)
{
    expect_reject("tests/data/eg_badcrc.mps.gz", "checksum");
}

static void test_a_truncated_file_is_refused(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_read_mps(m, "tests/data/eg_trunc.mps.gz"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "compressed input"));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(m));
    jaos_model_free(m);
}

static void test_a_corrupted_payload_never_becomes_a_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_NOT_EQUAL(JAOS_OK,
                          jaos_read_mps(m, "tests/data/eg_corrupt.mps.gz"));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(m));
    jaos_model_free(m);
}

static void test_an_uncompressed_file_is_untouched(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT64(3, jaos_num_col(m));
    TEST_ASSERT_EQUAL_INT64(6, jaos_num_nz(m));
    jaos_model_free(m);
}

static void test_a_missing_compressed_file_is_an_io_error(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_IO,
                          jaos_read_mps(m, "tests/data/does_not_exist.mps.gz"));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "cannot open"));
    jaos_model_free(m);
}

static const char *GZTMP = "build/ti_tmp.gz";

static char *gz_round_trip(jaos_model *m, const char *data, int64_t n,
                           int64_t *back_n, int64_t *packed_n)
{
    char *packed = nullptr;
    int64_t pn = 0;
    TEST_ASSERT_TRUE(jm_gzip(data, n, &packed, &pn));
    TEST_ASSERT_NOT_NULL(packed);
    TEST_ASSERT_TRUE(pn > 0);
    *packed_n = pn;

    FILE *f = fopen(GZTMP, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_size_t((size_t)pn, fwrite(packed, 1, (size_t)pn, f));
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
    free(packed);

    char *back = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_slurp(m, GZTMP, &back, back_n));
    remove(GZTMP);
    return back;
}

static void fill_pseudo(char *p, int64_t n, uint32_t seed)
{
    uint32_t s = seed;
    for (int64_t i = 0; i < n; i++) {
        s = s * UINT32_C(1103515245) + UINT32_C(12345);
        p[i] = (char)((s >> 16) & 0xffu);
    }
}

static void test_the_compressor_round_trips_every_shape(void)
{
    jaos_model *m = fresh();
    constexpr int64_t BIG = 200000;
    char *buf = malloc((size_t)BIG);
    TEST_ASSERT_NOT_NULL(buf);

    struct { const char *name; int64_t n; } cases[] = {
        {"empty", 0}, {"one", 1}, {"run", 5000}, {"text", 40000},
        {"random", 40000}, {"big", BIG},
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        const int64_t n = cases[k].n;
        if (strcmp(cases[k].name, "run") == 0) {
            memset(buf, 'a', (size_t)n);
        } else if (strcmp(cases[k].name, "text") == 0) {
            for (int64_t i = 0; i < n; i++)
                buf[i] = (char)("ROWS COLUMNS RHS BOUNDS ENDATA "[i % 30]);
        } else if (strcmp(cases[k].name, "random") == 0) {
            fill_pseudo(buf, n, 12345u);
        } else if (strcmp(cases[k].name, "big") == 0) {

            fill_pseudo(buf, n / 2, 999u);
            memcpy(buf + n / 2, buf, (size_t)(n / 2));
        } else {
            memset(buf, 'x', (size_t)(n > 0 ? n : 1));
        }

        int64_t back_n = -1, packed_n = 0;
        char *back = gz_round_trip(m, buf, n, &back_n, &packed_n);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(n, back_n, cases[k].name);
        if (n > 0)
            TEST_ASSERT_EQUAL_MEMORY_MESSAGE(buf, back, (size_t)n,
                                             cases[k].name);
        free(back);
    }
    free(buf);
    jaos_model_free(m);
}

static void test_the_compressor_actually_compresses(void)
{
    jaos_model *m = fresh();
    constexpr int64_t N = 20000;
    char *buf = malloc((size_t)N);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 'a', (size_t)N);

    int64_t back_n = 0, packed_n = 0;
    char *back = gz_round_trip(m, buf, N, &back_n, &packed_n);
    TEST_ASSERT_EQUAL_INT64(N, back_n);
    TEST_ASSERT_TRUE_MESSAGE(packed_n < N / 2,
                             "a run of one byte did not compress");
    free(back);

    fill_pseudo(buf, N, 4321u);
    char *back2 = gz_round_trip(m, buf, N, &back_n, &packed_n);
    TEST_ASSERT_EQUAL_INT64(N, back_n);
    TEST_ASSERT_EQUAL_MEMORY(buf, back2, (size_t)N);
    TEST_ASSERT_TRUE(packed_n > N);
    free(back2);

    free(buf);
    jaos_model_free(m);
}

static void test_the_compressor_is_reproducible(void)
{
    constexpr int64_t N = 3000;
    char *buf = malloc((size_t)N);
    TEST_ASSERT_NOT_NULL(buf);
    fill_pseudo(buf, N, 77u);

    char *a = nullptr, *b = nullptr;
    int64_t an = 0, bn = 0;
    TEST_ASSERT_TRUE(jm_gzip(buf, N, &a, &an));
    TEST_ASSERT_TRUE(jm_gzip(buf, N, &b, &bn));
    TEST_ASSERT_EQUAL_INT64(an, bn);
    TEST_ASSERT_EQUAL_MEMORY(a, b, (size_t)an);
    free(a);
    free(b);
    free(buf);
}

static void test_the_compressor_rejects_bad_arguments(void)
{
    char *out = nullptr;
    int64_t n = 0;
    TEST_ASSERT_FALSE(jm_gzip(nullptr, 0, &out, &n));
    TEST_ASSERT_FALSE(jm_gzip("x", 1, nullptr, &n));
    TEST_ASSERT_FALSE(jm_gzip("x", 1, &out, nullptr));
    TEST_ASSERT_FALSE(jm_gzip("x", -1, &out, &n));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_fixtures_cover_all_three_block_types);
    RUN_TEST(test_every_block_type_reads_the_same_model);
    RUN_TEST(test_all_the_optional_header_fields_are_skipped);
    RUN_TEST(test_two_members_read_as_one_file);
    RUN_TEST(test_zero_padding_after_the_member_is_ignored);
    RUN_TEST(test_a_longer_instance_round_trips);
    RUN_TEST(test_the_lp_reader_takes_a_compressed_file);
    RUN_TEST(test_a_block_with_no_distance_code_is_accepted);
    RUN_TEST(test_an_empty_member_decodes_to_nothing);
    RUN_TEST(test_a_broken_container_is_refused_with_a_reason);
    RUN_TEST(test_a_wrong_checksum_is_refused);
    RUN_TEST(test_a_truncated_file_is_refused);
    RUN_TEST(test_a_corrupted_payload_never_becomes_a_model);
    RUN_TEST(test_an_uncompressed_file_is_untouched);
    RUN_TEST(test_a_missing_compressed_file_is_an_io_error);
    RUN_TEST(test_the_compressor_round_trips_every_shape);
    RUN_TEST(test_the_compressor_actually_compresses);
    RUN_TEST(test_the_compressor_is_reproducible);
    RUN_TEST(test_the_compressor_rejects_bad_arguments);
    return UNITY_END();
}
