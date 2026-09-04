/* Exact integer and rational arithmetic, for verifying an answer rather
 * than for computing one.
 *
 * Every finite double is exactly a rational: it is m * 2^e with m an
 * integer of at most 53 bits and e between -1074 and 971. So a model's
 * data, and a claimed solution's, can be carried with no rounding at all,
 * and a question like "does this point satisfy this row" gets yes or no
 * instead of "within 1e-9". That is what src/check.c cannot do and what
 * SPECS.md section 5 lists as missing.
 *
 * The premises decide the shape. D11 excludes GMP and every other external
 * library. The build is -Wpedantic -Werror, and ISO C has no 128-bit
 * integer type, so __int128 is a compile error here. What is left is
 * standard and enough: limbs of uint32_t, products in uint64_t, identical
 * on every machine and in every run.
 *
 * No allocation happens. A magnitude is a fixed array of JM_EXACT_LIMBS
 * limbs and an operation that would not fit returns false. That is the
 * honest failure mode for a verifier: it proves the answer, or it says it
 * could not prove it. It never rounds and it never wraps.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <math.h>
#include <stdckdint.h>
#include <string.h>

/* ---------------------------------------------------------------- naturals
 *
 * A jm_nat is a magnitude in base 2^32, least significant limb first, with
 * no leading zero limb. n == 0 is zero and is its only representation, so
 * comparison can start by comparing lengths.
 *
 * Routines that can run out of limbs return false and leave the
 * destination unspecified. Callers stop at the first false. */

static void nat_trim(jm_nat *a)
{
    while (a->n > 0 && a->w[a->n - 1] == 0)
        a->n--;
}

void jm_nat_set_zero(jm_nat *a)
{
    a->n = 0;
}

bool jm_nat_is_zero(const jm_nat *a)
{
    return a->n == 0;
}

void jm_nat_set_u64(jm_nat *a, uint64_t v)
{
    a->n = 0;
    if (v == 0)
        return;
    a->w[a->n++] = (uint32_t)(v & 0xffffffffu);
    if (v >> 32)
        a->w[a->n++] = (uint32_t)(v >> 32);
}

/* Bit length: 0 for zero, otherwise one past the highest set bit. */
int64_t jm_nat_bits(const jm_nat *a)
{
    if (a->n == 0)
        return 0;
    uint32_t top = a->w[a->n - 1];
    int64_t b = 0;
    while (top) {
        b++;
        top >>= 1;
    }
    return (a->n - 1) * 32 + b;
}

/* Bit i, with out of range reading as zero so callers need not clamp. */
static bool nat_bit(const jm_nat *a, int64_t i)
{
    if (i < 0)
        return false;
    const int64_t limb = i / 32;
    if (limb >= a->n)
        return false;
    return ((a->w[limb] >> (i % 32)) & 1u) != 0u;
}

/* The value as a uint64_t. The caller has established it fits, which for
 * every use here means jm_nat_bits(a) <= 64. */
static uint64_t nat_to_u64(const jm_nat *a)
{
    uint64_t v = 0;
    if (a->n > 1)
        v = (uint64_t)a->w[1] << 32;
    if (a->n > 0)
        v |= a->w[0];
    return v;
}

int jm_nat_cmp(const jm_nat *a, const jm_nat *b)
{
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (int64_t i = a->n - 1; i >= 0; i--)
        if (a->w[i] != b->w[i])
            return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

bool jm_nat_add(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    const int64_t n = a->n > b->n ? a->n : b->n;
    uint64_t carry = 0;
    int64_t i = 0;
    for (; i < n; i++) {
        uint64_t s = carry;
        s += i < a->n ? a->w[i] : 0u;
        s += i < b->n ? b->w[i] : 0u;
        if (i >= JM_EXACT_LIMBS)
            return false;
        r->w[i] = (uint32_t)(s & 0xffffffffu);
        carry = s >> 32;
    }
    if (carry) {
        if (i >= JM_EXACT_LIMBS)
            return false;
        r->w[i++] = (uint32_t)carry;
    }
    r->n = i;
    nat_trim(r);
    return true;
}

/* r = a - b, where the caller has established a >= b. */
void jm_nat_sub(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    uint64_t borrow = 0;
    int64_t i = 0;
    for (; i < a->n; i++) {
        uint64_t d = a->w[i];
        const uint64_t s = (i < b->n ? (uint64_t)b->w[i] : 0u) + borrow;
        if (d < s) {
            d += 0x100000000u;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r->w[i] = (uint32_t)(d - s);
    }
    r->n = i;
    nat_trim(r);
}

bool jm_nat_mul(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    if (a->n == 0 || b->n == 0) {
        r->n = 0;
        return true;
    }
    const int64_t n = a->n + b->n;
    if (n > JM_EXACT_LIMBS + 1)
        return false;

    /* Schoolbook, into a local so that r may alias a or b. */
    uint32_t acc[JM_EXACT_LIMBS + 1];
    memset(acc, 0, (size_t)n * sizeof acc[0]);
    for (int64_t i = 0; i < a->n; i++) {
        uint64_t carry = 0;
        const uint64_t ai = a->w[i];
        for (int64_t j = 0; j < b->n; j++) {
            const uint64_t t = ai * (uint64_t)b->w[j] + acc[i + j] + carry;
            acc[i + j] = (uint32_t)(t & 0xffffffffu);
            carry = t >> 32;
        }
        acc[i + b->n] = (uint32_t)carry;
    }
    int64_t used = n;
    while (used > 0 && acc[used - 1] == 0)
        used--;
    if (used > JM_EXACT_LIMBS)
        return false;
    memcpy(r->w, acc, (size_t)used * sizeof acc[0]);
    r->n = used;
    return true;
}

bool jm_nat_shl(jm_nat *r, const jm_nat *a, int64_t bits)
{
    if (a->n == 0 || bits == 0) {
        if (r != a)
            *r = *a;
        return true;
    }
    const int64_t limbs = bits / 32, rest = bits % 32;

    /* The exact width of the answer, one limb per 32 bits of it. Charging
     * an extra limb whenever `rest` is non-zero would refuse a shift that
     * fits, and this test is what decides whether the verifier can prove
     * anything at all, so it may not be conservative. */
    const int64_t need = (jm_nat_bits(a) + bits + 31) / 32;
    if (need > JM_EXACT_LIMBS)
        return false;

    /* One limb of slack, because the loop below writes a top limb that the
     * width may say is zero; the trim then drops it. `span` cannot exceed
     * need + 1: a->n + limbs is at most need. */
    uint32_t acc[JM_EXACT_LIMBS + 1];
    const int64_t span = a->n + limbs + 1;
    memset(acc, 0, (size_t)span * sizeof acc[0]);
    for (int64_t i = 0; i < a->n; i++) {
        const uint64_t v = (uint64_t)a->w[i] << rest;
        acc[i + limbs] |= (uint32_t)(v & 0xffffffffu);
        if (rest)
            acc[i + limbs + 1] |= (uint32_t)(v >> 32);
    }
    int64_t used = span;
    while (used > 0 && acc[used - 1] == 0)
        used--;
    memcpy(r->w, acc, (size_t)used * sizeof acc[0]);
    r->n = used;
    return true;
}

void jm_nat_shr(jm_nat *r, const jm_nat *a, int64_t bits)
{
    if (a->n == 0 || bits == 0) {
        if (r != a)
            *r = *a;
        return;
    }
    const int64_t limbs = bits / 32, rest = bits % 32;
    if (limbs >= a->n) {
        r->n = 0;
        return;
    }
    const int64_t used = a->n - limbs;
    for (int64_t i = 0; i < used; i++) {
        uint64_t v = a->w[i + limbs] >> rest;
        if (rest && i + limbs + 1 < a->n)
            v |= (uint64_t)a->w[i + limbs + 1] << (32 - rest);
        r->w[i] = (uint32_t)(v & 0xffffffffu);
    }
    r->n = used;
    nat_trim(r);
}

/* q = a / b and rem = a % b, with b non-zero. Either output may be null,
 * and neither may alias an input.
 *
 * One shift-and-subtract per bit of a. Knuth's algorithm D is the faster
 * one and it is deliberately not here: this runs on a final basis and not
 * in a kernel, and the file exists so that nobody has to re-check its
 * arithmetic. Cost is bounded by jm_nat_bits(a), which is what a caller's
 * budget counts. */
bool jm_nat_divmod(jm_nat *q, jm_nat *rem, const jm_nat *a, const jm_nat *b)
{
    if (b->n == 0)
        return false;
    if (jm_nat_cmp(a, b) < 0) {
        if (rem)
            *rem = *a;
        if (q)
            q->n = 0;
        return true;
    }

    jm_nat cur, quo;
    jm_nat_set_zero(&cur);
    const int64_t top = jm_nat_bits(a);
    /* `a >= b >= 1` here, both earlier returns having taken the other cases,
     * so `top` is at least one and the count below is at least one. Saying so
     * is not decoration: with only the upper half of the range written down,
     * GCC's analysis at an LTO link cannot rule out a negative count and
     * warns that the memset's length reaches 1.8e19. The bound is real either
     * way; this states the half the code always relied on. */
    if (top <= 0 || (top + 31) / 32 > JM_EXACT_LIMBS)
        return false;
    quo.n = (top + 31) / 32;
    memset(quo.w, 0, (size_t)quo.n * sizeof quo.w[0]);

    for (int64_t i = top - 1; i >= 0; i--) {
        if (!jm_nat_shl(&cur, &cur, 1))
            return false;
        if (nat_bit(a, i)) {
            if (cur.n == 0) {
                cur.n = 1;
                cur.w[0] = 1u;
            } else {
                cur.w[0] |= 1u;
            }
        }
        if (jm_nat_cmp(&cur, b) >= 0) {
            jm_nat_sub(&cur, &cur, b);
            quo.w[i / 32] |= 1u << (i % 32);
        }
    }
    nat_trim(&quo);
    if (q)
        *q = quo;
    if (rem)
        *rem = cur;
    return true;
}

/* Greatest common divisor by Stein's binary algorithm: shifts, compares
 * and subtractions, so it never calls the division above. gcd(0, x) is x,
 * which is what the rational normaliser wants for a zero numerator. */
bool jm_nat_gcd(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    jm_nat u = *a, v = *b;
    if (u.n == 0) {
        *r = v;
        return true;
    }
    if (v.n == 0) {
        *r = u;
        return true;
    }

    int64_t shift = 0;
    while (!nat_bit(&u, 0) && !nat_bit(&v, 0)) {
        jm_nat_shr(&u, &u, 1);
        jm_nat_shr(&v, &v, 1);
        shift++;
    }
    while (!nat_bit(&u, 0))
        jm_nat_shr(&u, &u, 1);
    do {
        while (!nat_bit(&v, 0))
            jm_nat_shr(&v, &v, 1);
        if (jm_nat_cmp(&u, &v) > 0) {
            const jm_nat t = u;
            u = v;
            v = t;
        }
        jm_nat_sub(&v, &v, &u);
    } while (v.n != 0);

    return jm_nat_shl(r, &u, shift);
}

/* ---------------------------------------------------------------- integers
 *
 * A magnitude and a sign, with sign == 0 if and only if the magnitude is
 * zero. Keeping that invariant is what makes comparison a two-line
 * function instead of a table of cases. */

static void int_fix_sign(jm_bigint *a, int32_t sign)
{
    a->sign = a->mag.n == 0 ? 0 : sign;
}

void jm_bigint_set_zero(jm_bigint *a)
{
    jm_nat_set_zero(&a->mag);
    a->sign = 0;
}

bool jm_bigint_is_zero(const jm_bigint *a)
{
    return a->sign == 0;
}

int32_t jm_bigint_sign(const jm_bigint *a)
{
    return a->sign;
}

void jm_bigint_set_i64(jm_bigint *a, int64_t v)
{
    if (v == 0) {
        jm_bigint_set_zero(a);
        return;
    }
    /* Negating INT64_MIN overflows, so take the magnitude unsigned. */
    const uint64_t mag = v < 0 ? -(uint64_t)v : (uint64_t)v;
    jm_nat_set_u64(&a->mag, mag);
    a->sign = v < 0 ? -1 : 1;
}

void jm_bigint_neg(jm_bigint *a)
{
    a->sign = (int32_t)-a->sign;
}

int jm_bigint_cmp(const jm_bigint *a, const jm_bigint *b)
{
    if (a->sign != b->sign)
        return a->sign < b->sign ? -1 : 1;
    if (a->sign == 0)
        return 0;
    const int c = jm_nat_cmp(&a->mag, &b->mag);
    return a->sign > 0 ? c : -c;
}

/* r = a + b. Signs agreeing is an addition of magnitudes; signs differing
 * is a subtraction of the smaller from the larger, and the result takes
 * the sign of the larger. */
bool jm_bigint_add(jm_bigint *r, const jm_bigint *a, const jm_bigint *b)
{
    if (a->sign == 0) {
        *r = *b;
        return true;
    }
    if (b->sign == 0) {
        *r = *a;
        return true;
    }
    if (a->sign == b->sign) {
        if (!jm_nat_add(&r->mag, &a->mag, &b->mag))
            return false;
        int_fix_sign(r, a->sign);
        return true;
    }
    const int c = jm_nat_cmp(&a->mag, &b->mag);
    if (c == 0) {
        jm_bigint_set_zero(r);
        return true;
    }
    if (c > 0) {
        jm_nat_sub(&r->mag, &a->mag, &b->mag);
        int_fix_sign(r, a->sign);
    } else {
        jm_nat_sub(&r->mag, &b->mag, &a->mag);
        int_fix_sign(r, b->sign);
    }
    return true;
}

bool jm_bigint_sub(jm_bigint *r, const jm_bigint *a, const jm_bigint *b)
{
    jm_bigint nb = *b;
    jm_bigint_neg(&nb);
    return jm_bigint_add(r, a, &nb);
}

bool jm_bigint_mul(jm_bigint *r, const jm_bigint *a, const jm_bigint *b)
{
    if (!jm_nat_mul(&r->mag, &a->mag, &b->mag))
        return false;
    int_fix_sign(r, a->sign * b->sign);
    return true;
}

/* a * 2^bits. A negative shift is not accepted: this exists to make a row
 * of doubles integral, which only ever shifts up, and a right shift that
 * dropped a set bit would be a silent rounding in code whose whole point is
 * that there is none. */
bool jm_bigint_shl(jm_bigint *r, const jm_bigint *a, int64_t bits)
{
    if (bits < 0)
        return false;
    if (!jm_nat_shl(&r->mag, &a->mag, bits))
        return false;
    int_fix_sign(r, a->sign);
    return true;
}

/* a / b, where b divides a exactly. False when it does not, and false on a
 * zero divisor.
 *
 * Every division a fraction-free elimination performs is exact -- that is
 * what makes it fraction-free -- so a nonzero remainder here is not an
 * awkward input, it is the elimination being wrong. Checking rather than
 * assuming is what turns that from a wrong answer into a refusal. */
bool jm_bigint_divexact(jm_bigint *q, const jm_bigint *a, const jm_bigint *b)
{
    if (b->sign == 0)
        return false;
    if (a->sign == 0) {
        jm_bigint_set_zero(q);
        return true;
    }
    jm_nat rem;
    if (!jm_nat_divmod(&q->mag, &rem, &a->mag, &b->mag))
        return false;
    if (!jm_nat_is_zero(&rem))
        return false;
    int_fix_sign(q, a->sign * b->sign);
    return true;
}

/* --------------------------------------------------------------- rationals
 *
 * num / den, with den > 0 and gcd(|num|, den) == 1. Zero is 0/1, and it is
 * the only representation of zero, so a sign test is a look at num. Every
 * routine below re-establishes both invariants before returning. */

static bool rational_normalise(jm_rational *r)
{
    if (r->num.sign == 0) {
        jm_nat_set_u64(&r->den, 1);
        return true;
    }
    jm_nat g;
    if (!jm_nat_gcd(&g, &r->num.mag, &r->den))
        return false;
    jm_nat one;
    jm_nat_set_u64(&one, 1);
    if (jm_nat_cmp(&g, &one) == 0)
        return true;
    jm_nat q;
    if (!jm_nat_divmod(&q, nullptr, &r->num.mag, &g))
        return false;
    r->num.mag = q;
    if (!jm_nat_divmod(&q, nullptr, &r->den, &g))
        return false;
    r->den = q;
    int_fix_sign(&r->num, r->num.sign);
    return true;
}

void jm_rational_set_zero(jm_rational *r)
{
    jm_bigint_set_zero(&r->num);
    jm_nat_set_u64(&r->den, 1);
}

void jm_rational_set_i64(jm_rational *r, int64_t v)
{
    jm_bigint_set_i64(&r->num, v);
    jm_nat_set_u64(&r->den, 1);
}

bool jm_rational_is_zero(const jm_rational *r)
{
    return r->num.sign == 0;
}

int32_t jm_rational_sign(const jm_rational *r)
{
    return r->num.sign;
}

void jm_rational_neg(jm_rational *r)
{
    jm_bigint_neg(&r->num);
}

/* The exact value of a finite double, and false for an infinity or a NaN.
 *
 * frexp splits d into f * 2^e with f in [0.5, 1), so f * 2^53 is an
 * integer for every finite double including a subnormal one: the
 * significand is 53 bits and frexp has already moved the point. That
 * integer over 2^(53-e) is the value, and normalising strips whatever
 * power of two the two sides share. */
bool jm_rational_from_double(jm_rational *r, double d)
{
    if (!isfinite(d))
        return false;
    if (d == 0.0) {
        jm_rational_set_zero(r);
        return true;
    }

    int e = 0;
    const double f = frexp(d, &e);
    const double scaled = ldexp(f, 53);
    const int64_t mant = (int64_t)scaled;
    const int64_t exp2 = (int64_t)e - 53;

    jm_bigint_set_i64(&r->num, mant);
    jm_nat_set_u64(&r->den, 1);
    if (exp2 >= 0) {
        if (!jm_nat_shl(&r->num.mag, &r->num.mag, exp2))
            return false;
    } else {
        if (!jm_nat_shl(&r->den, &r->den, -exp2))
            return false;
    }
    return rational_normalise(r);
}

/* a/b + c/d, over the least common denominator rather than b*d. Reducing
 * by gcd(b, d) first is what keeps a long sum of doubles inside the limb
 * budget: the denominators are powers of two and share nearly all of it. */
bool jm_rational_add(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    if (a->num.sign == 0) {
        *r = *c;
        return true;
    }
    if (c->num.sign == 0) {
        *r = *a;
        return true;
    }

    jm_nat g;
    if (!jm_nat_gcd(&g, &a->den, &c->den))
        return false;
    jm_nat b1, d1;
    if (!jm_nat_divmod(&b1, nullptr, &a->den, &g))
        return false;
    if (!jm_nat_divmod(&d1, nullptr, &c->den, &g))
        return false;

    jm_nat den;
    if (!jm_nat_mul(&den, &b1, &c->den))
        return false;

    jm_bigint t1, t2, sum;
    jm_bigint wd1 = {.mag = d1, .sign = 1};
    jm_bigint wb1 = {.mag = b1, .sign = 1};
    if (!jm_bigint_mul(&t1, &a->num, &wd1))
        return false;
    if (!jm_bigint_mul(&t2, &c->num, &wb1))
        return false;
    if (!jm_bigint_add(&sum, &t1, &t2))
        return false;

    r->num = sum;
    r->den = den;
    return rational_normalise(r);
}

bool jm_rational_sub(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    jm_rational nc = *c;
    jm_rational_neg(&nc);
    return jm_rational_add(r, a, &nc);
}

/* (a/b) * (c/d), cross-reducing before multiplying so that the product of
 * the two numerators is never formed larger than it has to be. */
bool jm_rational_mul(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    if (a->num.sign == 0 || c->num.sign == 0) {
        jm_rational_set_zero(r);
        return true;
    }

    jm_nat g1, g2, an, cn, bd, dd;
    if (!jm_nat_gcd(&g1, &a->num.mag, &c->den))
        return false;
    if (!jm_nat_gcd(&g2, &c->num.mag, &a->den))
        return false;
    if (!jm_nat_divmod(&an, nullptr, &a->num.mag, &g1))
        return false;
    if (!jm_nat_divmod(&dd, nullptr, &c->den, &g1))
        return false;
    if (!jm_nat_divmod(&cn, nullptr, &c->num.mag, &g2))
        return false;
    if (!jm_nat_divmod(&bd, nullptr, &a->den, &g2))
        return false;

    if (!jm_nat_mul(&r->num.mag, &an, &cn))
        return false;
    if (!jm_nat_mul(&r->den, &bd, &dd))
        return false;
    int_fix_sign(&r->num, a->num.sign * c->num.sign);
    return true;
}

/* (a/b) / (c/d), and false when c is zero: a verifier has no business
 * inventing a value for that. */
bool jm_rational_div(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    if (c->num.sign == 0)
        return false;
    jm_rational inv;
    inv.num.mag = c->den;
    inv.num.sign = 1;
    inv.den = c->num.mag;
    if (c->num.sign < 0)
        inv.num.sign = -1;
    return jm_rational_mul(r, a, &inv);
}

/* Sign of a - c, without forming a - c: both denominators are positive, so
 * the comparison is between a.num * c.den and c.num * a.den. False when
 * either cross-multiply does not fit, and then *out is not written.
 *
 * A caller that cannot tell "equal" from "did not fit" can certify something
 * it never compared: a bound test reads a failed comparison as "inside the
 * bound" and calls the point good. `jaos_verify` compares solved values whose
 * numerators reach the whole limb budget, which is exactly the population
 * where the cross-multiply can fail, so it uses this and refuses. */
bool jm_rational_cmp_checked(const jm_rational *a, const jm_rational *c,
                             int *out)
{
    if (a->num.sign != c->num.sign) {
        *out = a->num.sign < c->num.sign ? -1 : 1;
        return true;
    }
    if (a->num.sign == 0) {
        *out = 0;
        return true;
    }

    jm_bigint l, rr;
    jm_bigint wcd = {.mag = c->den, .sign = 1};
    jm_bigint wad = {.mag = a->den, .sign = 1};
    if (!jm_bigint_mul(&l, &a->num, &wcd) ||
        !jm_bigint_mul(&rr, &c->num, &wad))
        return false;
    *out = jm_bigint_cmp(&l, &rr);
    return true;
}

/* The same, for a caller that has already established the widths fit.
 * Out of limbs it reports equal: both operands are normalised and share a
 * sign, so falling back on the difference of bit lengths cannot be done
 * honestly, and inventing an order would be worse. **A caller that cannot
 * rule the overflow out must use jm_rational_cmp_checked instead**, because
 * this zero is indistinguishable from a real equality. */
int jm_rational_cmp(const jm_rational *a, const jm_rational *c)
{
    int r = 0;
    if (!jm_rational_cmp_checked(a, c, &r))
        return 0;
    return r;
}

/* The nearest double, ties to even, or an infinity when the value is past
 * what a double holds. For a report: the proof itself never leaves the
 * rationals. Deterministic on every machine, which is the only property
 * this needs beyond being the right answer. */
double jm_rational_to_double(const jm_rational *r)
{
    if (r->num.sign == 0)
        return 0.0;

    const int64_t bn = jm_nat_bits(&r->num.mag), bd = jm_nat_bits(&r->den);

    /* Aim for a quotient of about 55 bits: two more than a significand, so
     * that the rounding bit and a sticky bit are both inside it. */
    const int64_t shift = 55 - (bn - bd);
    jm_nat num = r->num.mag, den = r->den;
    if (shift > 0) {
        if (!jm_nat_shl(&num, &num, shift))
            return r->num.sign > 0 ? HUGE_VAL : -HUGE_VAL;
    } else if (shift < 0) {
        if (!jm_nat_shl(&den, &den, -shift))
            return 0.0;
    }

    jm_nat q, rem;
    if (!jm_nat_divmod(&q, &rem, &num, &den))
        return r->num.sign > 0 ? HUGE_VAL : -HUGE_VAL;

    /* Drop to 53 bits, remembering whether anything was dropped -- or to
     * the subnormal grid, whichever is coarser. The result's exponent is
     * `drop - shift`, and below 2^-1022 the double grid is 2^-1074
     * whatever the magnitude, so stopping at 53 bits and letting ldexp
     * place the value rounds a second time (D268). */
    const int64_t qbits = jm_nat_bits(&q);
    int64_t drop = qbits - 53;
    if (shift - 1074 > drop)
        drop = shift - 1074;
    if (drop < 0)
        drop = 0;
    if (drop > qbits)
        drop = qbits + 1;   /* below half the last bit: a zero either way */
    bool sticky = !jm_nat_is_zero(&rem);
    bool round_bit = false;
    if (drop > 0) {
        round_bit = nat_bit(&q, drop - 1);
        for (int64_t i = 0; i < drop - 1 && !sticky; i++)
            sticky = nat_bit(&q, i);
        jm_nat_shr(&q, &q, drop);
    }

    uint64_t m = nat_to_u64(&q);
    int64_t e2 = drop - shift;
    if (round_bit && (sticky || (m & 1u))) {
        m++;
        if (m == (1ull << 53)) {
            m >>= 1;
            e2++;
        }
    }

    const double v = ldexp((double)m, (int)e2);
    return r->num.sign > 0 ? v : -v;
}

/* ------------------------------------------------------- dyadic rationals
 *
 * m * 2^e, with m a signed integer and e an ordinary int64_t. Every finite
 * double is one, and a sum or a product of them is one, so evaluating
 * `sum a_ij x_j` never leaves this type.
 *
 * That is the whole reason it exists beside jm_rational. A general
 * rational normalises after every operation, which is a gcd and two
 * divisions; over the nonzeros of a Kennington instance that is not a
 * cost anyone would pay. Here normalising is stripping trailing zero bits
 * off m, and adding is one shift and one addition. Both types are exact
 * and the choice between them is only ever about speed. */

/* Trailing zero bits of a magnitude, and 0 for zero itself. */
static int64_t nat_ctz(const jm_nat *a)
{
    if (a->n == 0)
        return 0;
    int64_t z = 0;
    for (int64_t i = 0; i < a->n; i++) {
        if (a->w[i] == 0) {
            z += 32;
            continue;
        }
        uint32_t v = a->w[i];
        while ((v & 1u) == 0u) {
            z++;
            v >>= 1;
        }
        break;
    }
    return z;
}

/* The one canonical form: m odd, or m zero with e zero. Keeping it is what
 * stops the mantissa growing by the exponent spread of the whole row. */
static void dyadic_trim(jm_dyadic *d)
{
    if (d->m.sign == 0) {
        d->e = 0;
        return;
    }
    const int64_t z = nat_ctz(&d->m.mag);
    if (z > 0) {
        jm_nat_shr(&d->m.mag, &d->m.mag, z);
        d->e += z;
    }
}

void jm_dyadic_set_zero(jm_dyadic *d)
{
    jm_bigint_set_zero(&d->m);
    d->e = 0;
}

bool jm_dyadic_is_zero(const jm_dyadic *d)
{
    return d->m.sign == 0;
}

int32_t jm_dyadic_sign(const jm_dyadic *d)
{
    return d->m.sign;
}

/* The exact value of a finite double, and false for an infinity or a NaN.
 * frexp puts the point where the 53-bit significand is an integer, for a
 * subnormal as much as for anything else. */
bool jm_dyadic_from_double(jm_dyadic *d, double v)
{
    if (!isfinite(v))
        return false;
    if (v == 0.0) {
        jm_dyadic_set_zero(d);
        return true;
    }
    int e = 0;
    const double f = frexp(v, &e);
    jm_bigint_set_i64(&d->m, (int64_t)ldexp(f, 53));
    d->e = (int64_t)e - 53;
    dyadic_trim(d);
    return true;
}

bool jm_dyadic_mul(jm_dyadic *r, const jm_dyadic *a, const jm_dyadic *b)
{
    if (!jm_bigint_mul(&r->m, &a->m, &b->m))
        return false;
    /* The mantissa's overflow is a refusal, so the exponent's is too.
     * Repeated squaring doubles `e` each time and reaches int64_t in 53
     * steps from the smallest subnormal, which is signed overflow and not
     * something a verifier may do (D268). */
    if (ckd_add(&r->e, a->e, b->e))
        return false;
    dyadic_trim(r);
    return true;
}

/* Align on the smaller exponent, then add. The shift is the only place
 * this can run out of limbs, and JM_EXACT_LIMBS is 4096 bits.
 *
 * A pair of doubles cannot reach that: `e` runs 971 down to -1074, a span
 * of 2045 bits, so 66 limbs hold any two of them. **A pair of PRODUCTS
 * can.** Their exponents run 1942 down to -2148, a span of 4090 bits, and
 * with up to 106 bits of mantissa on top the alignment wants 132 limbs.
 * One row holding `DBL_MAX * DBL_MAX` and `DBL_TRUE_MIN * DBL_TRUE_MIN`
 * refuses here, from four ordinary finite doubles; `1e300 * 1e300` beside
 * `1e-300 * 1e-300` is the last pair that fits (D268). The refusal is
 * correct and it is reported -- see jm_exact_evaluate, which publishes
 * nothing when it cannot finish. */
bool jm_dyadic_add(jm_dyadic *r, const jm_dyadic *a, const jm_dyadic *b)
{
    if (a->m.sign == 0) {
        *r = *b;
        return true;
    }
    if (b->m.sign == 0) {
        *r = *a;
        return true;
    }
    const jm_dyadic *lo = a->e <= b->e ? a : b;
    const jm_dyadic *hi = a->e <= b->e ? b : a;

    jm_bigint up = hi->m;
    int64_t diff;
    if (ckd_sub(&diff, hi->e, lo->e))
        return false;   /* the gap itself does not fit; the shift cannot */
    if (diff > 0 && !jm_nat_shl(&up.mag, &hi->m.mag, diff))
        return false;
    if (!jm_bigint_add(&r->m, &lo->m, &up))
        return false;
    r->e = lo->e;
    dyadic_trim(r);
    return true;
}

bool jm_dyadic_sub(jm_dyadic *r, const jm_dyadic *a, const jm_dyadic *b)
{
    jm_dyadic nb = *b;
    jm_bigint_neg(&nb.m);
    return jm_dyadic_add(r, a, &nb);
}

/* Sign of a - b into *out. False only when the difference does not fit,
 * which the caller reports rather than guessing an order. */
bool jm_dyadic_cmp(const jm_dyadic *a, const jm_dyadic *b, int *out)
{
    if (a->m.sign != b->m.sign) {
        *out = a->m.sign < b->m.sign ? -1 : 1;
        return true;
    }
    jm_dyadic d;
    if (!jm_dyadic_sub(&d, a, b))
        return false;
    *out = d.m.sign;
    return true;
}

/* The nearest double, ties to even. The value is m * 2^e with m exact, so
 * this is one rounding and not a chain of them -- which is the difference
 * the whole file is about. An exponent past what a double holds gives an
 * infinity or a zero, as the arithmetic itself would.
 *
 * Where to round is not always 53 bits. Below 2^-1022 the double grid is
 * coarser than the significand is wide: every subnormal's last bit sits at
 * 2^-1074 whatever its magnitude. Rounding to 53 bits and letting ldexp
 * round again is two roundings, and the second one breaks a tie the first
 * one manufactured -- 1.0% of subnormal results came out wrong that way
 * (D268). So the drop is the larger of the two demands, and the answer is
 * still one rounding. */
double jm_dyadic_to_double(const jm_dyadic *d)
{
    if (d->m.sign == 0)
        return 0.0;

    jm_nat q = d->m.mag;
    const int64_t bits = jm_nat_bits(&q);
    int64_t drop = bits - 53;
    if (-1074 - d->e > drop)
        drop = -1074 - d->e;   /* the subnormal grid: no bit below 2^-1074 */
    if (drop < 0)
        drop = 0;
    /* Past every bit there is, the answer is a zero either way, and this
     * keeps the sticky scan below from walking an exponent-sized range to
     * discover it. */
    if (drop > bits)
        drop = bits + 1;
    bool sticky = false, round_bit = false;
    if (drop > 0) {
        round_bit = nat_bit(&q, drop - 1);
        for (int64_t i = 0; i < drop - 1 && !sticky; i++)
            sticky = nat_bit(&q, i);
        jm_nat_shr(&q, &q, drop);
    }

    uint64_t m = nat_to_u64(&q);
    int64_t e2 = d->e + drop;
    if (round_bit && (sticky || (m & 1u))) {
        m++;
        if (m == (1ull << 53)) {
            m >>= 1;
            e2++;
        }
    }

    /* ldexp takes an int, and e2 is an int64_t that a long shift can put
     * far outside it. Clamping here rather than converting keeps the
     * overflow from wrapping into a finite answer. */
    if (e2 > 2048)
        return d->m.sign > 0 ? HUGE_VAL : -HUGE_VAL;
    if (e2 < -2200)
        return d->m.sign > 0 ? 0.0 : -0.0;
    const double v = ldexp((double)m, (int)e2);
    return d->m.sign > 0 ? v : -v;
}

/* ------------------------------------------------- evaluating a point
 *
 * The objective and every bound violation of a claimed point, computed
 * without a single rounding and reported as one.
 *
 * src/check.c does the same walk in long double and does NOT compensate:
 * `act[i] += term` at check.c:329 and `primal_obj += c_j x_j` at
 * check.c:340 are plain running sums. (`split_term` there splits the DUAL
 * gap into two halves for D219; it never touches the primal walk. The
 * compensated accumulators D168 and D169 measured are in src/simplex.c.)
 * So there is a middle option between the checker and this file -- a
 * Neumaier sum in check.c, roughly twice the walk rather than the ~1000x
 * here -- and any verdict that rejects exact evaluation on cost has to say
 * why it skipped that one (D268). What exact arithmetic reaches and no
 * compensated sum can is the rounding of each product, and D262 is the
 * case where that reached the answer on `finnis`. Its figures live there;
 * they are not restated here. */

/* violation of "v must lie in [lo, hi]", exactly, and zero when it does.
 * An infinite bound constrains nothing, which is why it is skipped rather
 * than converted: there is no dyadic infinity and there should not be. */
static bool exact_violation(jm_dyadic *out, const jm_dyadic *v, double lo,
                            double hi)
{
    jm_dyadic_set_zero(out);
    jm_dyadic b, d;
    int c = 0;
    if (isfinite(lo)) {
        if (!jm_dyadic_from_double(&b, lo) || !jm_dyadic_sub(&d, &b, v))
            return false;
        if (!jm_dyadic_cmp(&d, out, &c))
            return false;
        if (c > 0)
            *out = d;
    }
    if (isfinite(hi)) {
        if (!jm_dyadic_from_double(&b, hi) || !jm_dyadic_sub(&d, v, &b))
            return false;
        if (!jm_dyadic_cmp(&d, out, &c))
            return false;
        if (c > 0)
            *out = d;
    }
    return true;
}

bool jm_exact_evaluate(jaos_model *m, const double *x, jm_exact_point *out)
{
    if (m == nullptr || x == nullptr || out == nullptr)
        return false;

    /* Built here and published in one assignment at the end. Writing the
     * objective before the rows are walked would leave `row_violation` at
     * zero and `row_at` at -1 on a failure, and that is byte for byte what
     * a clean point produces: a caller that missed the false would read
     * "nothing is violated" out of a walk that never finished (D268). */
    jm_exact_point p = { .objective = 0.0,
                         .row_violation = 0.0,
                         .col_violation = 0.0,
                         .row_at = -1,
                         .col_at = -1,
                         .terms = 0 };
    jm_dyadic acc, term, xv, cv, viol, worst;

    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        goto fail;

    /* The objective, over the model as loaded: sum c_j x_j plus the
     * constant. The sense is not applied -- this reports what jaos_objective
     * reports, and that is the minimize-form value either way. */
    if (!jm_dyadic_from_double(&acc, m->obj_offset))
        goto fail;
    for (int64_t j = 0; j < m->num_col; j++) {
        if (m->col_cost[j] == 0.0 || x[j] == 0.0)
            continue;
        if (!jm_dyadic_from_double(&cv, m->col_cost[j]) ||
            !jm_dyadic_from_double(&xv, x[j]) ||
            !jm_dyadic_mul(&term, &cv, &xv) ||
            !jm_dyadic_add(&acc, &acc, &term))
            goto fail;
        p.terms++;
    }
    p.objective = jm_dyadic_to_double(&acc);

    /* Column bounds. Both sides are doubles, so the comparison itself is
     * exact in double already; the difference is what is not. */
    jm_dyadic_set_zero(&worst);
    for (int64_t j = 0; j < m->num_col; j++) {
        if (!jm_dyadic_from_double(&xv, x[j]))
            goto fail;
        if (!exact_violation(&viol, &xv, m->col_lower[j], m->col_upper[j]))
            goto fail;
        int c = 0;
        if (!jm_dyadic_cmp(&viol, &worst, &c))
            goto fail;
        if (c > 0) {
            worst = viol;
            p.col_at = j;
        }
    }
    p.col_violation = jm_dyadic_to_double(&worst);

    /* Row activities, one row at a time out of the CSR mirror. Column
     * order would need one accumulator per row, which on the largest gate
     * instance is six figures of them and not worth the memory. */
    jm_dyadic_set_zero(&worst);
    for (int64_t i = 0; i < m->num_row; i++) {
        jm_dyadic_set_zero(&acc);
        for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
            const int64_t j = m->ar_index[k];
            if (m->ar_value[k] == 0.0 || x[j] == 0.0)
                continue;
            if (!jm_dyadic_from_double(&cv, m->ar_value[k]) ||
                !jm_dyadic_from_double(&xv, x[j]) ||
                !jm_dyadic_mul(&term, &cv, &xv) ||
                !jm_dyadic_add(&acc, &acc, &term))
                goto fail;
            p.terms++;
        }
        if (!exact_violation(&viol, &acc, m->row_lower[i], m->row_upper[i]))
            goto fail;
        int c = 0;
        if (!jm_dyadic_cmp(&viol, &worst, &c))
            goto fail;
        if (c > 0) {
            worst = viol;
            p.row_at = i;
        }
    }
    p.row_violation = jm_dyadic_to_double(&worst);
    *out = p;
    return true;

fail:
    /* Neither a partial answer nor a clean one. A caller that ignores the
     * return value gets NaNs it cannot mistake for a verdict, and `terms`
     * says how far the walk got. */
    out->objective = (double)NAN;
    out->row_violation = (double)NAN;
    out->col_violation = (double)NAN;
    out->row_at = -1;
    out->col_at = -1;
    out->terms = p.terms;
    return false;
}
