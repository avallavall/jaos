import sys, itertools
k = int(sys.argv[1]); off = int(sys.argv[2]); out = sys.argv[3]
cols = list(range(1, 2**k)); rows = list(range(1, 2**k))
L = ["NAME mini", "ROWS", " N obj"] + [" G r%d" % s for s in rows] + ["COLUMNS", "    M1 'MARKER' 'INTORG'"]
for t in cols:
    L.append("    x%d obj 1" % t)
    for s in rows:
        if bin(s & t).count("1") % 2 == 1:
            L.append("    x%d r%d 1" % (t, s))
L.append("    M2 'MARKER' 'INTEND'")
L.append("RHS")
for s in rows:
    L.append("    rhs r%d %d" % (s, off - bin(s).count("1")))
L.append("BOUNDS")
for t in cols:
    L.append(" UP bnd x%d 1" % t)
L.append("ENDATA")
open(out, "w").write("\n".join(L) + "\n")
