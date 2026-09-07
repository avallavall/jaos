/* Writing a compressed file: gzip (RFC 1952) over DEFLATE (RFC 1951).
 *
 * The other half of src/inflate.c, and here for the same reason that one is
 * (D240): JAOS links nothing but libc and libm, so a compressor it needs is
 * a compressor it writes. Both readers have taken gzip since D240 and no
 * writer could produce it, which made the round trip this library promises
 * one-way for compressed files.
 *
 * **What this emits, and what it does not.** One DEFLATE block per chunk of
 * input, coded with the fixed Huffman tables of RFC 1951 section 3.2.6 over
 * a greedy LZ77 match search. There are no dynamic tables here. A dynamic
 * block would be smaller by roughly a fifth on an MPS file and costs a
 * second pass, a code-length tree and its own encoder, and the point of
 * this file is that a `.gz` JAOS writes is a `.gz` anything reads -- not
 * that it is the smallest one. `gzip -d`, `zlib` and src/inflate.c all
 * read what comes out, and `bench/measurements/02-217/` is the comparison
 * against the real gzip.
 *
 * **Reproducible, like everything else.** The match search is a bounded
 * walk down a hash chain in index order and nothing in it depends on an
 * address, a clock or a floating-point rounding, so the same bytes in give
 * the same bytes out on every machine and every run (D8).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* The bit sink, least-significant first, as DEFLATE packs it             */
/* --------------------------------------------------------------------- */

typedef struct {
    unsigned char *buf;
    int64_t len, cap;
    uint32_t acc;     /* bits not yet whole                              */
    int nbits;
    bool ok;          /* false once an allocation failed; every call after
                         that is a no-op, so the error is checked once at
                         the end rather than at every put                */
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

/* n bits of `v`, least significant first. n <= 16 at every call site, so
 * the accumulator never holds more than 23 bits before it drains. */
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

/* A Huffman code is packed starting from its MOST significant bit, and
 * everything else in DEFLATE from its least. Reversing here is what keeps
 * the one bit writer honest about both. */
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

/* --------------------------------------------------------------------- */
/* The fixed tables of RFC 1951                                            */
/* --------------------------------------------------------------------- */

/* Section 3.2.6: literal/length symbol -> (code, length). Written as the
 * ranges the RFC states rather than as a 288-entry table, because the
 * ranges are the specification and a table is a copy of it. */
static void fixed_lit(int sym, uint32_t *code, int *len)
{
    if (sym < 144)        { *code = (uint32_t)(0x30 + sym);        *len = 8; }
    else if (sym < 256)   { *code = (uint32_t)(0x190 + sym - 144); *len = 9; }
    else if (sym < 280)   { *code = (uint32_t)(sym - 256);         *len = 7; }
    else                  { *code = (uint32_t)(0xc0 + sym - 280);  *len = 8; }
}

/* Section 3.2.5, the same two tables src/inflate.c reads them from. */
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

/* --------------------------------------------------------------------- */
/* LZ77                                                                    */
/* --------------------------------------------------------------------- */

constexpr int32_t WINDOW = 32768;      /* the format's own maximum        */
constexpr int32_t MIN_MATCH = 3;
constexpr int32_t MAX_MATCH = 258;
constexpr int32_t HASH_BITS = 15;
constexpr int32_t HASH_SIZE = 1 << HASH_BITS;
/* How far down one hash chain the search walks before it takes what it
 * has. It trades size against time and it decides nothing else: any value
 * produces a valid file, and the same value produces the same file. 128 is
 * where the gain per step had flattened on the instance set
 * (bench/measurements/02-217/). */
constexpr int32_t CHAIN_MAX = 128;

static uint32_t hash3(const unsigned char *p)
{
    return (((uint32_t)p[0] << 10) ^ ((uint32_t)p[1] << 5) ^ (uint32_t)p[2])
           & (uint32_t)(HASH_SIZE - 1);
}

/* One deflate block over the whole input, fixed Huffman, greedy matching.
 * `head` and `prev` are the caller's so a failed allocation is handled in
 * one place. */
static void deflate_fixed(bitsink *s, const unsigned char *in, int64_t n,
                          int32_t *head, int32_t *prev)
{
    for (int32_t i = 0; i < HASH_SIZE; i++)
        head[i] = -1;

    bs_put(s, 1, 1);   /* BFINAL */
    bs_put(s, 1, 2);   /* BTYPE = 01, fixed Huffman */

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
            /* The length symbol: the last code whose base is not past the
             * length. The table is 29 long and ordered, so the walk is
             * bounded and needs no search structure. */
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
            bs_code(s, (uint32_t)dc, 5);   /* distances are 5-bit fixed */
            if (dist_extra[dc] > 0)
                bs_put(s, (uint32_t)(best_dist - dist_base[dc]),
                       dist_extra[dc]);
            take = best_len;
        } else {
            uint32_t code; int clen;
            fixed_lit(in[at], &code, &clen);
            bs_code(s, code, clen);
        }

        /* Every position the emitted run covers is inserted, matches
         * included: a chain that skipped them would still be correct and
         * would find fewer matches later. */
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
    fixed_lit(256, &code, &clen);        /* end of block */
    bs_code(s, code, clen);
    bs_align(s);
}

/* --------------------------------------------------------------------- */
/* CRC-32 and the gzip container                                           */
/* --------------------------------------------------------------------- */

/* RFC 1952's CRC-32, the same polynomial and the same per-call table
 * src/inflate.c builds. Duplicated rather than shared for the reason that
 * one gives: no mutable file-scope state, and 2048 integer operations
 * against a whole file is nothing. */
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
    /* Positions go into the hash chain as int32_t, and gzip's own length
     * field is 32 bits anyway. Two gigabytes is far past any model this
     * library writes, and refusing is better than wrapping. */
    if (n > INT32_MAX)
        return false;

    int32_t *head = jm_alloc_array(HASH_SIZE, sizeof *head);
    int32_t *prev = jm_alloc_array(WINDOW, sizeof *prev);
    if (head == nullptr || prev == nullptr) {
        free(head);
        free(prev);
        return false;
    }
    /* -1 is "no earlier position", and every slot is read before it is
     * written for the first time when the window wraps. */
    for (int32_t i = 0; i < WINDOW; i++)
        prev[i] = -1;

    bitsink s = {.buf = nullptr, .len = 0, .cap = 0, .acc = 0, .nbits = 0,
                 .ok = true};

    /* The ten-byte header of RFC 1952 section 2.3. No name, no comment,
     * no extra field, and the modification time is ZERO rather than the
     * clock: a file whose bytes depend on when it was written would break
     * the reproducibility every other output here keeps (D8). The OS byte
     * is 255, "unknown", for the same reason. */
    bs_byte(&s, 0x1fu);
    bs_byte(&s, 0x8bu);
    bs_byte(&s, 8u);          /* deflate                          */
    bs_byte(&s, 0u);          /* no optional fields               */
    put_le32(&s, 0u);         /* MTIME                            */
    bs_byte(&s, 0u);          /* XFL                              */
    bs_byte(&s, 255u);        /* OS unknown                       */

    if (n > 0) {
        deflate_fixed(&s, (const unsigned char *)data, n, head, prev);
    } else {
        /* An empty member still needs a block, and the smallest legal one
         * is a final fixed block holding nothing but end-of-block. */
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
