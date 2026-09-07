"""D331's oracle over the 29 pinned infeasible instances.

The relaxation reports a set of signed moves. The claim it makes is that
adding every move to the bound it names leaves a model with a feasible
point. No property of a number checks that, so this applies the moves and
asks the solver, which is the only oracle there is.

Two things the first version of this script got wrong, both of them the
oracle's fault and not the relaxation's, and both fixed here.

The objective is zeroed. A moved model solved with the caller's own
objective can answer UNBOUNDED, which says the feasible set is not empty
-- the question asked -- while reading as a failure. `cplex1` is that
case. With no objective, OPTIMAL and INFEASIBLE are the only two answers
and they mean feasible and not.

The moves are applied twice: exactly, and with one part in a million of
slack. A relaxation of the smallest total puts every relaxed constraint
exactly on its boundary, so the moved model's feasible set is a face and
not a region, and a solver with a feasibility tolerance can read a face
as empty. The slack column separates "the relaxation is wrong" from "the
relaxation is exactly tight", which are different findings.
"""
import glob
import os
import sys

sys.path.insert(0, 'python')
import jaos  # noqa: E402

SLACK = 1e-6


def moved(path, scale):
    """The model with every move applied, scaled, and no objective."""
    c = jaos.Model()
    c.read_mps(path)
    for j in range(c.num_col):
        c.set_col_cost(j, 0.0)
    return c


def apply_moves(c, r, scale):
    for i, v in enumerate(r.row_move):
        if v == 0.0:
            continue
        lo, hi = c.row_bounds(i)
        if v < 0.0:
            lo += v * scale
        else:
            hi += v * scale
        c.set_row_bounds(i, lo, hi)
    for j, v in enumerate(r.col_move):
        if v == 0.0:
            continue
        lo, hi = c.col_bounds(j)
        if v < 0.0:
            lo += v * scale
        else:
            hi += v * scale
        c.set_col_bounds(j, lo, hi)


def verdict(path, r, scale):
    c = moved(path, scale)
    apply_moves(c, r, scale)
    try:
        return c.solve().name
    except jaos.JaosError:
        return 'RAISED'


exact_ok = slack_ok = 0
rows = []
for path in sorted(glob.glob('bench/instances-infeas/*.mps')):
    name = os.path.basename(path)[:-4]
    m = jaos.Model()
    m.read_mps(path)
    r = m.feasrelax()
    a = verdict(path, r, 1.0)
    b = verdict(path, r, 1.0 + SLACK)
    exact_ok += a == 'OPTIMAL'
    slack_ok += b == 'OPTIMAL'
    total = sum(abs(v) for v in r.row_move) + sum(abs(v) for v in r.col_move)
    rows.append((name, r.report.rows_moved + r.report.cols_moved,
                 r.report.total, total, a, b))
    print('%-10s moved=%-5d total=%-24.17g sum=%-24.17g exact=%-11s slack=%s'
          % (name, rows[-1][1], r.report.total, total, a, b))

print()
print('feasible with the moves applied exactly            : %d of %d'
      % (exact_ok, len(rows)))
print('feasible with one part in a million of slack on top: %d of %d'
      % (slack_ok, len(rows)))
worst = max((abs(t - s) / (1.0 + abs(t)), n) for n, _, t, s, _, _ in rows)
print('largest relative gap between the reported total and the moves it '
      'lists: %g on %s' % worst)
