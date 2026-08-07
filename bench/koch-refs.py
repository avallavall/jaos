"""Extract Koch's exact optima (ZR-03-05) from the report's PostScript.

A dev-time tool. Nothing builds it, nothing links it, and the library does
not depend on Python — it is here so the two reference values this manifest
takes from Koch's tables rather than from a machine-readable dataset can be
re-derived by anyone, instead of being a number somebody typed in.

    curl -O https://opus4.kobv.de/opus4-zib/files/727/ZR-03-05.ps
    python3 bench/koch-refs.py ZR-03-05.ps bench/netlib.manifest > refs.txt
    python3 bench/koch-verify.py refs.txt bench/netlib.manifest

The exact rationals Koch published at zib.de/koch/perplex are no longer
reachable at that path, and parsing the report's PDF reproduced only 23 of
the 92 values already known to be his — not enough to set ground truth with.
The PostScript of the same report is a different matter, and it is what this
reads.

The PostScript is dvips output and carries the whole table as literal
strings, so nothing but the typesetting has to be undone:

  Fc(\\000)  the minus sign      Fa(:)  the decimal point
  Fq(n)      the superscript     kerning splits names and mantissas across
                                 consecutive strings

The instance names are taken from the manifest and used only as an index --
which strings to join back together, since kerning splits `pilot-we` into
`(pilot-w)n(e)` and `25fv47` into `(25)(fv47)`. Every *value* is read from
the PostScript alone.

Nothing here is trusted on its own: the values are checked against the ones
the manifest already carries from Koch, and only agreement on all of them
makes the two it is missing worth taking.
"""
import re
import sys

MINUS = 'Fc(' + chr(92) + '000)'
STRING = re.compile(r'\(([^()]*)\)')

PS_PATH, MANIFEST = sys.argv[1], sys.argv[2]

names = set()
for line in open(MANIFEST):
    if line.startswith('#') or not line.strip():
        continue
    names.add(line.split()[0])

ps = open(PS_PATH, 'rb').read().decode('latin-1')

out = {}
for m in re.finditer(r'\by\(', ps):
    seg = ps[m.end() - 1:m.end() - 1 + 1500]

    # Join consecutive strings until they spell an instance name.
    joined = ''
    start = None
    for k, p in enumerate(STRING.finditer(seg)):
        joined += p.group(1)
        if joined in names:
            start = p.end()
            break
        if k >= 3:
            break
    if start is None:
        continue
    name = joined

    end = seg.find('Fq(', start)
    if end < 0:
        continue
    close = seg.find(')', end)
    exp = seg[end + 3:close]
    if not re.fullmatch(r'-?[0-9]+', exp):
        continue

    body = seg[start:end]
    if 'Fa(:)' not in body:
        continue
    digits = ''.join(re.findall(r'\(([0-9]+)\)', body.split('Fa(:)', 1)[1]))
    if not digits:
        continue

    lit = ('-' if MINUS in body else '') + '0.' + digits + 'e' + exp
    out.setdefault(name, (float(lit), lit))

print('extracted %d instances' % len(out), file=sys.stderr)
for k in sorted(out):
    print('%s\t%r\t%s' % (k, out[k][0], out[k][1]))
