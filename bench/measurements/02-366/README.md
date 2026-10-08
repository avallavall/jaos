# 02-366 — a pin whose small wrong sign spans a wide box is freed

Taken on 2026-10-08 on the tree of f2214f4.

## What was published

`netqpb.py` is 02-365's circulation generator with every arc bounded
below: the arcs that carry no positive flow get `[-1e6, 0]` instead of
`(-inf, 0]`, so every model has an optimum of moderate size. On 400
models of 12 nodes and 30 arcs, one (seed 291) ended `optimal` with an
answer the checker refuses: the push settled, and the barrier asks the
checker only when the push does not settle.

The checker's gap on it was 1.18e-7 against 1e-7. Column `x12` sat on its
upper bound 0 with a reduced cost of +1.2e-8: the wrong sign, but under
the push's threshold of `QP_PUSH_USER_TOL`, so the push kept it pinned.
Its lower bound is 1e6 away, and the checker charges the reduced cost
times that distance, 1.2e-2, against `1 + |primal| + |dual|` of 1.04e5.

## The change

`push_wrong` in `src/barrier.c` decides a pin's wrong sign for both the
push's sign test and its freeing: past the threshold as before, or of the
wrong sign at all when the reduced cost times the width of the variable's
box passes `QP_PUSH_PIN_GAP` (1e-7) times `1 + |objective|`. On seed 291
the push frees `x12` and one more, settles in 4 rounds instead of 3, and
the gap is 7e-12.

At `QP_PUSH_GAP` (1e-8) in place of `QP_PUSH_PIN_GAP`, the convex MIQP
QPLIB_10069, whose objective is 0, freed reduced costs ten times under the
checker's level on its binaries; its root's point moved, the rounding at
the root took 14 solves instead of 6, and the work went from 3.78e8 to
1.04e9. At 1e-7 it reads as before.

## The readings

Against f2214f4 (02-318's `qpread.sh` and the two generators):

- QPLIB's continuous convex QPs (`cqp-*.txt`): the same lines.
- QPLIB's 17 convex MIQPs at 1e10 (`miqp-*.txt`): the same lines.
- The 6000 generated QPs of 02-248 (`gen-*.txt`): the same.
- Maros-Meszaros (`maros-meszaros.txt`): 137 of 138 solved and taken by
  the checker, 0 regressed and 0 improved against the baseline.
- `netqpb-12-30.txt`, 400 bounded circulations: seed 291 goes from
  `optimal` to `optimal` taken by the checker; the other 399 read the same
  statuses, and the work is the same except on seed 291.
- `netqp-12-30.txt`, 02-365's 400 circulations: the same.

`tests/data/qp_pin_gap.mps` is seed 291; its test in
`tests/test_barrier.c` checks the answer at 1e-7.
