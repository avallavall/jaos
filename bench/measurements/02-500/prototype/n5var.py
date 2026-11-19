import sys
src, dst, mode = sys.argv[1], sys.argv[2], sys.argv[3]
L = open(src).read().split('\n')
out = []
sec = None
inint = False
intcols = []
allcols = []
for l in L:
    if l.strip() and not l.startswith(' '):
        sec = l.split()[0]
        if sec == 'COLUMNS':
            out.append(l)
            continue
        if sec == 'RHS' and mode in ('w', 'wc'):
            out.append("    MARKW     'MARKER'                 'INTORG'")
            out.append("    W         RW                   1")
            out.append("    MARKWE    'MARKER'                 'INTEND'")
            if mode == 'wc':
                out.append("    V         RV                   1")
        if sec == 'BOUNDS' and False:
            pass
        out.append(l)
        if sec == 'ROWS' and mode in ('w', 'wc'):
            out.append(" E  RW")
            if mode == 'wc':
                out.append(" E  RV")
        continue
    if sec == 'COLUMNS' and l.strip():
        if 'MARKER' in l:
            inint = 'INTORG' in l
            if mode == 'allint':
                continue
            out.append(l); continue
        c = l.split()[0]
        if not allcols or allcols[-1] != c:
            allcols.append(c)
            if inint: intcols.append(c)
            if mode in ('w', 'wc'):
                if inint:
                    out.append("    %-8s  RW                  -1" % c)
                elif mode == 'wc':
                    out.append("    %-8s  RV                  -1" % c)
        out.append(l)
        continue
    out.append(l)
if mode == 'allint':
    i = out.index('COLUMNS')
    out.insert(i + 1, "    MARKA     'MARKER'                 'INTORG'")
    j = out.index('RHS')
    out.insert(j, "    MARKAE    'MARKER'                 'INTEND'")
open(dst, 'w').write('\n'.join(out))
print(len(allcols), len(intcols))
