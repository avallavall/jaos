import os, random, re, subprocess, sys
# flipsearch.py N : random tiny assignment models with overflow columns; report those where
# the MIR bound flip (JAOS_MIRD=6) changes the root relaxation or the node count
J = os.path.expanduser('~/jaos-y/build/cli/jaos')
D = os.path.expanduser('~/flips')
os.makedirs(D, exist_ok=True)
OPTS = ['--cut-rounds', '0', '--cover-rounds', '0', '--clique-rounds', '0', '--hull-rounds', '0',
        '--flow-cover-rounds', '0', '--zero-half-rounds', '0', '--no-heuristics', '--cut-depth', '0',
        '--log', 'summary']

def model(rng, items, bins):
    a = [rng.randint(3, 9) for _ in range(items)]
    cap = [rng.randint(6, 12) + 0.5 for _ in range(bins)]
    lines = ['Minimize', ' obj: ' + ' + '.join(f's{j}' for j in range(bins)), 'Subject To']
    for i in range(items):
        lines.append(f' one{i}: ' + ' + '.join(f'x{i}_{j}' for j in range(bins)) + ' = 1')
    for j in range(bins):
        lines.append(f' cap{j}: ' + ' + '.join(f'{a[i]} x{i}_{j}' for i in range(items)) + f' - s{j} <= {cap[j]}')
    lines.append('Binary')
    lines.append(' ' + ' '.join(f'x{i}_{j}' for i in range(items) for j in range(bins)))
    lines.append('End')
    return '\n'.join(lines) + '\n'

def run(path, mird):
    env = dict(os.environ, JAOS_MIRD=str(mird))
    p = subprocess.run([J, 'solve', path] + OPTS, capture_output=True, text=True, env=env)
    out = p.stdout + p.stderr
    rel = re.search(r'root: relaxation (\S+)', out)
    nodes = re.search(r'^nodes (\d+)', out, re.M)
    obj = re.search(r'^objective (\S+)', out, re.M)
    return (float(rel.group(1)) if rel else None, int(nodes.group(1)) if nodes else None,
            float(obj.group(1)) if obj else None)

rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
found = 0
for k in range(int(sys.argv[1])):
    items, bins = rng.randint(4, 7), rng.randint(2, 3)
    path = f'{D}/m{k}.lp'
    open(path, 'w').write(model(rng, items, bins))
    r0, r6 = run(path, 0), run(path, 6)
    if r0[2] is None or r6[2] is None or abs(r0[2] - r6[2]) > 1e-6:
        print('ANSWER DIFFERS', k, r0, r6)
        continue
    if r6[1] == 1 and r0[1] > 1 and r6[0] > r0[0] + 1e-3:
        found += 1
        print(k, items, bins, 'off', r0, 'on', r6)
        if found >= 12:
            break
