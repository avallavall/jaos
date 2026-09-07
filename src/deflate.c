/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char *buf;
    int64_t len, cap;
    uint32_t acc;
    int nbits;
    bool ok;
} bitsink;

static bool bs_room(bitsink *s, int64_t more)
{
    if (!s->ok)
        return false;
    if (s->len + more <= s->cap)
        return true;
    int64_t cap = s->cap > 0 ? s->cap : 1024;
    while (cap < s->len + more) {
        if (cap > INT64_MAX / 2) {
            s->ok = false;
            return false;
        }
        cap *= 2;
    }
    unsigned char *nb = realloc(s->buf, (size_t)cap);
    if (nb == nullptr) {
        s->ok = false;
        return false;
    }
    s->buf = nb;
    s->cap = cap;
    return true;
}

static void bs_byte(bitsink *s, unsigned char c)
{
    if (!bs_room(s, 1))
        return;
    s->buf[s->len++] = c;
}

static void bs_put(bitsink *s, uint32_t v, int n)
{
    s->acc |= (v & ((1u << n) - 1u)) << s->nbits;
    s->nbits += n;
    while (s->nbits >= 8) {
        bs_byte(s, (unsigned char)(s->acc & 0xffu));
        s->acc >>= 8;
        s->nbits -= 8;
    }
}

static void bs_code(bitsink *s, uint32_t code, int len)
{
    uint32_t r = 0;
    for (int k = 0; k < len; k++) {
        r = (r << 1) | ((code >> k) & 1u);
    }
    bs_put(s, r, len);
}

static void bs_align(bitsink *s)
{
    if (s->nbits > 0)
        bs_put(s, 0, 8 - s->nbits);
}

static void fixed_lit(int sym, uint32_t *code, int *len)
{
    if (sym < 144)        { *code = (uint32_t)(0x30 + sym);        *len = 8; }
    else if (sym < 256)   { *code = (uint32_t)(0x190 + sym - 144); *len = 9; }
    else if (sym < 280)   { *code = (uint32_t)(sym - 256);         *len = 7; }
    else                  { *code = (uint32_t)(0xc0 + sym - 280);  *len = 8; }
}

static const int16_t len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
    59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const unsigned char len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
    4, 5, 5, 5, 5, 0
};
static const int32_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const unsigned char dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
    10, 11, 11, 12, 12, 13, 13
};

constexpr int32_t WINDOW = 32768;
constexpr int32_t MIN_MATCH = 3;
constexpr int32_t MAX_MATCH = 258;
constexpr int32_t HASH_BITS = 15;
constexpr int32_t HASH_SIZE = 1 << HASH_BITS;

constexpr int32_t CHAIN_MAX = 128;

static uint32_t hash3(const unsigned char *p)
{
    return (((uint32_t)p[0] << 10) ^ ((uint32_t)p[1] << 5) ^ (uint32_t)p[2])
           & (uint32_t)(HASH_SIZE - 1);
}

static void deflate_fixed(bitsink *s, const unsigned char *in, int64_t n,
                          int32_t *head, int32_t *prev)
{
    for (int32_t i = 0; i < HASH_SIZE; i++)
        head[i] = -1;

    bs_put(s, 1, 1);
    bs_put(s, 1, 2);

    int64_t at = 0;
    while (at < n) {
        int32_t best_len = 0;
        int64_t best_dist = 0;

        if (n - at >= MIN_MATCH) {
            const uint32_t h = hash3(in + at);
            int64_t cand = head[h];
            int32_t steps = 0;
            while (cand >= 0 && steps < CHAIN_MAX) {
                const int64_t dist = at - cand;
                if (dist <= 0 || dist > WINDOW)
                    break;
                int32_t l = 0;
                const int64_t room = n - at < MAX_MATCH ? n - at : MAX_MATCH;
                while (l < room && in[cand + l] == in[at + l])
                    l++;
                if (l > best_len) {
                    best_len = l;
                    best_dist = dist;
                    if (l >= MAX_MATCH)
                        break;
                }
                cand = prev[cand & (WINDOW - 1)];
                steps++;
            }
        }

        int64_t take = 1;
        if (best_len >= MIN_MATCH) {

            int lc = 28;
            while (lc > 0 && len_base[lc] > best_len)
                lc--;
            uint32_t code; int clen;
            fixed_lit(257 + lc, &code, &clen);
            bs_code(s, code, clen);
            if (len_extra[lc] > 0)
                bs_put(s, (uint32_t)(best_len - len_base[lc]),
                       len_extra[lc]);

            int dc = 29;
            while (dc > 0 && dist_base[dc] > best_dist)
                dc--;
            bs_code(s, (uint32_t)dc, 5);
            if (dist_extra[dc] > 0)
                bs_put(s, (uint32_t)(best_dist - dist_base[dc]),
                       dist_extra[dc]);
            take = best_len;
        } else {
            uint32_t code; int clen;
            fixed_lit(in[at], &code, &clen);
            bs_code(s, code, clen);
        }

        for (int64_t k = 0; k < take; k++) {
            const int64_t p = at + k;
            if (n - p < MIN_MATCH)
                break;
            const uint32_t h = hash3(in + p);
            prev[p & (WINDOW - 1)] = head[h];
            head[h] = (int32_t)p;
        }
        at += take;
        if (!s->ok)
            return;
    }

    uint32_t code; int clen;
    fixed_lit(256, &code, &clen);
    bs_code(s, code, clen);
    bs_align(s);
}

static uint32_t crc32_of(const unsigned char *p, int64_t n)
{
    uint32_t tab[256];
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (UINT32_C(0xedb88320) ^ (c >> 1)) : (c >> 1);
        tab[i] = c;
    }
    uint32_t c = UINT32_C(0xffffffff);
    for (int64_t i = 0; i < n; i++)
        c = tab[(c ^ p[i]) & 0xffu] ^ (c >> 8);
    return c ^ UINT32_C(0xffffffff);
}

static void put_le32(bitsink *s, uint32_t v)
{
    bs_byte(s, (unsigned char)(v & 0xffu));
    bs_byte(s, (unsigned char)((v >> 8) & 0xffu));
    bs_byte(s, (unsigned char)((v >> 16) & 0xffu));
    bs_byte(s, (unsigned char)((v >> 24) & 0xffu));
}

bool jm_gzip(const char *data, int64_t n, char **out, int64_t *out_n)
{
    if (data == nullptr || out == nullptr || out_n == nullptr || n < 0)
        return false;

    if (n > INT32_MAX)
        return false;

    int32_t *head = jm_alloc_array(HASH_SIZE, sizeof *head);
    int32_t *prev = jm_alloc_array(WINDOW, sizeof *prev);
    if (head == nullptr || prev == nullptr) {
        free(head);
        free(prev);
        return false;
    }

    for (int32_t i = 0; i < WINDOW; i++)
        prev[i] = -1;

    bitsink s = {.buf = nullptr, .len = 0, .cap = 0, .acc = 0, .nbits = 0,
                 .ok = true};

    bs_byte(&s, 0x1fu);
    bs_byte(&s, 0x8bu);
    bs_byte(&s, 8u);
    bs_byte(&s, 0u);
    put_le32(&s, 0u);
    bs_byte(&s, 0u);
    bs_byte(&s, 255u);

    if (n > 0) {
        deflate_fixed(&s, (const unsigned char *)data, n, head, prev);
    } else {

        bs_put(&s, 1, 1);
        bs_put(&s, 1, 2);
        uint32_t code; int clen;
        fixed_lit(256, &code, &clen);
        bs_code(&s, code, clen);
        bs_align(&s);
    }

    put_le32(&s, crc32_of((const unsigned char *)data, n));
    put_le32(&s, (uint32_t)((uint64_t)n & UINT32_C(0xffffffff)));

    free(head);
    free(prev);
    if (!s.ok) {
        free(s.buf);
        return false;
    }
    *out = (char *)s.buf;
    *out_n = s.len;
    return true;
}
