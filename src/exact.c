/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <math.h>
#include "jaos_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr int64_t NAT_INLINE = 2;
constexpr uint64_t NAT_BASE = 0x100000000ull;

static uint32_t *nat_w(jm_nat *a)
{
    return a->cap > 0 ? a->w : a->in;
}

static const uint32_t *nat_cw(const jm_nat *a)
{
    return a->cap > 0 ? a->w : a->in;
}

static int64_t nat_room(const jm_nat *a)
{
    return a->cap > 0 ? a->cap : NAT_INLINE;
}

static bool nat_grow(jm_nat *a, int64_t need)
{
    if (need <= nat_room(a))
        return true;
    if (need > JM_EXACT_LIMBS + 1)
        return false;
    int64_t c = 2 * nat_room(a);
    if (c < need)
        c = need;
    if (c > JM_EXACT_LIMBS + 1)
        c = JM_EXACT_LIMBS + 1;
    uint32_t *p = a->cap > 0 ? jm_realloc_array(a->w, c, sizeof *p)
                             : jm_alloc_array(c, sizeof *p);
    if (p == nullptr)
        return false;
    if (a->cap == 0 && a->n > 0)
        memcpy(p, a->in, (size_t)a->n * sizeof *p);
    a->w = p;
    a->cap = c;
    return true;
}

static void nat_trim(jm_nat *a)
{
    const uint32_t *w = nat_cw(a);
    while (a->n > 0 && w[a->n - 1] == 0)
        a->n--;
}

static bool nat_fits(const jm_nat *a)
{
    return a->n <= JM_EXACT_LIMBS;
}

static int clz32(uint32_t v)
{
    int n = 0;
    if (v <= 0x0000ffffu) { n += 16; v <<= 16; }
    if (v <= 0x00ffffffu) { n += 8; v <<= 8; }
    if (v <= 0x0fffffffu) { n += 4; v <<= 4; }
    if (v <= 0x3fffffffu) { n += 2; v <<= 2; }
    if (v <= 0x7fffffffu) n += 1;
    return n;
}

static int ctz32(uint32_t v)
{
    int n = 0;
    if ((v & 0xffffu) == 0) { n += 16; v >>= 16; }
    if ((v & 0xffu) == 0) { n += 8; v >>= 8; }
    if ((v & 0xfu) == 0) { n += 4; v >>= 4; }
    if ((v & 0x3u) == 0) { n += 2; v >>= 2; }
    if ((v & 0x1u) == 0) n += 1;
    return n;
}

void jm_nat_free(jm_nat *a)
{
    if (a->cap > 0)
        free(a->w);
    *a = (jm_nat){0};
}

void jm_nat_swap(jm_nat *a, jm_nat *b)
{
    const jm_nat t = *a;
    *a = *b;
    *b = t;
}

int64_t jm_nat_bytes(const jm_nat *a)
{
    return a->cap * (int64_t)sizeof(uint32_t);
}

bool jm_nat_copy(jm_nat *r, const jm_nat *a)
{
    if (r == a)
        return true;
    r->n = 0;
    if (!nat_grow(r, a->n))
        return false;
    if (a->n > 0)
        memcpy(nat_w(r), nat_cw(a), (size_t)a->n * sizeof(uint32_t));
    r->n = a->n;
    return true;
}

void jm_nat_set_zero(jm_nat *a)
{
    a->n = 0;
}

bool jm_nat_is_zero(const jm_nat *a)
{
    return a->n == 0;
}

bool jm_nat_is_one(const jm_nat *a)
{
    return a->n == 1 && nat_cw(a)[0] == 1u;
}

void jm_nat_set_u64(jm_nat *a, uint64_t v)
{
    uint32_t *w = nat_w(a);
    a->n = 0;
    if (v == 0)
        return;
    w[a->n++] = (uint32_t)(v & 0xffffffffu);
    if (v >> 32)
        w[a->n++] = (uint32_t)(v >> 32);
}

int64_t jm_nat_bits(const jm_nat *a)
{
    if (a->n == 0)
        return 0;
    return a->n * 32 - clz32(nat_cw(a)[a->n - 1]);
}

int64_t jm_nat_twos(const jm_nat *a)
{
    const uint32_t *w = nat_cw(a);
    for (int64_t l = 0; l < a->n; l++)
        if (w[l] != 0)
            return 32 * l + ctz32(w[l]);
    return 0;
}

static bool nat_bit(const jm_nat *a, int64_t i)
{
    if (i < 0)
        return false;
    const int64_t limb = i / 32;
    if (limb >= a->n)
        return false;
    return ((nat_cw(a)[limb] >> (i % 32)) & 1u) != 0u;
}

static uint64_t nat_to_u64(const jm_nat *a)
{
    const uint32_t *w = nat_cw(a);
    uint64_t v = 0;
    if (a->n > 1)
        v = (uint64_t)w[1] << 32;
    if (a->n > 0)
        v |= w[0];
    return v;
}

int jm_nat_cmp(const jm_nat *a, const jm_nat *b)
{
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    const uint32_t *x = nat_cw(a), *y = nat_cw(b);
    for (int64_t i = a->n - 1; i >= 0; i--)
        if (x[i] != y[i])
            return x[i] < y[i] ? -1 : 1;
    return 0;
}

bool jm_nat_add(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    if (a->n < b->n) {
        const jm_nat *t = a;
        a = b;
        b = t;
    }
    const int64_t an = a->n, bn = b->n;
    if (r != a && r != b)
        r->n = 0;
    if (!nat_grow(r, an))
        return false;
    const uint32_t *x = nat_cw(a), *y = nat_cw(b);
    uint32_t *z = nat_w(r);
    uint64_t carry = 0;
    for (int64_t i = 0; i < an; i++) {
        const uint64_t s = carry + x[i] + (i < bn ? y[i] : 0u);
        z[i] = (uint32_t)(s & 0xffffffffu);
        carry = s >> 32;
    }
    r->n = an;
    if (carry != 0) {
        if (!nat_grow(r, an + 1))
            return false;
        nat_w(r)[an] = (uint32_t)carry;
        r->n = an + 1;
    }
    nat_trim(r);
    return nat_fits(r);
}

bool jm_nat_sub(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    const int64_t an = a->n, bn = b->n;
    if (r != a && r != b)
        r->n = 0;
    if (!nat_grow(r, an))
        return false;
    const uint32_t *x = nat_cw(a), *y = nat_cw(b);
    uint32_t *z = nat_w(r);
    uint64_t borrow = 0;
    for (int64_t i = 0; i < an; i++) {
        const uint64_t t = (uint64_t)x[i] - (i < bn ? y[i] : 0u) - borrow;
        z[i] = (uint32_t)(t & 0xffffffffu);
        borrow = t >> 63;
    }
    r->n = an;
    nat_trim(r);
    return true;
}

static void nat_mul_limbs(uint32_t *z, const uint32_t *x, int64_t xn,
                          const uint32_t *y, int64_t yn)
{
    memset(z, 0, (size_t)(xn + yn) * sizeof *z);
    for (int64_t i = 0; i < xn; i++) {
        uint64_t carry = 0;
        const uint64_t xi = x[i];
        if (xi == 0)
            continue;
        for (int64_t j = 0; j < yn; j++) {
            const uint64_t t = xi * (uint64_t)y[j] + z[i + j] + carry;
            z[i + j] = (uint32_t)(t & 0xffffffffu);
            carry = t >> 32;
        }
        z[i + yn] = (uint32_t)carry;
    }
}

bool jm_nat_mul(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    if (a->n == 0 || b->n == 0) {
        r->n = 0;
        return true;
    }
    const int64_t n = a->n + b->n;
    if (n - 1 > JM_EXACT_LIMBS)
        return false;
    if (n <= 2 * NAT_INLINE) {
        uint32_t z[2 * NAT_INLINE];
        nat_mul_limbs(z, nat_cw(a), a->n, nat_cw(b), b->n);
        int64_t used = n;
        while (used > 0 && z[used - 1] == 0)
            used--;
        r->n = 0;
        if (!nat_grow(r, used))
            return false;
        memcpy(nat_w(r), z, (size_t)used * sizeof *z);
        r->n = used;
        return true;
    }
    jm_nat t = {0};
    jm_nat *out = (r == a || r == b) ? &t : r;
    out->n = 0;
    if (!nat_grow(out, n)) {
        jm_nat_free(&t);
        return false;
    }
    nat_mul_limbs(nat_w(out), nat_cw(a), a->n, nat_cw(b), b->n);
    out->n = n;
    nat_trim(out);
    if (out == &t) {
        jm_nat_swap(r, &t);
        jm_nat_free(&t);
    }
    return nat_fits(r);
}

bool jm_nat_shl(jm_nat *r, const jm_nat *a, int64_t bits)
{
    if (bits < 0)
        return false;
    if (a->n == 0 || bits == 0)
        return jm_nat_copy(r, a);
    const int64_t limbs = bits / 32, rest = bits % 32;
    const int64_t need = (jm_nat_bits(a) + bits + 31) / 32;
    if (need > JM_EXACT_LIMBS)
        return false;
    const int64_t an = a->n;
    if (r != a)
        r->n = 0;
    if (!nat_grow(r, an + limbs + 1))
        return false;
    const uint32_t *x = nat_cw(a);
    uint32_t *z = nat_w(r);
    if (rest == 0) {
        memmove(z + limbs, x, (size_t)an * sizeof *z);
        z[an + limbs] = 0;
    } else {
        z[an + limbs] = x[an - 1] >> (32 - rest);
        for (int64_t i = an - 1; i > 0; i--)
            z[i + limbs] = (x[i] << rest) | (x[i - 1] >> (32 - rest));
        z[limbs] = x[0] << rest;
    }
    if (limbs > 0)
        memset(z, 0, (size_t)limbs * sizeof *z);
    r->n = an + limbs + 1;
    nat_trim(r);
    return true;
}

bool jm_nat_shr(jm_nat *r, const jm_nat *a, int64_t bits)
{
    if (a->n == 0 || bits <= 0)
        return jm_nat_copy(r, a);
    const int64_t limbs = bits / 32, rest = bits % 32;
    if (limbs >= a->n) {
        r->n = 0;
        return true;
    }
    const int64_t an = a->n, used = an - limbs;
    if (r != a) {
        r->n = 0;
        if (!nat_grow(r, used))
            return false;
    }
    const uint32_t *x = nat_cw(a);
    uint32_t *z = nat_w(r);
    for (int64_t i = 0; i < used; i++) {
        uint32_t v = x[i + limbs] >> rest;
        if (rest && i + limbs + 1 < an)
            v |= x[i + limbs + 1] << (32 - rest);
        z[i] = v;
    }
    r->n = used;
    nat_trim(r);
    return true;
}

static bool nat_divmod_small(jm_nat *q, uint32_t *rem, const jm_nat *a,
                             uint32_t d)
{
    const int64_t m = a->n;
    if (q != a)
        q->n = 0;
    if (!nat_grow(q, m))
        return false;
    const uint32_t *x = nat_cw(a);
    uint32_t *z = nat_w(q);
    uint64_t r = 0;
    for (int64_t i = m - 1; i >= 0; i--) {
        const uint64_t cur = (r << 32) | x[i];
        z[i] = (uint32_t)(cur / d);
        r = cur % d;
    }
    q->n = m;
    nat_trim(q);
    *rem = (uint32_t)r;
    return true;
}

static void knuth_d(uint32_t *q, uint32_t *u, const uint32_t *v, int64_t m,
                    int64_t n)
{
    const uint64_t vt = v[n - 1], vs = v[n - 2];
    for (int64_t j = m - n; j >= 0; j--) {
        const uint64_t num = ((uint64_t)u[j + n] << 32) | u[j + n - 1];
        uint64_t qhat = num / vt, rhat = num % vt;
        while (qhat >= NAT_BASE ||
               qhat * vs > ((rhat << 32) | u[j + n - 2])) {
            qhat--;
            rhat += vt;
            if (rhat >= NAT_BASE)
                break;
        }
        uint64_t carry = 0, borrow = 0;
        for (int64_t i = 0; i < n; i++) {
            const uint64_t p = qhat * v[i] + carry;
            carry = p >> 32;
            const uint64_t t = (uint64_t)u[i + j] - (p & 0xffffffffu) - borrow;
            u[i + j] = (uint32_t)(t & 0xffffffffu);
            borrow = t >> 63;
        }
        const uint64_t s = carry + borrow, d = u[j + n];
        if (d < s) {
            u[j + n] = (uint32_t)(d + NAT_BASE - s);
            qhat--;
            uint64_t c = 0;
            for (int64_t i = 0; i < n; i++) {
                const uint64_t t = (uint64_t)u[i + j] + v[i] + c;
                u[i + j] = (uint32_t)(t & 0xffffffffu);
                c = t >> 32;
            }
            u[j + n] = (uint32_t)((u[j + n] + c) & 0xffffffffu);
        } else {
            u[j + n] = (uint32_t)(d - s);
        }
        q[j] = (uint32_t)qhat;
    }
}

bool jm_nat_divmod(jm_nat *q, jm_nat *rem, const jm_nat *a, const jm_nat *b)
{
    if (b->n == 0)
        return false;
    if (jm_nat_cmp(a, b) < 0) {
        if (rem != nullptr && !jm_nat_copy(rem, a))
            return false;
        if (q != nullptr)
            q->n = 0;
        return true;
    }
    if (b->n == 1) {
        jm_nat t = {0};
        uint32_t r = 0;
        jm_nat *out = q != nullptr && q != b ? q : &t;
        if (!nat_divmod_small(out, &r, a, nat_cw(b)[0])) {
            jm_nat_free(&t);
            return false;
        }
        if (rem != nullptr)
            jm_nat_set_u64(rem, r);
        if (q != nullptr && out == &t)
            jm_nat_swap(q, &t);
        jm_nat_free(&t);
        return true;
    }

    const int64_t m = a->n, n = b->n;
    jm_nat un = {0}, vn = {0}, qt = {0};
    bool ok = nat_grow(&un, m + 1) && nat_grow(&vn, n) &&
              nat_grow(&qt, m - n + 1);
    if (ok) {
        const uint32_t *x = nat_cw(a), *y = nat_cw(b);
        uint32_t *u = nat_w(&un), *v = nat_w(&vn);
        const int s = clz32(y[n - 1]);
        for (int64_t i = n - 1; i > 0; i--)
            v[i] = s ? (y[i] << s) | (y[i - 1] >> (32 - s)) : y[i];
        v[0] = y[0] << s;
        u[m] = s ? x[m - 1] >> (32 - s) : 0u;
        for (int64_t i = m - 1; i > 0; i--)
            u[i] = s ? (x[i] << s) | (x[i - 1] >> (32 - s)) : x[i];
        u[0] = x[0] << s;
        knuth_d(nat_w(&qt), u, v, m, n);
        for (int64_t i = 0; i < n; i++)
            u[i] = s ? (u[i] >> s) | (u[i + 1] << (32 - s)) : u[i];
        un.n = n;
        nat_trim(&un);
        qt.n = m - n + 1;
        nat_trim(&qt);
        if (rem != nullptr)
            jm_nat_swap(rem, &un);
        if (q != nullptr)
            jm_nat_swap(q, &qt);
    }
    jm_nat_free(&un);
    jm_nat_free(&vn);
    jm_nat_free(&qt);
    return ok;
}

static uint32_t inverse32(uint32_t v)
{
    uint32_t x = v;
    for (int k = 0; k < 4; k++)
        x *= 2u - v * x;
    return x;
}

static uint64_t nat_mod_bm1(const jm_nat *a)
{
    const uint32_t *w = nat_cw(a);
    uint64_t s = 0;
    for (int64_t i = 0; i < a->n; i++)
        s += w[i];
    while (s >> 32)
        s = (s & 0xffffffffu) + (s >> 32);
    return s == 0xffffffffu ? 0 : s;
}

static uint64_t nat_mod_bp1(const jm_nat *a)
{
    const uint32_t *w = nat_cw(a);
    uint64_t e = 0, o = 0;
    for (int64_t i = 0; i < a->n; i++) {
        if (i & 1)
            o += w[i];
        else
            e += w[i];
    }
    e %= NAT_BASE + 1;
    o %= NAT_BASE + 1;
    return e >= o ? e - o : e + NAT_BASE + 1 - o;
}

static uint64_t mulmod_bp1(uint64_t x, uint64_t y)
{
    if (x == NAT_BASE)
        return y == 0 ? 0 : NAT_BASE + 1 - y;
    if (y == NAT_BASE)
        return x == 0 ? 0 : NAT_BASE + 1 - x;
    return x * y % (NAT_BASE + 1);
}

static bool divexact_holds(const jm_nat *q, const jm_nat *a, const jm_nat *b)
{
    return nat_mod_bm1(q) * nat_mod_bm1(b) % (NAT_BASE - 1) == nat_mod_bm1(a) &&
           mulmod_bp1(nat_mod_bp1(q), nat_mod_bp1(b)) == nat_mod_bp1(a);
}

static void jebelean(uint32_t *z, uint32_t *w, int64_t qn, const uint32_t *y,
                     int64_t yn)
{
    const uint32_t inv = inverse32(y[0]);
    for (int64_t i = 0; i < qn; i++) {
        const uint32_t qi = w[i] * inv;
        z[i] = qi;
        if (qi == 0)
            continue;
        const int64_t lim = yn < qn - i ? yn : qn - i;
        uint64_t carry = 0, borrow = 0;
        for (int64_t j = 0; j < lim; j++) {
            const uint64_t p = (uint64_t)qi * y[j] + carry;
            carry = p >> 32;
            const uint64_t t = (uint64_t)w[i + j] - (p & 0xffffffffu) - borrow;
            w[i + j] = (uint32_t)(t & 0xffffffffu);
            borrow = t >> 63;
        }
        uint64_t c = carry + borrow;
        for (int64_t j = i + lim; c != 0 && j < qn; j++) {
            const uint64_t t = (uint64_t)w[j] - c;
            w[j] = (uint32_t)(t & 0xffffffffu);
            c = t >> 63;
        }
    }
}

bool jm_nat_divexact(jm_nat *q, const jm_nat *a, const jm_nat *b)
{
    if (b->n == 0)
        return false;
    if (a->n == 0) {
        q->n = 0;
        return true;
    }
    if (jm_nat_is_one(b))
        return jm_nat_copy(q, a);
    if (b->n == 1) {
        uint32_t r = 0;
        jm_nat t = {0};
        const bool ok = nat_divmod_small(&t, &r, a, nat_cw(b)[0]) && r == 0;
        if (ok)
            jm_nat_swap(q, &t);
        jm_nat_free(&t);
        return ok;
    }
    const int64_t s = jm_nat_twos(b);
    if (jm_nat_cmp(a, b) < 0 || jm_nat_twos(a) < s)
        return false;
    jm_nat u = {0}, v = {0}, out = {0};
    const jm_nat *pv = b;
    bool ok = jm_nat_shr(&u, a, s);
    if (ok && s > 0) {
        ok = jm_nat_shr(&v, b, s);
        pv = &v;
    }
    const int64_t qn = u.n - pv->n + 1;
    ok = ok && qn > 0 && nat_grow(&out, qn);
    if (ok) {
        jebelean(nat_w(&out), nat_w(&u), qn, nat_cw(pv), pv->n);
        out.n = qn;
        nat_trim(&out);
        ok = divexact_holds(&out, a, b);
    }
    if (ok)
        jm_nat_swap(q, &out);
    jm_nat_free(&u);
    jm_nat_free(&v);
    jm_nat_free(&out);
    return ok;
}

static uint64_t gcd_u64(uint64_t x, uint64_t y)
{
    while (y != 0) {
        const uint64_t t = x % y;
        x = y;
        y = t;
    }
    return x;
}

static int64_t nat_bits_at(const jm_nat *a, int64_t sh)
{
    const uint32_t *w = nat_cw(a);
    const int64_t l = sh / 32, o = sh % 32;
    uint64_t v = l < a->n ? w[l] : 0u;
    if (l + 1 < a->n)
        v |= (uint64_t)w[l + 1] << 32;
    return (int64_t)((v >> o) & 0x7fffffffu);
}

static void nat_lincomb(uint32_t *z, const jm_nat *x, uint64_t fx,
                        const jm_nat *y, uint64_t fy, int64_t n)
{
    const uint32_t *px = nat_cw(x), *py = nat_cw(y);
    uint64_t cx = 0, cy = 0, borrow = 0;
    for (int64_t i = 0; i < n; i++) {
        const uint64_t p = fx * (i < x->n ? px[i] : 0u) + cx;
        const uint64_t q = fy * (i < y->n ? py[i] : 0u) + cy;
        cx = p >> 32;
        cy = q >> 32;
        const uint64_t t = (p & 0xffffffffu) - (q & 0xffffffffu) - borrow;
        z[i] = (uint32_t)(t & 0xffffffffu);
        borrow = t >> 63;
    }
}

static bool nat_combine(jm_nat *out, const jm_nat *u, int64_t a,
                        const jm_nat *v, int64_t b)
{
    const int64_t n = u->n;
    out->n = 0;
    if (!nat_grow(out, n))
        return false;
    if (b <= 0)
        nat_lincomb(nat_w(out), u, (uint64_t)a, v, (uint64_t)-b, n);
    else
        nat_lincomb(nat_w(out), v, (uint64_t)b, u, (uint64_t)-a, n);
    out->n = n;
    nat_trim(out);
    return true;
}

bool jm_nat_gcd(jm_nat *r, const jm_nat *a, const jm_nat *b)
{
    if (a->n == 0)
        return jm_nat_copy(r, b);
    if (b->n == 0)
        return jm_nat_copy(r, a);
    if (jm_nat_is_one(a) || jm_nat_is_one(b)) {
        jm_nat_set_u64(r, 1);
        return true;
    }
    if (a->n <= 2 && b->n <= 2) {
        jm_nat_set_u64(r, gcd_u64(nat_to_u64(a), nat_to_u64(b)));
        return true;
    }
    jm_nat u = {0}, v = {0}, t = {0}, w = {0};
    const bool big = jm_nat_cmp(a, b) >= 0;
    bool ok = jm_nat_copy(&u, big ? a : b) && jm_nat_copy(&v, big ? b : a);
    while (ok && v.n > 2) {
        const int64_t sh = jm_nat_bits(&u) - 31;
        int64_t uh = nat_bits_at(&u, sh), vh = nat_bits_at(&v, sh);
        int64_t A = 1, B = 0, C = 0, D = 1;
        for (;;) {
            if (vh + C <= 0 || vh + D <= 0 || uh + A < 0 || uh + B < 0)
                break;
            const int64_t q = (uh + A) / (vh + C);
            if (q != (uh + B) / (vh + D))
                break;
            int64_t T = A - q * C;
            A = C;
            C = T;
            T = B - q * D;
            B = D;
            D = T;
            T = uh - q * vh;
            uh = vh;
            vh = T;
        }
        if (B == 0) {
            ok = jm_nat_divmod(nullptr, &t, &u, &v);
            jm_nat_swap(&u, &v);
            jm_nat_swap(&v, &t);
        } else {
            ok = nat_combine(&t, &u, A, &v, B) && nat_combine(&w, &u, C, &v, D);
            jm_nat_swap(&u, &t);
            jm_nat_swap(&v, &w);
        }
    }
    if (ok) {
        if (v.n == 0) {
            jm_nat_swap(r, &u);
        } else {
            ok = jm_nat_divmod(nullptr, &t, &u, &v);
            if (ok)
                jm_nat_set_u64(r, gcd_u64(nat_to_u64(&v), nat_to_u64(&t)));
        }
    }
    jm_nat_free(&u);
    jm_nat_free(&v);
    jm_nat_free(&t);
    jm_nat_free(&w);
    return ok;
}

static bool nat_muladd_small(jm_nat *a, uint32_t f, uint32_t add)
{
    if (!nat_grow(a, a->n + 1))
        return false;
    uint32_t *w = nat_w(a);
    uint64_t c = add;
    for (int64_t i = 0; i < a->n; i++) {
        const uint64_t t = (uint64_t)w[i] * f + c;
        w[i] = (uint32_t)(t & 0xffffffffu);
        c = t >> 32;
    }
    if (c != 0)
        w[a->n++] = (uint32_t)c;
    return nat_fits(a);
}

void jm_bigint_free(jm_bigint *a)
{
    jm_nat_free(&a->mag);
    a->sign = 0;
}

void jm_bigint_swap(jm_bigint *a, jm_bigint *b)
{
    const jm_bigint t = *a;
    *a = *b;
    *b = t;
}

bool jm_bigint_copy(jm_bigint *r, const jm_bigint *a)
{
    if (r == a)
        return true;
    if (!jm_nat_copy(&r->mag, &a->mag))
        return false;
    r->sign = a->sign;
    return true;
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

static bool int_add_mag(jm_bigint *r, const jm_nat *x, int32_t sx,
                        const jm_nat *y, int32_t sy)
{
    if (sy == 0) {
        if (!jm_nat_copy(&r->mag, x))
            return false;
        int_fix_sign(r, sx);
        return true;
    }
    if (sx == 0) {
        if (!jm_nat_copy(&r->mag, y))
            return false;
        int_fix_sign(r, sy);
        return true;
    }
    if (sx == sy) {
        if (!jm_nat_add(&r->mag, x, y))
            return false;
        int_fix_sign(r, sx);
        return true;
    }
    const int c = jm_nat_cmp(x, y);
    if (c == 0) {
        jm_bigint_set_zero(r);
        return true;
    }
    if (!(c > 0 ? jm_nat_sub(&r->mag, x, y) : jm_nat_sub(&r->mag, y, x)))
        return false;
    int_fix_sign(r, c > 0 ? sx : sy);
    return true;
}

bool jm_bigint_add(jm_bigint *r, const jm_bigint *a, const jm_bigint *b)
{
    return int_add_mag(r, &a->mag, a->sign, &b->mag, b->sign);
}

bool jm_bigint_sub(jm_bigint *r, const jm_bigint *a, const jm_bigint *b)
{
    return int_add_mag(r, &a->mag, a->sign, &b->mag, (int32_t)-b->sign);
}

bool jm_bigint_mul(jm_bigint *r, const jm_bigint *a, const jm_bigint *b)
{
    const int32_t s = a->sign * b->sign;
    if (!jm_nat_mul(&r->mag, &a->mag, &b->mag))
        return false;
    int_fix_sign(r, s);
    return true;
}

bool jm_bigint_shl(jm_bigint *r, const jm_bigint *a, int64_t bits)
{
    if (bits < 0)
        return false;
    const int32_t s = a->sign;
    if (!jm_nat_shl(&r->mag, &a->mag, bits))
        return false;
    int_fix_sign(r, s);
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
    const int32_t s = a->sign * b->sign;
    if (!jm_nat_divexact(&q->mag, &a->mag, &b->mag))
        return false;
    int_fix_sign(q, s);
    return true;
}

void jm_rational_free(jm_rational *r)
{
    jm_bigint_free(&r->num);
    jm_nat_free(&r->den);
}

void jm_rational_swap(jm_rational *a, jm_rational *b)
{
    const jm_rational t = *a;
    *a = *b;
    *b = t;
}

void jm_rational_free_array(jm_rational *r, int64_t n)
{
    if (r == nullptr)
        return;
    for (int64_t k = 0; k < n; k++)
        jm_rational_free(&r[k]);
    free(r);
}

bool jm_rational_copy(jm_rational *r, const jm_rational *a)
{
    if (r == a)
        return true;
    return jm_bigint_copy(&r->num, &a->num) && jm_nat_copy(&r->den, &a->den);
}

bool jm_rational_from_bigint(jm_rational *r, const jm_bigint *v)
{
    if (!jm_bigint_copy(&r->num, v))
        return false;
    jm_nat_set_u64(&r->den, 1);
    return true;
}

static bool rational_normalise(jm_rational *r)
{
    if (r->num.sign == 0) {
        jm_nat_set_u64(&r->den, 1);
        return true;
    }
    jm_nat g = {0};
    bool ok = jm_nat_gcd(&g, &r->num.mag, &r->den);
    if (ok && !jm_nat_is_one(&g))
        ok = jm_nat_divexact(&r->num.mag, &r->num.mag, &g) &&
             jm_nat_divexact(&r->den, &r->den, &g);
    jm_nat_free(&g);
    if (ok)
        int_fix_sign(&r->num, r->num.sign);
    return ok;
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

bool jm_rational_scale2(jm_rational *r, int64_t e)
{
    if (e == 0 || r->num.sign == 0)
        return true;
    jm_nat *up = e > 0 ? &r->num.mag : &r->den;
    jm_nat *down = e > 0 ? &r->den : &r->num.mag;
    const int64_t k = e > 0 ? e : -e;
    int64_t t = jm_nat_twos(down);
    if (t > k)
        t = k;
    return jm_nat_shr(down, down, t) && jm_nat_shl(up, up, k - t);
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
    int64_t mant = (int64_t)ldexp(f, 53);
    int64_t exp2 = (int64_t)e - 53;
    while ((mant & 1) == 0) {
        mant /= 2;
        exp2++;
    }
    jm_rational_set_i64(r, mant);
    return jm_rational_scale2(r, exp2);
}

static bool rational_addsub(jm_rational *r, const jm_rational *a,
                            const jm_rational *c, int32_t flip)
{
    const int32_t sa = a->num.sign, sc = flip * c->num.sign;
    if (sc == 0)
        return jm_rational_copy(r, a);
    if (sa == 0) {
        if (!jm_rational_copy(r, c))
            return false;
        r->num.sign = sc;
        return true;
    }

    const jm_nat *u = &a->num.mag, *ud = &a->den;
    const jm_nat *v = &c->num.mag, *vd = &c->den;
    const bool ai = jm_nat_is_one(ud), ci = jm_nat_is_one(vd);
    jm_rational o = {0};
    jm_nat p = {0}, q = {0}, d1 = {0}, d2 = {0}, x = {0}, y = {0};
    bool ok;
    if (ai && ci) {
        ok = int_add_mag(&o.num, u, sa, v, sc);
        jm_nat_set_u64(&o.den, 1);
    } else if (ai || ci) {
        const jm_nat *in = ai ? u : v, *fr = ai ? v : u, *frd = ai ? vd : ud;
        const int32_t si = ai ? sa : sc, sf = ai ? sc : sa;
        ok = jm_nat_mul(&p, in, frd) && int_add_mag(&o.num, &p, si, fr, sf) &&
             jm_nat_copy(&o.den, frd);
    } else {
        ok = jm_nat_gcd(&d1, ud, vd);
        if (ok && jm_nat_is_one(&d1)) {
            ok = jm_nat_mul(&p, u, vd) && jm_nat_mul(&q, v, ud) &&
                 int_add_mag(&o.num, &p, sa, &q, sc) &&
                 jm_nat_mul(&o.den, ud, vd);
        } else if (ok) {
            ok = jm_nat_divexact(&x, vd, &d1) &&
                 jm_nat_divexact(&y, ud, &d1) &&
                 jm_nat_mul(&p, u, &x) && jm_nat_mul(&q, v, &y) &&
                 int_add_mag(&o.num, &p, sa, &q, sc);
            if (ok && o.num.sign != 0) {
                ok = jm_nat_gcd(&d2, &o.num.mag, &d1);
                const bool one = ok && jm_nat_is_one(&d2);
                if (ok && !one)
                    ok = jm_nat_divexact(&o.num.mag, &o.num.mag, &d2) &&
                         jm_nat_divexact(&x, vd, &d2);
                ok = ok && jm_nat_mul(&o.den, &y, one ? vd : &x);
            }
        }
    }
    if (ok) {
        if (o.num.sign == 0)
            jm_nat_set_u64(&o.den, 1);
        jm_rational_swap(r, &o);
    }
    jm_rational_free(&o);
    jm_nat_free(&p);
    jm_nat_free(&q);
    jm_nat_free(&d1);
    jm_nat_free(&d2);
    jm_nat_free(&x);
    jm_nat_free(&y);
    return ok;
}

bool jm_rational_add(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    return rational_addsub(r, a, c, 1);
}

bool jm_rational_sub(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    return rational_addsub(r, a, c, -1);
}

static bool rational_scale(jm_rational *r, const jm_rational *a,
                           const jm_nat *vn, int32_t vs, const jm_nat *vd)
{
    const int32_t s = a->num.sign * vs;
    if (s == 0) {
        jm_rational_set_zero(r);
        return true;
    }
    jm_nat g1 = {0}, g2 = {0}, an = {0}, cn = {0}, bd = {0}, dd = {0};
    jm_rational o = {0};
    bool ok = jm_nat_gcd(&g1, &a->num.mag, vd) && jm_nat_gcd(&g2, vn, &a->den);
    const jm_nat *pan = &a->num.mag, *pdd = vd, *pcn = vn, *pbd = &a->den;
    if (ok && !jm_nat_is_one(&g1)) {
        ok = jm_nat_divexact(&an, &a->num.mag, &g1) &&
             jm_nat_divexact(&dd, vd, &g1);
        pan = &an;
        pdd = &dd;
    }
    if (ok && !jm_nat_is_one(&g2)) {
        ok = jm_nat_divexact(&cn, vn, &g2) &&
             jm_nat_divexact(&bd, &a->den, &g2);
        pcn = &cn;
        pbd = &bd;
    }
    ok = ok && jm_nat_mul(&o.num.mag, pan, pcn) && jm_nat_mul(&o.den, pbd, pdd);
    if (ok) {
        int_fix_sign(&o.num, s);
        jm_rational_swap(r, &o);
    }
    jm_rational_free(&o);
    jm_nat_free(&g1);
    jm_nat_free(&g2);
    jm_nat_free(&an);
    jm_nat_free(&cn);
    jm_nat_free(&bd);
    jm_nat_free(&dd);
    return ok;
}

bool jm_rational_mul(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    return rational_scale(r, a, &c->num.mag, c->num.sign, &c->den);
}

bool jm_rational_div(jm_rational *r, const jm_rational *a,
                     const jm_rational *c)
{
    if (c->num.sign == 0)
        return false;
    return rational_scale(r, a, &c->den, c->num.sign, &c->num.mag);
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
    if (jm_nat_is_one(&a->den) && jm_nat_is_one(&c->den)) {
        *out = jm_bigint_cmp(&a->num, &c->num);
        return true;
    }
    jm_nat l = {0}, rr = {0};
    const bool ok = jm_nat_mul(&l, &a->num.mag, &c->den) &&
                    jm_nat_mul(&rr, &c->num.mag, &a->den);
    if (ok) {
        const int m = jm_nat_cmp(&l, &rr);
        *out = a->num.sign > 0 ? m : -m;
    }
    jm_nat_free(&l);
    jm_nat_free(&rr);
    return ok;
}

int jm_rational_cmp(const jm_rational *a, const jm_rational *c)
{
    int r = 0;
    if (!jm_rational_cmp_checked(a, c, &r))
        return 0;
    return r;
}

static bool nat_ten_power(jm_nat *p, int64_t k)
{
    jm_nat base = {0}, t = {0};
    jm_nat_set_u64(p, 1);
    jm_nat_set_u64(&base, 10);
    bool ok = true;
    for (int64_t n = k; ok && n > 0; n /= 2) {
        if (n % 2 == 1)
            ok = jm_nat_mul(p, p, &base);
        if (ok && n > 1)
            ok = jm_nat_mul(&t, &base, &base);
        jm_nat_swap(&base, &t);
    }
    jm_nat_free(&base);
    jm_nat_free(&t);
    return ok;
}

static bool rational_ten_power(jm_rational *r, int64_t k)
{
    jm_nat p = {0};
    const bool ok = nat_ten_power(&p, k < 0 ? -k : k);
    if (ok) {
        r->num.sign = 1;
        if (k < 0) {
            jm_nat_set_u64(&r->num.mag, 1);
            jm_nat_swap(&r->den, &p);
        } else {
            jm_nat_swap(&r->num.mag, &p);
            jm_nat_set_u64(&r->den, 1);
        }
    }
    jm_nat_free(&p);
    return ok;
}

static bool rational_half_gap(jm_rational *r, double gap)
{
    return jm_rational_from_double(r, gap) && jm_rational_scale2(r, -1);
}

bool jm_rational_from_double_decimal(jm_rational *r, double d)
{
    if (!isfinite(d))
        return false;
    if (d == 0.0) {
        jm_rational_set_zero(r);
        return true;
    }
    const double a = fabs(d);
    const double below = nextafter(a, 0.0), above = nextafter(a, HUGE_VAL);
    uint64_t bits = 0;
    memcpy(&bits, &a, sizeof bits);
    const bool closed = (bits & 1u) == 0;

    jm_rational v = {0}, lo = {0}, hi = {0}, gap = {0}, p10 = {0}, x = {0},
                cand = {0}, scaled = {0};
    jm_nat q = {0}, rem = {0}, twice = {0}, one = {0};
    bool ok = jm_rational_from_double(&v, a) &&
              rational_half_gap(&gap, a - below) &&
              jm_rational_sub(&lo, &v, &gap) &&
              rational_half_gap(&gap, isfinite(above) ? above - a : a - below) &&
              jm_rational_add(&hi, &v, &gap);
    bool found = false;

    int e2 = 0;
    (void)frexp(a, &e2);
    const int64_t t = (int64_t)(e2 - 1) * 78913;
    int64_t e10 = t >= 0 ? t / 262144 : -((-t + 262143) / 262144);
    int c = 0;
    while (ok) {
        ok = rational_ten_power(&p10, e10 + 1) &&
             jm_rational_cmp_checked(&v, &p10, &c);
        if (!ok || c < 0)
            break;
        e10++;
    }
    while (ok) {
        ok = rational_ten_power(&p10, e10) &&
             jm_rational_cmp_checked(&v, &p10, &c);
        if (!ok || c >= 0)
            break;
        e10--;
    }

    jm_nat_set_u64(&one, 1);
    for (int64_t digits = 1; ok && !found && digits <= 17; digits++) {
        ok = rational_ten_power(&p10, digits - 1 - e10) &&
             jm_rational_mul(&x, &v, &p10) &&
             jm_nat_divmod(&q, &rem, &x.num.mag, &x.den) &&
             jm_nat_shl(&twice, &rem, 1);
        if (!ok)
            break;
        const int up = jm_nat_cmp(&twice, &x.den);
        if ((up > 0 || (up == 0 && nat_bit(&q, 0))) && !jm_nat_add(&q, &q, &one)) {
            ok = false;
            break;
        }
        jm_nat_swap(&scaled.num.mag, &q);
        int_fix_sign(&scaled.num, 1);
        jm_nat_set_u64(&scaled.den, 1);
        int cl = 0, ch = 0;
        ok = jm_rational_div(&cand, &scaled, &p10) &&
             jm_rational_cmp_checked(&cand, &lo, &cl) &&
             jm_rational_cmp_checked(&cand, &hi, &ch);
        if (ok && (closed ? cl >= 0 : cl > 0) && (closed ? ch <= 0 : ch < 0)) {
            jm_rational_swap(r, &cand);
            if (d < 0.0)
                jm_rational_neg(r);
            found = true;
        }
    }
    jm_rational_free(&v);
    jm_rational_free(&lo);
    jm_rational_free(&hi);
    jm_rational_free(&gap);
    jm_rational_free(&p10);
    jm_rational_free(&x);
    jm_rational_free(&cand);
    jm_rational_free(&scaled);
    jm_nat_free(&q);
    jm_nat_free(&rem);
    jm_nat_free(&twice);
    jm_nat_free(&one);
    if (!ok)
        return false;
    return found || jm_rational_from_double(r, d);
}

bool jm_rational_from_value(jm_rational *r, double d, bool decimal)
{
    return decimal ? jm_rational_from_double_decimal(r, d)
                   : jm_rational_from_double(r, d);
}

static double round_to_double(jm_nat *q, bool sticky, int64_t shift,
                              int32_t sign)
{
    const int64_t qbits = jm_nat_bits(q);
    int64_t drop = qbits - 53;
    if (shift - 1074 > drop)
        drop = shift - 1074;
    if (drop < 0)
        drop = 0;
    if (drop > qbits)
        drop = qbits + 1;
    bool round_bit = false;
    if (drop > 0) {
        round_bit = nat_bit(q, drop - 1);
        for (int64_t i = 0; i < drop - 1 && !sticky; i++)
            sticky = nat_bit(q, i);
        if (!jm_nat_shr(q, q, drop))
            return (double)NAN;
    }
    uint64_t m = nat_to_u64(q);
    int64_t e2 = drop - shift;
    if (round_bit && (sticky || (m & 1u))) {
        m++;
        if (m == (1ull << 53)) {
            m >>= 1;
            e2++;
        }
    }
    const double v = ldexp((double)m, (int)e2);
    return sign > 0 ? v : -v;
}

double jm_rational_to_double(const jm_rational *r)
{
    if (r->num.sign == 0)
        return 0.0;

    const int64_t bn = jm_nat_bits(&r->num.mag), bd = jm_nat_bits(&r->den);
    const int64_t shift = 55 - (bn - bd);
    jm_nat num = {0}, den = {0}, q = {0}, rem = {0};
    double v = r->num.sign > 0 ? HUGE_VAL : -HUGE_VAL;
    bool ok = shift > 0 ? jm_nat_shl(&num, &r->num.mag, shift)
                        : jm_nat_copy(&num, &r->num.mag);
    if (!ok && shift > 0) {
        jm_nat_free(&num);
        return v;
    }
    if (ok)
        ok = shift < 0 ? jm_nat_shl(&den, &r->den, -shift)
                       : jm_nat_copy(&den, &r->den);
    if (!ok && shift < 0)
        v = 0.0;
    if (ok && jm_nat_divmod(&q, &rem, &num, &den))
        v = round_to_double(&q, !jm_nat_is_zero(&rem), shift, r->num.sign);
    jm_nat_free(&num);
    jm_nat_free(&den);
    jm_nat_free(&q);
    jm_nat_free(&rem);
    return v;
}

static bool dyadic_trim(jm_dyadic *d)
{
    if (d->m.sign == 0) {
        d->e = 0;
        return true;
    }
    const int64_t z = jm_nat_twos(&d->m.mag);
    if (z == 0)
        return true;
    if (!jm_nat_shr(&d->m.mag, &d->m.mag, z))
        return false;
    d->e += z;
    return true;
}

void jm_dyadic_free(jm_dyadic *d)
{
    jm_bigint_free(&d->m);
    d->e = 0;
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
    return dyadic_trim(d);
}

bool jm_dyadic_mul(jm_dyadic *r, const jm_dyadic *a, const jm_dyadic *b)
{
    int64_t e;
    if (ckd_add(&e, a->e, b->e))
        return false;
    if (!jm_bigint_mul(&r->m, &a->m, &b->m))
        return false;
    r->e = e;
    return dyadic_trim(r);
}

static bool dyadic_addsub(jm_dyadic *r, const jm_dyadic *a,
                          const jm_dyadic *b, int32_t flip)
{
    const int32_t sa = a->m.sign, sb = flip * b->m.sign;
    if (sb == 0) {
        if (!jm_bigint_copy(&r->m, &a->m))
            return false;
        r->e = a->e;
        return true;
    }
    if (sa == 0) {
        if (!jm_bigint_copy(&r->m, &b->m))
            return false;
        r->m.sign = sb;
        r->e = b->e;
        return true;
    }
    const bool alo = a->e <= b->e;
    const jm_dyadic *lo = alo ? a : b, *hi = alo ? b : a;
    const int32_t slo = alo ? sa : sb, shi = alo ? sb : sa;
    int64_t diff;
    if (ckd_sub(&diff, hi->e, lo->e))
        return false;
    const int64_t e = lo->e;
    jm_nat up = {0};
    bool ok = jm_nat_shl(&up, &hi->m.mag, diff) &&
              int_add_mag(&r->m, &lo->m.mag, slo, &up, shi);
    jm_nat_free(&up);
    if (!ok)
        return false;
    r->e = e;
    return dyadic_trim(r);
}

bool jm_dyadic_add(jm_dyadic *r, const jm_dyadic *a, const jm_dyadic *b)
{
    return dyadic_addsub(r, a, b, 1);
}

bool jm_dyadic_sub(jm_dyadic *r, const jm_dyadic *a, const jm_dyadic *b)
{
    return dyadic_addsub(r, a, b, -1);
}

bool jm_dyadic_cmp(const jm_dyadic *a, const jm_dyadic *b, int *out)
{
    if (a->m.sign != b->m.sign) {
        *out = a->m.sign < b->m.sign ? -1 : 1;
        return true;
    }
    jm_dyadic d = {0};
    const bool ok = jm_dyadic_sub(&d, a, b);
    if (ok)
        *out = d.m.sign;
    jm_dyadic_free(&d);
    return ok;
}

double jm_dyadic_to_double(const jm_dyadic *d)
{
    if (d->m.sign == 0)
        return 0.0;

    jm_nat q = {0};
    if (!jm_nat_copy(&q, &d->m.mag))
        return (double)NAN;
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
        if (!jm_nat_shr(&q, &q, drop)) {
            jm_nat_free(&q);
            return (double)NAN;
        }
    }

    uint64_t m = nat_to_u64(&q);
    jm_nat_free(&q);
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
    jm_dyadic b = {0}, d = {0};
    int c = 0;
    bool ok = true;
    if (isfinite(lo)) {
        ok = jm_dyadic_from_double(&b, lo) && jm_dyadic_sub(&d, &b, v) &&
             jm_dyadic_cmp(&d, out, &c);
        if (ok && c > 0) {
            jm_bigint_swap(&out->m, &d.m);
            out->e = d.e;
        }
    }
    if (ok && isfinite(hi)) {
        ok = jm_dyadic_from_double(&b, hi) && jm_dyadic_sub(&d, v, &b) &&
             jm_dyadic_cmp(&d, out, &c);
        if (ok && c > 0) {
            jm_bigint_swap(&out->m, &d.m);
            out->e = d.e;
        }
    }
    jm_dyadic_free(&b);
    jm_dyadic_free(&d);
    return ok;
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
    jm_dyadic acc = {0}, term = {0}, xv = {0}, cv = {0}, viol = {0},
              worst = {0};
    bool ok = jm_model_ensure_rowwise(m) == JAOS_OK &&
              jm_dyadic_from_double(&acc, m->obj_offset);

    for (int64_t j = 0; ok && j < m->num_col; j++) {
        if (m->col_cost[j] == 0.0 || x[j] == 0.0)
            continue;
        ok = jm_dyadic_from_double(&cv, m->col_cost[j]) &&
             jm_dyadic_from_double(&xv, x[j]) &&
             jm_dyadic_mul(&term, &cv, &xv) && jm_dyadic_add(&acc, &acc, &term);
        if (ok)
            p.terms++;
    }
    if (ok)
        p.objective = jm_dyadic_to_double(&acc);

    jm_dyadic_set_zero(&worst);
    for (int64_t j = 0; ok && j < m->num_col; j++) {
        int c = 0;
        ok = jm_dyadic_from_double(&xv, x[j]) &&
             exact_violation(&viol, &xv, m->col_lower[j], m->col_upper[j]) &&
             jm_dyadic_cmp(&viol, &worst, &c);
        if (ok && c > 0) {
            jm_bigint_swap(&worst.m, &viol.m);
            worst.e = viol.e;
            p.col_at = j;
        }
    }
    if (ok)
        p.col_violation = jm_dyadic_to_double(&worst);

    jm_dyadic_set_zero(&worst);
    for (int64_t i = 0; ok && i < m->num_row; i++) {
        jm_dyadic_set_zero(&acc);
        for (int64_t k = m->ar_start[i]; ok && k < m->ar_start[i + 1]; k++) {
            const int64_t j = m->ar_index[k];
            if (m->ar_value[k] == 0.0 || x[j] == 0.0)
                continue;
            ok = jm_dyadic_from_double(&cv, m->ar_value[k]) &&
                 jm_dyadic_from_double(&xv, x[j]) &&
                 jm_dyadic_mul(&term, &cv, &xv) &&
                 jm_dyadic_add(&acc, &acc, &term);
            if (ok)
                p.terms++;
        }
        int c = 0;
        ok = ok &&
             exact_violation(&viol, &acc, m->row_lower[i], m->row_upper[i]) &&
             jm_dyadic_cmp(&viol, &worst, &c);
        if (ok && c > 0) {
            jm_bigint_swap(&worst.m, &viol.m);
            worst.e = viol.e;
            p.row_at = i;
        }
    }
    if (ok)
        p.row_violation = jm_dyadic_to_double(&worst);
    jm_dyadic_free(&acc);
    jm_dyadic_free(&term);
    jm_dyadic_free(&xv);
    jm_dyadic_free(&cv);
    jm_dyadic_free(&viol);
    jm_dyadic_free(&worst);
    if (ok) {
        *out = p;
        return true;
    }

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
    if (jm_nat_is_zero(a)) {
        if (cap < 1)
            return -1;
        buf[0] = '0';
        return 1;
    }
    const int64_t most = jm_nat_bits(a) / 29 + 2;
    uint32_t *chunk = jm_alloc_array(most, sizeof *chunk);
    jm_nat cur = {0};
    if (chunk == nullptr || !jm_nat_copy(&cur, a)) {
        free(chunk);
        jm_nat_free(&cur);
        return -1;
    }
    int64_t n = 0;
    bool ok = true;
    while (ok && !jm_nat_is_zero(&cur) && n < most)
        ok = nat_divmod_small(&cur, &chunk[n++], &cur, 1000000000u);
    ok = ok && jm_nat_is_zero(&cur);
    jm_nat_free(&cur);
    int64_t len = 0;
    for (int64_t k = n - 1; ok && k >= 0; k--) {
        char piece[16];
        const int w = k == n - 1 ? snprintf(piece, sizeof piece, "%u", chunk[k])
                                 : snprintf(piece, sizeof piece, "%09u", chunk[k]);
        if (len + w > cap) {
            ok = false;
            break;
        }
        memcpy(buf + len, piece, (size_t)w);
        len += w;
    }
    free(chunk);
    return ok ? len : -1;
}

static bool nat_from_decimal(jm_nat *a, const char *s, const char **end)
{
    if (*s < '0' || *s > '9')
        return false;
    jm_nat_set_zero(a);
    const char *p = s;
    while (*p >= '0' && *p <= '9') {
        uint32_t chunk = 0, scale = 1;
        for (int k = 0; k < 9 && *p >= '0' && *p <= '9'; k++, p++) {
            chunk = chunk * 10u + (uint32_t)(*p - '0');
            scale *= 10u;
        }
        if (!nat_muladd_small(a, scale, chunk))
            return false;
    }
    nat_trim(a);
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
    const int64_t digits = (jm_nat_bits(&r->num.mag) * 30103) / 100000 + 2;
    const int64_t ddigits = (jm_nat_bits(&r->den) * 30103) / 100000 + 2;
    const int64_t cap = digits + ddigits + 4;
    char *s = jm_alloc_array(cap, 1);
    if (s == nullptr)
        return nullptr;
    int64_t at = 0;
    if (r->num.sign < 0)
        s[at++] = '-';
    int64_t w = nat_decimal(&r->num.mag, s + at, cap - at - 1);
    if (w < 0) {
        free(s);
        return nullptr;
    }
    at += w;
    if (!jm_nat_is_one(&r->den)) {
        s[at++] = '/';
        w = nat_decimal(&r->den, s + at, cap - at - 1);
        if (w < 0) {
            free(s);
            return nullptr;
        }
        at += w;
    }
    s[at] = '\0';
    return s;
}
