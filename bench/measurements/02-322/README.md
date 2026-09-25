# 02-322 — a row the columns' boxes satisfy leaves the conic walk

Taken on 2026-09-25 on the tree of 9dfaf09, for TODO row J3.

## The case

`tests/data/g_cone_badbox.mps` holds fifteen columns that touch nothing
else, two of them with bounds of 1.7e6 to 2.2e7 and 1.8e9 to 2.3e10, and
the walk leaves them out and solves the rest (02-294). With the row
`x11 + x13 >= 0` added, the two are no longer alone: the walk sees their
box, ends at an infeasibility certificate the checker refuses, and the
solve ends `numerical_error`. The row cannot bind: its activity over the
two boxes is at least 1.84e9.

## The change

Outside a tree node, `cm_loose_rows` in `src/conic.c` marks every linear
row whose activity over its columns' boxes stays inside its sides by
`CONIC_LOOSE_MARGIN` (1e-9) times `1 + Σ|a_ij bound_j|`. A row with a
quadratic part, an indicator row and a row over a semi-continuous column
are kept. `cm_without_rows` solves a copy without those rows and maps the
answer back: their duals and certificate multipliers are 0, and their
activities are computed from the columns. The model above then ends
`optimal` at 73622258.83, taken by the checker, since its two columns are
alone again. `tests/test_conic.c` holds it; the test fails on 9dfaf09.

## Readings

`conread.sh`, HEAD (9dfaf09) against the working tree, with `P=1 CJ=1`:

- The 3000 generated models of 02-253 (`gen-*.txt`): some of them hold
  such rows, so the digests change. Every count stays at 0 (checker, copy,
  MPS and rotated-form failures, failed models), the worst violations stay
  at 2.2e-16, and the walks take 45 to 50 fewer iterations and 0.4% less
  work on each seed.
- QPLIB's 13 continuous QCQPs (`qcqp-*.txt`): the same lines.
- QPLIB's 17 convex MIQPs at 1e10 (`miqp-*.txt`): the same lines, except
  the roots of QPLIB_5527, 5543 and 5577, which end without an answer in
  both and now take 1.44x, 1.49x and 1.01x the work: their roots fall back
  to the conic walk outside a tree node, and it runs on the smaller model.
- `make cblib`: 29 of 29, `bench/results/cblib.txt` the same line for line.

`make test` and `make sanitize` pass.
