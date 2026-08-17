# The floor-less window takes nothing on this set

The measurement that closed `TODO.md` §1b (D109), taken 2026-08-17. The
question: `ps_implied_free_margin`'s window is
`ULPS * DBL_EPSILON * max(1, scale)`, and §1b proposed that the `max(1, …)`
floor is what declines the exact-equality candidates, so removing it would
take them and leave the rest declined.

## The reasoning stated before the run

The containment test is `ilo >= cl + margin`. An exact-equality candidate
(`ilo == cl`) passes only when `cl + margin` rounds back to `cl`, which
needs `margin < ulp(cl)/2`. Two floors stack under the margin:
`ps_bound_scale` already returns at least 1, and `ps_implied_free_margin`
floors the quotient `scale/|a|` at 1 again. Removing the outer floor
changes the margin only where `|a| > max(1, |b|, traffic)`, and even there
the margin stays positive, so absorption still needs a bound large relative
to it. On a zero bound nothing is ever absorbed. Prediction, written before
the first run: `maros-r7`'s four exact-equality candidates sit on zero
bounds, so the floor-less window leaves it at 980 rows and only margin zero
reads 984.

## The instrument, and how it proves itself

`run-floorless.sh` copies the tree, removes the outer floor from
`ps_implied_free_margin` in the copy (one anchored replacement or abort),
and does not touch the repository. Before the measured run, the copy must
reproduce a committed reading that the patch cannot produce by accident:
built at `JAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=0` it reads `maros-r7` at
iters=2544 work=316766250, exactly the 02-12 sweep's margin-0 record. That
proves the copy's build is real, the flag flows, and the family is live in
it. The measured binary is then the same patched source at the shipping
margin 8.

## The result

**All 94 instance lines are bit-identical to the committed record** —
presolve counts, iterations, work units, objectives and solution digests.
`netlib-floorless8.txt` is the run's record; `diff` against
`bench/results/netlib.txt` returns nothing on the instance lines. Digest
equality is the strongest no-op proof this project has.

So the floor declines nothing on this population. The 1353 rows between
margin 8 (1041 removed) and margin 0 (2394) are declined by *any* nonzero
window, not by the floor: their bounds are zero or too small to absorb a
margin of any scale. §1b's premise fails in the direction that keeps the
code as it is, and the constant ships unchanged: `ULPS = 8`, both floors in
place. What margin zero buys and what it costs was already priced by the
D106 sweep (`d2q06c` 2.2163x, refused) and explained in
`bench/measurements/02-15/`.
