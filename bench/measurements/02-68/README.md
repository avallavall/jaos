# 02-68 — The collapsed fold's midpoint is clamped into the column's box, and it closes both halves of §1

2026-08-20. `TODO.md`'s §1, the only open item whose error had no stated
bound. It turned out to be one repair for both halves of the item, and the
review found two defects in the first version of it.

## What was asked

When a singleton row's implied interval collapses inside the fold's rounding
window, `src/presolve.c` writes the midpoint of the two ends into both folded
bounds. `new_lo` is at or above `cur_cl[j]` and `new_hi` at or below
`cur_cu[j]`, but the midpoint of a **collapsed** pair need not lie between
them: the branch admits a gap of up to `btol`, so the midpoint sits up to half
of it past whichever bound it crossed.

`btol` carries `row_traffic[i] / |a|` and nothing caps that. The stated size
was `4 * DBL_EPSILON * row_traffic[i] / |a|`, with a shape reading **0.89** — a
published value nearly a whole unit outside a bound the caller declared, which
`jaos.h` promises does not happen.

The item said it needed a decision rather than a patch, because the midpoint
is symmetric in the two ends and whatever replaces it has to keep that.

## 1. The instrument, before believing anything it says

`fold-case.c`, one shape at three magnitudes —
`min x0 s.t. x0 >= rl0, x0 in [0, 1e9]`:

```
A  rl0 = 1e9 + 5e-7   folds=1 collapse=1 out_orig=1 worst_out_orig=2.38419e-07
B  rl0 = 1e9 + 0.4    folds=0 collapse=0            (refused before the fold)
C  rl0 = 1e9 - 1.0    folds=1 collapse=0            (folds, does not collapse)
```

A's 2.38e-7 is the figure `tests/test_presolve.c` already carried for that
model, arrived at independently here. C folds without collapsing, so the probe
is not simply counting folds.

## 2. The branch never runs on the three sets

| | netlib (94) | infeas (29) | Kennington (16) |
|---|---|---|---|
| singleton-row folds | 8622 | 9750 | 81646 |
| intersection collapsed | **0** | **0** | **0** |

**100018 folds, 0 collapses.** The repair is provably a no-op on the gate
before the campaign runs, and the campaign is the check rather than the
question.

## The repair

The midpoint is unchanged and then clamped into the column's own box, when
that box is well formed:

```c
const double mid = 0.5 * (new_lo + new_hi);
if (cur_cl[j] <= cur_cu[j])
    fold_lo = fold_hi = mid < cur_cl[j] ? cur_cl[j]
                      : mid > cur_cu[j] ? cur_cu[j] : mid;
else
    fold_lo = fold_hi = mid;
```

**The symmetry the midpoint was chosen for survives on a well-formed box.**
The clamp reads the box and not which end was tightened, so mirroring the
model mirrors the result. That was the property `TODO.md` said any replacement
had to keep, and it is why a clamp is admissible where choosing one end would
not be.

**Which end gives way is D152's argument, with one qualification.** On the
FIRST fold into a column the box is the caller's own numbers while
`implied_lo`/`implied_hi` came out of `cur_rl[i] / a` — a running difference
divided by a coefficient — so the derived end carries the error and the stored
end wins. On a SECOND fold into the same column the box is itself a previous
fold's `rl/a` and both ends are derived; the clamp is still right there for a
different reason, which is that the box is what every other rule in the file
has already been told. Raised by `numerics-reviewer`; the source says so.

## It closes the DUAL half too, which was not the claim

`fold-dual.c`, on `min x0+x1+x2 s.t. x0 >= 5, x0 <= 5 - g, x1 + x2 >= 3,
x0 in [0, 10]`:

| g | before | after |
|---|---|---|
| 1e-13 — **TODO's own model** | INFEASIBLE | INFEASIBLE |
| 4e-15 | `max_dual_violation = 1`, `dual_feasible` false | **0, true** |
| 1e-15 | 0, true | 0, true |

**`TODO.md`'s recorded model no longer reproduces anything**, on either tree.
The window at scale 5 is `8 * DBL_EPSILON * 5 = 8.88e-15`, so a gap of 1e-13
is eleven times too wide and both builds refuse the model outright. It is
replaced there with a gap that reaches the branch.

The mechanism, which is why one clamp fixes both halves: two singleton rows
folding into one column leave the second fold's midpoint strictly inside the
box the first fold left, so **no record's recorded bound equals the published
value and the reduced cost goes unpaid**. The clamp puts the value back ON the
first fold's bound and restores that record's ownership.

## What it costs: the residue moves onto the row, times |a|

`fold-cost.c`. The residue is not removed. The midpoint splits the admitted
gap between the column bound and the row; the clamp puts the whole of it on
the row — **and it arrives there multiplied by |a|**, because the column
violation is in x units and the row's in a*x units. For a gap `g` in x units:

| | column | row |
|---|---|---|
| midpoint | `g/2` | `\|a\|*g/2` |
| clamp | `0` | `\|a\|*g` |

So the worst of the two changes by `2|a| / max(1, |a|)`: it doubles at
`|a| >= 1` and **shrinks** below `|a| = 0.5`. Measured at one gap and two
coefficients rather than derived (`numerics-reviewer`):

| model | before | after | worst side |
|---|---|---|---|
| `x0 >= 1e9 + 5e-7`, a=1 | col 2.38e-7, row 2.38e-7 | col **0**, row 4.77e-7 | 2.0x |
| `x0 >= 1e9 + 1.5e-6`, a=1 | col 7.15e-7, row 8.34e-7, **primal ok** | col **0**, row 1.55e-6, **primal NOT ok** | 1.9x |
| `4*x0 >= 4e9 + 6e-6` | col 7.15e-7, row 3.34e-6 | col **0**, row 6.20e-6 | 1.9x |
| `0.25*x0 >= 2.5e8 + 3.75e-7` | col 7.15e-7, row 2.09e-7 | col **0**, row 3.87e-7 | **0.54x** |

`primal_feasible` is an absolute test at the caller's tolerance, so at `a = 1`
and a gap near the top of the window the doubled row residual crosses a
`CHECK_TOL` the split stayed under. **That is the honest reading rather than a
regression.** The model is short by the whole gap whichever point is
published; the midpoint was not reducing the violation, it was keeping both
sides under a tolerance neither deserved to pass. The caller now gets a point
inside the box they declared with the residue reported.

`numerics-reviewer`'s sharper point was that the first version of the test
edit deleted the `window` assertion and so removed the only line pinning the
quantity that moved. The test pins `max_col_violation == 0` and
`max_row_violation <= window` now, and says why `primal_feasible` is
deliberately not asserted.

## The inverted box, which the first version aborted on

`min x0 s.t. x0 >= 0, x0 in [1e9, 1e9 - 5e-7]`.

`include/jaos.h` says `xl > xu` is legal input to be reported infeasible, not
refused at load. With the bounds crossed `new_lo > new_hi` holds for any row
at all, so the collapse branch is reached on a model that has nothing to do
with rounding — and the first version's `assert(fold_lo >= cur_cl[j] &&
fold_hi <= cur_cu[j])` **aborted there**, in a configuration `make test` and
`make sanitize` both build.

Nothing in the suite covered it. The only other inverted-box model,
`tests/test_model.c`'s `[5.0, 1.0]`, has a gap about 4.5e14 times the window
and takes the INFEASIBLE branch above without ever reaching the collapse.
That is why 221 of 221 passed. Found by `numerics-reviewer`.

The clamp is skipped when the box is inverted: there is no point to clamp
into, "inside the box" names the empty set, and the ternary would return
whichever end it tested first — which is also where the mirroring symmetry
broke, `cur_cl[j]` unmirrored against `-cur_cu[j]` mirrored. The midpoint
stands and the model is bit-identical to the pre-clamp tree.
`test_a_collapse_on_an_inverted_box_keeps_the_midpoint` pins it, validated by
removing the guard and watching the binary abort.

## The cost on the gate: nothing

94, 29 and 16 instances bit-identical to the committed record, 0 digest
changes, `gate: PASS` with `0 regressed, 0 improved, 0 new` on each, and
`make configs` passes all five build configurations. 222 of 222 tests.

## Reproducing it

```
bash bench/measurements/02-68/run-fold-probe.sh    # the collapse counts, at the parent
```

The probe matches the text of the unclamped midpoint, so it applies at
`78e7084` and not at HEAD; it builds its own detached worktree. `fold-cost.c`
and `fold-dual.c` are built against both trees by hand — the parent's
`src/presolve.c` via `git show`, everything else from the working tree.
