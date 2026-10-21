# Makes the barrier take the augmented system for a QP whenever J24AUG is
# set in the environment, in a copy of src/barrier.c.
#
#   python3 aug.py             run in the copy's root
#
# SPDX-License-Identifier: Apache-2.0
p = 'src/barrier.c'
s = open(p).read()
def rep(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)
rep('''    if (normal < BARRIER_AUG_TRY)
        return st;''', '''    if (normal < BARRIER_AUG_TRY && getenv("J24AUG") == nullptr)
        return st;''')
rep('''    s->augmented = aug * BARRIER_AUG_EDGE < normal;''',
    '''    s->augmented = getenv("J24AUG") != nullptr || aug * BARRIER_AUG_EDGE < normal;''')
if '#include <stdlib.h>' not in s:
    s = '#include <stdlib.h>\n' + s
open(p, 'w').write(s)
