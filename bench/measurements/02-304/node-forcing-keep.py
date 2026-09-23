import sys
p = sys.argv[1]
s = open(p).read()
a = """                    if (col_pending_dual[j]) {
                        at_own_bounds = false;
                        break;
                    }
                    const bool want_lo ="""
b = """                    if (col_pending_dual[j]) {
                        at_own_bounds = false;
                        break;
                    }
#if defined(JAOS_NODE_FORCING_KEEP)
                    if (m->cfg.node_solve && m->start_col_status != nullptr &&
                        m->start_row_status != nullptr &&
                        m->start_col_status[j] == JAOS_BASIS_BASIC) {
                        at_own_bounds = false;
                        break;
                    }
#endif
                    const bool want_lo ="""
c = """            if ((!isfinite(rl) || min_act >= rl - rtol) &&
                (!isfinite(ru) || max_act <= ru + rtol)) {
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_REDUNDANT_ROW, .index = i })) {"""
d = """            if ((!isfinite(rl) || min_act >= rl - rtol) &&
                (!isfinite(ru) || max_act <= ru + rtol)
#if defined(JAOS_NODE_REDUNDANT_KEEP)
                && !(m->cfg.node_solve && m->start_col_status != nullptr &&
                     m->start_row_status != nullptr &&
                     m->start_row_status[i] != JAOS_BASIS_BASIC)
#endif
                ) {
                if (!ps_push(p, (jm_presolve_rec){
                        .tag = JM_PS_REDUNDANT_ROW, .index = i })) {"""
assert s.count(a) == 1 and s.count(c) == 1
open(p, 'w').write(s.replace(a, b).replace(c, d))
