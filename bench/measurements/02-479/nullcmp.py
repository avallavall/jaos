# SPDX-License-Identifier: Apache-2.0
# Per MIPLIB 2017 instance: the record's and a reading's primal and dual
# gaps (02-328's scoring), and the range 02-474's five null readings span.
#   nullcmp.py record.txt reading.txt null1.txt ...
import re
import sys


def load(p):
    d = {}
    for ln in open(p):
        m = re.match(r'^(\S+)\s+(.*?)\s+rows=', ln)
        if not m:
            continue
        kv = dict(x.split('=', 1) for x in ln.split() if '=' in x)
        kv['ref'] = kv.get('ref', '').split('[')[0]
        d[m.group(1)] = (m.group(2), kv)
    return d


def num(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def score(st, kv):
    r = num(kv['ref'])
    s = max(1.0, abs(r))
    if st.startswith('optimal'):
        return 0.0, 0.0
    i = num(kv.get('inc', 'none'))
    b = num(kv.get('bound', ''))
    pg = 1.0 if i is None else min(1.0, abs(i - r) / s)
    dg = 1.0 if b is None else min(1.0, abs(r - b) / s)
    return pg, dg


rec, mine = load(sys.argv[1]), load(sys.argv[2])
nulls = [load(p) for p in sys.argv[3:]]
print(f"{'instance':22s} {'record':>15s} {'reading':>15s} {'nulls primal':>15s} {'nulls dual':>15s}  nodes record, reading")
for k, (st, kv) in rec.items():
    if k not in mine:
        continue
    a = score(st, kv)
    b = score(*mine[k])
    ns = [score(*n[k]) for n in nulls if k in n]
    pr = f"{min(x[0] for x in ns):.4f}-{max(x[0] for x in ns):.4f}" if ns else '-'
    du = f"{min(x[1] for x in ns):.4f}-{max(x[1] for x in ns):.4f}" if ns else '-'
    flag = '' if a == b else ' *'
    print(f"{k:22s} {a[0]:.4f} {a[1]:.4f}   {b[0]:.4f} {b[1]:.4f} {pr:>15s} {du:>15s}  "
          f"{kv.get('nodes')}, {mine[k][1].get('nodes')}{flag}")
