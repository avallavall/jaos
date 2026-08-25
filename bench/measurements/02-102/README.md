# 02-102 — the primal published a value outside a declared bound, as OPTIMAL

2026-08-25. `TODO.md` §0 stage 3, brought forward because stage 1 made it a
live wrong answer rather than a gap.

## The defect

`primal_ratio_test` scans basic variables and asks which of them a step would
push past a bound. **No basic variable can express the entering column's own
opposite bound**, so a ratio test built only from rows walks straight past it.

`overshoot.c` is the case. `min -x - 0.5y` over `x + y <= 10` and
`x + 2y <= 12`, with `x` in `[0, 1]` and `y` in `[0, 10]`. From the origin the
primal prices `x` first — `|d|` of 1 against 0.5 — and moves it up. Nothing
basic stops it before 10. `x`'s own upper bound is 1.

```
== dual   (reference)
   st=0 status=optimal obj=-3.75 x=(1, 5.5)
   x within [0,1]? yes
   checker: primal=ok dual=ok
== primal (before the fix)
   st=0 status=optimal obj=-10 x=(10, 0)
   x within [0,1]? *** NO ***
   checker: primal=*** REFUSED *** dual=ok
```

**The solver published `x = 10` on a column bounded at 1 and called it
optimal.** Only the independent checker refused the point. An objective of -10
against a true -3.75, and no signal anywhere in the solve.

## Why it was reachable only now

`primal_cleanup`, the primal machinery that predates this, enters a column
only through `wants_a_pivot`, which admits **no column with a declared bound
in the improving direction**. So the entering column's other bound was always
infinite there and the case could not arise. Stage 1's pricing rule chooses
freely among eligible columns, and `TODO.md` §0 had said in as many words that
the gap "goes live the moment a pricing rule chooses entering columns".

It went live in the same session that predicted it, and was found by asking.

## The repair

A bound flip: q crosses its own box to the other bound, no basis changes.

- **The limit is read from `real_upper`/`real_lower`, never `up`/`lo`.** Those
  strip the bounds dual phase 1 invented. Flipping onto an invented bound would
  park a variable on a value the model never declared — the case
  `repair_dual_infeasibility` refuses outright, and the evidence
  `classify_optimum` reads immediately afterwards. A column whose other side is
  only invented has no flip, and falls through to the refusal.
- **It costs no solve.** `primal_ratio_test` leaves `B^-1 M_q` in `s->col`, and
  moving q by `delta` moves the basics by `-delta * col`.
- **It terminates.** The duals do not move, so `d[q]` does not; q was eligible
  because its reduced cost pointed off the bound it rested on, and at the
  opposite bound that same sign is feasible. Each flip strictly reduces the
  number of dual infeasible columns.

After:

```
== primal (after the fix)
   st=0 status=optimal obj=-3.75 x=(1, 5.5)
   x within [0,1]? yes
   checker: primal=ok dual=ok
   2 primal iterations
```

Two primal iterations — the flip, then a pivot — and the same point the dual
reaches.

## Gate

`make configs`: all 5 configurations build and pass. All three sets
`0 regressed, 0 improved, 0 new`, and `git diff --stat bench/results/` empty.
The gate cannot reach the primal path at all, so this is what it should say;
the evidence for the fix is the case above and the test that pins it.

## The test that pins it

`test_the_entering_column_stops_at_its_own_bound` asserts the objective **and**
the bound separately, because they are different failures: a solve that lands
on the right objective through a point outside its bounds has still published
something the model forbids.
