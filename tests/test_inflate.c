/* Compressed input: the gzip and DEFLATE decoder in src/inflate.c.
 *
 * The oracle is the uncompressed file beside each fixture. A `.gz` that
 * reads to a different model than its plain twin is the failure that
 * matters, and comparing whole models rather than a digest is what catches
 * a decoder that is right for the first kilobyte only.
 *
 * The fixtures are built by hand rather than by `gzip`, because the three
 * DEFLATE block types have to be covered on purpose. The first test checks
 * that they are: without it, six passing comparisons could all be walking
 * the same branch. Paths are relative to the repository root, where make
 * runs.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h" /* white-box: two assembled models are compared */
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

/* ---- the control: which DEFLATE block each fixture actually uses ----- */

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

/* BTYPE of the first block: 0 stored, 1 fixed Huffman, 2 dynamic. Repeats
 * the header walk of src/inflate.c on purpose — a control that shared the
 * code it checks would agree with a wrong decoder. */
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

/* ---- the oracle: the plain file beside each fixture ------------------ */

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

/* Every optional header field at once: FEXTRA, FNAME, FCOMMENT and FHCRC
 * are all set on the fixed-Huffman fixture, so the skip arithmetic is under
 * test rather than assumed. */
static void test_all_the_optional_header_fields_are_skipped(void)
{
    long n = 0;
    unsigned char *p = slurp_bytes("tests/data/t1_fixed.mps.gz", &n);
    unsigned flg = p[3];
    free(p);
    TEST_ASSERT_EQUAL_UINT(0x02u | 0x04u | 0x08u | 0x10u, flg);
    mps_matches("tests/data/t1.mps", "tests/data/t1_fixed.mps.gz");
}

/* A gzip file may be several members end to end. Reading only the first one
 * would return half an instance and no error at all. */
static void test_two_members_read_as_one_file(void)
{
    mps_matches("tests/data/t1.mps", "tests/data/t1_two.mps.gz");
}

/* Some writers pad the end with zeros; gzip ignores them and so does this. */
static void test_zero_padding_after_the_member_is_ignored(void)
{
    mps_matches("tests/data/t1.mps", "tests/data/t1_padded.mps.gz");
}

/* A longer instance, so the decoder is asked for back-references beyond the
 * first few hundred bytes. */
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

/* ---- two shapes no format reader can express ------------------------ */

/* These call jm_slurp directly. What they decode to is not a model, so
 * going through jaos_read_mps would test the MPS parser's opinion of the
 * bytes instead of the decoder's. */

/* A block that emits no back-reference may declare one distance code of
 * length zero. zlib never writes one, so no fixture built by gzip covers
 * this; the file is assembled bit by bit instead. */
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

/* An empty member is a legal gzip file, and the one case where the decoder
 * produces no bytes at all. */
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

/* ---- the cases that must be refused --------------------------------- */

static void expect_reject(const char *path, const char *needle)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_ERR_INVALID_INPUT,
                                  jaos_read_mps(m, path), path);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(jaos_model_error(m), needle),
                                 jaos_model_error(m));
    /* A refused read leaves no model behind. */
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

/* The checksum is the only thing standing between a silently corrupted
 * instance and a solved wrong model, so it is tested on its own. */
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

/* A bit flipped inside the compressed data. Which message comes out depends
 * on where it lands, so the contract under test is the weaker one that
 * matters: corruption never becomes a model. */
static void test_a_corrupted_payload_never_becomes_a_model(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_NOT_EQUAL(JAOS_OK,
                          jaos_read_mps(m, "tests/data/eg_corrupt.mps.gz"));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_col(m));
    jaos_model_free(m);
}

/* An uncompressed file must still read exactly as before: the magic test is
 * on the bytes, and an MPS file cannot begin with them. */
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


/* --------------------------------------------------------------------- */
/* The compressor (D340)                                                  */
/* --------------------------------------------------------------------- */

/* The claim is one sentence: what jm_gzip writes, the decoder above reads
 * back byte for byte. It is checked on the shapes that reach different
 * parts of the encoder -- nothing, one byte, a run longer than one match,
 * text with repeats far apart, data with no repeats at all, and input
 * past the 32 KB window so the hash chain wraps -- because a compressor
 * that got the window wrong would still pass on a short file.
 *
 * The reader is the real one: the bytes go through a file and back
 * through jm_slurp, which is what every input to this library goes
 * through. */

static const char *GZTMP = "build/ti_tmp.gz";

/* Writes `n` bytes through jm_gzip and reads them back through jm_slurp.
 * Returns the round trip's own bytes, which the caller frees, and the
 * compressed size through `packed_n`. */
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

/* A fixed linear congruential sequence, so "incompressible" input is the
 * same on every machine and every run and a failure is reproducible. */
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
    constexpr int64_t BIG = 200000;   /* past the 32768-byte window */
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
            /* Half repeats and half does not, so one file exercises both
             * the match path and the literal path past the window. */
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

/* And it compresses, which the round trip alone does not say: a
 * compressor that emitted every byte as a literal would pass every
 * assertion above. The bar is deliberately loose -- half the input on a
 * file that is one repeated run -- because the number that matters is
 * measured against the real gzip in bench/measurements/02-217/ and not
 * pinned here. */
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

    /* The control: input with no structure does not shrink, and the file
     * is still valid. A "compressor" that shrank this would be losing
     * something. */
    fill_pseudo(buf, N, 4321u);
    char *back2 = gz_round_trip(m, buf, N, &back_n, &packed_n);
    TEST_ASSERT_EQUAL_INT64(N, back_n);
    TEST_ASSERT_EQUAL_MEMORY(buf, back2, (size_t)N);
    TEST_ASSERT_TRUE(packed_n > N);
    free(back2);

    free(buf);
    jaos_model_free(m);
}

/* The same bytes in give the same bytes out, which is the reproducibility
 * rule this library keeps everywhere (D8). The gzip header's modification
 * time is the field that would break it, and it is written as zero. */
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

/* Bad arguments, and the one size it refuses. */
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
