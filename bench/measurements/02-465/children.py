import sys
p = sys.argv[1] + '/src/mip.c'
t = open(p, encoding='utf-8').read()
old = '''            const jaos_status st = jm_probe(lp, j, lo, hi, room, &bound, &o,
                                            &w, &it);'''
assert t.count(old) == 1
new = old + '''
            if (st == JAOS_OK && o != JAOS_SOLVE_NOT_RUN && lp->sol_col_status != nullptr) {
                double kb = 0.0;
                jaos_solve_status ko = JAOS_SOLVE_NOT_RUN;
                int64_t kw = 0, ki = 0;
                const jaos_status ks = jm_probe(lp, j, lo, hi, 0, &kb, &ko, &kw, &ki);
                jaos_model *cp = nullptr;
                if (ks == JAOS_OK && ko == JAOS_SOLVE_OPTIMAL &&
                    jaos_model_copy(lp, &cp) == JAOS_OK) {
                    cp->cfg = lp->cfg;
                    cp->cfg.node_keep = false;
                    cp->cfg.node_probe = false;
                    cp->cfg.work_limit = 0;
                    if (jaos_set_col_bounds(cp, j, lo, hi) == JAOS_OK &&
                        jaos_set_basis(cp, lp->sol_col_status, lp->sol_row_status) == JAOS_OK &&
                        jaos_solve(cp) == JAOS_OK &&
                        jaos_status_of(cp) == JAOS_SOLVE_OPTIMAL) {
                        wc_n++;
                        wc_kit += ki;
                        wc_kw += kw;
                        wc_fit += cp->solve_iters;
                        wc_fw += jaos_work_units(cp);
                    }
                    jaos_model_free(cp);
                }
            }'''
t = t.replace(old, new)
old2 = '''static void probe_learn(double *pc_sum,'''
assert t.count(old2) == 1
t = t.replace(old2, '''static long long wc_n, wc_kit, wc_kw, wc_fit, wc_fw;
static bool wc_armed;
static void wc_print(void)
{
    fprintf(stderr, "WARM children=%lld kept_iters=%lld kept_work=%lld warm_iters=%lld warm_work=%lld\\n",
            wc_n, wc_kit, wc_kw, wc_fit, wc_fw);
}

static void probe_learn(double *pc_sum,''')
old3 = '''    const int64_t nc = m->num_col;
    int64_t out = 0;'''
assert t.count(old3) == 1
t = t.replace(old3, '''    const int64_t nc = m->num_col;
    int64_t out = 0;
    if (!wc_armed) {
        wc_armed = true;
        atexit(wc_print);
    }''')
for inc in ('#include <stdio.h>', '#include <stdlib.h>'):
    if inc not in t:
        t = inc + '\n' + t
open(p, 'w', encoding='utf-8').write(t)
print('warmcmp ok')
