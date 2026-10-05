import sys
sys.path.insert(0, '/mnt/c/Users/vall-/Desktop/projectes/jaos/python')
import jaos
# warmcost.py MPS... : the LP relaxation solved cold, then again from its own optimal basis (no iteration needed),
# and once more from that basis after one integer column's bound moves; work units of each
for f in sys.argv[1:]:
    m = jaos.Model()
    m.read_mps(f)
    n = m.num_col
    for j in range(n):
        m.set_col_integer(j, False)
    m.solve()
    w0, i0 = m.work_units, m.iterations
    cs, rs = m.basis()
    m.set_basis(cs, rs)
    m.solve()
    w1, i1 = m.work_units, m.iterations
    print(f'{f.split("/")[-1]:14s} cols {n:6d} cold {w0:10d} work {i0:6d} it | warm again {w1:9d} work {i1:3d} it', flush=True)
