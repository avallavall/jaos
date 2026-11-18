# The 34 models of 02-394 that the tree left at 2e9 and HiGHS proved, written
# from mipcmp.py's generators: gen394.py mipcmp.py OUTDIR mipcmp.txt
# SPDX-License-Identifier: Apache-2.0
import sys, random
src = open(sys.argv[1]).read()
src = src[:src.rindex("main()")]
g = {"__name__": "gen394"}
exec(compile(src, "mipcmp", "exec"), g)
out, listfile = sys.argv[2], sys.argv[3]
for ln in open(listfile):
    t = ln.split()
    if len(t) < 2 or not t[1].isdigit():
        continue
    if not (" skip" in ln and " optimal " in ln):
        continue
    seed = int(t[1])
    rng = random.Random(seed)
    fam = g["FAMILIES"][seed % len(g["FAMILIES"])]
    sense, cost, rows, lo, up, integer = fam(rng)
    path = "%s/%s-%d.mps" % (out, fam.__name__, seed)
    g["write_mps"](path, "m%d" % seed, sense, cost, rows, lo, up, integer)
    print(path)
