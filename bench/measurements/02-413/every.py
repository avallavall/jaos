# 02-413's every-push form: the first pin rule of qp_push in
# src/barrier.c pins a variable only when its dual is over MARGIN times its
# slack, in every push. Run from the root of a copy of the tree of 9f69696.
#
#   python3 every.py MARGIN
#
# SPDX-License-Identifier: Apache-2.0
import sys

margin = sys.argv[1]
path = 'src/barrier.c'
text = open(path).read()
for slack, dual in (('w', 'zl'), ('v', 'zu')):
    old = '                           (s->%s[j] < s->%s[j] ||' % (slack, dual)
    assert text.count(old) == 1
    text = text.replace(old, '                           (s->%s[j] * %s < s->%s[j] ||'
                        % (slack, margin, dual))
open(path, 'w').write(text)
