import sys
p = sys.argv[1]
s = open(p).read()
old = "constexpr double MIP_NODE_PRESOLVE_ITERS = 20.0;"
assert s.count(old) == 1
s = s.replace(old, "constexpr double MIP_NODE_PRESOLVE_ITERS = 1e30;")
open(p, 'w').write(s)
print("ok")
