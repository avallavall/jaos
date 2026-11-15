# Compares read.sh's records across HEAD and the three values of
# CONIC_FINISH_REG: per seed, every count that differs, the brute-force
# unsure and failed counts always, and the UNSURE lines.
#
#   python3 summary.py DIR
#
# SPDX-License-Identifier: Apache-2.0
import re
import sys

D = sys.argv[1]
VALS = ['head', '1e-9', '1e-11', '1e-13']
TREE = ['optimal', 'infeasible', 'unbounded', 'brute force unsure',
        'tree numerical errors', 'worst objective gap', 'worst violation',
        'nodes', 'work units', 'failed models']
KEYS = {
    'misocp': TREE,
    'discrete': TREE,
    'conic': ['checker failures', 'copy failures', 'mps failures',
              'rotated-form failures', 'worst dual violation',
              'worst row violation', 'worst rotated-form gap', 'work units',
              'iterations', 'failed models'],
}
ALWAYS = ('brute force unsure', 'failed models', 'checker failures')


def parse(path, keys):
    seeds, cur = {}, None
    for line in open(path):
        m = re.match(r'== seed (\d+)', line)
        if m:
            cur = m.group(1)
            seeds[cur] = {}
            continue
        if cur is None:
            continue
        for k in keys:
            if line.startswith(k + ' '):
                seeds[cur][k] = line[len(k) + 1:].strip()
        if line.startswith('UNSURE'):
            seeds[cur].setdefault('UNSURE', []).append(line.strip()[:110])
    return seeds


print('values: ' + ' | '.join(VALS))
for name, keys in KEYS.items():
    data = {v: parse(f'{D}/{name}-{v}.txt', keys) for v in VALS}
    print(f'#### {name}')
    for seed in data['head']:
        print(f'seed {seed}')
        for k in keys:
            got = [data[v].get(seed, {}).get(k, '?') for v in VALS]
            if len(set(got)) > 1 or k in ALWAYS:
                print(f'   {k:24s} ' + ' | '.join(f'{g:>12s}' for g in got))
        for v in VALS:
            for u in data[v].get(seed, {}).get('UNSURE', []):
                print(f'   [{v}] {u}')
