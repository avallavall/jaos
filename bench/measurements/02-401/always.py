# Gives every free column of the QP push a proximal term of the smaller of
# QP_PUSH_REG and K times its curvature, in a copy of src/barrier.c.
#
#   python3 always.py K        run in the copy's root
#
# SPDX-License-Identifier: Apache-2.0
import sys
k = sys.argv[1]
p = 'src/barrier.c'
s = open(p).read()
old = '''static jaos_status qp_push(bx *s)
{'''
new = '''static double push_reg_of(const bx *s, int64_t j, double reg)
{
    const double q = %s * s->quad[j];
    return q > 0.0 && q < reg ? q : reg;
}

static jaos_status qp_push(bx *s)
{''' % k
assert s.count(old) == 1
s = s.replace(old, new)
old = '''            const double inv = s->quad[j] + push_reg;'''
new = '''            const double inv = s->quad[j] + push_reg_of(s, j, push_reg);'''
assert s.count(old) == 1
s = s.replace(old, new)
old = '''                                   push_reg * s->dz[j];'''
new = '''                                   push_reg_of(s, j, push_reg) * s->dz[j];'''
assert s.count(old) == 1
s = s.replace(old, new)
open(p, 'w').write(s)
