# SPDX-License-Identifier: Apache-2.0
import sys
src, dst = sys.argv[1], sys.argv[2]
out = []
for line in open(src):
    if "'MARKER'" in line:
        continue
    out.append(line)
    if line.startswith('COLUMNS'):
        out.append("    INT0      'MARKER'                 'INTORG'\n")
text = ''.join(out)
k = text.index('    C0027 ')
text = text[:k] + "    INT0END   'MARKER'                 'INTEND'\n" + text[k:]
open(dst, 'w').write(text)
