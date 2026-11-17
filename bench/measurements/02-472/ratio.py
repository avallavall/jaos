"""Total and per-instance work of a MIPLIB 3 run against an earlier record,
for 02-472. Prints each instance whose work moved by more than 1% and the
ratio of the totals.

    python3 -I ratio.py BEFORE RUN

SPDX-License-Identifier: Apache-2.0
"""
import re
import sys


def load(path):
    d = {}
    for line in open(path):
        if line.startswith('#'):
            continue
        m = re.match(r'^(\S+)\s', line)
        w = re.search(r'\bwork=(\d+)', line)
        n = re.search(r'\bnodes=(\d+)', line)
        if m and w:
            d[m.group(1)] = (int(w.group(1)), int(n.group(1)) if n else 0)
    return d


def main():
    base, run = load(sys.argv[1]), load(sys.argv[2])
    tb = tr = 0
    for k in sorted(run):
        if k not in base:
            continue
        (wb, nb), (wr, nr) = base[k], run[k]
        tb += wb
        tr += wr
        if abs(wr - wb) > 0.01 * wb:
            print('%-12s work %.2fx nodes %d to %d' % (k, wr / wb, nb, nr))
    print('total %.4fx over %d instances' % (tr / tb, len(run)))


if __name__ == '__main__':
    main()
