"""Instrumentation for klein2's declined pivots. Applies and reverts.

Guarded by JAOS_DIAG so the normal build cannot change, and applied to a
COPY of the tree under mktemp -d, never under build/ (jaos-debug).

What it dumps, at each declined pivot: the iteration, the basis position
r, the leaving variable, the entering variable q, the two readings of the
pivot element and their relative disagreement. That is what says whether
the same pivot is declined over and over -- a livelock -- or whether the
solve is grinding through different ones.
"""
import sys

path = sys.argv[1] + '/src/simplex.c'
s = open(path, encoding='utf-8').read()

anchor = """        if (big > 0.0 && fabs(alpha_q - s->col[r]) > LU_AGREE_TOL * big &&
            s->lu.n_updates > 0) {
            s->needs_refactor = true;
            s->n_stability++;
            *took = false;
            return JAOS_OK;
        }"""
assert s.count(anchor) == 1, 'anchor matched %d times' % s.count(anchor)

patched = """        if (big > 0.0 && fabs(alpha_q - s->col[r]) > LU_AGREE_TOL * big &&
            s->lu.n_updates > 0) {
#ifdef JAOS_DIAG
            fprintf(stderr, "DECL %lld r=%lld leave=%lld q=%lld "
                            "alpha=%.17g col=%.17g rel=%.6g upd=%lld\\n",
                    (long long)s->iters, (long long)r, (long long)leaving,
                    (long long)q, alpha_q, s->col[r],
                    fabs(alpha_q - s->col[r]) / big,
                    (long long)s->lu.n_updates);
#endif
            s->needs_refactor = true;
            s->n_stability++;
            *took = false;
            return JAOS_OK;
        }
#ifdef JAOS_DIAG
        fprintf(stderr, "TAKE %lld r=%lld leave=%lld q=%lld alpha=%.17g "
                        "theta=%.17g upd=%lld\\n",
                (long long)s->iters, (long long)r, (long long)leaving,
                (long long)q, alpha_q, (s->xb[r] - bound) / alpha_q,
                (long long)s->lu.n_updates);
#endif"""

s = s.replace(anchor, patched, 1)

# stdio for the fprintf.
inc = '#include <math.h>'
assert s.count(inc) >= 1
s = s.replace(inc, '#include <math.h>\n#ifdef JAOS_DIAG\n#include <stdio.h>\n#endif', 1)

open(path, 'w', encoding='utf-8', newline='').write(s)
print('patched', path)
