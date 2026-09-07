"""How often a proved MIP incumbent publishes a basis of the wrong size.

`numerics-reviewer` found that `incumbent_take` copies the node LP's
statuses truncated to the caller's own rows, which drops a cut row's
status but keeps the basic it paid for, so the count comes out one too
high per binding cut. The argument is structural; this is the count.

`jaos_basis` refuses a vector whose count is wrong since D330, so the
refusal IS the measurement: an instance whose basis reads back is one
where the count came out right.
"""
import os
import sys
import time

sys.path.insert(0, 'python')
import jaos  # noqa: E402

names = [ln.split()[0] for ln in open('bench/miplib.manifest')
         if ln.strip() and not ln.startswith('#')]

ok = bad = skipped = 0
for n in names:
    path = 'bench/instances-miplib/%s.mps' % n
    if not os.path.exists(path):
        skipped += 1
        continue
    m = jaos.Model()
    m.read_mps(path)
    t = time.time()
    st = m.solve()
    if st is not jaos.SolveStatus.OPTIMAL:
        print('%-12s %-12s skipped' % (n, st.name))
        skipped += 1
        continue
    try:
        b = m.basis()
        nb = sum(s is jaos.BasisStatus.BASIC
                 for s in list(b.col_status) + list(b.row_status))
        print('%-12s basis of %d basics for %d rows  (%.1fs)'
              % (n, nb, m.num_row, time.time() - t))
        ok += 1
    except jaos.JaosError:
        print('%-12s basis REFUSED: the count is not %d  (%.1fs)'
              % (n, m.num_row, time.time() - t))
        bad += 1

print()
print('proved incumbents publishing a basis of the right size: %d' % ok)
print('publishing one of the wrong size, and so refused       : %d' % bad)
print('not measured (no file, or not solved to optimality)    : %d' % skipped)
