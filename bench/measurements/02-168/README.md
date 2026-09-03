# 02-168 — a column left resting on a bound the solve lent it

D261. Dual phase 1 lends an artificial bound to a column whose cost points
at a side the model left open (D19). At the end of an OPTIMAL solve a
column can still be sitting nonbasic ON that bound: `classify_optimum`
never looks at it, because its test is `held_by_an_invented_bound`, which
needs the reduced cost to press on the loan past `dual_tol`. The column is
then published nonbasic on a bound the model does not have, at the loan's
own value of 1e10, which is the opposite of what `jaos_basis` promises.

D258's population run found four such columns on `finnis`, and ranging
refuses that instance by name (`02-167/`). This is how far the defect
reaches and what the repair costs.

## What is here

| file | what it does |
|---|---|
| `lent-bound-census.c` / `run-lent-bound-census.sh` | every published nonbasic status naming a bound the model has not got, over all three gate sets; writes `lent-bound-census.txt`. A `-UNDEBUG` argument builds it with every assert live and keeps no file |
| `finnis-probe.c` / `run-finnis-probe.sh` | `finnis` alone against a named tree, at `JAOS_LOG_DETAIL`, so the retirement's own line is readable |
| `model-search.c` / `run-model-search.sh` | six shapes built by hand to trigger it. **None of them does** |
| `random-search.c` / `run-random-search.sh` / `run-family.sh` | the family instead: a deterministic walk of small models, counting the published answers that carry a bad status or a value at the loan's magnitude; runs both trees and both builds, writes `family-search.txt` |
| `pinned-models.c` / `run-pinned-models.sh` | the two models the family search returned, against a named tree |
| `survivors.c` / `run-survivors.sh` | the models an earlier version of the repair did NOT clean up, which is what moved the admission rule off "exactly zero" |

## The census over the three gate sets

139 instances. Only `finnis` is affected, and after the change nothing is.

| tree | netlib | netlib-infeas | Kennington |
|---|---|---|---|
| before (`642f71a`) | 1 instance, 4 offenders: `finnis`, AT_UPPER at 1e10 to 4e10 on `[0, inf)` boxes | 0 optimal, nothing to read | 0 |
| after | **0** | 0 optimal | **0** |

## `finnis`, and the objective it had been getting wrong

`run-finnis-probe.sh` against both trees. The netlib published reference
for `finnis` is **172791.06559561158** (`bench/netlib.manifest`).

| | `642f71a` | after |
|---|---|---|
| published objective | 172791.06567185125 | **172791.06559561161** |
| absolute error against the reference | 7.6e-5 | **3e-8** |
| `jaos_check_solution` row violation | 8.43917e-07 | **1.57527e-13** |
| `jaos_check_solution` objective gap | 2.20608e-10 | **1.19851e-16** |
| x on columns 6, 12, 13, 14 | 4e10, 2e10, 1e10, 1e10 | 23.995, 0, 1650.96, 1163.37 |
| statuses | four AT_UPPER | BASIC, AT_LOWER, BASIC, BASIC |
| iterations | 281 | 284 (three pivots and one bound flip) |
| work units | 521722 | 542379, **+3.96%** |

So this was never only a status defect. The four columns at 1e10 made
`c'x` a sum of 1e10-magnitude terms cancelling to 1.7e5, and the published
objective was wrong in its eighth significant figure. With the loan gone
from `x_N`, `compute_primal`'s `b - N x_N` stops cancelling too, which is
where the row violation's five orders come from.

The other 138 instances are byte-identical: the retirement's scan bills no
work units and returns at once when no loan is outstanding.

## The family search, and why the hand-built shapes missed it

`model-search.c` builds six shapes that ought to leave a column on its
loan: two columns sharing one capacity row, a column touching no binding
row, an equality row with a capped partner. **All six are retired by the
dual simplex before the verdict.** The defect needs a degenerate optimal
face the pricing never visits, and that does not come out of a shape
chosen by hand.

`random-search.c` walks a deterministic family instead — 2 to 6 columns, 1
to 4 rows, integer coefficients in [-2, 2], half the columns with an open
top. It reads the published answer twice: a nonbasic status naming a bound
the model lacks, and a value at or past 1e9, which a BASIC status hides
and which no model in this family can reach honestly.

| tree | build | models | optimal | bad status | value past 1e9 |
|---|---|---|---|---|---|
| `642f71a` | default | 200000 | 144092 | **1717** | 1717 |
| `642f71a` | `-DJAOS_NO_PRESOLVE` | 200000 | 144092 | **1701** | 1701 |
| candidate | default | 200000 | 144092 | **0** | **0** |
| candidate | `-DJAOS_NO_PRESOLVE` | 200000 | 144092 | **0** | **0** |

The retirement fires on exactly 1717 and 1701 solves, which is every
offender. `family-search.txt` is the reading.

## What the survivors changed about the rule

The first version of the repair admitted a column whose reduced cost was
**exactly** zero, and ranked the retired point against the original with
`better_point` (D89). Both halves were wrong, and `survivors.c` is why.

**The ranking kept the wrong point.** A point holding a value of 1e10
carries about 1e-6 of cancellation in `c'x` — terms of 2e10 summing to an
objective of single digits — so its objective reads LOWER than the retired
point's by more than the retired point's whole error. `better_point` ranks
by objective and put the 1e10 point back on four models. On one of them the
independent checker calls the point it kept **primal infeasible** by
2.4e-7, where the retired point is feasible. The guard now reads only the
two conditions the OPTIMAL verdict itself rests on: dual signs, and the
model's own bounds.

**Exactly zero was too strict.** Three survivors at 200000 models had
reduced costs of 1.1e-16 and 2.2e-16, one and two ulps, and were left on
1e10. The rule is now `|d_j| * reach <= DBL_EPSILON * objective_traffic`,
where `objective_traffic` is the sum of the magnitudes of `c'x`'s own
terms: the retirement may not move the objective by more than the
objective at that point leaves undetermined. It needs no constant of its
own. At `DUAL_TOL` and a reach of `ARTIFICIAL_BOUND` the product is 1e1,
seven orders past a `bar` of about 2e-6, and the column is refused — which
is the case the rule exists for.

## The tests, validated against the tree without the repair

`tests/test_simplex.c` pins both models from the family search. Compiled
against the `642f71a` worktree they fail, both with `a column is nonbasic
on an upper bound the model has not got`, and the other 117 tests in that
file still pass:

```
119 Tests 2 Failures 9 Ignored
```

## The new assert, and the population that is quiet under it

`retire_one_loan` asserts that the column has left its loan before the
loan is undone. `run-lent-bound-census.sh -UNDEBUG` builds the census with
that assert and every other one live and runs all three gate sets; nothing
aborts. The canary is the retirement's own log line on `finnis`, which
says four loans were retired: the assert sits on that path, so a quiet run
is a run where it was evaluated and held, not one where it was skipped.

## Ranging over the population, re-checked

`run-ranging-recheck.sh` is D258's own driver, copied here rather than
re-run in `02-167/`, which owns that decision's reading. With the loans
retired, **94 of 94** netlib instances range where 93 did, `finnis` among
them at a nonbasic-end gap of 1.14e-13, and 16 of 16 Kennington. Zero
refusals and zero failed checks on both sets. `ranging-recheck.txt` is the
reading.

## The gate, and whose the baseline rewrite is

All three sets: 0 regressed, 0 improved, 0 new. `record_diff.py` reads 93
of 94 netlib instances bit-identical, and all 29 infeasible and 16
Kennington ones, with `finnis` the only line that moved.

The baseline rewrite touches **five** lines, not one. `bandm`, `nesm`,
`perold` and `pilotnov` moved their `rsub` in digits the record's
three-figure `rsub=` field rounds away, and `bench/netlib.baseline` had
last been written at `27e40c7` with seven `src/` commits landed since.
Running `make netlib-baseline` at the parent commit `642f71a`, in a
worktree outside the repository, reproduces all four of the new values and
none of the committed ones:

```
committed (642f71a)  vs  the parent tree re-run:  5 lines differ
the parent tree      vs  the candidate:           1 line differs (finnis)
```

So those four belong to the seven earlier commits and the rewrite is where
they are finally written down. Leaving a baseline stale for seven commits
is what hid them; the results file's rounding is what let it happen
quietly.

## Every assert live, over all three sets

`run-lent-bound-census.sh -UNDEBUG`: 94 netlib, 29 infeasible and 16
Kennington instances, nothing aborts.
