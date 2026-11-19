import sys

p = sys.argv[1]
s = open(p).read()
a = "    for (int64_t round = s->settle_round; round < rounds; round++) {\n        s->settle_round = round;\n"
nl = chr(92) + "n"
b = a + ('        if (getenv("JAOS_SETDBG")) fprintf(stderr, "SETDBG round %lld iters %lld work %lld warm %d node %d' + nl +
         '", (long long)round, (long long)s->iters, (long long)s->work.units, (int)!s->cold, (int)s->m->cfg.node_solve);\n')
assert s.count(a) == 1
s = s.replace(a, b)
for inc in ("#include <stdlib.h>", "#include <stdio.h>"):
    if inc not in s:
        s = inc + "\n" + s
open(p, "w").write(s)
print("patched")
