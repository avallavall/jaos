/* Reading a compressed instance: gzip (RFC 1952) over DEFLATE (RFC 1951).
 *
 * Written here because JAOS links nothing but libc and libm, and a
 * dependency on zlib would buy one file format at the cost of the rule
 * (SPECS.md, "premises"). The decoder is integer arithmetic end to end, so
 * it keeps D8's bit-identical guarantee without needing anything from it.
 *
 * jm_slurp returns the bytes of a file: inflated when the file starts with
 * the gzip magic, verbatim when it does not. Both format readers go through
 * it, so `.gz` costs each of them one call.
 *
 * Canonical Huffman decoding follows RFC 1951 section 3.2.2 directly. The
 * count of codes per length plus the symbols in canonical order is enough to
 * walk a code bit by bit, and builds no lookup table.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _POSIX_C_SOURCE 200809L

#include "jaos_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* Bits, least-significant first, as DEFLATE packs them                    */
/* --------------------------------------------------------------------- */

typedef struct {
    const unsigned char *in;
    int64_t len;
    int64_t pos;    /* next byte not yet in buf */
    uint32_t buf;   /* bits held, the next one in the low bit */
    int cnt;        /* how many of them are valid */
    bool over;      /* a read ran past the end of the input */
} bits;

/* n <= 16 at every call site, so cnt never exceeds 23 and the shift below
 * stays inside uint32_t. */
static uint32_t bits_get(bits *b, int n)
{
    if (n == 0)
        return 0;
    while (b->cnt < n) {
        if (b->pos >= b->len) {
            b->over = true;
            return 0;
        }
        b->buf |= (uint32_t)b->in[b->pos++] << b->cnt;
        b->cnt += 8;
    }
    uint32_t v = b->buf & ((UINT32_C(1) << n) - 1u);
    b->buf >>= n;
    b->cnt -= n;
    return v;
}

/* Drops the partial byte. A stored block starts on a byte boundary. */
static void bits_align(bits *b)
{
    int drop = b->cnt % 8;
    b->buf >>= drop;
    b->cnt -= drop;
}

/* Where the whole bytes resume, which is where the gzip trailer sits. */
static int64_t bits_byte_pos(const bits *b)
{
    return b->pos - b->cnt / 8;
}

/* --------------------------------------------------------------------- */
/* Canonical Huffman                                                       */
/* --------------------------------------------------------------------- */

#define HUFF_MAXSYM 288

typedef struct {
    int16_t count[16];             /* codes of each length, index 0 unused */
    int16_t symbol[HUFF_MAXSYM];   /* symbols, canonical order */
} huff;

/* Builds from one length per symbol. Rejects an over-subscribed set, which
 * is what a bit flipped inside a dynamic header produces. An incomplete set
 * is accepted: RFC 1951 allows one for the distance tree of a block that
 * emits no match, and a code it fails to cover fails the decode below rather
 * than decoding to something. */
static bool huff_build(huff *h, const unsigned char *lengths, int n)
{
    for (int i = 0; i < 16; i++)
        h->count[i] = 0;
    for (int i = 0; i < n; i++)
        h->count[lengths[i]]++;
    if (h->count[0] == n)
        return false;              /* no code at all */

    int left = 1;
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0)
            return false;          /* more codes than the length can carry */
    }

    int16_t offs[16];
    offs[0] = 0;
    offs[1] = 0;
    for (int len = 1; len < 15; len++)
        offs[len + 1] = (int16_t)(offs[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (lengths[i] != 0)
            h->symbol[offs[lengths[i]]++] = (int16_t)i;
    return true;
}

/* Returns the symbol, or -1 on a code no length covers and on running out of
 * input. */
static int huff_decode(bits *b, const huff *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        code |= (int)bits_get(b, 1);
        if (b->over)
            return -1;
        int count = h->count[len];
        if (code - first < count)
            return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

/* --------------------------------------------------------------------- */
/* The output, grown as it goes                                            */
/* --------------------------------------------------------------------- */

typedef struct {
    char *buf;
    int64_t len, cap;
} sink;

static bool sink_put(sink *s, unsigned char c)
{
    if (!JM_GROW(s->buf, s->cap, s->len + 1))
        return false;
    s->buf[s->len++] = (char)c;
    return true;
}

/* --------------------------------------------------------------------- */
/* DEFLATE                                                                 */
/* --------------------------------------------------------------------- */

/* RFC 1951 section 3.2.5: what each length and distance code stands for. */
static const int16_t len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
    59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const unsigned char len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
    5, 5, 5, 5, 0
};
static const int32_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const unsigned char dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
    10, 11, 11, 12, 12, 13, 13
};

/* The order the code-length code lengths arrive in, RFC 1951 section 3.2.7. */
static const unsigned char clen_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

#define BAD(...) do { jm_set_err(m, __VA_ARGS__); \
                      return JAOS_ERR_INVALID_INPUT; } while (0)

static jaos_status inflate_codes(jaos_model *m, bits *b, sink *s,
                                 const huff *lit, const huff *dist)
{
    for (;;) {
        int sym = huff_decode(b, lit);
        if (sym < 0)
            BAD("compressed input: bad literal/length code");
        if (sym < 256) {
            if (!sink_put(s, (unsigned char)sym))
                return JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }
        if (sym == 256)
            return JAOS_OK;        /* end of block */

        sym -= 257;
        if (sym >= 29)
            BAD("compressed input: length code %d is reserved", sym + 257);
        int64_t length = len_base[sym] + (int64_t)bits_get(b, len_extra[sym]);

        int dsym = huff_decode(b, dist);
        if (dsym < 0 || dsym >= 30)
            BAD("compressed input: bad distance code");
        int64_t back = dist_base[dsym] + (int64_t)bits_get(b, dist_extra[dsym]);
        if (b->over)
            BAD("compressed input: stream ends inside a block");
        if (back > s->len)
            BAD("compressed input: back-reference reaches before the start");

        /* One byte at a time on purpose: DEFLATE lets the copy overlap
         * itself, which is how a run of one repeated byte is encoded. */
        for (int64_t k = 0; k < length; k++)
            if (!sink_put(s, (unsigned char)s->buf[s->len - back]))
                return JAOS_ERR_OUT_OF_MEMORY;
    }
}

static jaos_status inflate_stored(jaos_model *m, bits *b, sink *s)
{
    bits_align(b);
    uint32_t nbytes = bits_get(b, 16);
    uint32_t nlen = bits_get(b, 16);
    if (b->over)
        BAD("compressed input: stream ends inside a stored block header");
    if ((nbytes ^ 0xffffu) != nlen)
        BAD("compressed input: stored block length is not its own complement");
    for (uint32_t k = 0; k < nbytes; k++) {
        uint32_t v = bits_get(b, 8);
        if (b->over)
            BAD("compressed input: stream ends inside a stored block");
        if (!sink_put(s, (unsigned char)v))
            return JAOS_ERR_OUT_OF_MEMORY;
    }
    return JAOS_OK;
}

/* The fixed tables of RFC 1951 section 3.2.6, built from their lengths so
 * one decoder serves both block types. */
static void fixed_trees(huff *lit, huff *dist)
{
    unsigned char ll[288], dl[30];
    for (int i = 0; i < 144; i++)
        ll[i] = 8;
    for (int i = 144; i < 256; i++)
        ll[i] = 9;
    for (int i = 256; i < 280; i++)
        ll[i] = 7;
    for (int i = 280; i < 288; i++)
        ll[i] = 8;
    for (int i = 0; i < 30; i++)
        dl[i] = 5;
    (void)huff_build(lit, ll, 288);
    (void)huff_build(dist, dl, 30);
}

static jaos_status inflate_dynamic(jaos_model *m, bits *b, sink *s)
{
    int nlit = (int)bits_get(b, 5) + 257;
    int ndist = (int)bits_get(b, 5) + 1;
    int nclen = (int)bits_get(b, 4) + 4;
    if (b->over)
        BAD("compressed input: stream ends inside a block header");
    if (nlit > 286 || ndist > 30)
        BAD("compressed input: block header claims too many codes");

    unsigned char clen[19] = {0};
    for (int i = 0; i < nclen; i++)
        clen[clen_order[i]] = (unsigned char)bits_get(b, 3);
    if (b->over)
        BAD("compressed input: stream ends inside a block header");

    huff clh;
    if (!huff_build(&clh, clen, 19))
        BAD("compressed input: code-length code set is not a Huffman code");

    unsigned char lengths[288 + 30] = {0};
    int i = 0;
    while (i < nlit + ndist) {
        int sym = huff_decode(b, &clh);
        if (sym < 0)
            BAD("compressed input: bad code-length code");
        if (sym < 16) {
            lengths[i++] = (unsigned char)sym;
            continue;
        }
        int repeat;
        unsigned char with;
        if (sym == 16) {
            if (i == 0)
                BAD("compressed input: a repeat with nothing to repeat");
            with = lengths[i - 1];
            repeat = 3 + (int)bits_get(b, 2);
        } else if (sym == 17) {
            with = 0;
            repeat = 3 + (int)bits_get(b, 3);
        } else {
            with = 0;
            repeat = 11 + (int)bits_get(b, 7);
        }
        if (b->over)
            BAD("compressed input: stream ends inside a block header");
        if (i + repeat > nlit + ndist)
            BAD("compressed input: code lengths overrun their own count");
        while (repeat-- > 0)
            lengths[i++] = with;
    }
    if (lengths[256] == 0)
        BAD("compressed input: block has no end-of-block code");

    huff lit, dist;
    if (!huff_build(&lit, lengths, nlit))
        BAD("compressed input: literal/length code set is not a Huffman code");

    /* A block that emits no match needs no distance code, and an encoder is
     * allowed to declare none. zlib always declares one, so this branch is
     * invisible to a corpus compressed by gzip; refusing it would refuse a
     * legal file from some other encoder. Every count stays zero, so a
     * distance code that does turn up decodes to nothing and the block is
     * refused there. */
    bool any_dist = false;
    for (int k = 0; k < ndist; k++)
        if (lengths[nlit + k] != 0)
            any_dist = true;
    if (any_dist) {
        if (!huff_build(&dist, lengths + nlit, ndist))
            BAD("compressed input: distance code set is not a Huffman code");
    } else {
        memset(&dist, 0, sizeof dist);
    }
    return inflate_codes(m, b, s, &lit, &dist);
}

static jaos_status inflate_raw(jaos_model *m, bits *b, sink *s)
{
    for (;;) {
        int final = (int)bits_get(b, 1);
        int type = (int)bits_get(b, 2);
        if (b->over)
            BAD("compressed input: stream ends where a block should start");

        jaos_status st;
        switch (type) {
        case 0:
            st = inflate_stored(m, b, s);
            break;
        case 1: {
            huff lit, dist;
            fixed_trees(&lit, &dist);
            st = inflate_codes(m, b, s, &lit, &dist);
            break;
        }
        case 2:
            st = inflate_dynamic(m, b, s);
            break;
        default:
            BAD("compressed input: block type 3 is reserved");
        }
        if (st != JAOS_OK)
            return st;
        if (final)
            return JAOS_OK;
    }
}

/* --------------------------------------------------------------------- */
/* gzip container                                                          */
/* --------------------------------------------------------------------- */

/* CRC-32 as RFC 1952 defines it. The table is built per call: 2048 integer
 * operations against a whole instance file, and no mutable file-scope state
 * to reason about. */
static uint32_t crc32_of(const char *p, int64_t n)
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
        c = tab[(c ^ (unsigned char)p[i]) & 0xffu] ^ (c >> 8);
    return c ^ UINT32_C(0xffffffff);
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define GZ_FHCRC    0x02u
#define GZ_FEXTRA   0x04u
#define GZ_FNAME    0x08u
#define GZ_FCOMMENT 0x10u
#define GZ_RESERVED 0xe0u

/* Inflates every member of a gzip file into one buffer. Concatenated members
 * are a valid gzip file and some writers produce them, so stopping at the
 * first one would read an instance short and say nothing. */
static jaos_status gunzip(jaos_model *m, const unsigned char *in, int64_t n,
                          sink *s)
{
    int64_t at = 0;
    for (;;) {
        if (n - at < 18)
            BAD("compressed input: gzip member is too short to be one");
        if (in[at] != 0x1fu || in[at + 1] != 0x8bu)
            BAD("compressed input: gzip magic missing at offset %" PRId64, at);
        if (in[at + 2] != 8u)
            BAD("compressed input: compression method %u is not deflate",
                (unsigned)in[at + 2]);
        unsigned flg = in[at + 3];
        if (flg & GZ_RESERVED)
            BAD("compressed input: reserved gzip header flags are set");
        int64_t p = at + 10;

        if (flg & GZ_FEXTRA) {
            if (p + 2 > n)
                BAD("compressed input: gzip header ends inside FEXTRA");
            int64_t xlen = (int64_t)in[p] | ((int64_t)in[p + 1] << 8);
            p += 2 + xlen;
            if (p > n)
                BAD("compressed input: gzip FEXTRA runs past the file");
        }
        if (flg & GZ_FNAME) {
            while (p < n && in[p] != 0u)
                p++;
            if (p >= n)
                BAD("compressed input: gzip header ends inside FNAME");
            p++;
        }
        if (flg & GZ_FCOMMENT) {
            while (p < n && in[p] != 0u)
                p++;
            if (p >= n)
                BAD("compressed input: gzip header ends inside FCOMMENT");
            p++;
        }
        if (flg & GZ_FHCRC) {
            p += 2;
            if (p > n)
                BAD("compressed input: gzip header ends inside FHCRC");
            /* The header carries its own checksum when the flag is set, and
             * the trailer's covers only the data. Without this, a header
             * damaged in its length fields would be followed rather than
             * refused. */
            uint32_t head_crc = crc32_of((const char *)(in + at),
                                         p - 2 - at) & 0xffffu;
            uint32_t want = (uint32_t)in[p - 2] |
                            ((uint32_t)in[p - 1] << 8);
            if (head_crc != want)
                BAD("compressed input: gzip header checksum does not match "
                    "the header");
        }

        int64_t member_start = s->len;
        bits b = {.in = in, .len = n, .pos = p};
        jaos_status st = inflate_raw(m, &b, s);
        if (st != JAOS_OK)
            return st;

        int64_t end = bits_byte_pos(&b);
        if (end + 8 > n)
            BAD("compressed input: gzip member has no trailer");
        uint32_t want_crc = le32(in + end);
        uint32_t want_size = le32(in + end + 4);
        int64_t got_size = s->len - member_start;
        /* A member may be empty, and then s->buf is still NULL: adding an
         * offset to it would be undefined even though the offset is zero. */
        const char *body = s->buf != nullptr ? s->buf + member_start : "";
        if (crc32_of(body, got_size) != want_crc)
            BAD("compressed input: gzip checksum does not match the data");
        if ((uint32_t)(got_size & 0xffffffff) != want_size)
            BAD("compressed input: gzip length does not match the data");
        at = end + 8;

        if (at >= n)
            return JAOS_OK;
        if (n - at >= 2 && in[at] == 0x1fu && in[at + 1] == 0x8bu)
            continue;              /* a second member */
        /* Some writers pad with zeros and gzip itself ignores that. Anything
         * else after the last member means the file is not what it claims. */
        for (int64_t k = at; k < n; k++)
            if (in[k] != 0u)
                BAD("compressed input: %" PRId64 " unexpected bytes after the "
                    "last gzip member", n - at);
        return JAOS_OK;
    }
}

#undef BAD

/* --------------------------------------------------------------------- */
/* What the format readers call                                            */
/* --------------------------------------------------------------------- */

jaos_status jm_slurp(jaos_model *m, const char *path,
                     char **out, int64_t *out_len)
{
    *out = nullptr;
    *out_len = 0;

    FILE *f = fopen(path, "rb");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s'", path);
        return JAOS_ERR_IO;
    }

    sink raw = {0};
    jaos_status st = JAOS_OK;
    char chunk[65536];
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) {
        if (!JM_GROW(raw.buf, raw.cap, raw.len + (int64_t)got + 1)) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            jm_set_err(m, "out of memory reading '%s'", path);
            goto done;
        }
        memcpy(raw.buf + raw.len, chunk, got);
        raw.len += (int64_t)got;
    }
    if (ferror(f)) {
        st = JAOS_ERR_IO;
        jm_set_err(m, "read error on '%s'", path);
        goto done;
    }
    if (!JM_GROW(raw.buf, raw.cap, raw.len + 1)) {
        st = JAOS_ERR_OUT_OF_MEMORY;
        jm_set_err(m, "out of memory reading '%s'", path);
        goto done;
    }
    raw.buf[raw.len] = '\0';

    if (raw.len >= 2 && (unsigned char)raw.buf[0] == 0x1fu &&
        (unsigned char)raw.buf[1] == 0x8bu) {
        sink dec = {0};
        st = gunzip(m, (const unsigned char *)raw.buf, raw.len, &dec);
        if (st == JAOS_OK && !JM_GROW(dec.buf, dec.cap, dec.len + 1)) {
            st = JAOS_ERR_OUT_OF_MEMORY;
            jm_set_err(m, "out of memory inflating '%s'", path);
        }
        if (st != JAOS_OK) {
            free(dec.buf);
            goto done;
        }
        dec.buf[dec.len] = '\0';
        free(raw.buf);
        raw = dec;
    }

    *out = raw.buf;
    *out_len = raw.len;
    raw.buf = nullptr;

done:
    free(raw.buf);
    fclose(f);
    return st;
}
