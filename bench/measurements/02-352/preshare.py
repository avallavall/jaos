import sys
# preshare.py (any arg) : counts the presolve work and the whole work of node LP solves, printed at the end of the tree (scratch)
p = 'src/simplex.c'
s = open(p).read()
NL = "\\n"
s = s.replace('static jaos_status publish(sx *s, jaos_solve_status status, jm_presolve *p)',
              'long long jaos_scratch_pre = 0, jaos_scratch_all = 0, jaos_scratch_n = 0;\nstatic jaos_status publish(sx *s, jaos_solve_status status, jm_presolve *p)', 1)
a = "        s.work = pre_work;\n"
assert s.count(a) == 1
s = s.replace(a, a + "        if (m->cfg.node_solve) {\n            jaos_scratch_pre += pre_work.units;\n            jaos_scratch_n++;\n        }\n")
for b in ["        m->solve_work = s->work.units;\n", "    m->solve_work = s->work.units;\n"]:
    assert s.count(b) >= 1
s = s.replace("        m->solve_work = s->work.units;\n", "        m->solve_work = s->work.units;\n        if (m->cfg.node_solve)\n            jaos_scratch_all += s->work.units;\n")
s = s.replace("\n    m->solve_work = s->work.units;\n", "\n    m->solve_work = s->work.units;\n    if (m->cfg.node_solve)\n        jaos_scratch_all += s->work.units;\n")
open(p, 'w').write(s)
p = 'src/mip.c'
s = open(p).read()
s = s.replace('#include "jaos_sys.h"', '#include "jaos_sys.h"\nextern long long jaos_scratch_pre, jaos_scratch_all, jaos_scratch_n;', 1)
c = 'jm_log(m, JAOS_LOG_SUMMARY,\n                   "branch and bound work: '
i = s.find(c)
if i < 0:
    c = '"branch and bound work: '
    i = s.find(c)
    i = s.rfind('jm_log(', 0, i)
assert i > 0
s = s[:i] + 'fprintf(stderr, "PRESHARE pre %lld all %lld solves %lld' + NL + '", jaos_scratch_pre, jaos_scratch_all, jaos_scratch_n);\n            ' + s[i:]
if '#include <stdio.h>' not in s:
    s = s.replace('#include <stdlib.h>', '#include <stdio.h>\n#include <stdlib.h>', 1)
open(p, 'w').write(s)
print('patched')
