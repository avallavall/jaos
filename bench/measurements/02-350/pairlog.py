import sys
# pairlog.py FILE : under JAOS_PAIRLOG, probes print "PRB j d parentkey probeobj rows" and tree children "CHD j d parentkey childkey rows" (scratch)
p = sys.argv[1]
s = open(p).read()
NL = "\\n"
a = """                if (jaos_objective(lp, &obj) == JAOS_OK) {
                    double gain = sigma * obj - key;"""
assert s.count(a) == 1
s = s.replace(a, """                if (jaos_objective(lp, &obj) == JAOS_OK && getenv("JAOS_PAIRLOG"))
                    fprintf(stderr, "PRB %lld %d %.17g %.17g %lld""" + NL + """", (long long)j, d, key, sigma * obj, (long long)lp->num_row);
""" + a)
b = """    double gain = key - n->key;
    if (gain < 0.0)
        gain = 0.0;
    pc_sum[d * nc + j] += gain / n->frac;"""
assert s.count(b) == 1
s = s.replace(b, """    if (getenv("JAOS_PAIRLOG"))
        fprintf(stderr, "CHD %lld %d %.17g %.17g %lld""" + NL + """", (long long)j, d, n->key, key, (long long)n->nrow);
""" + b)
if '#include <stdio.h>' not in s:
    s = s.replace('#include <stdlib.h>', '#include <stdio.h>\n#include <stdlib.h>', 1)
open(p, 'w').write(s)
print('patched')
