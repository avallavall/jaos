#!/usr/bin/env python3
"""Builds the tests/data/*.gz fixtures for tests/test_inflate.c.

Run from the repository root. The files it writes are checked in, so this is
here to make them re-derivable and to say what each one is for, not because
the build runs it.

Every member is assembled by hand rather than by `gzip`, for two reasons.
The DEFLATE block type has to be chosen instead of guessed, so that the
three of them are covered on purpose. And the bytes have to be the same on
every machine, so MTIME is zero and the OS byte is 255.

The evidence that this decoder agrees with the real one is not here. It is
the population run in bench/measurements/02-152/, which compares against
`gzip` itself over every instance in the tree.
"""
import os
import sys
import zlib

DATA = "tests/data"


def raw_deflate(payload, level, strategy):
    co = zlib.compressobj(level, zlib.DEFLATED, -15, 9, strategy)
    return co.compress(payload) + co.flush()


def block_type(stream):
    """BTYPE of the first block: 0 stored, 1 fixed, 2 dynamic."""
    return (stream[0] >> 1) & 3


def member(payload, level=9, strategy=zlib.Z_DEFAULT_STRATEGY,
           fname=None, fcomment=None, fextra=None, fhcrc=False, cm=8):
    flg = 0
    head_tail = b""
    if fextra is not None:
        flg |= 0x04
        head_tail += len(fextra).to_bytes(2, "little") + fextra
    if fname is not None:
        flg |= 0x08
        head_tail += fname.encode() + b"\x00"
    if fcomment is not None:
        flg |= 0x10
        head_tail += fcomment.encode() + b"\x00"
    if fhcrc:
        flg |= 0x02
    head = bytes([0x1F, 0x8B, cm, flg, 0, 0, 0, 0, 0, 255]) + head_tail
    if fhcrc:
        head += (zlib.crc32(head) & 0xFFFF).to_bytes(2, "little")
    body = raw_deflate(payload, level, strategy)
    tail = (zlib.crc32(payload) & 0xFFFFFFFF).to_bytes(4, "little") + \
           (len(payload) & 0xFFFFFFFF).to_bytes(4, "little")
    return head + body + tail, block_type(body)


class BW:
    """DEFLATE bit writer: plain fields go least-significant bit first,
    Huffman codes most-significant bit first (RFC 1951, section 3.1.1)."""

    def __init__(self):
        self.b = bytearray()
        self.acc = 0
        self.n = 0

    def bit(self, x):
        self.acc |= (x & 1) << self.n
        self.n += 1
        if self.n == 8:
            self.b.append(self.acc)
            self.acc = 0
            self.n = 0

    def bits(self, v, n):
        for i in range(n):
            self.bit((v >> i) & 1)

    def code(self, c, n):
        for i in range(n - 1, -1, -1):
            self.bit((c >> i) & 1)

    def flush(self):
        if self.n:
            self.b.append(self.acc)
            self.acc = 0
            self.n = 0
        return bytes(self.b)


def no_distance_block():
    """A dynamic block declaring one distance code of length zero, which is
    legal for a block with no back-reference and which zlib never writes.
    Payload is 'AAA': symbol 65 and the end-of-block code, one bit each."""
    w = BW()
    w.bit(1)            # BFINAL
    w.bits(2, 2)        # BTYPE = dynamic
    w.bits(0, 5)        # HLIT  -> 257 literal/length codes
    w.bits(0, 5)        # HDIST -> 1 distance code
    w.bits(14, 4)       # HCLEN -> 18 code-length codes sent
    # Lengths for the code-length alphabet, in the order RFC 1951 fixes.
    # Only symbols 18 (a long run of zeros), 0 and 1 are used.
    for v in [0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2]:
        w.bits(v, 3)
    C18, C0, C1 = (0, 1), (2, 2), (3, 2)   # canonical over {18:1, 0:2, 1:2}
    w.code(*C18)
    w.bits(65 - 11, 7)      # 65 zeros: symbols 0..64 are unused
    w.code(*C1)             # symbol 65 has length 1
    w.code(*C18)
    w.bits(138 - 11, 7)     # 138 zeros
    w.code(*C18)
    w.bits(52 - 11, 7)      # 52 more, so symbols 66..255 are unused
    w.code(*C1)             # symbol 256, end of block, has length 1
    w.code(*C0)             # the single distance code, length 0
    w.code(0, 1)            # 'A'
    w.code(0, 1)            # 'A'
    w.code(0, 1)            # 'A'
    w.code(1, 1)            # end of block
    return w.flush(), b"AAA"


def raw_member(body, payload):
    head = bytes([0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 255])
    tail = (zlib.crc32(payload) & 0xFFFFFFFF).to_bytes(4, "little") + \
           (len(payload) & 0xFFFFFFFF).to_bytes(4, "little")
    return head + body + tail


def put(name, data):
    with open(os.path.join(DATA, name), "wb") as f:
        f.write(data)
    print(f"  {name}: {len(data)} bytes")


def main():
    t1 = open(os.path.join(DATA, "t1.mps"), "rb").read()
    g1 = open(os.path.join(DATA, "g1.lp"), "rb").read()
    solve1 = open(os.path.join(DATA, "solve1.mps"), "rb").read()

    types = {}

    # The three block types, all decoding to the same model as t1.mps.
    m, types["t1.mps.gz"] = member(t1, level=9, fname="t1.mps")
    put("t1.mps.gz", m)
    m, types["t1_stored.mps.gz"] = member(t1, level=0)
    put("t1_stored.mps.gz", m)
    m, types["t1_fixed.mps.gz"] = member(t1, level=9, strategy=zlib.Z_FIXED,
                                         fextra=b"XX\x02\x00ab",
                                         fname="t1.mps",
                                         fcomment="a comment", fhcrc=True)
    put("t1_fixed.mps.gz", m)

    # Two members end to end, which is a legal gzip file.
    half = len(t1) // 2
    a, _ = member(t1[:half], level=9)
    b, _ = member(t1[half:], level=9)
    put("t1_two.mps.gz", a + b)

    # The LP reader, and a longer instance for back-references further back.
    m, types["g1.lp.gz"] = member(g1, level=9, fname="g1.lp")
    put("g1.lp.gz", m)
    m, types["solve1.mps.gz"] = member(solve1, level=9, fname="solve1.mps")
    put("solve1.mps.gz", m)

    # Zero padding after the member, which gzip itself ignores.
    m, _ = member(t1, level=9)
    put("t1_padded.mps.gz", m + b"\x00" * 32)

    # Two shapes no format reader can express, decoded through jm_slurp.
    body, payload = no_distance_block()
    put("gz_nodist.gz", raw_member(body, payload))
    put("gz_empty.gz", raw_member(raw_deflate(b"", 9,
                                              zlib.Z_DEFAULT_STRATEGY), b""))

    # The rejections, one file per class.
    bad, _ = member(t1, level=9, cm=7)
    put("eg_method.mps.gz", bad)

    good, _ = member(t1, level=9)

    corrupt = bytearray(good)
    corrupt[len(good) - 8] ^= 0xFF
    put("eg_badcrc.mps.gz", bytes(corrupt))

    put("eg_trunc.mps.gz", good[:-6])
    put("eg_trailing.mps.gz", good + b"junk after the member")

    reserved = bytearray(good)
    reserved[3] |= 0x20
    put("eg_reserved.mps.gz", bytes(reserved))

    # The header's own checksum, damaged. On this fixture FHCRC sits after
    # 10 header bytes, the FEXTRA block, and the two NUL-terminated names.
    withcrc, _ = member(t1, level=9, strategy=zlib.Z_FIXED,
                        fextra=b"XX\x02\x00ab", fname="t1.mps",
                        fcomment="a comment", fhcrc=True)
    at = 10 + 2 + len(b"XX\x02\x00ab") + len("t1.mps") + 1 + \
        len("a comment") + 1
    bad_head = bytearray(withcrc)
    bad_head[at] ^= 0xFF
    put("eg_badheadcrc.mps.gz", bytes(bad_head))

    payload = bytearray(good)
    payload[len(good) // 2] ^= 0x5A
    put("eg_corrupt.mps.gz", bytes(payload))

    print("block types:", types)
    if (types["t1.mps.gz"], types["t1_fixed.mps.gz"],
            types["t1_stored.mps.gz"]) != (2, 1, 0):
        print("FAIL: the three block types are not all covered",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
