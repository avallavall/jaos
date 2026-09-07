"""D333: does the exact Farkas ray close the eleven D328 could not?

D328 wrote the published doubles into the proof file and measured 18 of
the 29 pinned infeasibilities certifying with no tolerance at all. Every
one of the eleven failures was a single column whose (A'y)_j sat a
rounding away from zero with no finite bound on the side it pointed at.

`jaos_exact_certificate` solves the same system over the rationals from
the basis the refusal stopped on, which is readable since D330. Four
columns per instance:

  basis      whether a basis was published at all (D330: 19 of 29)
  derived    whether the exact derivation fitted the limb budget
  doubles    whether a file of the published doubles certifies
  shipped    whether the file `jaos_write_proof` actually writes does

`shipped` is the one that matters and the first version of this script
did not measure it: `write_proof` falls back to the doubles when there is
no derivation, so an instance whose derivation refuses keeps whatever it
had. Reading the exact column alone counted those as losses, which is not
what the library does.

`jaos_check_proof` judges every file, over the rationals, with no
tolerance, and shares no code with the derivation.
"""
import glob
import os
import sys
import tempfile

sys.path.insert(0, 'python')
import jaos  # noqa: E402

tmp = tempfile.mkdtemp()
n_basis = n_derived = n_doubles = n_shipped = 0
rows = []
for path in sorted(glob.glob('bench/instances-infeas/*.mps')):
    name = os.path.basename(path)[:-4]

    # The doubles, on their own model, which is what D328 measured.
    m = jaos.Model()
    m.read_mps(path)
    m.solve()
    doubles = has_basis = False
    try:
        p0 = os.path.join(tmp, name + '-0.proof')
        m.write_proof(p0)
        doubles = m.check_proof(p0).certified
    except jaos.JaosError:
        pass
    try:
        m.basis()
        has_basis = True
    except jaos.JaosError:
        pass

    # What ships: derive where it fits, then write, whichever the writer
    # then puts in the file.
    c = jaos.Model()
    c.read_mps(path)
    c.solve()
    derived = shipped = False
    bits = cap = 0.0
    try:
        rep = c.exact_certificate()
        derived = rep.derived
        bits, cap = rep.bound_bits, rep.capacity_bits
    except jaos.JaosError:
        pass
    try:
        p1 = os.path.join(tmp, name + '-1.proof')
        c.write_proof(p1)
        shipped = c.check_proof(p1).certified
    except jaos.JaosError:
        pass

    n_basis += has_basis
    n_derived += derived
    n_doubles += doubles
    n_shipped += shipped
    rows.append((name, has_basis, derived, doubles, shipped, bits, cap))
    print('%-10s basis=%-5s derived=%-5s doubles=%-5s shipped=%-5s '
          'bound=%.0f/%.0f'
          % (name, has_basis, derived, doubles, shipped, bits, cap))

print()
print('publish a basis                 : %d of %d' % (n_basis, len(rows)))
print('exact derivation fits the budget: %d of %d' % (n_derived, len(rows)))
print('certify on the published doubles: %d of %d' % (n_doubles, len(rows)))
print('certify on what the writer emits: %d of %d' % (n_shipped, len(rows)))
d_ok = sum(1 for r in rows if r[2] and r[4])
print('derivations that fit AND certify: %d of %d' % (d_ok, n_derived))
gained = [r[0] for r in rows if r[4] and not r[3]]
lost = [r[0] for r in rows if r[3] and not r[4]]
print('closed by the exact ray         : %s' % (', '.join(gained) or 'none'))
print('lost                            : %s' % (', '.join(lost) or 'none'))
