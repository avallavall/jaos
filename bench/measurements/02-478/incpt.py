# 02-478: the incumbent of a LATDUMP as a point file of gen-ip002.
#
#   python3 incpt.py LATDUMP OUT.pt
#
# SPDX-License-Identifier: Apache-2.0
import sys
f = open(sys.argv[1]).read().split('\n')
n = int(f[0])
x = [float(v) for v in f[1 + 2 * n].split()]
with open(sys.argv[2], 'w') as o:
    for j in range(n):
        o.write('C%07d %d\n' % (j, int(round(x[j]))))
