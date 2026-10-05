import sys
# nodenopre.py (any arg) : patches src/simplex.c in the current tree. Under JAOS_NODENOPRE, a node solve that starts
# from a basis skips presolve, as 02-300's refused warm-node-no-presolve did (scratch)
p = 'src/simplex.c'
s = open(p).read()
a = """        const bool quadratic = jm_model_has_quadratic(m);
        jaos_status pst = quadratic ? JAOS_OK : jm_presolve_run(m, &p, &pre_work);
        if (pst != JAOS_OK) {
            jm_presolve_free(&p);
            return pst;
        }
        if (quadratic) {
            p.outcome = JM_PRESOLVE_NONE;"""
assert s.count(a) == 1
b = """        const bool quadratic = jm_model_has_quadratic(m);
        const bool skip_pre = getenv("JAOS_NODENOPRE") != nullptr && m->cfg.node_solve &&
                              m->start_col_status != nullptr && m->start_row_status != nullptr;
        jaos_status pst = quadratic || skip_pre ? JAOS_OK : jm_presolve_run(m, &p, &pre_work);
        if (pst != JAOS_OK) {
            jm_presolve_free(&p);
            return pst;
        }
        if (skip_pre && !quadratic)
            p.outcome = JM_PRESOLVE_NONE;
        if (quadratic) {
            p.outcome = JM_PRESOLVE_NONE;"""
s = s.replace(a, b)
if '#include <stdlib.h>' not in s:
    s = s.replace('#include "jaos_internal.h"', '#include "jaos_internal.h"\n#include <stdlib.h>', 1)
open(p, 'w').write(s)
print('patched simplex.c')
