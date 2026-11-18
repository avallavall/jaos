# 02-478: COUNT sub-models of a model, each without 8 to 14 of its columns
# and 0 to 4 of its rows, drawn with seed 478.
#
#   python3 sub3.py MODEL.mps DIR OUTDIR TAG COUNT
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
import importlib.util
mps, sdir, outdir, tag = sys.argv[1:5]
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
def write(name, rows, colsel):
    with open('%s/%s.mps' % (outdir, name), 'w') as f:
        f.write('NAME %s\nROWS\n N OBJ\n' % name)
        for i in rows:
            f.write(' %s R%d\n' % (rt[i], i))
        f.write('COLUMNS\n    M1 \'MARKER\' \'INTORG\'\n')
        for j in colsel:
            f.write('    X%d OBJ %.10g\n' % (j, c[j]))
            for i in rows:
                if M[i, j] != 0:
                    f.write('    X%d R%d %.10g\n' % (j, i, M[i, j]))
        f.write('    M2 \'MARKER\' \'INTEND\'\nRHS\n')
        for i in rows:
            f.write('    RHS R%d %.10g\n' % (i, b[i]))
        f.write('ENDATA\n')
nr, nc = M.shape
rng = np.random.default_rng(478)
for t in range(int(sys.argv[5])):
    kc = int(rng.integers(8, 15))
    kr = int(rng.integers(0, 5))
    colsel = sorted(rng.choice(nc, nc - kc, replace=False).tolist())
    rows = sorted(rng.choice(nr, nr - kr, replace=False).tolist())
    write('%s_%02d' % (tag, t), rows, colsel)
