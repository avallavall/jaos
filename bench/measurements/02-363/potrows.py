# Counts, per MPS file, the rows whose continuous columns are exactly two,
# with coefficients a and -a, beside at least one integer column.
#
#   python3 potrows.py bench/instances-miplib2017/*.mps
#
# SPDX-License-Identifier: Apache-2.0
import os, sys
from collections import defaultdict


def read_mps(path):
    rows, obj, sec, inint = [], None, None, False
    coef = defaultdict(dict)
    ints = set()
    for line in open(path):
        if line.startswith("*") or not line.strip():
            continue
        if not line[0].isspace():
            sec = line.split()[0]
            continue
        f = line.split()
        if sec == "ROWS":
            if f[0] == "N" and obj is None:
                obj = f[1]
            elif f[0] != "N":
                rows.append(f[1])
        elif sec == "COLUMNS":
            if "'MARKER'" in f:
                inint = "'INTORG'" in f
                continue
            if inint:
                ints.add(f[0])
            for k in range(1, len(f) - 1, 2):
                if f[k] != obj:
                    coef[f[k]][f[0]] = float(f[k + 1])
        elif sec == "BOUNDS" and f[0] in ("BV", "UI", "LI"):
            ints.add(f[2])
    return rows, coef, ints


for path in sys.argv[1:]:
    rows, coef, ints = read_mps(path)
    count, nodes = 0, set()
    for r in rows:
        cont = [(c, v) for c, v in coef[r].items() if c not in ints]
        if len(cont) == 2 and cont[0][1] == -cont[1][1] and \
                any(c in ints for c in coef[r]):
            count += 1
            nodes.update(c for c, _ in cont)
    if count:
        print(f"{os.path.basename(path)}: {len(rows)} rows, {count} rows "
              f"over {len(nodes)} continuous columns")
