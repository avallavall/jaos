import re
import sys
p = sys.argv[1] + '/src/mip.c'
t = open(p, encoding='utf-8').read()
calls = [
    ('locks', 'const int got = locks_for_point(m, lp, x, side == 1, xr, &work,'),
    ('fj', 'int got = fj_for_point(m, lp, ilo, ihi, x, fj_limit, xr, &work,'),
    ('dive', 'const int got = dive_for_point(m, lp, dive_heur, nullptr, nullptr,'),
    ('pump', 'const int got = pump_for_point(m, lp, x, feaspump, pump_general,'),
    ('submip', 'const int got = submip_for_point('),
    ('rins', 'const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,'),
    ('localbr', 'const int got = local_branching(m, inc.x, inc.obj, lb_size,'),
]
for name, s in calls:
    assert t.count(s) == 1, (name, t.count(s))
    i = t.index(s)
    j = t.index(';', i)
    stmt = t[i:j + 1]
    ind = re.match(r'\s*', t[t.rfind('\n', 0, i) + 1:i]).group(0)
    new = (f'const int64_t hw_{name} = work;\n{ind}' + stmt +
           f'\n{ind}if (nodes == 1) fprintf(stderr, "HEURW {name} work=%lld got=%d\\n", (long long)(work - hw_{name}), got);')
    t = t[:i] + new + t[j + 1:]
if '#include <stdio.h>' not in t:
    t = '#include <stdio.h>\n' + t
open(p, 'w', encoding='utf-8').write(t)
print('heurw ok')
