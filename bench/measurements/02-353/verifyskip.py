import sys
# verifyskip2.py (any arg) : as verifyskip.py, with up to JAOS_VSKIP_K updates on the factors; : under JAOS_VSKIP, refresh() with refine set skips the refactorization when no pivot or basis
# change happened since the last one (LU fresh): the primal and dual values are refined on the same factors (scratch)
p = 'src/simplex.c'
s = open(p).read()
a = """static jaos_status refresh(sx *s, bool *ok, bool refine)
{
    bool repaired = false;
    s->n_refactor++;

    for (int attempt = 0;; attempt++) {"""
assert s.count(a) == 1
b = """static jaos_status refresh(sx *s, bool *ok, bool refine)
{
    bool repaired = false;
    if (getenv("JAOS_VSKIP") && refine && !s->needs_refactor &&
        s->lu.n_updates <= (getenv("JAOS_VSKIP_K") ? atoll(getenv("JAOS_VSKIP_K")) : 0) && s->lu.rank == s->nrow && s->lu_fresh) {
        compute_primal(s, refine);
        compute_duals(s, refine);
        if (shifts_costs(s)) {
            const bool sweep = s->shift_pending;
            s->shift_pending = false;
            if (sweep)
                for (int64_t v = 0; v < s->nvar; v++)
                    shift_to_feasible(s, v);
        }
        *ok = true;
        return JAOS_OK;
    }
    s->n_refactor++;

    for (int attempt = 0;; attempt++) {"""
s = s.replace(a, b)
# lu_fresh: set by refactorize, cleared by anything that changes the basis without an update
a2 = "    s->needs_refactor = false;\n"
assert s.count(a2) == 1
s = s.replace(a2, "    s->needs_refactor = false;\n    s->lu_fresh = true;\n")
a3 = "    bool needs_refactor;\n"
assert s.count(a3) == 1
s = s.replace(a3, "    bool needs_refactor;\n    bool lu_fresh;\n")
if '#include <stdlib.h>' not in s:
    s = s.replace('#include "jaos_internal.h"', '#include "jaos_internal.h"\n#include <stdlib.h>', 1)
open(p, 'w').write(s)
print('patched')
