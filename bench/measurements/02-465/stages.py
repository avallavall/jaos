import sys
p = sys.argv[1] + '/src/simplex.c'
t = open(p, encoding='utf-8').read()

def sub(old, new):
    global t
    assert t.count(old) == 1, (old[:60], t.count(old))
    t = t.replace(old, new)

sub('''            st = s.primal_run ? run_primal(&s, &outcome)
                              : run(&s, &outcome);
        }''', '''            const int64_t kw0 = s.work.units, ki0 = s.iters;
            const int64_t kf0 = s.n_refactor;
            st = s.primal_run ? run_primal(&s, &outcome)
                              : run(&s, &outcome);
            if (m->cfg.node_solve) {
                kc_pre += kw0;
                kc_run += s.work.units - kw0;
                kc_it += s.iters - ki0;
                kc_ref += s.n_refactor - kf0;
            }
        }''')
sub('''    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL)
        polish_unscaled(&s);
    if (st == JAOS_OK)
        st = publish(&s, outcome, &p);''', '''    const int64_t kp0 = s.work.units;
    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL)
        polish_unscaled(&s);
    if (st == JAOS_OK)
        st = publish(&s, outcome, &p);
    if (m->cfg.node_solve)
        kc_pub += s.work.units - kp0;''')
sub('''    *units = s.work.units;
    *iters = s.iters;
    if (m->kept != nullptr) {''', '''    *units = s.work.units;
    *iters = s.iters;
    if (m->cfg.node_solve) {
        kc_tot += s.work.units;
        kc_n++;
        kc_presolved += target != m;
    }
    if (m->kept != nullptr) {''')
sub('''static jaos_status dual_simplex_once(jaos_model *m, bool *aggregated,''', '''static long long kc_pre, kc_run, kc_pub, kc_tot, kc_n, kc_it, kc_ref, kc_presolved;
static bool kc_armed;
static void kc_print(void)
{
    fprintf(stderr, "KC solves=%lld presolved=%lld total=%lld before_run=%lld run=%lld publish=%lld iters=%lld refactors=%lld\\n",
            kc_n, kc_presolved, kc_tot, kc_pre, kc_run, kc_pub, kc_it, kc_ref);
}

static jaos_status dual_simplex_once(jaos_model *m, bool *aggregated,''')
sub('''    jm_work pre_work = {0};''', '''    if (!kc_armed) {
        kc_armed = true;
        atexit(kc_print);
    }
    jm_work pre_work = {0};''')
for inc in ('#include <stdio.h>', '#include <stdlib.h>'):
    if inc not in t:
        t = inc + '\n' + t
open(p, 'w', encoding='utf-8').write(t)
print('kcount ok')
t = open(p, encoding='utf-8').read()
old = '''        st = reenter_after_settling(&s, &stopped, settle_resume);
        if (st == JAOS_ERR_NUMERICAL && s.early_stalled) {'''
assert t.count(old) == 1
t = t.replace(old, '''        const int64_t ks0 = s.iters, kw1 = s.work.units;
        st = reenter_after_settling(&s, &stopped, settle_resume);
        if (m->cfg.node_solve) {
            kc_set_it += s.iters - ks0;
            kc_set_w += s.work.units - kw1;
            kc_set_n += s.iters > ks0;
            kc_pert += s.costs_perturbed || s.early_perturbed;
        }
        if (st == JAOS_ERR_NUMERICAL && s.early_stalled) {''')
t = t.replace('''static long long kc_pre, kc_run,''', '''static long long kc_set_it, kc_set_w, kc_set_n, kc_pert;
static long long kc_pre, kc_run,''')
t = t.replace('''    fprintf(stderr, "KC solves=%lld presolved''', '''    fprintf(stderr, "KC settle iters=%lld work=%lld solves_with_settle_iters=%lld perturbed=%lld\\n", kc_set_it, kc_set_w, kc_set_n, kc_pert);
    fprintf(stderr, "KC solves=%lld presolved''')
open(p, 'w', encoding='utf-8').write(t)
print('settle ok')
