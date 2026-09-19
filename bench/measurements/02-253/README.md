# 02-253 — cones and quadratic rows, judged on generated models

The conic interior point (`src/conic.c`) came with sixteen unit tests on
models small enough to solve by hand. This reading is what showed that
those were not enough: its first run over 300 generated models found
every kind of defect the solve can have, and each fix below is measured
on the same models.

## The models

`conic.c`: 3 to 12 columns and a planted point. 0 to 2 explicit cones,
quadratic or rotated, over columns not shared between cones; the head
columns are moved so the point sits strictly inside. Column boxes of four
shapes around the point (box, free, open above, open below), 0 to 5
linear rows of four shapes around the point's activity (a ranged row's
ends on a 2^-20 grid, so the MPS writer can reproduce them), and 1 to 3
quadratic rows `½x'Qx + a'x <= u` with `Q = B'B + D`, or the concave form
`-½x'Qx + a'x >= l`, both strictly satisfied at the point. A third of the
models carry a convex diagonal objective `Q`. Every fifth model gets one
of two planted infeasibilities instead of the quadratic rows: a ball
`x'x <= 1` and the half-space `Σx >= 1.5 sqrt(k)` past it, or a cone
head capped at 1 with a member fixed at 2.

For each feasible model the solve has to end `OPTIMAL` or `UNBOUNDED`
(the free columns make many of them unbounded). An optimum has to pass
`jaos_check_conic_solution` at 1e-7 on both sides; a copy has to solve to
the same bits; the MPS the model writes has to read back and solve to the
same bits; and the model with each quadratic row rewritten as a rotated
cone over new columns `w = F x`, `t = u - a'x` and `s = 1` has to reach
the same objective within 1e-7 relative. An `UNBOUNDED` answer has to
publish a ray that `jaos_check_ray` takes. A planted infeasibility has to
end `INFEASIBLE`, and a certificate, when one is published, has to pass
`jaos_check_conic_certificate`.

`conic.sh [RUNS] [SEED] [OUT]` builds `conic.c` against
`build/release/libjaos.a` and runs it; a model that fails a check is
written to `OUT/fNNNNN.mps`.

## What the first run found, 300 models at seed 1

- **58 optima the checker refused on the dual side**, by 1e-7 to 5e-5.
  The walk converged to 1e-12, and the duals were still off: in a cone,
  the dual's part along the cone's boundary is determined only to about
  `sqrt(mu)`, and an off-centre last point leaves it there. A quadratic
  row's dual is such a part, and so is anything a cone shares columns
  with. The primal point has the same error along the curved
  constraints. A refit of the duals alone could not close it (the
  misfit stayed at 6e-7), because the point itself was off.
- **59 solves ended as a numerical error.** Most were unbounded: a free
  column with a cost and no rows made the first step fall to 1e-15,
  because the step for `tau` used an identity that holds for the
  unregularised Newton system only. The rest ran on to a broken
  direction just before their certificate test would have passed.
- **No ray was published** for any of 34 unbounded answers: parts of
  1e-9 that should have been zero pushed past a finite bound, and the
  ray checker counts any push.
- **Planted infeasibilities answered `UNBOUNDED`**: a model infeasible and
  with an improving direction both, where the walk found the direction
  first.
- **6 MPS round trips failed**: the writer refuses a ranged row whose
  width is not exact in binary, which is the generator's doing.

## The fixes

- The `tau` step takes its denominator directly,
  `-(kappa/tau - q'dx2 - b'dz2 + (xi - dx2)'P(xi - dx2) - dx2'P dx2)`,
  instead of through the identity.
- **A Newton finish** (`newton_polish`): the constraints the walk ends on
  are read off by comparing each slack with its dual (a row, a bound, a
  cone on its boundary, a cone at its apex, whose members are pinned at
  zero), and Newton's method on the KKT conditions of that active set,
  a cone taken as `||v|| - h <= 0` so that the Lagrangian stays convex,
  runs `CONIC_NEWTON_STEPS` times on the quasi-definite LDL. The finished
  point replaces the walk's only when the checker takes it and its worst
  violation is lower. When it is refused, a least-squares refit of the
  duals alone on the same active set was tried as a fallback and
  removed: it changed no outcome over the 3000 models
  (`bench/refusals.txt`, `conic-dual-refit`).
- An improving ray needs a **feasibility solve**: the same cones and rows
  with the objective at zero. An infeasible answer there turns the verdict
  into `INFEASIBLE` with that solve's certificate.
- A ray's parts below `CONIC_RAY_ZERO` of its largest are zeroed, and so
  are an infeasibility certificate's multipliers: before it, 135 of the
  286 capped-cone infeasibilities published no certificate, a multiplier
  of 1e-9 on a row with a free column making the checker see an
  unbounded column. A ray the checker refuses is **projected** onto the rows it barely moves
  and onto `F d = 0` for every quadratic row, by conjugate gradients on
  `J J'`, and checked again. An `UNBOUNDED` answer needs a ray the
  checker takes.
- A direction with a non-finite part is not taken. When the walk stalls,
  the two certificate tests run again at `CONIC_TOL_INFEAS_STALL`, and a
  last point within `CONIC_TOL_ROUGH` of converged stands if, after the
  Newton finish, the checker takes it on both sides.
- An infeasibility certificate is published only when
  `jaos_check_conic_certificate` takes it; one found at the relaxed
  tolerance that the checker refuses ends the solve as a numerical error.
- The sweep puts a ranged row's ends on a 2^-20 grid.

## The reading after the fixes

1000 models at each of seeds 1, 2 and 3, with the constants of
`docs/tolerances.md` ("The conic interior point's numbers", where each
one's own sweep is):

| seed | optimal | unbounded, ray taken | infeasible: capped cone, certified | ball, certified | numerical error |
|---|---|---|---|---|---|
| 1 | 556 | 240 of 240 | 91 of 91 | 2 of 109 | 4 |
| 2 | 553 | 244 of 244 | 97 of 97 | 1 of 103 | 3 |
| 3 | 570 | 227 of 227 | 98 of 98 | 0 of 102 | 3 |

- **Every optimum passes the checker at 1e-7 on both sides**, the worst
  dual violation 2.2e-16 and the worst relative row violation 1.6e-13.
- **The same models with explicit rotated cones** in place of the
  quadratic rows reach the same objective within 2.3e-10 relative.
- **Every copy solves to the same bits, and so does every model read back
  from the MPS it wrote.**
- **Every planted infeasibility ends `INFEASIBLE`.** The ball and
  half-space models publish no certificate but three: the certificate
  checker takes a quadratic row linearly, and dropping `x'x` from the
  ball leaves the half-space reachable. `TODO.md` row 5 has it.
- **The 10 numerical errors** are nine rays that stay 2e-6 to 9e-2 past
  a row side or a bound after the projection, and one walk that stalls
  with nothing to answer from.
- The work over the 3000 is 429475776 units in 34597 iterations; the
  longest walk takes 26.
