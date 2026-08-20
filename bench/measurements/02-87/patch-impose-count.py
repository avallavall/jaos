"""Counts what the §8d refusal would cost, in a COPY of an old tree.

`docs/research/dual-postsolve-imposed-bound.md` §8d proposes the refusal a
first version of D97 should carry: **do not impose a bound from a row that
already imposed one on another of its columns.** The rank argument in §8c
holds for one imposed bound per row and breaks for two.

Nobody has counted how much that refusal throws away. This does, at the
activity-tightening site itself.

**The tree is `7c7375c`, where the tightening exists.** It is the minimal
failing design D114 later refused, so the numbers below describe THAT
tightening and not a corrected one. What they are good for is the order of
the collision rate, which is what decides whether the refusal is cheap or
guts the family.

One `write(2)` per record: `bench/run` forks children onto one stderr and a
single `fprintf` with many conversions tears.
"""
import sys

d = sys.argv[1]
p = d + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

ANCHOR = """                if (!moved)
                    continue;"""

assert s.count(ANCHOR) == 1, s.count(ANCHOR)

HOOK = """                if (!moved)
                    continue;
#ifdef JAOS_DIAG
                {
                    /* One record per imposed bound: the row, the column, and
                     * whether the row is an equality — §8d argues the
                     * configuration that breaks the rank argument forces
                     * one. The analysis groups by row. */
                    char b[256];
                    int n = snprintf(b, sizeof b,
                        "IMPOSE row=%lld col=%lld a=%.17g rl=%.17g ru=%.17g "
                        "eq=%d\\n",
                        (long long)i, (long long)j, a, cur_rl[i], cur_ru[i],
                        (cur_rl[i] == cur_ru[i]) ? 1 : 0);
                    if (n > 0)
                        (void)!write(2, b, (size_t)n);
                }
#endif"""

s = s.replace(ANCHOR, HOOK)

# The headers the hook needs, added once at the top of the file.
INC = '#include "jaos_internal.h"'
assert s.count(INC) == 1
s = s.replace(INC, INC + """
#ifdef JAOS_DIAG
#include <unistd.h>
#include <stdio.h>
#endif""")

open(p, "w", encoding="utf-8").write(s)
print(f"patched {p}")
