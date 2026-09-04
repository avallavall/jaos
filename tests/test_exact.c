/* Exact integer and rational arithmetic (src/exact.c).
 *
 * The suite is built the way jaos-testing asks for one whose subject has
 * no tolerance in it: an oracle that is not the code under test, and, for
 * every predicate that can say "equal", a case it must say "not equal" to.
 *
 * Three oracles are used, in this order of preference.
 *
 * 1. uint64_t and int64_t arithmetic, where the operands are small enough
 *    that C's own integers are exact. That is the strongest oracle here
 *    because it is not this file's code.
 * 2. The double itself. jm_rational_from_double followed by
 *    jm_rational_to_double must return the same bits, for every finite
 *    double including subnormals and the two extremes. That one property
 *    exercises the split, the shifts, the normaliser, the division and the
 *    rounding at once, and it has an oracle that cannot be argued with.
 * 3. Values worked by hand, for the shapes the first two cannot reach.
 *
 * The numbers driving the loops come from a fixed-seed generator written
 * here. No clock and no library randomness: a failure has to be
 * reproducible on any machine, which is the whole premise of the project
 * (D8), and a test that picks different numbers each run is a test that
 * passes for a different reason each run.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* xorshift64*, fixed seed. Deterministic everywhere. */
static uint64_t rng_state = 0x9e3779b97f4a7c15ull;

static void rng_reset(void)
{
    rng_state = 0x9e3779b97f4a7c15ull;
}

static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x2545f4914f6cdd1dull;
}

/* A non-zero value in [1, bound]. */
static uint64_t rng_upto(uint64_t bound)
{
    return rng_next() % bound + 1;
}

/* ------------------------------------------------------------- naturals */

static jm_nat nat_of(uint64_t v)
{
    jm_nat a;
    jm_nat_set_u64(&a, v);
    return a;
}

static void test_a_natural_holds_what_a_u64_holds(void)
{
    const uint64_t cases[] = {0, 1, 2, 0xffffffffu, 0x100000000ull,
                              0xfffffffffffffffeull, UINT64_MAX};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        jm_nat a = nat_of(cases[i]);
        TEST_ASSERT_EQUAL_INT(cases[i] == 0, jm_nat_is_zero(&a));
        /* Bit length against a count done the obvious way. */
        int64_t bits = 0;
        for (uint64_t v = cases[i]; v; v >>= 1)
            bits++;
        TEST_ASSERT_EQUAL_INT64(bits, jm_nat_bits(&a));
    }
}

static void test_add_sub_mul_agree_with_u64_where_u64_is_exact(void)
{
    rng_reset();
    for (int k = 0; k < 4000; k++) {
        /* 32-bit operands: every sum and product below fits a u64. */
        const uint64_t x = rng_next() >> 32, y = rng_next() >> 32;
        jm_nat a = nat_of(x), b = nat_of(y), r;

        TEST_ASSERT_TRUE(jm_nat_add(&r, &a, &b));
        jm_nat want = nat_of(x + y);
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&r, &want));

        TEST_ASSERT_TRUE(jm_nat_mul(&r, &a, &b));
        want = nat_of(x * y);
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&r, &want));

        const uint64_t hi = x > y ? x : y, lo = x > y ? y : x;
        jm_nat ha = nat_of(hi), la = nat_of(lo);
        jm_nat_sub(&r, &ha, &la);
        want = nat_of(hi - lo);
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&r, &want));

        TEST_ASSERT_EQUAL_INT(x < y ? -1 : x > y ? 1 : 0,
                              jm_nat_cmp(&a, &b));
    }
}

static void test_divmod_agrees_with_u64(void)
{
    rng_reset();
    for (int k = 0; k < 2000; k++) {
        const uint64_t x = rng_next(), y = rng_upto(0xffffffffull);
        jm_nat a = nat_of(x), b = nat_of(y), q, rem;
        TEST_ASSERT_TRUE(jm_nat_divmod(&q, &rem, &a, &b));
        jm_nat wq = nat_of(x / y), wr = nat_of(x % y);
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&q, &wq));
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&rem, &wr));
    }
}

static void test_divmod_refuses_a_zero_divisor(void)
{
    jm_nat a = nat_of(7), z = nat_of(0), q, rem;
    TEST_ASSERT_FALSE(jm_nat_divmod(&q, &rem, &a, &z));
}

static void test_gcd_agrees_with_euclid_in_u64(void)
{
    rng_reset();
    for (int k = 0; k < 2000; k++) {
        uint64_t x = rng_upto(0xffffffffull), y = rng_upto(0xffffffffull);
        jm_nat a = nat_of(x), b = nat_of(y), g;
        TEST_ASSERT_TRUE(jm_nat_gcd(&g, &a, &b));
        while (y) {
            const uint64_t t = x % y;
            x = y;
            y = t;
        }
        jm_nat want = nat_of(x);
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&g, &want));
    }
    /* gcd with zero is the other operand, which is what the rational
     * normaliser leans on for a zero numerator. */
    jm_nat z = nat_of(0), n = nat_of(12), g;
    TEST_ASSERT_TRUE(jm_nat_gcd(&g, &z, &n));
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&g, &n));
    TEST_ASSERT_TRUE(jm_nat_gcd(&g, &n, &z));
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&g, &n));
}

static void test_shifting_is_multiplying_and_dividing_by_a_power_of_two(void)
{
    rng_reset();
    for (int k = 0; k < 1000; k++) {
        const uint64_t x = rng_next() >> 20;
        const int64_t s = (int64_t)(rng_next() % 40);
        jm_nat a = nat_of(x), r;
        TEST_ASSERT_TRUE(jm_nat_shl(&r, &a, s));
        jm_nat two = nat_of(1);
        TEST_ASSERT_TRUE(jm_nat_shl(&two, &two, s));
        jm_nat want;
        TEST_ASSERT_TRUE(jm_nat_mul(&want, &a, &two));
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&r, &want));

        jm_nat back;
        jm_nat_shr(&back, &r, s);
        TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&back, &a));
    }
    /* Shifting a magnitude past the whole array leaves zero, not rubbish. */
    jm_nat a = nat_of(0xdeadbeefu), r;
    jm_nat_shr(&r, &a, 64);
    TEST_ASSERT_TRUE(jm_nat_is_zero(&r));
}

static void test_running_out_of_limbs_is_reported_and_not_wrapped(void)
{
    /* The largest magnitude the array holds, then one bit more. */
    jm_nat a = nat_of(1), b;
    TEST_ASSERT_TRUE(jm_nat_shl(&a, &a, JM_EXACT_LIMBS * 32 - 1));
    TEST_ASSERT_EQUAL_INT64(JM_EXACT_LIMBS * 32, jm_nat_bits(&a));
    TEST_ASSERT_FALSE(jm_nat_shl(&b, &a, 1));
    TEST_ASSERT_FALSE(jm_nat_add(&b, &a, &a));

    jm_nat two = nat_of(2);
    TEST_ASSERT_FALSE(jm_nat_mul(&b, &a, &two));

    /* And the control: one bit below the limit, every one of them fits. */
    jm_nat c = nat_of(1);
    TEST_ASSERT_TRUE(jm_nat_shl(&c, &c, JM_EXACT_LIMBS * 32 - 2));
    TEST_ASSERT_TRUE(jm_nat_shl(&b, &c, 1));
    TEST_ASSERT_TRUE(jm_nat_add(&b, &c, &c));
    TEST_ASSERT_TRUE(jm_nat_mul(&b, &c, &two));
}

/* ------------------------------------------------------------- integers */

static void test_a_signed_integer_agrees_with_i64(void)
{
    rng_reset();
    for (int k = 0; k < 4000; k++) {
        const int64_t x = (int64_t)(rng_next() >> 34) - (1 << 29);
        const int64_t y = (int64_t)(rng_next() >> 34) - (1 << 29);
        jm_bigint a, b, r, want;
        jm_bigint_set_i64(&a, x);
        jm_bigint_set_i64(&b, y);

        TEST_ASSERT_TRUE(jm_bigint_add(&r, &a, &b));
        jm_bigint_set_i64(&want, x + y);
        TEST_ASSERT_EQUAL_INT(0, jm_bigint_cmp(&r, &want));

        TEST_ASSERT_TRUE(jm_bigint_sub(&r, &a, &b));
        jm_bigint_set_i64(&want, x - y);
        TEST_ASSERT_EQUAL_INT(0, jm_bigint_cmp(&r, &want));

        TEST_ASSERT_TRUE(jm_bigint_mul(&r, &a, &b));
        jm_bigint_set_i64(&want, x * y);
        TEST_ASSERT_EQUAL_INT(0, jm_bigint_cmp(&r, &want));

        TEST_ASSERT_EQUAL_INT(x < y ? -1 : x > y ? 1 : 0,
                              jm_bigint_cmp(&a, &b));
    }
}

static void test_zero_has_one_representation_and_one_sign(void)
{
    jm_bigint a, b, r;
    jm_bigint_set_i64(&a, 5);
    jm_bigint_set_i64(&b, -5);
    TEST_ASSERT_TRUE(jm_bigint_add(&r, &a, &b));
    TEST_ASSERT_TRUE(jm_bigint_is_zero(&r));
    TEST_ASSERT_EQUAL_INT(0, jm_bigint_sign(&r));

    /* Negating zero leaves zero, so no path can produce a minus zero. */
    jm_bigint_neg(&r);
    TEST_ASSERT_EQUAL_INT(0, jm_bigint_sign(&r));

    /* Multiplying by zero takes the sign with it. */
    jm_bigint z;
    jm_bigint_set_i64(&z, 0);
    TEST_ASSERT_TRUE(jm_bigint_mul(&r, &b, &z));
    TEST_ASSERT_EQUAL_INT(0, jm_bigint_sign(&r));
}

static void test_the_most_negative_i64_survives_its_own_negation(void)
{
    /* -INT64_MIN overflows a signed negation, so the magnitude is taken
     * unsigned. Nothing else in the file needs INT64_MIN, and this is the
     * one place it would have gone wrong quietly. */
    jm_bigint a, r, want;
    jm_bigint_set_i64(&a, INT64_MIN);
    TEST_ASSERT_EQUAL_INT(-1, jm_bigint_sign(&a));
    TEST_ASSERT_EQUAL_INT64(64, jm_nat_bits(&a.mag));

    jm_bigint one;
    jm_bigint_set_i64(&one, 1);
    TEST_ASSERT_TRUE(jm_bigint_add(&r, &a, &one));
    jm_bigint_set_i64(&want, INT64_MIN + 1);
    TEST_ASSERT_EQUAL_INT(0, jm_bigint_cmp(&r, &want));
}

/* ------------------------------------------------------------ rationals */

/* The rational a/b, built from two integers so the test never depends on
 * jm_rational_from_double to state its own expectation. */
static jm_rational rat_of(int64_t num, int64_t den)
{
    jm_rational n, d, r;
    jm_rational_set_i64(&n, num);
    jm_rational_set_i64(&d, den);
    TEST_ASSERT_TRUE(jm_rational_div(&r, &n, &d));
    return r;
}

static void test_a_rational_is_kept_in_lowest_terms(void)
{
    const jm_rational a = rat_of(6, 8), b = rat_of(3, 4);
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&a, &b));
    /* Not merely equal in value: the same stored numerator and
     * denominator, which is what makes cmp a two-line function. */
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&a.num.mag, &b.num.mag));
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&a.den, &b.den));

    /* A negative keeps its sign on the numerator and a positive
     * denominator, whichever side the minus came in on. */
    const jm_rational c = rat_of(-6, 8), d = rat_of(6, -8);
    TEST_ASSERT_EQUAL_INT(-1, jm_rational_sign(&c));
    TEST_ASSERT_EQUAL_INT(-1, jm_rational_sign(&d));
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&c, &d));

    /* Zero is 0/1 however it arrives. */
    jm_rational z = rat_of(0, 7);
    TEST_ASSERT_TRUE(jm_rational_is_zero(&z));
    jm_nat one = nat_of(1);
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&z.den, &one));
}

static void test_arithmetic_agrees_with_cross_multiplied_i64(void)
{
    rng_reset();
    for (int k = 0; k < 3000; k++) {
        /* 15-bit parts: every cross product below stays inside i64. */
        const int64_t a = (int64_t)(rng_next() % 60000) - 30000;
        const int64_t b = (int64_t)rng_upto(30000);
        const int64_t c = (int64_t)(rng_next() % 60000) - 30000;
        const int64_t d = (int64_t)rng_upto(30000);

        const jm_rational x = rat_of(a, b), y = rat_of(c, d);
        jm_rational got;

        TEST_ASSERT_TRUE(jm_rational_add(&got, &x, &y));
        jm_rational want = rat_of(a * d + c * b, b * d);
        TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&got, &want));

        TEST_ASSERT_TRUE(jm_rational_sub(&got, &x, &y));
        want = rat_of(a * d - c * b, b * d);
        TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&got, &want));

        TEST_ASSERT_TRUE(jm_rational_mul(&got, &x, &y));
        want = rat_of(a * c, b * d);
        TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&got, &want));

        /* Order, against the sign of the cross difference. */
        const int64_t diff = a * d - c * b;
        TEST_ASSERT_EQUAL_INT(diff < 0 ? -1 : diff > 0 ? 1 : 0,
                              jm_rational_cmp(&x, &y));

        if (c != 0) {
            TEST_ASSERT_TRUE(jm_rational_div(&got, &x, &y));
            want = rat_of(a * d, b * c);
            TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&got, &want));
        }
    }
}

static void test_dividing_by_zero_is_refused(void)
{
    const jm_rational a = rat_of(3, 4), z = rat_of(0, 5);
    jm_rational r;
    TEST_ASSERT_FALSE(jm_rational_div(&r, &a, &z));
}

/* The property the whole file exists for: what a double cannot tell apart,
 * this can. A double has 53 bits, so 1/3 and 1/3 + 2^-80 are the same
 * double and must not be the same rational. */
static void test_it_separates_two_values_one_double_cannot(void)
{
    const jm_rational third = rat_of(1, 3);
    jm_rational tiny;
    TEST_ASSERT_TRUE(jm_rational_from_double(&tiny, ldexp(1.0, -80)));

    jm_rational sum;
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &third, &tiny));
    TEST_ASSERT_EQUAL_INT(-1, jm_rational_cmp(&third, &sum));
    TEST_ASSERT_EQUAL_INT(1, jm_rational_cmp(&sum, &third));

    /* The control, and it is the point: as doubles they are one value. */
    const double a = jm_rational_to_double(&third);
    const double b = jm_rational_to_double(&sum);
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);

    /* And a third of three is one, exactly, which no double gets. */
    jm_rational three, back;
    jm_rational_set_i64(&three, 3);
    TEST_ASSERT_TRUE(jm_rational_mul(&back, &third, &three));
    jm_rational one;
    jm_rational_set_i64(&one, 1);
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&back, &one));
}

/* ------------------------------------------- the double round trip */

static void round_trips(double d)
{
    jm_rational r;
    TEST_ASSERT_TRUE(jm_rational_from_double(&r, d));
    const double back = jm_rational_to_double(&r);
    TEST_ASSERT_EQUAL_MEMORY(&d, &back, sizeof d);
}

static void test_every_named_double_round_trips_bit_for_bit(void)
{
    const double cases[] = {
        0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 3.0, 0.1, -0.1, 1.0 / 3.0,
        DBL_MIN, -DBL_MIN, DBL_MAX, -DBL_MAX, DBL_TRUE_MIN, -DBL_TRUE_MIN,
        DBL_EPSILON, 1.0 + DBL_EPSILON, 1e-300, 1e300, -1e300,
        ldexp(1.0, -1074), ldexp(1.0, 1023), 4503599627370497.0,
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
        round_trips(cases[i]);
}

static void test_random_doubles_round_trip_bit_for_bit(void)
{
    rng_reset();
    for (int k = 0; k < 5000; k++) {
        /* Bit patterns, so the exponent range is covered rather than the
         * narrow band a uniform mantissa would give. Infinities and NaNs
         * are skipped here; the next test is about them. */
        uint64_t bits = rng_next();
        double d;
        memcpy(&d, &bits, sizeof d);
        if (!isfinite(d))
            continue;
        round_trips(d);
    }
}

static void test_a_non_finite_double_is_refused_rather_than_approximated(void)
{
    jm_rational r;
    TEST_ASSERT_FALSE(jm_rational_from_double(&r, HUGE_VAL));
    TEST_ASSERT_FALSE(jm_rational_from_double(&r, -HUGE_VAL));
    TEST_ASSERT_FALSE(jm_rational_from_double(&r, NAN));
}

static void test_minus_zero_arrives_as_zero(void)
{
    jm_rational r;
    TEST_ASSERT_TRUE(jm_rational_from_double(&r, -0.0));
    TEST_ASSERT_TRUE(jm_rational_is_zero(&r));
    TEST_ASSERT_EQUAL_INT(0, jm_rational_sign(&r));
    /* And it comes back as a positive zero, not a negative one: the
     * rationals have one zero and the conversion may not invent a sign. */
    const double back = jm_rational_to_double(&r), want = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &back, sizeof want);
}

/* to_double rounds to nearest and ties to even. Halfway values are the
 * only place a truncating conversion would pass everything above and still
 * be wrong, so they are named rather than sampled. */
static void test_a_halfway_value_rounds_to_even(void)
{
    /* 2^53 + 1 is not a double. Between 2^53 and 2^53 + 2, it is a tie,
     * and 2^53 is the even one. */
    const jm_rational a = rat_of((int64_t)1 << 53, 1);
    jm_rational one, half, sum;
    jm_rational_set_i64(&one, 1);
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &a, &one));
    double got = jm_rational_to_double(&sum);
    double want = ldexp(1.0, 53);
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);

    /* 2^53 + 3 ties the other way, and 2^53 + 4 is the even one. */
    jm_rational three;
    jm_rational_set_i64(&three, 3);
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &a, &three));
    got = jm_rational_to_double(&sum);
    want = ldexp(1.0, 53) + 4.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);

    /* Not a tie: 2^53 + 3 is above the midpoint of the pair below it. */
    half = rat_of(1, 2);
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &a, &half));
    got = jm_rational_to_double(&sum);
    want = ldexp(1.0, 53);
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);
}

/* The negative control for the round trip: a conversion that dropped the
 * low bits would still pass every test above that uses small integers, so
 * here is a value where it cannot. */
static void test_the_round_trip_would_notice_a_dropped_bit(void)
{
    /* The largest odd integer a double holds exactly. Losing one bit makes
     * it even, and the memory compare would fire. */
    const double d = 9007199254740991.0; /* 2^53 - 1 */
    round_trips(d);

    jm_rational r;
    TEST_ASSERT_TRUE(jm_rational_from_double(&r, d));
    /* Its denominator is one and its numerator is odd, which is the
     * property a dropped bit destroys. */
    jm_nat one = nat_of(1);
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&r.den, &one));
    TEST_ASSERT_EQUAL_INT(1, (int)(r.num.mag.w[0] & 1u));
}

/* A long exact sum of doubles: what the reduced-denominator addition is
 * for. Adding 1/2 + 1/4 + ... + 1/2^k must land on 1 - 2^-k exactly, and
 * the denominators share every bit but one, so a naive b*d would grow the
 * limb count much faster than this should. */
static void test_a_long_sum_of_halves_stays_exact_and_small(void)
{
    jm_rational sum;
    jm_rational_set_zero(&sum);
    const int k = 400;
    for (int i = 1; i <= k; i++) {
        jm_rational term;
        TEST_ASSERT_TRUE(jm_rational_from_double(&term, ldexp(1.0, -i)));
        TEST_ASSERT_TRUE(jm_rational_add(&sum, &sum, &term));
    }
    /* 1 - 2^-k, as (2^k - 1) / 2^k. */
    jm_rational one, tail, want;
    jm_rational_set_i64(&one, 1);
    jm_nat p = nat_of(1);
    TEST_ASSERT_TRUE(jm_nat_shl(&p, &p, k));
    tail.num.mag = nat_of(1);
    tail.num.sign = 1;
    tail.den = p;
    TEST_ASSERT_TRUE(jm_rational_sub(&want, &one, &tail));
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&sum, &want));

    /* The denominator is 2^k and nothing more: the reduction worked. */
    TEST_ASSERT_EQUAL_INT64(k + 1, jm_nat_bits(&sum.den));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_natural_holds_what_a_u64_holds);
    RUN_TEST(test_add_sub_mul_agree_with_u64_where_u64_is_exact);
    RUN_TEST(test_divmod_agrees_with_u64);
    RUN_TEST(test_divmod_refuses_a_zero_divisor);
    RUN_TEST(test_gcd_agrees_with_euclid_in_u64);
    RUN_TEST(test_shifting_is_multiplying_and_dividing_by_a_power_of_two);
    RUN_TEST(test_running_out_of_limbs_is_reported_and_not_wrapped);
    RUN_TEST(test_a_signed_integer_agrees_with_i64);
    RUN_TEST(test_zero_has_one_representation_and_one_sign);
    RUN_TEST(test_the_most_negative_i64_survives_its_own_negation);
    RUN_TEST(test_a_rational_is_kept_in_lowest_terms);
    RUN_TEST(test_arithmetic_agrees_with_cross_multiplied_i64);
    RUN_TEST(test_dividing_by_zero_is_refused);
    RUN_TEST(test_it_separates_two_values_one_double_cannot);
    RUN_TEST(test_every_named_double_round_trips_bit_for_bit);
    RUN_TEST(test_random_doubles_round_trip_bit_for_bit);
    RUN_TEST(test_a_non_finite_double_is_refused_rather_than_approximated);
    RUN_TEST(test_minus_zero_arrives_as_zero);
    RUN_TEST(test_a_halfway_value_rounds_to_even);
    RUN_TEST(test_the_round_trip_would_notice_a_dropped_bit);
    RUN_TEST(test_a_long_sum_of_halves_stays_exact_and_small);
    return UNITY_END();
}
