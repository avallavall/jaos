"""What gran's 588 moves actually are, before anything is repaired.

The oracle read gran INFEASIBLE after its moves were applied, and it is
the only one of the 29 that did. Two explanations fit and they want
different repairs, so this separates them: the moves are mostly noise
below the tolerance the solve judges feasibility by, or they are real and
the relaxation is wrong. The histogram says which.
"""
import sys

sys.path.insert(0, 'python')
import jaos  # noqa: E402

m = jaos.Model()
m.read_mps('bench/instances-infeas/gran.mps')
r = m.feasrelax()
moves = [abs(v) for v in r.row_move if v != 0.0] + \
        [abs(v) for v in r.col_move if v != 0.0]
moves.sort()

print('moves:', len(moves))
print('reported total:', repr(r.report.total))
print('sum of sizes  :', repr(sum(moves)))
print('largest       :', repr(moves[-1]) if moves else '-')
print('smallest      :', repr(moves[0]) if moves else '-')
for bar in (1e-14, 1e-12, 1e-9, 1e-7, 1e-5, 1e-3, 1.0):
    under = [v for v in moves if v <= bar]
    print('  <= %-8g : %4d moves, %g of the total'
          % (bar, len(under), sum(under)))
