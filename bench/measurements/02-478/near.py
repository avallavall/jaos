# 02-478: the l1 distance from each node's rounded point to the optimum,
# in the columns, for both trees' dumps.
#
#   python3 near.py MODEL.mps LATDUMP OPTIMUM DIR REDUCED_DUMP COLUMNS_DUMP
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
import importlib.util
mps, lat, opt, sdir = sys.argv[1:5]
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
U, V, xinc = A.read_lat(lat)
xopt = A.read_pt(opt, cols)
for label, f, red in (('reduced', sys.argv[5], True), ('columns', sys.argv[6], False)):
    cnt = {}
    nn = 0
    ex = {}
    for line in open(f):
        t = line.split()
        p = np.array([float(v) for v in t[3:]])
        xs = V @ p if red else p
        xr = np.round(xs); xr[xr < 0] = 0
        d = int(np.abs(xopt - xr).sum())
        nn += 1
        if d <= 4:
            cnt[d] = cnt.get(d, 0) + 1
            ex.setdefault(d, []).append(int(t[0]))
    print(label, nn, 'nodes; optimum at x-distance d from round(x*):', sorted(cnt.items()), {k: v[:6] for k, v in ex.items()})
