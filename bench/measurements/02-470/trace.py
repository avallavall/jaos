"""Adds three prints to a copy of src/mip.c for 02-470's search reading.

Each new best point of the moves over the disjunctions of big-M rows
prints its objective and the work the run has spent, by a descent step or
by a kick, and the end of each run prints where it started, the start's
and the best objective, the work and the run's budget.

    python3 -I trace.py COPY/src/mip.c

SPDX-License-Identifier: Apache-2.0
"""
import sys

p = sys.argv[1]
s = open(p).read()
spent = "(long long)(*work + ops - (stop - room))"
edits = [
    ("            if (key < kbest - eps) {\n                kbest = key;\n",
     "            if (key < kbest - eps) {\n                kbest = key;\n"
     "                fprintf(stderr, \"BMBEST step z=%.10g work=%lld\\n\", "
     "kbest, " + spent + ");\n"),
    ("                if (key < kbest - 1e-9 * (1.0 + fabs(kbest))) {\n"
     "                    kbest = key;\n",
     "                if (key < kbest - 1e-9 * (1.0 + fabs(kbest))) {\n"
     "                    kbest = key;\n"
     "                    fprintf(stderr, \"BMBEST kick z=%.10g work=%lld\\n\", "
     "kbest, " + spent + ");\n"),
    ("    if (start != nullptr && !(kbest < key0 - 1e-9 * (1.0 + fabs(key0))))\n",
     "    fprintf(stderr, \"BMEND from=%s switches=%lld groups=%lld z0=%.10g "
     "best=%.10g work=%lld room=%lld\\n\", start != nullptr ? (kick_first ? "
     "\"own\" : \"incumbent\") : \"relaxation\", (long long)w.nsw, "
     "(long long)w.ngrp, key0, kbest, " + spent + ", (long long)room);\n"
     "    if (start != nullptr && !(kbest < key0 - 1e-9 * (1.0 + fabs(key0))))\n"),
]
for a, b in edits:
    assert s.count(a) == 1, a
    s = s.replace(a, b)
open(p, 'w').write(s)
