"""Adds three prints to a copy of src/mip.c for 02-472's budget reading.

The detector prints the jobs, items and chain positions it found; each
round's point prints its objective, the work spent so far and the budget;
the end of the run prints the work and the budget.

    python3 -I trace.py COPY/src/mip.c

SPDX-License-Identifier: Apache-2.0
"""
import sys

p = sys.argv[1]
s = open(p).read()
edits = [
    ("    if (!sc_shape(m, ilo, ihi, xlp, tol, &w, &ops))\n        goto out;\n",
     "    if (!sc_shape(m, ilo, ihi, xlp, tol, &w, &ops))\n        goto out;\n"
     "    fprintf(stderr, \"SCSHAPE jobs=%lld items=%lld chain=%lld\\n\", "
     "(long long)w.nj, (long long)w.ni, (long long)w.len);\n"),
    ("        ops += nc;\n        if (isfinite(zbest) && !(z < zbest",
     "        ops += nc;\n"
     "        fprintf(stderr, \"SCROUND z=%.17g ops=%lld budget=%lld\\n\", z, "
     "(long long)ops, (long long)budget_ops);\n"
     "        if (isfinite(zbest) && !(z < zbest"),
    ("    rc = have ? 1 : 0;\nout:\n",
     "    rc = have ? 1 : 0;\n"
     "    fprintf(stderr, \"SCEND ops=%lld budget=%lld\\n\", (long long)ops, "
     "(long long)budget_ops);\nout:\n"),
]
for a, b in edits:
    assert s.count(a) == 1, a
    s = s.replace(a, b)
open(p, 'w').write(s)
