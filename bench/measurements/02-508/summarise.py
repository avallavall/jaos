import re, sys, os

def read(path):
    root = None
    rounds = []
    sep = {}
    lpline = None
    final = {}
    for ln in open(path, errors='replace'):
        m = re.match(r'round 0 root: rows (\d+) nz (\d+) iters (\d+) work (\d+) secs ([\d.]+) bound (\S+)', ln)
        if m:
            root = dict(rows=int(m[1]), nz=int(m[2]), iters=int(m[3]),
                        work=int(m[4]), secs=float(m[5]), bound=float(m[6]))
            continue
        m = re.match(r'round (\d+) separate: (.*) work (\d+) secs', ln)
        if m:
            sep[int(m[1])] = m[2]
            continue
        m = re.match(r'.* after (\d+) iterations, (\d+) work units; (\d+) refactorizations, (\d+) weight restarts', ln)
        if m:
            lpline = (int(m[3]), int(m[4]))
            continue
        m = re.match(r'round (\d+) (\w+): added (\d+) rows (\d+) nz (\d+) \(\+(-?\d+)\) iters (\d+) primal (\d+) phase1 (\d+) work (\d+) secs ([\d.]+) status (.*) bound (\S+)', ln)
        if m:
            refac, restarts = lpline if lpline else (-1, -1)
            lpline = None
            rounds.append(dict(r=int(m[1]), what=m[2], added=int(m[3]),
                               rows=int(m[4]), nz=int(m[5]), dnz=int(m[6]),
                               iters=int(m[7]), primal=int(m[8]),
                               work=int(m[10]), secs=float(m[11]),
                               status=m[12], bound=m[13], refac=refac,
                               restarts=restarts))
            continue
        m = re.match(r'^(status|work_units|bound|nodes|iterations|cuts|time) (\S+)', ln)
        if m and m[1] not in final:
            final[m[1]] = m[2]
    return root, rounds, sep, final

def main():
    d = sys.argv[1]
    detail = len(sys.argv) > 2 and sys.argv[2] == '-v'
    names = sorted(f[:-4] for f in os.listdir(d) if f.endswith('.log'))
    print('# instance: root LP; then each re-solve: round kind cuts nz-added(x root LP nz) iters restarts refactorizations work status bound')
    for n in names:
        root, rounds, sep, final = read(os.path.join(d, n + '.log'))
        if root is None:
            print(f'{n}: no root line; status {final.get("status")}')
            continue
        tot_it = sum(x['iters'] for x in rounds)
        tot_w = sum(x['work'] for x in rounds)
        tot_rs = sum(max(x['restarts'], 0) for x in rounds)
        last = rounds[-1] if rounds else None
        print(f'{n}: root LP {root["iters"]} iters, {root["work"]:.3e} work, {root["nz"]} nz, bound {root["bound"]:.10g}; '
              f'{len(rounds)} re-solves, {tot_it} iters, {tot_rs} restarts, {tot_w:.3e} work; '
              f'last {last["what"] + " " + last["status"] if last else "-"}; '
              f'end {final.get("status")} work {final.get("work_units")} bound {final.get("bound")} nodes {final.get("nodes")}')
        if detail:
            for x in rounds:
                print(f'  r{x["r"]:<3} {x["what"]:<5} +{x["added"]:<5} nz+{x["dnz"]:<8} ({x["dnz"] / root["nz"]:.2f}x) '
                      f'it {x["iters"]:<7} rs {x["restarts"]:<7} rf {x["refac"]:<5} w {x["work"]:.3e} {x["status"]:<20} {x["bound"]}')

main()
