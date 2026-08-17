# Why D106 fires on none of fome's candidates: another family takes them first

Taken 2026-08-18, answering `TODO.md` §4a. 02-25 counted 166 / 332 / 664
implied-free column singleton candidates on `fome11` / `fome12` / `fome13`,
found D106 firing on none of them, and ruled out the margin with a canary that
moves. This asks the code which condition declined each candidate.

`jaos-debug`'s procedure. `run-decline.sh` builds a throwaway diagnostic and
reverts nothing, because it never touches the tree: it copies `src/` and
`include/` into `build/diag/02-26/`, applies `patch.py`'s hooks to the copy,
and compiles that. Every hook is inside `#ifdef JAOS_DIAG`. A campaign running
in the main tree is unaffected.

## The answer

**`JM_PS_SINGLETON_COL` takes 100% of them, in round 0, before D106 runs.**

That is D95's family: a singleton column with cost exactly zero and at least
one finite bound. It removes the column, relaxes the row to absorb what the
column could have contributed, and freezes the row against every row-removing
family for the rest of the run. Its branch sits in the same column pass as
D106 and above it, so a column both families would take goes to D95.

| | candidates | taken by D95 | D106's own verdict on them |
|---|---|---|---|
| `fome11` | 166 | **166** | 148 declined by the margin, **18 would have fired** |
| `fome12` | 332 | **332** | 296 declined by the margin, **36 would have fired** |
| `fome13` | 664 | **664** | 592 declined by the margin, **72 would have fired** |
| `fome21` | 0 | 0 | — |

18, 36, 72. The family's doubling holds on this half too.

**The frozen row was the leading suspect and it is refuted.** Over the whole
94-instance standard set exactly **2** candidates are declined because their
row was frozen. On `fome` none are: D95 takes the column before the question
is reached.

**And it explains 02-25's margin canary.** At `PRESOLVE_IMPLIED_FREE_ULPS = 0`
`maros-r7` moves 980 → 984 while `fome11` is bit-identical. D95 takes all 166
at either setting, so no margin can change what D106 does with them. 02-25
concluded the margin was not the cause and the mechanism is now named.

## What D95 freezes, which is the larger number here

| | rows | frozen by D95 | rows presolve removes |
|---|---|---|---|
| `fome11` | 12142 | **1468 (12.09%)** | 0 |
| `fome12` | 24284 | **2936 (12.09%)** | 0 |
| `fome13` | 48568 | **5872 (12.09%)** | 0 |
| `fome21` | 67748 | 0 | 3174 |

The same share at every size. `fome21` carries no cost-0 bounded singleton
column at all, freezes nothing, and is the one instance of the four where
presolve removes rows.

On netlib the same family freezes **8309 rows over 60 of the 94 instances**,
against the 8639 rows every family together removes. `fit2p` freezes all 3000
of its rows, `dfl001` 734 of 6071, then `fit1p` 627, `sctap3` 620, `sctap2`
470, `seba` 360, `cycle` 310, `d2q06c` 277.

`pds` (8 instances) and `nug` (3) carry no candidates and freeze nothing. `nug`
removes no rows at all, by any family.

## The full split over netlib, where D106 was measured

3321 candidates by 02-13's predicate, one fate each:

| what happened | count | what D106 would have said |
|---|---|---|
| D106 fired | 1043 | would fire |
| declined by the margin | 1353 | — |
| taken by D95 | 524 | 446 margin, **55 would fire**, 18 not an equality, 5 frozen |
| the row is not an equality | 315 | — |
| taken by `JM_PS_FIXED_COL` | 76 | 67 live degree, 9 margin |
| taken by `JM_PS_EMPTY_COL` | 8 | live degree |
| the row was frozen | 2 | — |

The 1353 reproduces D109's own figure for the rows between margin 8 and margin
0 (`bench/measurements/02-16/`), which is a second calibration nobody asked
for.

**The 55 are on nine instances**: `ganges` 12, `czprob` 11, `dfl001` 9,
`pilotnov` 7, `pilot-ja` 7, `perold` 6, `seba` 1, `scrs8` 1, `d2q06c` 1. So the
ordering effect is not a `fome` peculiarity. It is 5.3% of what D106 removes
today.

## Calibration, and what would have made this unreadable

Three checks run before any new number is printed, and the script exits on any
of them:

1. **`maros-r7`**: 984 candidates (02-10's `hits`) and 980 D106 firings
   (`docs/tolerances.md`, `TODO.md` §1). Both exact.
2. **netlib as a whole**: 3321 candidates (02-10) and 8639 rows removed by
   every family together (02-12's own figure at the shipping margin). Both
   exact.
3. **The reader against the code.** The decline reader recomputes D106's four
   conditions and its margin beside D106, at the top of the column pass. Every
   branch between there and D106's block ends in `continue`, so a column that
   reaches D106 has had nothing changed under it. A candidate left on
   `WOULDFIRE` at the end would mean the reader and the code disagree. **Zero
   over netlib.**

02-12's other figure, the **1041** rows this family "adds", is a delta against
the 7598 the set read before D106 existed. It is not this counter and is not
calibrated against. D106's own firing count over netlib is **1044**: three rows
that other families used to remove are no longer removed once D106 takes theirs
first, which is the same ordering effect this record is about, measured from
the other side.

One firing of the 1044 is on a column 02-13's predicate does not call a
candidate. The predicate reads the original bounds; D106 reads the current
ones, and a row shifted by a removal can imply a tighter box than the model as
loaded did. So 1043 of the 1044 are candidates.

## What this does not say

**It does not say D95 is wrong or that the order should change.** Both families
are exact. On a cost-0 column D106's cost transfer `c_k -= (c_j / a_ij) * a_ik`
is zero, so D106 would remove the same column, remove its row as well, and
leave no cost behind. That makes D106 the larger reduction on exactly the
columns both can take. It does not make it cheaper: D108 and D112 both measured
this family's cost landing on the trajectory rather than at the reduction site,
and neither found a rule that predicts the direction.

**The 18 / 36 / 72 and the 55 are lower bounds on what changes, not
predictions.** D106 firing removes a row, which changes what every later round
sees. Only a campaign says what the reordering costs.

Handed to `TODO.md` §4b.
