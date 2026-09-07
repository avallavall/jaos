"""What is left of gran and gosh after their own relaxation is applied.

Two of the 29 do not read OPTIMAL once their moves are applied: `gran`
reads INFEASIBLE and `gosh` raises. Neither says by itself whether the
relaxation is wrong or whether the moved model sits exactly on the
boundary of feasibility, where a solver with a feasibility tolerance can
read a face as empty.

The instrument is the relaxation itself, run a second time on the moved
model. A relaxation that is right leaves a residual at the tolerance and
nothing more; one that is wrong leaves a residual of the size it got
wrong. The residual is a number and not a verdict, which is what makes it
readable where the verdict is not.
"""
import sys

sys.path.insert(0, 'python')
import jaos  # noqa: E402


def moved(path, r):
    c = jaos.Model()
    c.read_mps(path)
    for j in range(c.num_col):
        c.set_col_cost(j, 0.0)
    for i, v in enumerate(r.row_move):
        if v == 0.0:
            continue
        lo, hi = c.row_bounds(i)
        if v < 0.0:
            lo += v
        else:
            hi += v
        c.set_row_bounds(i, lo, hi)
    for j, v in enumerate(r.col_move):
        if v == 0.0:
            continue
        lo, hi = c.col_bounds(j)
        if v < 0.0:
            lo += v
        else:
            hi += v
        c.set_col_bounds(j, lo, hi)
    return c


for name in ('gran', 'gosh'):
    path = 'bench/instances-infeas/%s.mps' % name
    m = jaos.Model()
    m.read_mps(path)
    first = m.feasrelax()
    c = moved(path, first)
    try:
        second = c.feasrelax()
        print('%-6s first total=%-24.17g  residual after the moves=%-24.17g '
              'ratio=%.3g'
              % (name, first.report.total, second.report.total,
                 second.report.total / first.report.total))
    except jaos.JaosError as e:
        print('%-6s first total=%-24.17g  residual: raised %s'
              % (name, first.report.total, e))
