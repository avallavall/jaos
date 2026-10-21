# The same proximal term as always.py, taken only from the first round in
# which the push stalls (no wrong sign, free reduced costs left, the gap
# no longer halving), with the normal matrix formed again there.
#
#   python3 stall.py K         run in the copy's root
#
# SPDX-License-Identifier: Apache-2.0
import sys
k = sys.argv[1]
p = 'src/barrier.c'
s = open(p).read()
def rep(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)
rep('''static jaos_status qp_push(bx *s)
{''', '''static double push_reg_of(const bx *s, int64_t j, double reg)
{
    const double q = %s * s->quad[j];
    return q > 0.0 && q < reg ? q : reg;
}

static jaos_status qp_push(bx *s)
{''' % k)
rep('''    bool settled = false, fresh = true, stagnant = false;''',
    '''    bool settled = false, fresh = true, stagnant = false, curved = false;''')
rep('''            const double inv = s->quad[j] + push_reg;''',
    '''            const double inv = s->quad[j] + (curved ? push_reg_of(s, j, push_reg) : push_reg);''')
rep('''                                   push_reg * s->dz[j];''',
    '''                                   (curved ? push_reg_of(s, j, push_reg) : push_reg) * s->dz[j];''')
rep('''        if (wrong == 0) {
            stagnant = !fresh;
            continue;
        }''', '''        if (wrong == 0) {
            stagnant = !fresh;
            if (stagnant && !curved) {
                curved = true;
                fresh = true;
                stagnant = false;
            }
            continue;
        }''')
open(p, 'w').write(s)
