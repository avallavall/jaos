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
    quo.n = (top + 31) / 32;
    if (quo.n > JM_EXACT_LIMBS)
        return false;
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
 * the comparison is between a.num * c.den and c.num * a.den. */
int jm_rational_cmp(const jm_rational *a, const jm_rational *c)
{
    if (a->num.sign != c->num.sign)
        return a->num.sign < c->num.sign ? -1 : 1;
    if (a->num.sign == 0)
        return 0;

    jm_bigint l, rr;
    jm_bigint wcd = {.mag = c->den, .sign = 1};
    jm_bigint wad = {.mag = a->den, .sign = 1};
    if (!jm_bigint_mul(&l, &a->num, &wcd) ||
        !jm_bigint_mul(&rr, &c->num, &wad)) {
        /* Out of limbs. Both operands are normalised and share a sign, so
         * falling back on the difference of bit lengths cannot be done
         * honestly; report equal rather than invent an order, and let the
         * caller's own overflow check be the thing that fires. */
        return 0;
    }
    return jm_bigint_cmp(&l, &rr);
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

    /* Drop to 53 bits, remembering whether anything was dropped. */
    int64_t drop = jm_nat_bits(&q) - 53;
    if (drop < 0)
        drop = 0;
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
