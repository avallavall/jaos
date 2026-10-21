# SPDX-License-Identifier: Apache-2.0
# python3 rescale.py IN.mps OUT.mps K
import sys

# x_C1 = C1' / k and x_C2 = k * C2': a column's linear entries and bounds
# scale by its factor f (x = f x'), a quadratic entry (a, b) by f_a * f_b.
src, dst, k = sys.argv[1], sys.argv[2], float(sys.argv[3])
f = {"C1": 1.0 / k, "C2": k}
out, sec = [], None
for ln in open(src).read().split("\n"):
    if not ln.strip() or ln.startswith("*"):
        out.append(ln)
        continue
    if not ln.startswith(" "):
        sec = ln.split()[0]
        out.append(ln)
        continue
    p = ln.split()
    if sec == "COLUMNS" and p[0] in f:
        for i in range(2, len(p), 2):
            p[i] = repr(float(p[i]) * f[p[0]])
        ln = "    " + "  ".join(p)
    elif sec in ("QUADOBJ", "QCMATRIX") and (p[0] in f or p[1] in f):
        p[2] = repr(float(p[2]) * f.get(p[0], 1.0) * f.get(p[1], 1.0))
        ln = "    " + "  ".join(p)
    elif sec == "BOUNDS" and len(p) == 4 and p[2] in f:
        p[3] = repr(float(p[3]) / f[p[2]])
        ln = " " + "  ".join(p)
    out.append(ln)
open(dst, "w").write("\n".join(out))
