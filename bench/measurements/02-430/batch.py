"""Makes a generator solve its tree with a round of four nodes on four
threads: inserts jaos_set_mip_tree_batch and jaos_set_threads before the
one `const jaos_status st = jaos_solve(m);` of the file given.
"""
import sys

p = sys.argv[1]
s = open(p).read()
old = "        const jaos_status st = jaos_solve(m);\n"
assert s.count(old) == 1
new = ("        if (jaos_set_mip_tree_batch(m, 4) != JAOS_OK ||\n"
       "            jaos_set_threads(m, 4) != JAOS_OK)\n"
       "            return 1;\n") + old
open(p, 'w').write(s.replace(old, new))
