/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <math.h>
#include "jaos_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool nat_bit(const jm_nat *a, int64_t i)
{
    if (i < 0)
        return false;
    const int64_t limb = i / 32;
    if (limb >= a->n)
        return false;
    return ((a->w[limb] >> (i % 32)) & 1u) != 0u;
}

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

    const int64_t need = (jm_nat_bits(a) + bits + 31) / 32;
    if (need > JM_EXACT_LIMBS)
        return false;

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

bool jm_bigint_shl(jm_bigint *r, const jm_bigint *a, int64_t bits)
{
    if (bits < 0)
        return false;
    if (!jm_nat_shl(&r->mag, &a->mag, bits))
        return false;
    int_fix_sign(r, a->sign);
    return true;
}

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

int jm_rational_cmp(const jm_rational *a, const jm_rational *c)
{
    int r = 0;
    if (!jm_rational_cmp_checked(a, c, &r))
        return 0;
    return r;
}

double jm_rational_to_double(const jm_rational *r)
{
    if (r->num.sign == 0)
        return 0.0;

    const int64_t bn = jm_nat_bits(&r->num.mag), bd = jm_nat_bits(&r->den);

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

    const int64_t qbits = jm_nat_bits(&q);
    int64_t drop = qbits - 53;
    if (shift - 1074 > drop)
        drop = shift - 1074;
    if (drop < 0)
        drop = 0;
    if (drop > qbits)
        drop = qbits + 1;
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

    if (ckd_add(&r->e, a->e, b->e))
        return false;
    dyadic_trim(r);
    return true;
}

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
        return false;
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

double jm_dyadic_to_double(const jm_dyadic *d)
{
    if (d->m.sign == 0)
        return 0.0;

    jm_nat q = d->m.mag;
    const int64_t bits = jm_nat_bits(&q);
    int64_t drop = bits - 53;
    if (-1074 - d->e > drop)
        drop = -1074 - d->e;
    if (drop < 0)
        drop = 0;

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

    if (e2 > 2048)
        return d->m.sign > 0 ? HUGE_VAL : -HUGE_VAL;
    if (e2 < -2200)
        return d->m.sign > 0 ? 0.0 : -0.0;
    const double v = ldexp((double)m, (int)e2);
    return d->m.sign > 0 ? v : -v;
}

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

    jm_exact_point p = { .objective = 0.0,
                         .row_violation = 0.0,
                         .col_violation = 0.0,
                         .row_at = -1,
                         .col_at = -1,
                         .terms = 0 };
    jm_dyadic acc, term, xv, cv, viol, worst;

    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        goto fail;

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

    out->objective = (double)NAN;
    out->row_violation = (double)NAN;
    out->col_violation = (double)NAN;
    out->row_at = -1;
    out->col_at = -1;
    out->terms = p.terms;
    return false;
}

static int64_t nat_decimal(const jm_nat *a, char *buf, int64_t cap)
{

    uint32_t chunk[(32 * JM_EXACT_LIMBS) / 29 + 2];
    int64_t n = 0;
    if (jm_nat_is_zero(a)) {
        if (cap < 1)
            return -1;
        buf[0] = '0';
        return 1;
    }
    jm_nat cur = *a, q, rem, base;
    jm_nat_set_u64(&base, 1000000000u);
    while (!jm_nat_is_zero(&cur)) {
        if (!jm_nat_divmod(&q, &rem, &cur, &base))
            return -1;
        chunk[n++] = rem.n > 0 ? rem.w[0] : 0;
        cur = q;
    }
    int64_t len = 0;
    for (int64_t k = n - 1; k >= 0; k--) {
        char piece[16];
        const int w = k == n - 1 ? snprintf(piece, sizeof piece, "%u", chunk[k])
                                 : snprintf(piece, sizeof piece, "%09u", chunk[k]);
        if (len + w > cap)
            return -1;
        memcpy(buf + len, piece, (size_t)w);
        len += w;
    }
    return len;
}

static bool nat_from_decimal(jm_nat *a, const char *s, const char **end)
{
    if (*s < '0' || *s > '9')
        return false;
    jm_nat ten, digit, t;
    jm_nat_set_u64(&ten, 10);
    jm_nat_set_zero(a);
    const char *p = s;
    for (; *p >= '0' && *p <= '9'; p++) {
        if (!jm_nat_mul(&t, a, &ten))
            return false;
        jm_nat_set_u64(&digit, (uint64_t)(*p - '0'));
        if (!jm_nat_add(a, &t, &digit))
            return false;
    }
    *end = p;
    return true;
}

bool jm_rational_from_decimal(jm_rational *r, const char *s)
{
    if (s == nullptr)
        return false;
    int32_t sign = 1;
    const char *p = s;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    const char *end = nullptr;
    if (!nat_from_decimal(&r->num.mag, p, &end))
        return false;
    p = end;
    jm_nat_set_u64(&r->den, 1);
    if (*p == '/') {
        p++;
        if (!nat_from_decimal(&r->den, p, &end))
            return false;
        p = end;
        if (jm_nat_is_zero(&r->den))
            return false;
    }
    if (*p != '\0')
        return false;
    r->num.sign = jm_nat_is_zero(&r->num.mag) ? 0 : sign;
    return rational_normalise(r);
}

char *jm_rational_decimal(const jm_rational *r)
{

    constexpr int64_t CAP = 2 * ((32 * JM_EXACT_LIMBS) * 30103 / 100000 + 2) + 4;
    char *s = malloc((size_t)CAP);
    if (s == nullptr)
        return nullptr;
    int64_t at = 0;
    if (r->num.sign < 0)
        s[at++] = '-';
    int64_t w = nat_decimal(&r->num.mag, s + at, CAP - at - 1);
    if (w < 0) {
        free(s);
        return nullptr;
    }
    at += w;
    jm_nat one;
    jm_nat_set_u64(&one, 1);
    if (jm_nat_cmp(&r->den, &one) != 0) {
        s[at++] = '/';
        w = nat_decimal(&r->den, s + at, CAP - at - 1);
        if (w < 0) {
            free(s);
            return nullptr;
        }
        at += w;
    }
    s[at] = '\0';
    return s;
}
