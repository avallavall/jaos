/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

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

static uint64_t rng_upto(uint64_t bound)
{
    return rng_next() % bound + 1;
}

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

    jm_nat a = nat_of(0xdeadbeefu), r;
    jm_nat_shr(&r, &a, 64);
    TEST_ASSERT_TRUE(jm_nat_is_zero(&r));
}

static void test_running_out_of_limbs_is_reported_and_not_wrapped(void)
{

    jm_nat a = nat_of(1), b;
    TEST_ASSERT_TRUE(jm_nat_shl(&a, &a, JM_EXACT_LIMBS * 32 - 1));
    TEST_ASSERT_EQUAL_INT64(JM_EXACT_LIMBS * 32, jm_nat_bits(&a));
    TEST_ASSERT_FALSE(jm_nat_shl(&b, &a, 1));
    TEST_ASSERT_FALSE(jm_nat_add(&b, &a, &a));

    jm_nat two = nat_of(2);
    TEST_ASSERT_FALSE(jm_nat_mul(&b, &a, &two));

    jm_nat c = nat_of(1);
    TEST_ASSERT_TRUE(jm_nat_shl(&c, &c, JM_EXACT_LIMBS * 32 - 2));
    TEST_ASSERT_TRUE(jm_nat_shl(&b, &c, 1));
    TEST_ASSERT_TRUE(jm_nat_add(&b, &c, &c));
    TEST_ASSERT_TRUE(jm_nat_mul(&b, &c, &two));
}

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

    jm_bigint_neg(&r);
    TEST_ASSERT_EQUAL_INT(0, jm_bigint_sign(&r));

    jm_bigint z;
    jm_bigint_set_i64(&z, 0);
    TEST_ASSERT_TRUE(jm_bigint_mul(&r, &b, &z));
    TEST_ASSERT_EQUAL_INT(0, jm_bigint_sign(&r));
}

static void test_the_most_negative_i64_survives_its_own_negation(void)
{

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

    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&a.num.mag, &b.num.mag));
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&a.den, &b.den));

    const jm_rational c = rat_of(-6, 8), d = rat_of(6, -8);
    TEST_ASSERT_EQUAL_INT(-1, jm_rational_sign(&c));
    TEST_ASSERT_EQUAL_INT(-1, jm_rational_sign(&d));
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&c, &d));

    jm_rational z = rat_of(0, 7);
    TEST_ASSERT_TRUE(jm_rational_is_zero(&z));
    jm_nat one = nat_of(1);
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&z.den, &one));
}

static void test_arithmetic_agrees_with_cross_multiplied_i64(void)
{
    rng_reset();
    for (int k = 0; k < 3000; k++) {

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

static void test_it_separates_two_values_one_double_cannot(void)
{
    const jm_rational third = rat_of(1, 3);
    jm_rational tiny;
    TEST_ASSERT_TRUE(jm_rational_from_double(&tiny, ldexp(1.0, -80)));

    jm_rational sum;
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &third, &tiny));
    TEST_ASSERT_EQUAL_INT(-1, jm_rational_cmp(&third, &sum));
    TEST_ASSERT_EQUAL_INT(1, jm_rational_cmp(&sum, &third));

    const double a = jm_rational_to_double(&third);
    const double b = jm_rational_to_double(&sum);
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);

    jm_rational three, back;
    jm_rational_set_i64(&three, 3);
    TEST_ASSERT_TRUE(jm_rational_mul(&back, &third, &three));
    jm_rational one;
    jm_rational_set_i64(&one, 1);
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&back, &one));
}

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

    const double back = jm_rational_to_double(&r), want = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &back, sizeof want);
}

static void test_a_halfway_value_rounds_to_even(void)
{

    const jm_rational a = rat_of((int64_t)1 << 53, 1);
    jm_rational one, half, sum;
    jm_rational_set_i64(&one, 1);
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &a, &one));
    double got = jm_rational_to_double(&sum);
    double want = ldexp(1.0, 53);
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);

    jm_rational three;
    jm_rational_set_i64(&three, 3);
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &a, &three));
    got = jm_rational_to_double(&sum);
    want = ldexp(1.0, 53) + 4.0;
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);

    half = rat_of(1, 2);
    TEST_ASSERT_TRUE(jm_rational_add(&sum, &a, &half));
    got = jm_rational_to_double(&sum);
    want = ldexp(1.0, 53);
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);
}

static void test_the_round_trip_would_notice_a_dropped_bit(void)
{

    const double d = 9007199254740991.0;
    round_trips(d);

    jm_rational r;
    TEST_ASSERT_TRUE(jm_rational_from_double(&r, d));

    jm_nat one = nat_of(1);
    TEST_ASSERT_EQUAL_INT(0, jm_nat_cmp(&r.den, &one));
    TEST_ASSERT_EQUAL_INT(1, (int)(r.num.mag.w[0] & 1u));
}

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

    jm_rational one, tail, want;
    jm_rational_set_i64(&one, 1);
    jm_nat p = nat_of(1);
    TEST_ASSERT_TRUE(jm_nat_shl(&p, &p, k));
    tail.num.mag = nat_of(1);
    tail.num.sign = 1;
    tail.den = p;
    TEST_ASSERT_TRUE(jm_rational_sub(&want, &one, &tail));
    TEST_ASSERT_EQUAL_INT(0, jm_rational_cmp(&sum, &want));

    TEST_ASSERT_EQUAL_INT64(k + 1, jm_nat_bits(&sum.den));
}

static jm_rational rat_from_dyadic(const jm_dyadic *d)
{
    jm_rational r;
    r.num = d->m;
    jm_nat_set_u64(&r.den, 1);
    if (d->e >= 0)
        TEST_ASSERT_TRUE(jm_nat_shl(&r.num.mag, &r.num.mag, d->e));
    else
        TEST_ASSERT_TRUE(jm_nat_shl(&r.den, &r.den, -d->e));
    return r;
}

static void dyadic_round_trips(double v)
{
    jm_dyadic d;
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&d, v));
    const double back = jm_dyadic_to_double(&d);
    TEST_ASSERT_EQUAL_MEMORY(&v, &back, sizeof v);
}

static void test_a_dyadic_round_trips_every_double_bit_for_bit(void)
{
    const double named[] = {
        0.0, 1.0, -1.0, 0.1, -0.1, DBL_MIN, DBL_MAX, -DBL_MAX,
        DBL_TRUE_MIN, DBL_EPSILON, ldexp(1.0, -1074), ldexp(1.0, 1023),
    };
    for (size_t i = 0; i < sizeof named / sizeof named[0]; i++)
        dyadic_round_trips(named[i]);

    jm_dyadic z;
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&z, -0.0));
    TEST_ASSERT_TRUE(jm_dyadic_is_zero(&z));
    const double zback = jm_dyadic_to_double(&z);
    TEST_ASSERT_EQUAL_MEMORY(&(double){0.0}, &zback, sizeof zback);

    rng_reset();
    for (int k = 0; k < 5000; k++) {
        uint64_t bits = rng_next();
        double v;
        memcpy(&v, &bits, sizeof v);
        if (!isfinite(v) || v == 0.0)
            continue;
        dyadic_round_trips(v);
    }
}

static void rationals_must_agree(const jm_rational *a, const jm_rational *b,
                                 int want)
{
    const int64_t cap = 32 * (int64_t)JM_EXACT_LIMBS;
    TEST_ASSERT_TRUE(jm_nat_bits(&a->num.mag) + jm_nat_bits(&b->den) < cap);
    TEST_ASSERT_TRUE(jm_nat_bits(&b->num.mag) + jm_nat_bits(&a->den) < cap);
    TEST_ASSERT_EQUAL_INT(want, jm_rational_cmp(a, b));
}

static void test_a_dyadic_agrees_with_the_general_rational(void)
{
    rng_reset();
    int checked = 0;
    for (int k = 0; k < 2000; k++) {
        uint64_t ba = rng_next(), bb = rng_next();
        double a, b;
        memcpy(&a, &ba, sizeof a);
        memcpy(&b, &bb, sizeof b);
        if (!isfinite(a) || !isfinite(b))
            continue;

        jm_dyadic da, db, dr;
        jm_rational ra, rb, rr;
        TEST_ASSERT_TRUE(jm_dyadic_from_double(&da, a));
        TEST_ASSERT_TRUE(jm_dyadic_from_double(&db, b));
        TEST_ASSERT_TRUE(jm_rational_from_double(&ra, a));
        TEST_ASSERT_TRUE(jm_rational_from_double(&rb, b));

        TEST_ASSERT_TRUE(jm_dyadic_add(&dr, &da, &db));
        TEST_ASSERT_TRUE(jm_rational_add(&rr, &ra, &rb));
        jm_rational conv = rat_from_dyadic(&dr);
        rationals_must_agree(&conv, &rr, 0);
        checked++;

        TEST_ASSERT_TRUE(jm_dyadic_mul(&dr, &da, &db));
        TEST_ASSERT_TRUE(jm_rational_mul(&rr, &ra, &rb));
        conv = rat_from_dyadic(&dr);
        rationals_must_agree(&conv, &rr, 0);

        int c = 0;
        TEST_ASSERT_TRUE(jm_dyadic_cmp(&da, &db, &c));
        rationals_must_agree(&ra, &rb, c);
    }

    TEST_ASSERT_TRUE(checked > 100);
}

static void test_a_dyadic_keeps_its_mantissa_odd(void)
{

    jm_dyadic d;
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&d, 4.0));
    TEST_ASSERT_EQUAL_INT64(1, jm_nat_bits(&d.m.mag));
    TEST_ASSERT_EQUAL_INT64(2, d.e);

    jm_dyadic a, b, r;
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&a, 0.75));
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&b, 0.25));
    TEST_ASSERT_TRUE(jm_dyadic_add(&r, &a, &b));
    TEST_ASSERT_EQUAL_INT64(1, jm_nat_bits(&r.m.mag));
    TEST_ASSERT_EQUAL_INT64(0, r.e);

    TEST_ASSERT_TRUE(jm_dyadic_sub(&r, &a, &a));
    TEST_ASSERT_TRUE(jm_dyadic_is_zero(&r));
    TEST_ASSERT_EQUAL_INT64(0, r.e);
    TEST_ASSERT_EQUAL_INT(0, jm_dyadic_sign(&r));
}

static void test_a_rational_subnormal_is_rounded_once(void)
{

    jm_rational r;
    jm_nat one, big;
    jm_nat_set_u64(&big, 37);
    TEST_ASSERT_TRUE(jm_nat_shl(&big, &big, 125));
    jm_nat_set_u64(&one, 1);
    TEST_ASSERT_TRUE(jm_nat_add(&r.num.mag, &big, &one));
    r.num.sign = 1;
    jm_nat_set_u64(&r.den, 1);
    TEST_ASSERT_TRUE(jm_nat_shl(&r.den, &r.den, 1200));

    const double want = ldexp(19.0, -1074);
    const double got = jm_rational_to_double(&r);
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);

    TEST_ASSERT_EQUAL_MEMORY(&(double){ldexp(18.0, -1074)},
                             &(double){ldexp(18.5, -1074)}, sizeof(double));
}

static void test_an_exponent_that_does_not_fit_is_refused(void)
{
    jm_dyadic hi, lo, r;
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&hi, 1.0));
    hi.e = INT64_MAX / 2 + 2;
    TEST_ASSERT_FALSE(jm_dyadic_mul(&r, &hi, &hi));

    TEST_ASSERT_TRUE(jm_dyadic_from_double(&lo, 1.0));
    lo.e = INT64_MIN / 2 - 2;
    TEST_ASSERT_FALSE(jm_dyadic_add(&r, &hi, &lo));

    TEST_ASSERT_TRUE(jm_dyadic_from_double(&hi, 3.0));
    TEST_ASSERT_TRUE(jm_dyadic_from_double(&lo, 0.5));
    TEST_ASSERT_TRUE(jm_dyadic_mul(&r, &hi, &lo));
    TEST_ASSERT_EQUAL_DOUBLE(1.5, jm_dyadic_to_double(&r));
}

static void dyadic_pow2(jm_dyadic *d, int64_t e)
{
    TEST_ASSERT_TRUE(jm_dyadic_from_double(d, 1.0));
    d->e = e;
}

static void test_a_subnormal_result_is_rounded_once(void)
{

    jm_dyadic v, t;
    dyadic_pow2(&v, -1073);
    dyadic_pow2(&t, -1075);
    TEST_ASSERT_TRUE(jm_dyadic_add(&v, &v, &t));
    dyadic_pow2(&t, -1200);
    TEST_ASSERT_TRUE(jm_dyadic_add(&v, &v, &t));

    const double want = ldexp(3.0, -1074);
    const double got = jm_dyadic_to_double(&v);
    TEST_ASSERT_EQUAL_MEMORY(&want, &got, sizeof want);

    const double twice = ldexp(2.5, -1074);
    TEST_ASSERT_EQUAL_MEMORY(&twice, &(double){ldexp(2.0, -1074)},
                             sizeof twice);
    TEST_ASSERT_FALSE(want == twice);

    dyadic_pow2(&v, -1075);
    const double even = jm_dyadic_to_double(&v);
    TEST_ASSERT_EQUAL_MEMORY(&(double){0.0}, &even, sizeof even);
    dyadic_pow2(&t, -1200);
    TEST_ASSERT_TRUE(jm_dyadic_add(&v, &v, &t));
    const double up = jm_dyadic_to_double(&v);
    TEST_ASSERT_EQUAL_MEMORY(&(double){ldexp(1.0, -1074)}, &up, sizeof up);

    dyadic_pow2(&v, -4000);
    TEST_ASSERT_EQUAL_MEMORY(&(double){0.0}, &(double){jm_dyadic_to_double(&v)},
                             sizeof(double));
    jm_bigint_neg(&v.m);
    TEST_ASSERT_EQUAL_MEMORY(&(double){-0.0},
                             &(double){jm_dyadic_to_double(&v)},
                             sizeof(double));
}

static jaos_model *two_column_model(const double *cost, const double *cl,
                                    const double *cu, double rl, double ru)
{
    const int64_t s[] = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, &rl, &ru,
                     2, s, ix, v));
    return m;
}

static jaos_model *one_row_model(double a0, double a1)
{
    const int64_t s[] = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[] = {a0, a1};
    const double cost[] = {0.0, 0.0};
    const double cl[] = {-INFINITY, -INFINITY};
    const double cu[] = {INFINITY, INFINITY};
    double rl = 0.0, ru = 1.0;
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, &rl, &ru,
                     2, s, ix, v));
    return m;
}

static void test_a_row_wider_than_the_limbs_refuses_and_says_so(void)
{
    const double tiny = 0x1p-1074;
    jaos_model *m = one_row_model(DBL_MAX, tiny);
    const double x[] = {DBL_MAX, tiny};
    jm_exact_point p;
    TEST_ASSERT_FALSE(jm_exact_evaluate(m, x, &p));
    TEST_ASSERT_TRUE(isnan(p.row_violation));
    TEST_ASSERT_TRUE(isnan(p.objective));
    TEST_ASSERT_EQUAL_INT64(-1, p.row_at);
    jaos_model_free(m);

    m = one_row_model(1e150, 1e-150);
    const double y[] = {1e150, 1e-150};
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, y, &p));
    TEST_ASSERT_EQUAL_INT64(2, p.terms);
    TEST_ASSERT_FALSE(isnan(p.row_violation));
    jaos_model_free(m);
}

static void test_the_evaluator_keeps_a_product_a_double_sum_cannot(void)
{
    const double a = 1.0 + DBL_EPSILON;
    const double b = 1.0 + 2.0 * DBL_EPSILON;
    const double cost[] = {a, -1.0};
    const double cl[] = {-INFINITY, -INFINITY};
    const double cu[] = {INFINITY, INFINITY};
    const double x[] = {a, b};

    const double naive = cost[0] * x[0] + cost[1] * x[1];
    const double zero = 0.0;
    TEST_ASSERT_EQUAL_MEMORY(&zero, &naive, sizeof zero);

    jaos_model *m = two_column_model(cost, cl, cu, -INFINITY, INFINITY);
    jm_exact_point p;
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, x, &p));

    const double want = ldexp(1.0, -104);
    TEST_ASSERT_EQUAL_MEMORY(&want, &p.objective, sizeof want);

    TEST_ASSERT_EQUAL_INT64(4, p.terms);
    jaos_model_free(m);
}

static void test_the_evaluator_names_the_worst_row_and_column(void)
{
    const double cost[] = {0.0, 0.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {1.0, 1.0};

    jaos_model *m = two_column_model(cost, cl, cu, 0.0, 1.0);

    const double good[] = {0.25, 0.5};
    jm_exact_point p;
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, good, &p));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p.row_violation);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p.col_violation);
    TEST_ASSERT_EQUAL_INT64(-1, p.row_at);
    TEST_ASSERT_EQUAL_INT64(-1, p.col_at);

    const double bad[] = {1.5, 1.5};
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, bad, &p));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, p.row_violation);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, p.col_violation);
    TEST_ASSERT_EQUAL_INT64(0, p.row_at);
    TEST_ASSERT_EQUAL_INT64(0, p.col_at);

    const double low[] = {-0.25, 0.0};
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, low, &p));
    TEST_ASSERT_EQUAL_DOUBLE(0.25, p.col_violation);
    TEST_ASSERT_EQUAL_INT64(0, p.col_at);

    TEST_ASSERT_EQUAL_DOUBLE(0.25, p.row_violation);
    jaos_model_free(m);
}

static void test_an_infinite_bound_is_not_a_violation(void)
{
    const double cost[] = {1.0, 1.0};
    const double cl[] = {-INFINITY, -INFINITY};
    const double cu[] = {INFINITY, INFINITY};
    jaos_model *m = two_column_model(cost, cl, cu, -INFINITY, INFINITY);
    const double x[] = {1e300, -1e300};
    jm_exact_point p;
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, x, &p));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p.row_violation);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p.col_violation);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p.objective);
    jaos_model_free(m);
}

static void test_the_evaluator_refuses_what_it_cannot_read(void)
{
    const double cost[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {1.0, 1.0};
    jaos_model *m = two_column_model(cost, cl, cu, 0.0, 1.0);
    const double x[] = {0.5, 0.5};
    jm_exact_point p;
    TEST_ASSERT_FALSE(jm_exact_evaluate(nullptr, x, &p));
    TEST_ASSERT_FALSE(jm_exact_evaluate(m, nullptr, &p));
    TEST_ASSERT_FALSE(jm_exact_evaluate(m, x, nullptr));

    const double inf_x[] = {HUGE_VAL, 0.5};
    TEST_ASSERT_FALSE(jm_exact_evaluate(m, inf_x, &p));
    jaos_model_free(m);
}

static void test_a_refused_walk_does_not_read_as_a_clean_point(void)
{
    const double cost[] = {0.0, 1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {1.0, 1.0};
    jaos_model *m = two_column_model(cost, cl, cu, 0.0, 1.0);

    const double good[] = {0.25, 0.5};
    jm_exact_point p;
    TEST_ASSERT_TRUE(jm_exact_evaluate(m, good, &p));
    TEST_ASSERT_EQUAL_DOUBLE(0.5, p.objective);
    TEST_ASSERT_EQUAL_INT64(-1, p.row_at);

    const double bad[] = {HUGE_VAL, 0.5};
    TEST_ASSERT_FALSE(jm_exact_evaluate(m, bad, &p));
    TEST_ASSERT_TRUE(isnan(p.objective));
    TEST_ASSERT_TRUE(isnan(p.row_violation));
    TEST_ASSERT_TRUE(isnan(p.col_violation));
    TEST_ASSERT_EQUAL_INT64(-1, p.row_at);
    TEST_ASSERT_EQUAL_INT64(-1, p.col_at);
    jaos_model_free(m);
}

static void test_a_comparison_that_does_not_fit_says_so(void)
{
    const int64_t cap = 32 * JM_EXACT_LIMBS;

    jm_rational a, c;
    jm_rational_set_i64(&a, 1);
    TEST_ASSERT_TRUE(jm_nat_shl(&a.num.mag, &a.num.mag, cap - 100));
    a.num.sign = 1;
    jm_rational_set_i64(&c, 1);
    TEST_ASSERT_TRUE(jm_nat_shl(&c.den, &c.den, cap - 100));

    TEST_ASSERT_TRUE_MESSAGE(jm_nat_bits(&a.num.mag) + jm_nat_bits(&c.den)
                             > cap,
        "the pair does not actually overflow, so the test proves nothing");

    int got = 12345;
    TEST_ASSERT_FALSE_MESSAGE(jm_rational_cmp_checked(&a, &c, &got),
        "a comparison that cannot fit reports success");
    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, got,
        "the failed comparison wrote an answer anyway");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, jm_rational_cmp(&a, &c),
        "the plain form no longer reports equal on an overflow, so every "
        "caller of it needs rereading");

    jm_rational one, two;
    jm_rational_set_i64(&one, 1);
    jm_rational_set_i64(&two, 2);
    int ord = 0;
    TEST_ASSERT_TRUE(jm_rational_cmp_checked(&one, &two, &ord));
    TEST_ASSERT_EQUAL_INT(-1, ord);
    TEST_ASSERT_TRUE(jm_rational_cmp_checked(&two, &one, &ord));
    TEST_ASSERT_EQUAL_INT(1, ord);
    TEST_ASSERT_TRUE(jm_rational_cmp_checked(&one, &one, &ord));
    TEST_ASSERT_EQUAL_INT(0, ord);
}

static void test_a_rational_spells_itself_in_decimal(void)
{
    struct { int64_t num; int64_t den; const char *want; } small[] = {
        {0, 1, "0"}, {1, 1, "1"}, {-1, 1, "-1"}, {7, 3, "7/3"},
        {-7, 3, "-7/3"}, {6, 4, "3/2"}, {1000000000, 1, "1000000000"},
        {999999999, 1, "999999999"}, {1000000001, 9, "1000000001/9"},
        {INT64_MAX, 1, "9223372036854775807"},
        {INT64_MIN + 1, 1, "-9223372036854775807"},
    };
    for (size_t k = 0; k < sizeof small / sizeof *small; k++) {
        jm_rational a, b, r;
        jm_rational_set_i64(&a, small[k].num);
        jm_rational_set_i64(&b, small[k].den);
        TEST_ASSERT_TRUE(jm_rational_div(&r, &a, &b));
        char *s = jm_rational_decimal(&r);
        TEST_ASSERT_NOT_NULL(s);
        TEST_ASSERT_EQUAL_STRING(small[k].want, s);
        free(s);
    }

    jm_rational p;
    jm_rational_set_i64(&p, 1);
    TEST_ASSERT_TRUE(jm_nat_shl(&p.num.mag, &p.num.mag, 100));
    p.num.sign = 1;
    char *s = jm_rational_decimal(&p);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("1267650600228229401496703205376", s);
    free(s);
    jm_rational one, inv;
    jm_rational_set_i64(&one, 1);
    TEST_ASSERT_TRUE(jm_rational_div(&inv, &one, &p));
    s = jm_rational_decimal(&inv);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("1/1267650600228229401496703205376", s);
    free(s);
    jm_rational ten, big;
    jm_rational_set_i64(&ten, 10);
    jm_rational_set_i64(&big, 1);
    for (int e = 0; e < 30; e++)
        TEST_ASSERT_TRUE(jm_rational_mul(&big, &big, &ten));
    s = jm_rational_decimal(&big);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("1000000000000000000000000000000", s);
    free(s);

    jm_rational tenth;
    TEST_ASSERT_TRUE(jm_rational_from_double(&tenth, 0.1));
    s = jm_rational_decimal(&tenth);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("3602879701896397/36028797018963968", s);
    free(s);
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
    RUN_TEST(test_a_dyadic_round_trips_every_double_bit_for_bit);
    RUN_TEST(test_a_dyadic_agrees_with_the_general_rational);
    RUN_TEST(test_a_dyadic_keeps_its_mantissa_odd);
    RUN_TEST(test_a_rational_subnormal_is_rounded_once);
    RUN_TEST(test_an_exponent_that_does_not_fit_is_refused);
    RUN_TEST(test_a_subnormal_result_is_rounded_once);
    RUN_TEST(test_a_row_wider_than_the_limbs_refuses_and_says_so);
    RUN_TEST(test_the_evaluator_keeps_a_product_a_double_sum_cannot);
    RUN_TEST(test_the_evaluator_names_the_worst_row_and_column);
    RUN_TEST(test_an_infinite_bound_is_not_a_violation);
    RUN_TEST(test_the_evaluator_refuses_what_it_cannot_read);
    RUN_TEST(test_a_refused_walk_does_not_read_as_a_clean_point);
    RUN_TEST(test_a_comparison_that_does_not_fit_says_so);
    RUN_TEST(test_a_rational_spells_itself_in_decimal);
    return UNITY_END();
}
