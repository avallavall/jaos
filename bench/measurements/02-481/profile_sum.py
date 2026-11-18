# SPDX-License-Identifier: Apache-2.0
# Reads profile.patch's P lines (work units by file index and line, node
# relaxations below the presolve trial only) and prints the share of each
# function, with the source the lines come from.
#   profile_sum.py ERRFILE SRCDIR
import collections
import os
import re
import sys

src = sys.argv[2]
names = {}
for fn in os.listdir(src + '/src'):
    if fn.endswith('.c'):
        k = 'src/' + fn
        names.setdefault((ord(k[4]) * 31 + ord(k[5])) & 63, []).append(fn)
tab = {}
for line in open(sys.argv[1]):
    if line.startswith('P '):
        _, f, l, n = line.split()
        tab.setdefault((int(f), int(l)), int(n))
funcs = {}


def func_of(fn, line):
    if fn not in funcs:
        cur, out = '?', []
        for t in open(src + '/src/' + fn).read().split('\n'):
            m = re.match(r'^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w \*]*?\b(\w+)\(', t)
            if m and not t.startswith(' '):
                cur = m.group(1)
            out.append(cur)
        funcs[fn] = out
    return funcs[fn][line - 1]


by = collections.Counter()
nested = {('lu.c', 'ftran_any'), ('lu.c', 'jm_lu_ftran')}
for (f, l), n in tab.items():
    fn = names.get(f, ['?'])[0]
    by[(fn, func_of(fn, l))] += n
total = sum(by.values())
print(f"work units counted {total}")
for (fn, fu), n in by.most_common(25):
    print(f"{n / total:6.3f}  {fn}:{fu}")
