# Compares two Maros-Meszaros records of bench/run: every instance whose
# status or checker verdict changed, every work ratio outside 0.8 to 1.2,
# and the geometric mean of the work ratios where the status is the same.
#
#   python3 mmcmp.py BASE.txt NEW.txt
#
# SPDX-License-Identifier: Apache-2.0
import math
import sys


def read(path):
    rows = {}
    for line in open(path):
        f = line.split()
        if len(f) < 3 or line.startswith('#'):
            continue
        work = [x for x in f if x.startswith('work=')]
        verdict = ' '.join(x for x in f if x.startswith(('checker=', 'objective=')))
        rows[f[0]] = (f[1], int(work[0][5:]) if work else None, verdict)
    return rows


base, new = read(sys.argv[1]), read(sys.argv[2])
logs, cheaper, dearer = [], 0, 0
for name in base:
    b, n = base[name], new[name]
    if b[0] != n[0] or b[2] != n[2]:
        print('status', name, b, n)
    if b[1] and n[1] and b[0] == n[0]:
        r = n[1] / b[1]
        logs.append(math.log(r))
        cheaper += r < 1
        dearer += r > 1
        if r > 1.2 or r < 0.8:
            print(name, b[1], n[1], round(r, 3))
print('work, same status: %d instances, geometric mean %.4fx, %d cheaper, %d dearer'
      % (len(logs), math.exp(sum(logs) / len(logs)), cheaper, dearer))
