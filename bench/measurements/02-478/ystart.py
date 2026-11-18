# 02-478: the incumbent of a LATDUMP in the reduced coordinates, as a
# point file of the model JAOS_LATMPS writes.
#
#   python3 ystart.py LATDUMP OUT.pt
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
f = open(sys.argv[1]).read().split('\n')
n = int(f[0])
U = np.array([[int(x) for x in f[1 + i].split()] for i in range(n)])
xinc = np.array([float(x) for x in f[1 + 2 * n].split()])
y = U.T @ np.round(xinc)
with open(sys.argv[2], 'w') as o:
    for k in range(n):
        o.write('C%d %d\n' % (k + 1, int(round(y[k]))))
