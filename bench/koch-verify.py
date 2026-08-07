"""Check the PostScript extraction against the references already in the
manifest.

The extraction is only worth anything if it reproduces every value the
manifest already carries from Koch. That is 92 of them, independently
transcribed at some earlier point; agreement on all 92 is what makes the two
it is missing -- maros-r7 and pilot87 -- worth taking from the same source.
Disagreement anywhere means the decoding is wrong and nothing here is used.
"""
import sys

extracted = {}
for line in open(sys.argv[1]):
    parts = line.rstrip('\n').split('\t')
    if len(parts) >= 3:
        extracted[parts[0]] = (float(parts[1]), parts[2])

manifest = {}
for line in open(sys.argv[2]):
    if line.startswith('#') or not line.strip():
        continue
    f = line.split()
    if len(f) < 7:
        continue
    manifest[f[0]] = (float(f[4]), f[5])

agree = disagree = missing = 0
for name, (ref, source) in sorted(manifest.items()):
    if source != 'koch':
        continue
    if name not in extracted:
        print('MISSING  %-12s manifest %r' % (name, ref))
        missing += 1
        continue
    got = extracted[name][0]
    if got == ref:
        agree += 1
    else:
        rel = abs(got - ref) / max(1.0, abs(ref))
        print('DIFFER   %-12s manifest %r  extracted %r  rel %.3g'
              % (name, ref, got, rel))
        disagree += 1

print('koch-sourced in manifest: %d agree exactly, %d differ, %d missing'
      % (agree, disagree, missing))

print()
for name in ('maros-r7', 'pilot87'):
    ref, source = manifest[name]
    if name in extracted:
        got, lit = extracted[name]
        rel = abs(got - ref) / max(1.0, abs(ref))
        print('%-10s manifest %r [%s]' % (name, ref, source))
        print('%-10s Koch     %r   (%s)' % ('', got, lit))
        print('%-10s relative difference %.3g' % ('', rel))
    else:
        print('%-10s NOT EXTRACTED' % name)
