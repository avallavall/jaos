# 02-270 — a refused certificate is re-weighted until it holds

`TODO.md` row 5 said 12 of 02-253's 314 ball-and-half-space models publish
no infeasibility certificate, and named the checker's off-diagonal limit as
the cause. The reading below says the cause is elsewhere: the walk's
multipliers are in the wrong proportion, and the checker's test is sharp in
that proportion.

## What the 12 actually are

Each of the 12 was run alone. `jaos_certificate` returns nothing on all of
them, because the solve's own check refuses the ray it holds.

| what the checker says | count |
|---|---|
| the columns reach further than the rows need, both finite | 10 |
| the columns reach infinity | 2 |

The planted infeasibility is a ball `2Σx² ≤ 1` and a half-space
`Σx ≥ 1.5√k` past it. Its quadratic part is diagonal, so
`row_curves_its_way` takes it. With the ball's multiplier `λ` and the
half-space's `μ`, the checker's test on a model of 4 columns reads
`3μ - λ > μ²/λ`, which holds for `μ/λ` between 0.382 and 2.618 and fails
outside. The test is homogeneous, so scaling the whole ray changes nothing.
Only the proportion counts, and the walk does not aim at it.

## The change

`src/conic.c` keeps the ray it has and looks for a better one, after the
two cleanups it already tries and only when the check has failed.

1. **The tilt.** Every quadratic row's multiplier is scaled by one `t`, the
   rest held. The gap is concave along that line, so a ladder of `2^k` for
   `k` from -32 to 32 and a refinement inside the best step find the best
   `t`.
2. **The climb.** One multiplier at a time, a ladder of steps up and down
   from the largest multiplier's size, each tried against the checker, the
   best kept.
3. **The signs.** A quadratic row whose multiplier curves the row the wrong
   way, and whose negation curves it the right way, is negated, and the
   climb runs again.
4. **The cone.** A cone dual outside its cone is projected onto it, and the
   climb runs again.

The search may call the checker `CONIC_CERT_CALLS` times, which caps its
cost, and the calls are charged to the solve's work units. It does not run
at a tree node (`node_solve`), where no certificate is read and it cost
5.8% of the 02-255 set's work.

## The reading

02-253's 3000 generated models, seeds 1 to 3.

| | before | after |
|---|---|---|
| ball infeasibilities certified | 106, 94, 102 of 109, 103, 102 | 109, 102, 102 |
| all infeasibilities certified | 197, 191, 200 of 200 | 200, 199, 200 |
| checker failures | 0 | 0 |
| failed models | 0 | 0 |
| work units | 155526627, 152674327, 150793751 | 155649561, 152866518, 150957036 |

313 of the 314 ball models publish a certificate, where 302 did and 3 did
before 02-267. The work rises 0.079%, 0.126% and 0.108%.

All 12 of the models written out as MPS pass on their own. One of seed 2's
103 balls still does not inside the generator, which reads no file, so its
walk ends a few bits away from the copy's. What stops a search like this
one is a free column with no curvature: its certificate needs `a = 0`
exactly, which is one equation over the multipliers, and a search that
moves one multiplier at a time cannot hold an equation. A certificate there
needs a solve over the multipliers, which is a second-order cone program:
`a²/(-2h) ≤ t` is a rotated cone in `(t, -h, a)`.

02-255's 3000 mixed-integer models: seed 1 unchanged in every answer
(digest `098d9a85f72bcc6b`), work 380455280 to 383958391, 0.92%.
`make cblib`'s 29 continuous instances: every line byte-identical, 0
regressed, 0 improved. `make test` and `make sanitize` pass.

## The constants

`docs/tolerances.md` holds `CONIC_CERT_TILT` (32), `CONIC_CERT_SWEEPS` (1)
and `CONIC_CERT_CALLS` (1024) with their sweeps.
