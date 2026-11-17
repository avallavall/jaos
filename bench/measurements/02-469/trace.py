"""Adds two prints to a copy of src/mip.c for 02-469's budget reading.

Each new best point of the threshold accepting over the items prints its
objective and the work spent so far, and the end of the run prints the
work, the budget and the first threshold.

    python3 -I trace.py COPY/src/mip.c

SPDX-License-Identifier: Apache-2.0
"""
import sys

p = sys.argv[1]
s = open(p).read()
a = "            zbest = w.z;\n            have = true;\n        }\n    }\n"
assert s.count(a) == 1
b = ("            zbest = w.z;\n            have = true;\n"
     "            fprintf(stderr, \"ANBEST z=%.6g ops=%lld budget=%lld\\n\", "
     "zbest, (long long)ops, (long long)budget_ops);\n"
     "        }\n    }\n"
     "    fprintf(stderr, \"ANEND ops=%lld budget=%lld T0=%.4g nlive=%lld "
     "nopt=%lld\\n\", (long long)ops, (long long)budget_ops, lev[0], "
     "(long long)w.nlive, (long long)w.nopt);\n")
open(p, 'w').write(s.replace(a, b))
