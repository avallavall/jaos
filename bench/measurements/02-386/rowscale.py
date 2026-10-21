# SPDX-License-Identifier: Apache-2.0
# python3 rowscale.py IN.mps OUT.mps ROW S
import sys

# Multiply row ROW (its linear entries, its quadratic block and its sides)
# by S.
src, dst, row, s = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
out, sec, qrow = [], None, None
for ln in open(src).read().split("\n"):
    if not ln.strip() or ln.startswith("*"):
        out.append(ln)
        continue
    if not ln.startswith(" "):
        p = ln.split()
        sec = p[0]
        qrow = p[1] if sec == "QCMATRIX" and len(p) > 1 else None
        out.append(ln)
        continue
    p = ln.split()
    if sec in ("COLUMNS", "RHS", "RANGES"):
        for i in range(1, len(p) - 1, 2):
            if p[i] == row:
                p[i + 1] = repr(float(p[i + 1]) * s)
        ln = "    " + "  ".join(p)
    elif sec == "QCMATRIX" and qrow == row:
        p[2] = repr(float(p[2]) * s)
        ln = "    " + "  ".join(p)
    out.append(ln)
open(dst, "w").write("\n".join(out))
