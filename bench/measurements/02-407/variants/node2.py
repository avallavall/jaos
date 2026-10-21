# Adds to a copy of src/mip.c (before 02-407) node propagation of J7PROP
# passes, read from the environment, at every node after the root
# while the tree has no incumbent, on a linear model whose --propagate is
# not set. This is the patch behind node2-34.txt and node2-miplib.txt.
#
#   python3 node2.py              run in the copy's root
#
# SPDX-License-Identifier: Apache-2.0
p = 'src/mip.c'
s = open(p).read()
def rep(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)
rep('''    const int64_t propagate_depth = m->cfg.mip_propagate_depth_set
        ? m->cfg.mip_propagate_depth : MIP_PROPAGATE_DEPTH;
''', '''    const int64_t propagate_depth = m->cfg.mip_propagate_depth_set
        ? m->cfg.mip_propagate_depth : MIP_PROPAGATE_DEPTH;
    const int64_t noinc_propagate = !quadratic && !m->cfg.mip_propagate_set &&
                                    getenv("J7PROP") != nullptr
                                        ? atoll(getenv("J7PROP")) : 0;
''')
rep('''    if (propagate > 0) {
        plo = jm_alloc_array(nc > 0 ? nc : 1, sizeof *plo);''', '''    if (propagate > 0 || noinc_propagate > 0) {
        plo = jm_alloc_array(nc > 0 ? nc : 1, sizeof *plo);''')
rep('''        if (propagate > 0 &&
            (propagate_depth < 0 || depth_here <= propagate_depth)) {
            const int64_t got = propagate_node(m, lp, plo, phi, propagate,
                                               &work);''', '''        const int64_t passes = propagate > 0 ? propagate
                             : !inc.have && nodes > 1 ? noinc_propagate : 0;
        if (passes > 0 &&
            (propagate_depth < 0 || depth_here <= propagate_depth)) {
            const int64_t got = propagate_node(m, lp, plo, phi, passes,
                                               &work);''')
if '#include <stdlib.h>' not in s:
    s = '#include <stdlib.h>\n' + s
open(p, 'w').write(s)
