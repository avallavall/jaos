# Compares two bench/run records instance by instance: the status, the work
# units and the digest of the answer's bits. Prints each instance that
# differs, then a count.
#
#   python3 compare.py A.txt B.txt
#
# SPDX-License-Identifier: Apache-2.0
import re
import sys


def read(path):
    out = {}
    for ln in open(path):
        t = ln.split()
        if len(t) < 2 or '=' in t[0]:
            continue
        w = re.search(r' work=(\d+)', ln)
        d = re.search(r' digest=([0-9a-f]+)', ln)
        out[t[0]] = (t[1], w.group(1) if w else '-', d.group(1) if d else '-')
    return out


a, b = read(sys.argv[1]), read(sys.argv[2])
diff = 0
for k in sorted(a):
    if k not in b:
        print(k, 'missing')
        diff += 1
    elif a[k] != b[k]:
        print(k, 'status %s/%s work %s/%s digest %s/%s' % (a[k][0], b[k][0], a[k][1], b[k][1], a[k][2], b[k][2]))
        diff += 1
print('instances %d, differ %d' % (len(a), diff))
