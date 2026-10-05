import sys
# rootcmp.py SCALL.csv JALL.txt MANIFEST... : share of the LP-to-optimum gap each root closes, JAOS against SCIP
sc = {}
for ln in open(sys.argv[1]):
    t = ln.strip().split(',')
    if len(t) >= 4 and t[1] and t[2]:
        try:
            sc[t[0]] = (float(t[1]), float(t[2]), t[3])
        except ValueError:
            print(f'{t[0]:22s} SCIP gave {t[1]} / {t[2]}')
jo = {}
for ln in open(sys.argv[2]):
    t = ln.split()
    if len(t) >= 3 and t[0] != 'done':
        try:
            jo[t[0]] = (float(t[1]), t[2], int(t[3]) if len(t) > 3 else 0)
        except ValueError:
            jo[t[0]] = (None, ' '.join(t[1:]), 0)
ref = {}
for f in sys.argv[3:]:
    for ln in open(f):
        if ln.startswith('#') or not ln.strip():
            continue
        t = ln.split()
        ref[t[0]] = float(t[4])
rows = []
for n, r in ref.items():
    if n not in sc or n not in jo or jo[n][0] is None:
        print(f'{n:22s} missing {n in sc} {n in jo}')
        continue
    lp, sroot, sense = sc[n]
    jroot = jo[n][0]
    span = r - lp
    if abs(span) < 1e-9 * (1 + abs(r)):
        continue
    cs = (sroot - lp) / span
    cj = (jroot - lp) / span
    rows.append((cs - cj, n, cs, cj, lp, sroot, jroot, r, jo[n][2]))
rows.sort(reverse=True)
print(f'{"model":22s} {"SCIP closes":>11s} {"JAOS closes":>11s}   LP / SCIP root / JAOS root / optimum, JAOS root work')
for d, n, cs, cj, lp, sroot, jroot, r, w in rows:
    print(f'{n:22s} {cs:11.3f} {cj:11.3f}   {lp:.6g} / {sroot:.6g} / {jroot:.6g} / {r:.6g}, {w:.3g}')
