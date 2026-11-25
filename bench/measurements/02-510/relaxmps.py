# relaxmps.py IN OUT — the MPS with its integer markers dropped and BV/LI/UI bounds made continuous
import sys
src, dst = sys.argv[1], sys.argv[2]
out = open(dst, 'w')
sec = None
for ln in open(src):
    t = ln.split()
    if ln[:1] not in (' ', '\t', '*') and t:
        sec = t[0]
    if sec == 'COLUMNS' and len(t) >= 3 and t[1] == "'MARKER'":
        continue
    if sec == 'BOUNDS' and t and ln[:1] in (' ', '\t'):
        if t[0] == 'BV':
            out.write(' UP %s %s 1\n' % (t[1], t[2]))
            out.write(' LO %s %s 0\n' % (t[1], t[2]))
            continue
        if t[0] == 'LI':
            t[0] = 'LO'; out.write(' ' + ' '.join(t) + '\n'); continue
        if t[0] == 'UI':
            t[0] = 'UP'; out.write(' ' + ' '.join(t) + '\n'); continue
    out.write(ln)
