# Generated MILPs with integer data, solved by JAOS and by HiGHS 1.15.1,
# answers compared. Brute force cannot reach 20 to 60 integer columns, so
# HiGHS with mip_rel_gap = 0 is the judge.
#
#   python3 mipcmp.py JAOS HIGHS DIR FIRST LAST
#
# The families: multi-row knapsacks (binaries), set covering, fixed-charge
# transport (continuous flow, binary opening), and general-integer models
# with equality rows and some continuous columns. Every coefficient, side
# and cost is an integer. Two optima agree within 1e-5 of (1 + |HiGHS's|).
# One line a model, then the counts.
#
# SPDX-License-Identifier: Apache-2.0
import random
import subprocess
import sys


def write_mps(path, name, sense, cost, rows, lo, up, integer):
    n = len(cost)
    with open(path, "w") as f:
        f.write("NAME %s\n" % name)
        if sense == "max":
            f.write("OBJSENSE\n    MAX\n")
        f.write("ROWS\n N obj\n")
        for i, (kind, _, _) in enumerate(rows):
            f.write(" %s r%d\n" % (kind, i))
        f.write("COLUMNS\n")
        cols = [[] for _ in range(n)]
        for i, (_, coef, _) in enumerate(rows):
            for j, a in coef.items():
                cols[j].append((i, a))
        inint = False
        for j in range(n):
            if integer[j] and not inint:
                f.write("    M1 'MARKER' 'INTORG'\n")
                inint = True
            if not integer[j] and inint:
                f.write("    M2 'MARKER' 'INTEND'\n")
                inint = False
            f.write("    x%d obj %d\n" % (j, cost[j]))
            for i, a in cols[j]:
                f.write("    x%d r%d %d\n" % (j, i, a))
        if inint:
            f.write("    M2 'MARKER' 'INTEND'\n")
        f.write("RHS\n")
        for i, (_, _, b) in enumerate(rows):
            f.write("    rhs r%d %d\n" % (i, b))
        f.write("BOUNDS\n")
        for j in range(n):
            if lo[j] == up[j]:
                f.write(" FX bnd x%d %d\n" % (j, lo[j]))
                continue
            if lo[j] != 0:
                f.write(" LO bnd x%d %d\n" % (j, lo[j]))
            if up[j] is not None:
                f.write(" UP bnd x%d %d\n" % (j, up[j]))
        f.write("ENDATA\n")


def knapsack(rng):
    n, m = rng.randint(20, 50), rng.randint(2, 8)
    cost = [rng.randint(1, 60) for _ in range(n)]
    rows = []
    for _ in range(m):
        coef = {j: rng.randint(1, 40) for j in range(n) if rng.random() < 0.7}
        rows.append(("L", coef, max(1, sum(coef.values()) // rng.randint(2, 4))))
    return "max", cost, rows, [0] * n, [1] * n, [True] * n


def cover(rng):
    n, m = rng.randint(25, 60), rng.randint(15, 40)
    cost = [rng.randint(1, 30) for _ in range(n)]
    rows = []
    for _ in range(m):
        k = rng.randint(2, 6)
        coef = {j: 1 for j in rng.sample(range(n), k)}
        rows.append(("G", coef, 1))
    return "min", cost, rows, [0] * n, [1] * n, [True] * n


def transport(rng):
    ns, nd = rng.randint(3, 6), rng.randint(4, 8)
    n = ns * nd + ns
    cost = []
    for s in range(ns):
        for d in range(nd):
            cost.append(rng.randint(1, 20))
    for s in range(ns):
        cost.append(rng.randint(20, 120))
    demand = [rng.randint(5, 30) for _ in range(nd)]
    cap = [rng.randint(sum(demand) // ns, sum(demand)) for _ in range(ns)]
    rows = []
    for d in range(nd):
        rows.append(("G", {s * nd + d: 1 for s in range(ns)}, demand[d]))
    for s in range(ns):
        coef = {s * nd + d: 1 for d in range(nd)}
        coef[ns * nd + s] = -cap[s]
        rows.append(("L", coef, 0))
    lo = [0] * n
    up = [None] * (ns * nd) + [1] * ns
    integer = [False] * (ns * nd) + [True] * ns
    return "min", cost, rows, lo, up, integer


def general(rng):
    n, m = rng.randint(15, 35), rng.randint(5, 15)
    x0 = [rng.randint(0, 6) for _ in range(n)]
    cost = [rng.randint(-10, 10) for _ in range(n)]
    rows = []
    for i in range(m):
        coef = {j: rng.randint(-6, 6) for j in range(n) if rng.random() < 0.3}
        coef = {j: a for j, a in coef.items() if a != 0}
        if not coef:
            coef = {rng.randrange(n): 1}
        act = sum(a * x0[j] for j, a in coef.items())
        kind = rng.choice("LGE") if i % 3 else "E"
        b = act if kind == "E" else (act + rng.randint(0, 8) if kind == "L"
                                     else act - rng.randint(0, 8))
        rows.append((kind, coef, b))
    integer = [rng.random() < 0.7 for _ in range(n)]
    return "min", cost, rows, [0] * n, [10] * n, integer


def binpack(rng):
    items, bins = rng.randint(6, 12), rng.randint(3, 6)
    size = [rng.randint(5, 40) for _ in range(items)]
    cap = max(max(size), sum(size) // bins + rng.randint(0, 20))
    n = items * bins + bins
    cost = [0] * (items * bins) + [1] * bins
    rows = []
    for i in range(items):
        rows.append(("E", {i * bins + b: 1 for b in range(bins)}, 1))
    for b in range(bins):
        coef = {i * bins + b: size[i] for i in range(items)}
        coef[items * bins + b] = -cap
        rows.append(("L", coef, 0))
    return "min", cost, rows, [0] * n, [1] * n, [True] * n


def bigknap(rng):
    n, m = rng.randint(60, 120), rng.randint(4, 12)
    cost = [rng.randint(1, 100) for _ in range(n)]
    rows = []
    for _ in range(m):
        coef = {j: rng.randint(1, 60) for j in range(n) if rng.random() < 0.5}
        rows.append(("L", coef, max(1, sum(coef.values()) // rng.randint(2, 5))))
    return "max", cost, rows, [0] * n, [1] * n, [True] * n


def biggeneral(rng):
    n, m = rng.randint(30, 60), rng.randint(15, 30)
    x0 = [rng.randint(0, 4) for _ in range(n)]
    cost = [rng.randint(-15, 15) for _ in range(n)]
    rows = []
    for i in range(m):
        coef = {j: rng.randint(-5, 5) for j in range(n) if rng.random() < 0.2}
        coef = {j: a for j, a in coef.items() if a != 0}
        if not coef:
            coef = {rng.randrange(n): 1}
        act = sum(a * x0[j] for j, a in coef.items())
        kind = "E" if i % 2 == 0 else rng.choice("LG")
        b = act if kind == "E" else (act + rng.randint(0, 5) if kind == "L"
                                     else act - rng.randint(0, 5))
        rows.append((kind, coef, b))
    integer = [rng.random() < 0.85 for _ in range(n)]
    return "min", cost, rows, [0] * n, [8] * n, integer


FAMILIES = [knapsack, cover, transport, general, binpack, bigknap,
            biggeneral]


def jaos(binary, path):
    out = subprocess.run([binary, "solve", path, "--check", "--work-limit",
                          "2000000000"], capture_output=True, text=True,
                         timeout=600).stdout
    st, obj = None, None
    for line in out.splitlines():
        if line.startswith("status "):
            st = line.split()[1]
        elif line.startswith("objective "):
            obj = float(line.split()[1])
    return st, obj


def highs(binary, path, opts):
    out = subprocess.run([binary, "--model_file", path, "--options_file",
                          opts], capture_output=True, text=True,
                         timeout=600).stdout
    st, obj = None, None
    for line in out.splitlines():
        t = line.split()
        if len(t) >= 2 and t[0] == "Status" and st is None:
            st = t[1].lower()
        elif len(t) >= 3 and t[0] == "Primal" and t[1] == "bound":
            try:
                obj = float(t[2])
            except ValueError:
                pass
    return st, obj


def main():
    jb, hb, out, first, last = sys.argv[1:6]
    opts = out + "/highs.opt"
    with open(opts, "w") as f:
        f.write("mip_rel_gap = 0\nmip_abs_gap = 0\ntime_limit = 300\n"
                "threads = 1\nlog_file = %s/Highs.log\n" % out)
    counts = {}
    for seed in range(int(first), int(last) + 1):
        rng = random.Random(seed)
        fam = FAMILIES[seed % len(FAMILIES)]
        sense, cost, rows, lo, up, integer = fam(rng)
        path = "%s/%s-%d.mps" % (out, fam.__name__, seed)
        write_mps(path, "m%d" % seed, sense, cost, rows, lo, up, integer)
        js, jo = jaos(jb, path)
        hs, ho = highs(hb, path, opts)
        verdict = "skip"
        if js == "work_limit" or hs not in ("optimal", "infeasible"):
            verdict = "skip"
        elif js == "optimal" and hs == "optimal":
            verdict = ("same" if abs(jo - ho) <= 1e-5 * (1.0 + abs(ho))
                       else "differ")
        elif js == "infeasible" and hs == "infeasible":
            verdict = "same"
        else:
            verdict = "differ"
        counts[verdict] = counts.get(verdict, 0) + 1
        print("%-10s %5d jaos %-12s %-22s highs %-12s %-22s %s" % (
            fam.__name__, seed, js, jo, hs, ho, verdict), flush=True)
    print("counts", " ".join("%s %d" % kv for kv in sorted(counts.items())))


main()
