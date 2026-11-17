# SPDX-License-Identifier: Apache-2.0
import sys
src, dst = sys.argv[1], sys.argv[2]
out = []
for line in open(src):
    if "'INTORG'" in line:
        continue
    out.append(line)
    if line.startswith('COLUMNS'):
        out.append("    INT0      'MARKER'                 'INTORG'\n")
open(dst, 'w').writelines(out)
