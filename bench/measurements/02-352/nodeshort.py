import sys
# nodeshort.py (any arg) : patches src/simplex.c. Under JAOS_NODESHORT, a node solve whose presolved model would start
# from a basis short by more than WARM_REPAIR_MAX_SHORT drops the presolve and solves its own LP from the full basis (scratch)
p = 'src/simplex.c'
s = open(p).read()
NL = "\\n"
a = """        if (quadratic) {
            p.outcome = JM_PRESOLVE_NONE;
            jm_log(m, JAOS_LOG_SUMMARY,
                   "presolve: skipped, the objective has a quadratic term");
        }"""
assert s.count(a) == 1
b = a + """
        if (getenv("JAOS_NODESHORT") && m->cfg.node_solve &&
            p.outcome == JM_PRESOLVE_REDUCED &&
            m->start_col_status != nullptr && m->start_row_status != nullptr &&
            p.reduced.start_col_status != nullptr &&
            p.reduced.start_row_status != nullptr) {
            int64_t nb = 0;
            for (int64_t k = 0; k < p.reduced.num_col; k++)
                nb += p.reduced.start_col_status[k] == JAOS_BASIS_BASIC;
            for (int64_t k = 0; k < p.reduced.num_row; k++)
                nb += p.reduced.start_row_status[k] == JAOS_BASIS_BASIC;
            if (p.reduced.num_row - nb > WARM_REPAIR_MAX_SHORT) {
                if (getenv("JAOS_NODESHORT_LOG"))
                    fprintf(stderr, "NODESHORT short %lld of %lld rows""" + NL + """", (long long)(p.reduced.num_row - nb),
                            (long long)p.reduced.num_row);
                jm_presolve_free(&p);
                jm_presolve_init(&p);
                p.outcome = JM_PRESOLVE_NONE;
            }
        }"""
s = s.replace(a, b)
if '#include <stdio.h>' not in s:
    s = s.replace('#include "jaos_internal.h"', '#include "jaos_internal.h"\n#include <stdio.h>', 1)
if '#include <stdlib.h>' not in s:
    s = s.replace('#include "jaos_internal.h"', '#include "jaos_internal.h"\n#include <stdlib.h>', 1)
open(p, 'w').write(s)
print('patched simplex.c')
