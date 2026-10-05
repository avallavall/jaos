import sys
# gainlog.py FILE : under JAOS_GAINLOG, print every pseudocost observation: SBG (probe) and TRG (tree child), per unit (scratch)
p = sys.argv[1]
s = open(p).read()
NL = "\\n"
a = """                    pc_sum[d * nc + j] += gain / (d == 0 ? f : 1.0 - f);
                    pc_n[d * nc + j] += 1;"""
assert s.count(a) == 1
s = s.replace(a, a + """
                    if (getenv("JAOS_GAINLOG"))
                        fprintf(stderr, "SBG %lld %d %.9g %.9g""" + NL + """", (long long)j, d, gain / (d == 0 ? f : 1.0 - f), d == 0 ? f : 1.0 - f);""")
b = """    pc_sum[d * nc + j] += gain / n->frac;
    pc_n[d * nc + j] += 1;"""
assert s.count(b) == 1
s = s.replace(b, b + """
    if (getenv("JAOS_GAINLOG"))
        fprintf(stderr, "TRG %lld %d %.9g %.9g""" + NL + """", (long long)j, d, gain / n->frac, n->frac);""")
if '#include <stdio.h>' not in s:
    s = s.replace('#include <stdlib.h>', '#include <stdio.h>\n#include <stdlib.h>', 1)
open(p, 'w').write(s)
print('patched')
