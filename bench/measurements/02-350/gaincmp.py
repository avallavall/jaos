import sys, collections
# gaincmp.py LOG : per column and direction, the mean per-unit gain from probes against the mean from tree children
sb = collections.defaultdict(list); tr = collections.defaultdict(list)
for ln in open(sys.argv[1]):
    t = ln.split()
    if len(t) == 5 and t[0] in ('SBG', 'TRG'):
        key = (int(t[1]), int(t[2]))
        (sb if t[0] == 'SBG' else tr)[key].append(float(t[3]))
both = [k for k in sb if k in tr]
print('probe observations', sum(len(v) for v in sb.values()), 'tree observations', sum(len(v) for v in tr.values()), 'pairs with both', len(both))
ratios = []
zs = zt = 0
for k in both:
    ms = sum(sb[k]) / len(sb[k]); mt = sum(tr[k]) / len(tr[k])
    if ms == 0: zs += 1
    if mt == 0: zt += 1
    if mt > 0 and ms > 0:
        ratios.append(ms / mt)
ratios.sort()
if ratios:
    print('probe mean / tree mean over', len(ratios), 'pairs: median', round(ratios[len(ratios) // 2], 3), 'quartiles', round(ratios[len(ratios) // 4], 3), round(ratios[3 * len(ratios) // 4], 3))
print('pairs with probe mean 0:', zs, 'with tree mean 0:', zt)
allsb = [x for v in sb.values() for x in v]; alltr = [x for v in tr.values() for x in v]
print('share of zero gains: probes', round(sum(1 for x in allsb if x == 0) / max(1, len(allsb)), 3), 'tree', round(sum(1 for x in alltr if x == 0) / max(1, len(alltr)), 3))
print('mean per-unit gain: probes', round(sum(allsb) / max(1, len(allsb)), 5), 'tree', round(sum(alltr) / max(1, len(alltr)), 5))
