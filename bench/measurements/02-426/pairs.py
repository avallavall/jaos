"""Two `pairs.patch` records of one seed, compared pair by pair.

A model's edits draw their values near the last answer's point, so after
the two builds answer one edit differently, that model's later edits
differ. Only pairs whose edits so far are the same text are compared.
"""
import sys
from collections import Counter


def load(path):
    out = {}
    for line in open(path):
        if not line.startswith('PAIR '):
            continue
        head, how = line.rstrip('\n').split(' | ', 1)
        t = head.split(' ', 3)
        r, s = int(t[1]), int(t[2])
        rest = t[3]
        out.setdefault(r, []).append((s, rest, how))
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
same = 0
moves = Counter()
examples = {}
for r in sorted(set(a) | set(b)):
    pa, pb = a.get(r, []), b.get(r, [])
    for (sa, ra, ha), (sb, rb, hb) in zip(pa, pb):
        if ha != hb:
            break
        if ra == rb:
            same += 1
            continue
        key = (' '.join(ra.split()[:-1]), ' '.join(rb.split()[:-1]))
        moves[key] += 1
        examples.setdefault(key, []).append((r, sa, ra.split()[-1], rb.split()[-1], ha))
print('pairs with the same history and the same answer', same)
for k, v in moves.most_common():
    print(v, 'HEAD', k[0], '-> change', k[1])
    for e in examples[k][:8]:
        print('    model %d edit %d: %s -> %s (%s)' % e)
