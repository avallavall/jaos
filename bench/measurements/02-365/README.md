# 02-365 — a barrier point the checker refuses is polished, and QPLIB_9002 passes

Taken on 2026-10-08 on the tree of 35e652a, for TODO row J2.

## Why QPLIB_9002 failed

A network flow, 2890 columns and 1649 equality rows of coefficients ±1,
right-hand sides 0, bounds up to 1.08e11 and separable curvatures from
9e-12 to 2. The barrier converges: primal residual 1e-16, dual 9e-9, gap
2e-12 at iteration 49. The push then fails (734 pinned variables with the
wrong sign), the barrier's own point is published, and the checker
refuses it, so the solve fell through to the conic walk and ended
`numerical_error` at 3.99e9 work units.

The checker's numbers on the barrier's point: objective 5698097498.14474,
the dual objective the same to 16 digits, gap 8.8e-11, and two refusals.

1. Twelve rows are off by up to 8.9e-7. Their traffic is under 1, so the
   window is the absolute 1e-7. The barrier's residual of 1e-16 is
   relative to the model's scale of 1e11.
2. A row off its side by more than its window is not "at" that side, so
   its dual counts as a sign violation, up to 2.12e4. Five columns sit
   1.2e-7 below their upper bound 0 with reduced costs near -7e-5; they
   belong on the bound.

## The polish

`bx_polish` in `src/barrier.c` runs when the checker refuses the barrier's
point after a push that did not settle, and keeps the result only when
the checker passes it:

1. Each column the barrier reads at a bound (distance under its dual
   slack, the test `bx_publish` uses for the basis status) is set on it.
2. The rows are closed over the other columns, weighted by
   `1 / (q + QP_PUSH_REG + zl/w + zu/v)`, on the normal matrix regularised
   by `QP_PUSH_DELTA`, in at most `QP_PUSH_REFINE` passes of iterative
   refinement. Columns with small curvature take the move.
3. A least-squares step on the duals, `(E Θ E' + δI) Δy = E Θ d` over the
   free columns' reduced costs `d`, repeated `QP_PUSH_REFINE` times, takes
   those reduced costs back to zero. Its right-hand side lies in the range
   of the free columns, so it does not move the duals in directions only
   the held columns see.

Without step 3 the rows pass and 67 free columns with curvature 2, moved
by about 3e-7, keep reduced costs up to 6.5e-7. Adding the rows' own dual
step instead moved the duals by up to 45 in those directions.

`QPLIB_9002` now ends `optimal` at 1.50e9 work units: rows 7.5e-8 of their
traffic, duals 0, gap 2.3e-10.

## The readings

- QPLIB's continuous convex QPs that finish under 1e11 work units
  (02-318's `qpread.sh`, `cqp-base.txt`, `cqp-new.txt`): QPLIB_9002 goes
  from `numerical_error` to `optimal` with the checker's yes; the other
  ten read the same lines.
- QPLIB's 17 convex MIQPs at 1e10 (`miqp-base.txt`, `miqp-new.txt`): the
  same lines.
- The 6000 generated QPs of 02-248 (`gen-base.txt`, `gen-new.txt`): the
  same.
- Maros-Meszaros (`maros-meszaros.txt`): 137 of 138 solved and taken by
  the checker, `values` refused as not convex, 0 regressed and 0 improved
  against the baseline.
- `netqp.py` generates circulations with the same traits on a small scale
  (planted flows from 1e-2 to 1e5, curvatures from 1e-11 to 2), and
  `netqp_read.py` solves each with HEAD and the tree (`netqp-12-30.txt`,
  `netqp-40-100.txt`). On 400 models of 12 nodes and 30 arcs the polish
  takes one from `numerical_error` to `optimal`, passes on six more that
  the conic walk had rescued, at less work, and reads 0.994x in the
  geometric mean of work over the 378 optimal both ways. On 120 models of
  40 nodes and 100 arcs it passes on four, and reads 0.981x over 116. The
  21 and 4 models that end `numerical_error` both ways carry a cycle of
  arcs bounded only above, with curvature down to 1e-11, so their optimum
  sits near 1e15 and the barrier reads the walk as running away.

`tests/data/qp_polish.mps` is the 12-node model of seed 33; the test in
`tests/test_barrier.c` asks that the log says the polished point passes.
