"""Which instances of a MIP set have the shape of 02-470's moves.

A binary is a switch when it is the one free binary of a row that has two
or more other free columns of both signs, and its coefficient is at least
1e3 times the median of theirs or one of its values leaves the row slack
over the bounds of the others. The moves run where every free binary is a
switch and no switch sits in two partitioning rows. For each instance of
each manifest this prints the free binaries, the switches, the rows that
make them, the switches in one partitioning row, those in two or more, and
the free general integer columns; ALL marks the shape. It reads the MPS
files, not JAOS's presolved models.

    python3 -I shape.py CENSUS MANIFEST DIR [MANIFEST DIR ...]

CENSUS is 02-463's census.py, whose MPS reader this uses.

SPDX-License-Identifier: Apache-2.0
"""
import os
import sys

CENSUS = sys.argv[1]
src = open(CENSUS).read().replace('\nmain()\n', '\n')
g = {'__name__': 'census'}
exec(compile(src, CENSUS, 'exec'), g)
read_mps = g['read_mps']
RATIO = 1e3


def detect(path):
    rows, ent, bnd, ci, cols, obj = read_mps(path)

    def isb(c):
        return ci[c][0] and ci[c][1] == 0 and ci[c][2] == 1

    def fixed(c):
        return ci[c][1] == ci[c][2]

    part = set()
    for r in rows:
        lw, hi = bnd[r]
        if lw == 1 and hi == 1 and ent[r] and \
                all(a == 1.0 and isb(c) for c, a in ent[r]):
            part.add(r)
    sw = set()
    drows = 0
    for r in rows:
        e = [(c, a) for c, a in ent[r] if not fixed(c)]
        bins = [(c, a) for c, a in e if isb(c)]
        oth = [(c, a) for c, a in e if not isb(c)]
        if len(bins) != 1 or len(oth) < 2:
            continue
        if not (any(a > 0 for c, a in oth) and any(a < 0 for c, a in oth)):
            continue
        b, ab = bins[0]
        med = sorted(abs(a) for c, a in oth)[len(oth) // 2]
        big = abs(ab) >= RATIO * med
        if not big:
            lw, hi = bnd[r]
            mn = sum(a * (ci[c][1] if a > 0 else ci[c][2]) for c, a in oth)
            mx = sum(a * (ci[c][2] if a > 0 else ci[c][1]) for c, a in oth)
            for v in (0.0, 1.0):
                if mn + ab * v >= lw - 1e-9 and mx + ab * v <= hi + 1e-9:
                    big = True
        if big:
            sw.add(b)
            drows += 1
    freeb = [c for c in ci if isb(c)]
    nsw = sum(1 for c in freeb if c in sw)
    ingrp = sum(1 for c in sw if sum(1 for r in cols[c] if r in part) == 1)
    multi = sum(1 for c in sw if sum(1 for r in cols[c] if r in part) > 1)
    gi = sum(1 for c in ci if ci[c][0] and not isb(c) and not fixed(c))
    return len(freeb), nsw, drows, ingrp, multi, gi


def main():
    for man, d in zip(sys.argv[2::2], sys.argv[3::2]):
        for line in open(man):
            if line.startswith('#') or not line.strip():
                continue
            name = line.split()[0]
            p = None
            for ext in ('.mps', '.mps.gz'):
                if os.path.exists(os.path.join(d, name + ext)):
                    p = os.path.join(d, name + ext)
            if p is None:
                print(f'{name:22s} missing')
                continue
            nb, nsw, drows, ingrp, multi, gi = detect(p)
            flag = 'ALL' if nb > 0 and nsw == nb and multi == 0 else ''
            print(f'{name:22s} bin={nb:5d} switches={nsw:5d} rows={drows:5d} '
                  f'in_one_part={ingrp:5d} in_two_or_more={multi:4d} '
                  f'gint={gi:4d} {flag}')


main()
