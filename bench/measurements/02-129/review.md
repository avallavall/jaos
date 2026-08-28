# numerics-reviewer on the `can_move` diff, before the campaign ran

Run on the one-line candidate `return wrong_way > s->dual_tol;`. Every claim
below was checked against the source before the arms were built. The review is
here because the reviewer had no Write tool.

**The identity the whole review turns on.** Past `can_move`'s two earlier
gates, `wrong_way > s->dual_tol` is bit-for-bit the comparison `dual_breach`
makes: at `JM_AT_LOWER` it is `d < -tol`, at `JM_AT_UPPER` it is `d > tol`. So
the candidate reduces `can_move` to `dual_breach(s, v) != 0 && above the noise
floor && the other real bound is finite`.

## 1. HIGH — a published-space breach loses its repair. ACTED ON: second arm

The candidate reads the scaled space. The verdict on the point is read in the
published space: `settled_dual_violation` (src/simplex.c:1959) maxes over
`published_breach`, and the external checker judges `rep.max_dual_violation`
in original space at `CHECK_TOL = 1e-6` (bench/run.c:159, bench/run.c:685).

A column with scaled `|d| <= dual_tol` but published `|d| > dual_tol` is then
dropped by every mechanism at once:

- `can_move` rejects it, the scaled rate being below tol, so no flip
- `arm_reentry`'s else branch (src/simplex.c:2178) is guarded by `dual_breach`,
  also scaled, so no `shift_to_feasible`
- `wants_a_pivot` (src/simplex.c:2196) does see it, through `breached`, then
  requires `!isfinite` on the other real bound. This column has a finite one,
  so no primal pivot.

Nothing repairs it and `settled_dual_violation` still counts it.

**The trigger, in numbers the record already carries.**
`jaos_set_dual_tolerance(m, 1e-7)` is public (include/jaos.h:328). D27 records
`etamacro`'s column: scaled breach 4.89e-8, `col_scale` 1/32, published
1.56e-6. At `dual_tol = 1e-7` the candidate reads `4.89e-8 > 1e-7` as false,
so no flip, no shift and no pivot, while `published_breach` reads 1.56e-6,
past the checker's 1e-6. That is D27's founding defect, reached through the
public tolerance setter. At the built-in 1e-9 the same window needs
`col_scale < 2^-10`, which is why 02-128's netlib run can be byte-identical
and this still be live.

**The fix the reviewer proposed** is `return breached(s, v);` -- the union of
both spaces, the D92 rule, and the filter `wants_a_pivot` already uses. It is
still a rate against a rate, so it keeps the point of the change and gives up
no space. It is `02-129`'s `union` arm.

## 2. HIGH — the comment's account of `pds-20` was wrong. FIXED: comment cut

The first draft of the comment said D27's cautionary case "is answered by the
relative noise test above". D27 says otherwise: of `pds-20`'s 14 columns the
noise test kills 13, and ONE survives -- "the one whose traffic equals its
`|d|`, a single term with nothing to cancel, so its reduced cost is exact
however small". Its `|d|` is 2.2e-11 to 1.7e-10, which is BELOW
`DUAL_TOL = 1e-9`. The candidate therefore rejects that column too, not
because it is noise but because it is small. The comment credited the noise
test with an outcome it does not produce.

**The falsifiable prediction, which this campaign tests.** If the flip is
dropped, `pds-20`'s `dual=` moves off 0 and `Q=` rises. Base line:
`iters=90938 work=29627237041 dual=0 Q=4.22e-05 cert=yes
digest=7e72be68e812f370`.

## 3. MEDIUM — fixed columns. CARRIED, with a destination

**Termination is bounded and 02-128's reading of it is right.**
`reenter_after_settling` is `for (round = 0; round < SETTLE_ROUNDS; round++)`
(src/simplex.c:2447, `SETTLE_ROUNDS = 32`), a hard ceiling, and a flip cannot
loop: at the other bound the same sign is dual feasible, so `wrong_way` is 0
next round.

**But two of 02-128's supporting claims do not hold.** Fixed columns are not a
`-DJAOS_NO_PRESOLVE` artefact -- every equality row's logical has `lo == up`
(src/simplex.c:483), which is why two pricing loops carry an explicit
`lo == up -> continue` (src/simplex.c:2680, src/simplex.c:2987) in the default
build. And `repair_dual_infeasibility` (src/simplex.c:1784) already flips every
wrong-signed nonbasic to its other bound at ZERO tolerance, inside
`settle_shifts`, before every `anything_to_move` after the first. So the repair
02-128 credits to this change has usually already happened.

**Not confirmed on any instance.** Settle it by counting `arm_reentry` flips of
`lo[v] == up[v]` columns on the reference build, and by reading the `basis=`
field on netlib built with `-DJAOS_NO_PRESOLVE` -- flipping a fixed column
moves `basis=` with `digest=` unchanged, because `basis_digest` (bench/run.c:188)
hashes the published statuses.

## 4. LOW — record defects. FIXED where they are mine, CARRIED where they are older

- **`points_outwards` does not exist in `src/`.** `grep -rn points_outwards
  src/` returns nothing. The function that behaves that way is
  `held_by_an_invented_bound` (src/simplex.c:2543). The stale name lives in
  `docs/tolerances.md:27` and `DECISIONS.md:13148`, and the first draft of the
  comment copied it into the source. Cut from the comment; the two documents
  are still wrong.
- `docs/tolerances.md:27` ends "One site reads this constant in the wrong
  units ... so it decides nothing today". False once this lands, and
  `make record-check` compares constants, not prose.
- `bench/measurements/02-84/`'s `DUAL_TOL` sweep, the evidence on both sides
  for that row, was taken against the product. Finding 1 shows the two versions
  diverge as `dual_tol` loosens, so that sweep no longer describes this code.

## Checked and clean

- **Strict `>` versus `>=`.** A column exactly at `dual_tol` is rejected by
  both `can_move` and `dual_breach` now. The product could disagree there:
  `|d| == tol` with a width above 1 read as movable while `dual_breach` read it
  as fine. This is an improvement.
- **The `isfinite(other)` guard is still the right question.** It separates
  "flip to a real bound" from "needs a primal pivot", and `wants_a_pivot` takes
  exactly the complementary case. `real_upper`/`real_lower` (src/simplex.c:513)
  undo a lent bound, so a flip can never park a column on an invented bound.
  `other` stays live through the guard.
- **Reproducibility improves.** The line drops a multiply and a subtraction, so
  it removes a contraction site rather than adding one, and it no longer reads
  `s->lo`/`s->up`, so the decision is insensitive to bound-tightening order.
- **No borrowed scratch changes hands.** `arm_reentry` mutates only column
  `v`'s own cost, `d` and status and visits each `v` once, so its interleaved
  `can_move` calls read nothing a previous iteration wrote.
- **No repair hides a residue.** The change moves columns out of the
  `shift_to_feasible` branch, so there are fewer cost loans, and loans are
  repaid by `settle_shifts` before `settled_dual_violation` is read
  (src/simplex.c:2487). Finding 1 is the opposite shape: a residue that stops
  being repaired and stays visible in `dual=`.
