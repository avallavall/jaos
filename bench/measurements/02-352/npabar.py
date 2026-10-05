import sys
# npabar.py FILE : on the tree of e502eea, MIP_NODE_PRESOLVE_ITERS read from JAOS_NPA_ITERS (scratch)
p = 'src/mip.c'
s = open(p).read()
a = "                    MIP_NODE_PRESOLVE_ITERS * (double)MIP_NODE_PRESOLVE_TRIAL) {"
assert s.count(a) == 1
s = s.replace(a, "                    (getenv(\"JAOS_NPA_ITERS\") ? atof(getenv(\"JAOS_NPA_ITERS\")) : MIP_NODE_PRESOLVE_ITERS) * (double)MIP_NODE_PRESOLVE_TRIAL) {")
open(p, 'w').write(s)
print('patched')
