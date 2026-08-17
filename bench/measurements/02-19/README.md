# The widening the candidate rule would refuse is the family's normal act

The count `TODO.md` §2 needed before its refuse rule could be swept, taken
2026-08-17. D112 is the decision it closed. A patched copy of the tree
prints one line per `JM_PS_SINGLETON_COL` firing — the row's current
bounds and the contribution range being absorbed — and the repository is
not modified (`run-widening.sh`).

Two calibrations before the totals were believed: `grow22` must fire
exactly 20 times per solve, 02-11's committed count (the first run read 40
and the instrument was corrected: the runner solves every instance twice
for its determinism check, and the aggregator now requires the two passes
to be identical, which doubles as a determinism check on presolve itself);
and the diagnostic binary reproduces `grow22`'s committed iters and work
exactly.

## The distribution (`distribution.txt`)

8617 firings over 60 of the 94 standard instances, one pass each:

- **8096 of 8617 (94%) fire on equality rows**, where any positive widening
  is unbounded relative to the row's zero width.
- 8495 widen past the row's own scale; 4934 past 100x it.
- The typical relaxation is not a bounded widening at all: a half-infinite
  column box (`[0, inf)`) makes `cmax` infinite and removes one row end
  entirely, turning the equality into a one-sided row. `bandm` and
  `dfl001`'s traces show it plainly (`cmin=0 cmax=inf`).

## Why this closes the rule

The candidate rule was: refuse a firing whose relaxation widens the row
beyond some multiple of its own scale, with the `grow*` `== 0` rows
becoming `[0, 5e5]` as the motivating case. Two facts kill it:

1. **The discriminator does not discriminate.** `grow7`, `grow15` and
   `grow22` carry the same maximum relative widening, 5.524e5, to four
   digits — and presolve halves `grow15`'s iterations while inflating
   `grow22`'s 7.5x and `grow7`'s 8.8x (02-11). No threshold on the widening
   separates the helped instance from the hurt ones.
2. **Any threshold that catches the `grow*` firings catches the family.**
   98.6% of all firings widen past the row's scale, because absorbing a
   singleton's slack is what the family is for. The rule would disable the
   family set-wide to target two instances, and the set-wide price of that
   is exactly the campaign D112 refuses to spend on a rule its own
   discriminator already failed.

What remains true and open is D108's reopen condition, which §2 now
shares: a measured mechanism that predicts trajectory direction from the
firing site would make a rule possible, and none exists.
