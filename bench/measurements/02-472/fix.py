"""Writes a copy of an MPS model with every integer column fixed, for 02-472.

The columns named in POINT (one line, the names after `point`) are fixed at
1 and every other integer column at 0, so that solving the copy is the LP
over the continuous columns with the integers held.

    python3 -I fix.py MPS POINTFILE OUT

SPDX-License-Identifier: Apache-2.0
"""
import sys


def main():
    src, ptf, out = sys.argv[1:4]
    on = set()
    for line in open(ptf):
        if line.startswith('point '):
            on = set(line.split()[1:])
    ints = []
    seen = set()
    mark = False
    sec = None
    lines = open(src).read().split('\n')
    body = []
    for line in lines:
        if line and not line[0].isspace():
            sec = line.split()[0]
            if sec == 'ENDATA':
                continue
            body.append(line)
            continue
        t = line.split()
        if sec == 'COLUMNS' and len(t) >= 3 and t[1] == "'MARKER'":
            mark = t[2] == "'INTORG'"
            body.append(line)
            continue
        if sec == 'COLUMNS' and t and mark and t[0] not in seen:
            seen.add(t[0])
            ints.append(t[0])
        if sec == 'BOUNDS' and len(t) >= 3 and t[2] in seen:
            continue
        body.append(line)
    if not any(l.startswith('BOUNDS') for l in body):
        body.append('BOUNDS')
    for c in ints:
        body.append(' FX bnd       %-8s %d' % (c, 1 if c in on else 0))
    body.append('ENDATA')
    open(out, 'w').write('\n'.join(l for l in body if l.strip()) + '\n')


if __name__ == '__main__':
    main()
