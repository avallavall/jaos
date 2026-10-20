# 02-373 — a certificate from a solve over its multipliers

Taken on 2026-10-21 on the tree of a659159, for TODO row J3.

## The failure

One of 02-253's seed-2 ball models publishes no infeasibility certificate:
model 924, now `tests/data/qc_cert_free.mps`. Read from that file, it
fails the same way. The checker says the columns reach infinity: a free
column with no curvature (C5 or C7) keeps a coefficient that is not zero.
The checker needs that coefficient to be zero within its tolerance times
the column's traffic, and C5 has a single term, so only an exact zero
passes. The searches of 02-270 move one multiplier at a time, and one move
cannot keep a sum at zero.

## The change

`cm_cert_solve` in `src/conic.c`. Outside a tree node, when a model with
quadratic rows still has no certificate after the trims and the searches,
it writes the checker's test as a conic program and solves it with JAOS's
own conic solver:

- a multiplier `y_i = p_i - n_i` per row: `p_i >= 0` when the row has a
  finite lower side, `n_i >= 0` when it has a finite upper side. A
  quadratic row keeps only the sign that makes its curvature concave, and
  a row with an off-diagonal entry gets neither;
- a dual `z` per cone member, in the cone itself (both cone types are
  self-dual);
- per column, the coefficient `a_j = Σ y_i a_ij + Σ z`. A column that a
  quadratic row can curve gets `w_j = -Σ y_i q_jj >= 0`, `t_j >= 0` and
  the rotated cone `2 t_j w_j >= a_j²`, so `t_j` bounds the checker's
  `a_j²/(-2h_j)`. Any other column pays `a_j⁺ u_j - a_j⁻ l_j` on its
  finite bounds, and a free one needs `a_j = 0`;
- one row asks the rows' sides minus the columns' reach to be at least 1.

The objective is the sum of `p`, `n` and the cones' heads. Multipliers
below `CONIC_RAY_ZERO` times the largest are set to zero, and the checker
judges the result. The program has no quadratic rows, so its own solve
never comes back to this step.

The conic tree's two helper solves, the one with the integers fixed and
the feasibility solve, set `no_cert_solve` and skip the step. The tree
reads only their status, and with quadratic rows the status does not
depend on this step. A first reading without the flag gave 02-255's
mixed-integer models the same answers for 2.9% to 3.3% more work.

## The reading

`read.sh`: HEAD against the tree. `mip.sh`: 02-255's mixed-integer models
on the same two libraries.

- 02-253's 3000 generated models (`gen-head.txt`, `gen-new.txt`): on seed
  2, 200 of 200 infeasible models publish a certificate the checker takes,
  where 199 did, and the ball models 103 of 103. The work rises by 523841
  units (0.34%), all of it on model 924 (208512 to 732353). Seeds 1 and 3
  write the same digests.
- QPLIB's 13 continuous QCQPs (`qcqp-head.txt`, `qcqp-new.txt`): the same
  lines.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved, and
  `bench/results/cblib.txt` does not change.
- 02-255's 3000 mixed-integer models (`mip-head.txt`, `mip-new.txt`): the
  same lines, digests and work.

`make test` and `make sanitize` pass.
