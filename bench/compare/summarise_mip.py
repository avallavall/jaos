# SPDX-License-Identifier: Apache-2.0
"""Summarise a comparison record written by run-mip.sh or run-qp.sh.

Per solver: how many instances it solved to the reference objective
(within 1e-6 of max(|ref|, 1)) inside the time limit, and the shifted
geometric mean of its seconds with a shift of 1 s, an instance it did not
solve counted at the limit. A solve ends optimal, or gaplimit, SCIP's name
for a stop at the relative gap all three solvers are given. Ratios are JAOS
over each rival.
"""
import math
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text().splitlines()
limit = 20.0
for line in text:
    m = re.search(r", ([0-9.]+) s each", line)
    if line.startswith("#") and m:
        limit = float(m.group(1))

rows = {}
for line in text:
    if line.startswith("#") or not line.strip():
        continue
    f = line.split("\t")
    if len(f) < 7:
        continue
    name, solver, status, obj, nodes, secs, ref = f[:7]
    try:
        o, r = float(obj), float(ref)
        ok = (status.lower() in ("optimal", "gaplimit") and
              abs(o - r) <= 1e-6 * max(abs(r), 1.0))
    except ValueError:
        ok = False
    try:
        t = float(secs)
    except ValueError:
        t = limit
    rows.setdefault(solver, {})[name] = (ok, min(t, limit) if ok else limit)

names = sorted({n for s in rows.values() for n in s})
sgm = {}
for solver, got in rows.items():
    solved = sum(1 for n in names if got.get(n, (False, limit))[0])
    logs = [math.log(got.get(n, (False, limit))[1] + 1.0) for n in names]
    sgm[solver] = math.exp(sum(logs) / len(logs)) - 1.0
    print(f"{solver:6s} solved {solved:3d} of {len(names)}, "
          f"shifted geometric mean {sgm[solver]:.2f} s")
for rival in sorted(s for s in sgm if s != "jaos"):
    if "jaos" in sgm and rival in sgm and sgm[rival] > 0:
        print(f"jaos / {rival}: {(sgm['jaos'] + 1.0) / (sgm[rival] + 1.0):.2f}x "
              f"(shifted means)")
