# Solves the generated network QPs with two builds and compares them.
#
#   python3 netqp_read.py BASE_JAOS NEW_JAOS DIR N M FIRST LAST
#
# writes DIR/N-M-SEED.mps with netqp.py's generator and prints one line a
# model, then the counts.
#
# SPDX-License-Identifier: Apache-2.0
import math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from netqp import gen

base, new, out, n, m, first, last = sys.argv[1:8]
n, m, first, last = int(n), int(m), int(first), int(last)
os.makedirs(out, exist_ok=True)


def run(jaos, path):
    p = subprocess.run([jaos, "solve", path, "--check", "--log", "summary"],
                       capture_output=True, text=True, timeout=600)
    kv = {}
    for line in p.stdout.split("\n"):
        f = line.split(" ", 1)
        if len(f) == 2:
            kv[f[0]] = f[1]
    kv["polished"] = "polished, it passes" in p.stderr
    return kv


count = {}
ratios = []
for seed in range(first, last + 1):
    path = os.path.join(out, "%d-%d-%d.mps" % (n, m, seed))
    gen(seed, n, m, path)
    a, b = run(base, path), run(new, path)
    sa = a.get("status", "?") + ("/checked" if a.get("check_ok") == "yes" else "")
    sb = b.get("status", "?") + ("/checked" if b.get("check_ok") == "yes" else "")
    key = (sa, sb, b["polished"])
    count[key] = count.get(key, 0) + 1
    if a.get("status") == "optimal" and b.get("status") == "optimal":
        ratios.append(float(b["work_units"]) / float(a["work_units"]))
    print(seed, sa, sb, "polished" if b["polished"] else "-",
          a.get("work_units"), b.get("work_units"), flush=True)
for k in sorted(count):
    print("count", count[k], "base", k[0], "new", k[1],
          "polished" if k[2] else "not polished")
if ratios:
    g = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
    print("work, optimal in both: %d models, geometric mean %.4fx, "
          "%d cheaper, %d dearer" % (len(ratios), g,
          sum(r < 1 for r in ratios), sum(r > 1 for r in ratios)))
