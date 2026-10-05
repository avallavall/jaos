import sys
# paircmp.py LOG : match each tree child to the probe of the same column, direction and parent bound
prb = {}
pairs = []
for ln in open(sys.argv[1]):
    t = ln.split()
    if len(t) == 6 and t[0] == 'PRB':
        prb[(t[1], t[2], t[3])] = (float(t[4]), int(t[5]))
    elif len(t) == 6 and t[0] == 'CHD':
        k = (t[1], t[2], t[3])
        if k in prb:
            pk = float(t[3]); po, prow = prb[k]; ck = float(t[4]); crow = int(t[5])
            pairs.append((po - pk, ck - pk, prow, crow))
print('pairs', len(pairs))
if pairs:
    eq = sum(1 for a, b, _, _ in pairs if abs(a - b) <= 1e-7 * (1 + abs(b)))
    hi = sum(1 for a, b, _, _ in pairs if b > a + 1e-7 * (1 + abs(b)))
    lo = sum(1 for a, b, _, _ in pairs if b < a - 1e-7 * (1 + abs(b)))
    print('child gain equal to probe gain', eq, 'child higher', hi, 'child lower', lo)
    rows_less = sum(1 for _, _, pr, cr in pairs if cr < pr)
    print('child LP with fewer rows than the probe LP', rows_less)
    for a, b, pr, cr in pairs[:12]:
        print(f'  probe gain {a:.6g} child gain {b:.6g} probe rows {pr} child rows {cr}')
