import sys, re
# refreshdbg.py (any arg) : every call of refresh() in simplex.c prints its source line under JAOS_REFRESHDBG (scratch)
p = 'src/simplex.c'
s = open(p).read()
NL = "\\n"
a = "static jaos_status refresh(sx *s, bool *ok, bool refine)\n{"
assert s.count(a) == 1
s = s.replace(a, "static jaos_status refresh_impl(sx *s, bool *ok, bool refine)\n{")
# a forward declaration of refresh may exist; rename all other uses to the macro after the definition
idx = s.index("static jaos_status refresh_impl(sx *s, bool *ok, bool refine)")
head, tail = s[:idx], s[idx:]
head = head.replace("static jaos_status refresh(sx *s, bool *ok, bool refine);", "static jaos_status refresh_impl(sx *s, bool *ok, bool refine);")
macro = ('#define refresh(s_, ok_, r_) (getenv("JAOS_REFRESHDBG") ? fprintf(stderr, "REFRESH line %d iters %lld' + NL +
         '", __LINE__, (long long)(s_)->iters) : 0, refresh_impl(s_, ok_, r_))\n')
s = head + tail
# put the macro right after the includes
i = s.index('\n', s.index('#include "jaos_internal.h"'))
s = s[:i + 1] + '#include <stdio.h>\n#include <stdlib.h>\n' + macro + s[i + 1:]
# the definition line must not be macro-expanded: it was renamed already
open(p, 'w').write(s)
print('patched', s.count('refresh('))
