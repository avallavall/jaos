# A within-row demotion cannot pay for the residue: 152 of the 232 declines have no basic column at a bound

Taken 2026-08-19, on the tree with the value guard landed. Closed as D141.

## The question

D140 left netlib's published over-count at +272: 80 firings whose row
logical the reduced solve left nonbasic on an exact degenerate tie, 152
whose row is interior with its logical basic, and D136's 40 singleton rows.
The swap already takes the logical where it can; the two declining classes
need a different variable taken out.

The snap D140 sketched for the 80 does not survive the arithmetic: 02-49
measured 74 of the 80 rows landing exactly on their original bound with the
interior `xv`, and moving `xv` to the column bound perturbs that activity by
ulps, trading a column-status defect for a row-status one. So the candidate
this probe measures is the one both classes share: demote some OTHER basic
column of the same row that rests exactly on its own bound — a degenerate
basic member whose demotion claims nothing false and moves no value. This
is availability only, the same question D135 asked about the logical before
D139 built the exchange; rank was the design question that would follow.

## The measurement

Same harness as 02-49's two-point probe; the true population is filtered by
the column's status at its own replay write. Canaries: 5902 true firings on
netlib, 482 on Kennington, class B = 80, class L = 152, Kennington zero
declines. All held exactly.

| netlib | class B (the 80) | class L (the 152) |
|---|---|---|
| no demotion partner (`cands = 0`) | **66** | **86** |
| forced partner (`cands = 1`) | 14 | 18 |
| a choice (`cands >= 2`) | 0 | 48 |
| rows with no other basic column at all | 0 | 0 |
| mean basic columns in the row | 8.03 | 10.59 |

## What it closes

**Refused.** A within-row demotion rule reaches at most 80 of the 232
declines and leaves 152 with no candidate at all. The rows are not short of
basic columns — eight to eleven on average — but basic variables rest
strictly inside their bounds almost everywhere, and the degenerate
basic-at-bound member the rule needs is rare. No rule that only looks at
the firing row can close the residue, so none should be built.

## What is left, handed to `TODO.md`

- A design that widens the candidate set beyond the row needs a rank
  argument for an arbitrary demotion, which is the attempt-and-fallback
  shape Galabova 2023 describes for HiGHS — and the fallback there is
  accepting the residue.
- Accepting the residue has a measurable price: the 48 solves publish a
  count `build_warm_basis` rejects, so they lose their warm start and
  nothing else. The `warm` re-measure (item 4 of the ordered list) prices
  exactly that.

## Reproducing it

`run-demotion-avail.sh`, beside this file with its output. `src/` is read
and never written; the patch is applied in a throwaway worktree.
