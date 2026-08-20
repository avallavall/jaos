"""Records every imposed bound so the driver can ask whether it is RESTED AT.

02-87 counted how often a row imposes bounds on two or more of its columns,
which is what §8d's refusal declines. That over-approximates the hazard: the
rank argument breaks only when two of those columns actually **rest at** those
imposed bounds in the final solution. Counting that needs the solve.

So the bounds are kept in an array the driver can read after `jaos_solve`
returns, rather than written to stderr — the published `x` is in original
column indices and so is `j` here, but pairing them through a text stream
would mean emitting a value for every column of every model.

Applies to a COPY of the tree, never to the repository.
"""
import sys

d = sys.argv[1]
p = d + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

ANCHOR = """                if (!moved)
                    continue;"""
assert s.count(ANCHOR) == 1, s.count(ANCHOR)

s = s.replace(ANCHOR, """                if (!moved)
                    continue;
#ifdef JAOS_DIAG
                /* Which end moved is what decides the value to compare the
                 * published x against. Both may have moved on one pass. */
                if (cur_cl[j] != before_lo)
                    jm_diag_impose(i, j, cur_cl[j], cur_rl[i] == cur_ru[i]);
                if (cur_cu[j] != before_hi)
                    jm_diag_impose(i, j, cur_cu[j], cur_rl[i] == cur_ru[i]);
#endif""")

# The pre-tightening values, captured before the two branches can move them.
BEFORE = """                const double ctol = PRESOLVE_TIGHTEN_EPS *"""
assert s.count(BEFORE) == 1
s = s.replace(BEFORE, """#ifdef JAOS_DIAG
                const double before_lo = cur_cl[j], before_hi = cur_cu[j];
#endif
                const double ctol = PRESOLVE_TIGHTEN_EPS *""")

INC = '#include "jaos_internal.h"'
assert s.count(INC) == 1
s = s.replace(INC, INC + """
#ifdef JAOS_DIAG
#include <stdlib.h>
/* Read by the driver after the solve. Not static: that is the whole point. */
struct jm_diag_rec { long long row, col; double val; int eq; };
struct jm_diag_rec *jm_diag_recs = NULL;
long long jm_diag_n = 0, jm_diag_cap = 0, jm_diag_lost = 0;

void jm_diag_impose(long long row, long long col, double val, int eq)
{
    if (jm_diag_n == jm_diag_cap) {
        long long c = jm_diag_cap ? jm_diag_cap * 2 : 1024;
        void *q = realloc(jm_diag_recs, (size_t)c * sizeof *jm_diag_recs);
        if (q == NULL) { jm_diag_lost++; return; }
        jm_diag_recs = q;
        jm_diag_cap = c;
    }
    jm_diag_recs[jm_diag_n].row = row;
    jm_diag_recs[jm_diag_n].col = col;
    jm_diag_recs[jm_diag_n].val = val;
    jm_diag_recs[jm_diag_n].eq  = eq;
    jm_diag_n++;
}
#endif""")

open(p, "w", encoding="utf-8").write(s)
print(f"patched {p}")
