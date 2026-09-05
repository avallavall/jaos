#!/usr/bin/env python3
"""Per-instance work ratios of every sweep arm against the control, and the
geometric mean over the instances both arms solved (D46: never a sum)."""
import math, re, sys, os

here = os.path.dirname(os.path.abspath(__file__))
KV = re.compile(r"(\w+)=(\S*)")

def load(tag):
    out = {}
    with open(os.path.join(here, "sweep-%s.txt" % tag)) as f:
        for line in f:
            name = line.split()[0]
            d = dict(KV.findall(line))
            out[name] = d
    return out

arms = sys.argv[1:] or ["dive", "c1", "c1nd", "c2", "c5"]
ctl = load("control")
names = sorted(ctl)
print("%-9s %14s " % ("instance", "control") + " ".join("%22s" % a for a in arms))
for n in names:
    row = "%-9s %14s " % (n, ctl[n].get("work") if ctl[n].get("status") == "optimal" else "(%s)" % ctl[n].get("status"))
    for a in arms:
        d = load(a).get(n, {})
        if d.get("status") == "optimal" and ctl[n].get("status") == "optimal":
            r = float(d["work"]) / float(ctl[n]["work"])
            row += " %8s %6sc %5.3fx" % (d["nodes"], d["cuts"], r)
        else:
            row += " %22s" % ("(%s)" % d.get("status", "?"))
    print(row)
print()
for a in arms:
    arm = load(a)
    logs = []; better = worse = same = 0
    for n in names:
        if ctl[n].get("status") == "optimal" and arm.get(n, {}).get("status") == "optimal":
            r = float(arm[n]["work"]) / float(ctl[n]["work"])
            logs.append(math.log(r))
            if r < 0.95: better += 1
            elif r > 1.05: worse += 1
            else: same += 1
    solved = sum(1 for n in names if arm.get(n, {}).get("status") == "optimal")
    gm = math.exp(sum(logs) / len(logs)) if logs else float("nan")
    print("%-6s solved %2d of %2d; over %2d solved by both: geomean work %.3fx, %d better, %d worse, %d within 5%%"
          % (a, solved, len(names), len(logs), gm, better, worse, same))
