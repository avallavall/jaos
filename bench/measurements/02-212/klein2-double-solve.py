"""Is klein2's warm re-solve from its own infeasible basis new with D330?

D330 publishes a basis a non-optimal solve stopped on. It does not create
one: `jm_model_remember_basis` has stored exactly that basis on every
INFEASIBLE answer since long before, and a second `jaos_solve` on the same
model has always started from it. So solving klein2 twice in one process
should behave the same way the file route does, and that is what this
asks. If it does, the finding is a property of the model and not of D330.
"""
import sys

sys.path.insert(0, 'python')
import jaos  # noqa: E402

m = jaos.Model()
m.read_mps('bench/instances-infeas/klein2.mps')
print('first  :', m.solve().name, 'iterations', m.iterations)
try:
    print('second :', m.solve().name, 'iterations', m.iterations)
except jaos.JaosError as e:
    print('second : raised', e)
