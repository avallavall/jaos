"""What D334's re-solve costs, per instance, over the MIP set.

The runner's own line reads `0 regressed, 0 improved, 0 new`, which only
means no predicate flipped and no instance passed 2.0x work. This is the
cost itself: the baseline's work against the new record's, and the
iterations and node counts beside them, because a repair that runs after
the tree must not move either.

The two files do not share a column layout -- the baseline writes bare
values and the record writes `name=value` -- so each is read by its own
rule rather than by position in both.
"""
import math

BASE = 'bench/miplib.baseline'
NEW = 'bench/results/miplib.txt'


def baseline():
    """name status solved shape objective checker det iters work ? nodes"""
    out = {}
    for ln in open(BASE, encoding='utf-8'):
        ln = ln.strip()
        if not ln or ln.startswith('#'):
            continue
        f = ln.split()
        out[f[0]] = {'iters': int(f[7]), 'work': int(f[8]),
                     'nodes': int(f[10])}
    return out


def record():
    out = {}
    for ln in open(NEW, encoding='utf-8'):
        ln = ln.strip()
        if not ln or ln.startswith('#') or '=' not in ln:
            continue
        f = ln.split()
        kv = dict(p.split('=', 1) for p in f if '=' in p)
        if 'iters' not in kv:
            continue
        out[f[0]] = {'iters': int(kv['iters']), 'work': int(kv['work']),
                     'nodes': int(kv['nodes'])}
    return out


a, b = baseline(), record()
assert set(a) == set(b), 'the two records name different instances'

logs = []
moved = unchanged = 0
bad = []
for n in sorted(a):
    r = b[n]['work'] / a[n]['work']
    logs.append(math.log(r))
    if b[n]['work'] == a[n]['work']:
        unchanged += 1
    else:
        moved += 1
    if a[n]['iters'] != b[n]['iters'] or a[n]['nodes'] != b[n]['nodes']:
        bad.append(n)
    print('%-12s work %.6fx   iters %d -> %d   nodes %d -> %d'
          % (n, r, a[n]['iters'], b[n]['iters'], a[n]['nodes'], b[n]['nodes']))

print()
print('geometric mean of the work ratios: %.6fx' % math.exp(sum(logs) /
                                                            len(logs)))
print('largest single ratio             : %.6fx'
      % max(b[n]['work'] / a[n]['work'] for n in a))
print('instances whose work did not move: %d of %d' % (unchanged, len(a)))
print('instances whose tree moved       : %s' % (', '.join(bad) or 'none'))
