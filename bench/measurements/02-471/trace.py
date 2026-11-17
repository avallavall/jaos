"""Adds prints to a copy of src/mip.c for 02-471's budget reading.

The search over the items with an LP over the linking columns prints the
work and the LPs spent when its first phase ends, each new best point of
its second phase with the work spent so far, and the end of the run with
the work, the budget and the LP's size.

    python3 -I trace.py COPY/src/mip.c

SPDX-License-Identifier: Apache-2.0
"""
import sys

p = sys.argv[1]
s = open(p).read()
edits = [
    ("static int lk_solve(",
     "static int64_t lk_trace_lps;\n\nstatic int lk_solve("),
    ("    *ops += jaos_work_units(r);\n",
     "    *ops += jaos_work_units(r);\n    lk_trace_lps++;\n"),
    ("    if (!feas)\n        goto out;\n    double z = zi + zr, zbest = z;\n",
     "    fprintf(stderr, \"LKPHASE1 feasible=%d ops=%lld lps=%lld violation=%.6g\\n\", "
     "(int)feas, (long long)ops, (long long)lk_trace_lps, feas ? 0.0 : v);\n"
     "    if (!feas)\n        goto out;\n    double z = zi + zr, zbest = z;\n"
     "    fprintf(stderr, \"LKFIRST z=%.10g from_inc=%d\\n\", z, (int)from_inc);\n"),
    ("            ops += w.nlive;\n            zbest = z;\n        }\n    }\n",
     "            ops += w.nlive;\n            zbest = z;\n"
     "            fprintf(stderr, \"LKBEST z=%.10g ops=%lld lps=%lld budget=%lld\\n\", "
     "zbest, (long long)ops, (long long)lk_trace_lps, (long long)budget_ops);\n"
     "        }\n    }\n"
     "    fprintf(stderr, \"LKEND ops=%lld lps=%lld budget=%lld T0=%.4g nlive=%lld "
     "lpcols=%lld lpint=%lld lprows=%lld\\n\", (long long)ops, (long long)lk_trace_lps, "
     "(long long)budget_ops, lev[0], (long long)w.nlive, (long long)k.ncol, "
     "(long long)k.nint, (long long)k.nrow);\n"),
]
for a, b in edits:
    assert s.count(a) == 1, a
    s = s.replace(a, b)
open(p, 'w').write(s)
