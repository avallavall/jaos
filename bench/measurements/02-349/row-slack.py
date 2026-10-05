import sys
# row-slack.py FILE : the refused form of 02-349, applied to src/mip.c of ebf79c2: each candidate row's
# slack at the point, times |lambda|, joins the bound distance the aggregate keeps
p = sys.argv[1]
s = open(p).read()
a = """                    double mass = 0.0;
                    *work += lp->ar_start[r + 1] - lp->ar_start[r];
                    for (int64_t q = lp->ar_start[r]; q < lp->ar_start[r + 1];
                         q++) {
                        const int64_t j = lp->ar_index[q];
                        if (dist[j] == 0.0)"""
assert s.count(a) == 1
s = s.replace(a, a.replace("double mass = 0.0;", "double mass = 0.0, act = 0.0;").replace(
    "                        if (dist[j] == 0.0)",
    "                        act += lp->ar_value[q] * x[j];\n                        if (dist[j] == 0.0)"))
b = """                            mass += (fabs(now) - fabs(was)) * dist[j];
                    }
"""
assert s.count(b) == 1
s = s.replace(b, b + """                    mass += fabs(lam) * fmax(0.0, lam > 0.0 ? act - bnd
                                                            : bnd - act);
""")
open(p, 'w').write(s)
print('patched')
