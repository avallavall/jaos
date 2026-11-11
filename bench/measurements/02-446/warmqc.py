"""02-427's warmqc.c for a solver that can publish an optimum with no
finite duals: such an optimum (duals all NaN, so the checker judges the
point alone) is counted, not failed.

    python3 warmqc.py IN.c OUT.c

SPDX-License-Identifier: Apache-2.0
"""
import sys

s = open(sys.argv[1]).read()


def rep(old, new):
    global s
    assert s.count(old) == 1, (old[:50], s.count(old))
    s = s.replace(old, new)


rep("""    int64_t bare = 0;
""", """    int64_t bare = 0, dualless = 0;
""")
rep("""                if (fabs(wo - co) > 1e-6 * (1.0 + fabs(co)) || !integral ||
                    !ck.primal_feasible || (!mip && !ck.dual_feasible)) {""",
    """                if (!mip && !ck.checked_duals)
                    dualless++;
                if (fabs(wo - co) > 1e-6 * (1.0 + fabs(co)) || !integral ||
                    !ck.primal_feasible ||
                    (!mip && ck.checked_duals && !ck.dual_feasible)) {""")
rep("""           " other %" PRId64 "; infeasible with no certificate %" PRId64 "\\n",
           runs, runs * edits, verdicts[0], verdicts[1], verdicts[2], capped,
           other, bare);""",
    """           " other %" PRId64 "; infeasible with no certificate %" PRId64
           "; optimal with no duals %" PRId64 "\\n",
           runs, runs * edits, verdicts[0], verdicts[1], verdicts[2], capped,
           other, bare, dualless);""")
open(sys.argv[2], "w").write(s)
