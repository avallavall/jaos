# RENS from outside the tool, for 02-394: for each named model, solve its
# relaxation, fix every integer column whose value is integral, box the
# others between floor and ceil, and solve that MIP at a work limit.
#
#   python3 rens.py JAOS MODELS NAMES [LIMIT] [SCRATCH]
#
# MODELS holds read.sh's NAME.mps files; NAMES lists one name a line;
# LIMIT defaults to 1e8 work units; SCRATCH, to /tmp, takes the
# intermediate files.
#
# SPDX-License-Identifier: Apache-2.0
import math
import subprocess
import sys

J = sys.argv[1]
models = sys.argv[2]
names = [l.split()[0] for l in open(sys.argv[3]) if l.strip()]
limit = sys.argv[4] if len(sys.argv) > 4 else "100000000"
scratch = sys.argv[5] if len(sys.argv) > 5 else "/tmp"


def strip_markers(src, dst):
    with open(dst, "w") as f:
        for line in open(src):
            if "MARKER" in line:
                continue
            f.write(line)


def integer_cols(src):
    cols, inside = set(), False
    for line in open(src):
        if "'INTORG'" in line:
            inside = True
            continue
        if "'INTEND'" in line:
            inside = False
            continue
        if inside and line.startswith("    "):
            cols.add(line.split()[0])
    return cols


def solve(path, extra):
    r = subprocess.run([J, "solve", path] + extra, capture_output=True,
                       text=True, timeout=900).stdout
    out = {}
    for line in r.splitlines():
        t = line.split()
        if len(t) == 2:
            out[t[0]] = t[1]
    return out


for name in names:
    base = "%s/%s.mps" % (models, name)
    lp = scratch + "/rens-lp.mps"
    strip_markers(base, lp)
    sol = scratch + "/rens-lp.sol"
    res = solve(lp, ["--solution", sol])
    if res.get("status") != "optimal":
        print(name, "lp", res.get("status"))
        continue
    val = {}
    for line in open(sol):
        t = line.split()
        if t and t[0] == "col":
            val[t[1]] = float(t[2])
    ints = integer_cols(base)
    fixed = boxed = 0
    lines = open(base).read().split("\n")
    out = []
    for line in lines:
        if line.startswith("ENDATA"):
            for c in sorted(ints):
                v = val[c]
                if abs(v - round(v)) <= 1e-6:
                    out.append(" FX bnd %s %d" % (c, round(v)))
                    fixed += 1
                else:
                    out.append(" LO bnd %s %d" % (c, math.floor(v)))
                    out.append(" UP bnd %s %d" % (c, math.ceil(v)))
                    boxed += 1
        out.append(line)
    sub = scratch + "/rens-sub.mps"
    with open(sub, "w") as f:
        f.write("\n".join(out))
    r = solve(sub, ["--work-limit", limit])
    print("%-16s fixed %3d boxed %3d -> %s inc %s obj %s nodes %s work %s" % (
        name, fixed, boxed, r.get("status"), r.get("incumbent"),
        r.get("objective"), r.get("nodes"), r.get("work_units")), flush=True)
