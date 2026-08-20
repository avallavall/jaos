# 02-71 — The frozen-row window took its number from the row's far end

2026-08-20. Found by `numerics-reviewer` while reviewing D160, which had just
dropped the same term from the activity pass for the same reason.

## The defect

```
min 0  s.t.  R: -1e12 <= x0 + x1 <= 0,  x0 and x1 both cost 0 in [1e-4, 1]
```

Both columns are cost-0 bounded singletons, so both relax and freeze R, which
is then empty. The model is infeasible by 2e-4 — neither column can go below
1e-4 — and the frozen-row test is the last word, because an emptied frozen row
is deleted with everything else and the simplex never sees it.

`ps_bound_scale(-1e12, 0)` is 1e12, so the window was `8 * DBL_EPSILON * 1e12`
= 1.78e-3 and swallowed it. **The window came entirely from the row's LOWER
bound, for a test on the UPPER side.**

```
before D159 (0ac44fd)      optimal, x = {1e-4, 1e-4}
the parent (0078244)       optimal, x = {1e-4, 1e-4}
the working tree           INFEASIBLE
reference build, oracle    INFEASIBLE
```

**It predates D159 and D159 did not fix it.** D159 widened around the term
rather than asking whether it belonged, because it was the shipped window and
keeping it could only widen. The question it did not ask is the one D160 was
forced to ask two entries later.

## The control is what makes it a measurement

The same model with `rl = -INFINITY` reads INFEASIBLE on all four trees. That
is only explicable if the finite lower bound was supplying the number, and it
is what separates this from a model that happens to be near a tolerance.

## The repair

`ps_bound_scale(cur_rl[i], cur_ru[i])` is dropped from `rscale`. It is not
narrowed, because there is no third error term for it to cover at any size:
`min_act`/`max_act` carry `eps * rg.traffic` and `cur_rl`/`cur_ru` carry
`eps * row_traffic[i]`, and both are already in the max. `ps_bound_scale`'s
own comment says it is the window a comparison between two BOUNDS uses, and
this test compares a computed activity against one bound.

**Dropping it narrows the window**, which is the direction that refuses good
models, so it is the direction that had to be checked rather than argued:

- 94, 29 and 16 instances bit-identical, 0 digest changes, `gate: PASS` with
  `0 regressed, 0 improved, 0 new` on each.
- All five build configurations pass, 227 tests.
- **Every one of D159's frozen-row tests still passes**, including
  `test_a_frozen_row_is_not_refused_below_its_own_traffic`, which exists to
  catch exactly a window that has become too narrow, and
  `test_a_frozen_row_that_is_exactly_satisfiable_is_not_refused`, which sits
  on the boundary with zero slack.

## Reproducing it

```
bash bench/measurements/02-71/run-far-bound.sh
```

Builds the model against the tree before D159, the parent of this change, the
working tree and `-DJAOS_NO_PRESOLVE`.
