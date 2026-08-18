# Giving the implied free family first refusal is REFUSED: pilotnov answers wrong

Taken 2026-08-18, answering `TODO.md` §4b, which D117 opened. Nothing landed
except one test. The candidate was built in a worktree, and `candidate.diff`
here is the whole of it.

## What was tried

D117 measured that `JM_PS_SINGLETON_COL` — D95's cost-0 bounded singleton
column — takes every column the implied free column singleton (D106) could
also take, because its branch sits above D106's in the same column pass. The
candidate moves **only** that branch below D106's. The free cost-0 cases keep
their old precedence, so exactly one thing changes.

`numerics-reviewer` read the diff first and found the source change clean in
its classes. Its two findings were both about tests and both were applied
before the campaign; one of them is the only thing that survived this record.

## The footprint is exactly what D117 predicted, per instance

| | rows presolve removes, parent → candidate | D117's count |
|---|---|---|
| `ganges` | 1125 → 1113 | **12** |
| `czprob` | 689 → 678 | **11** |
| `dfl001` | 6071 → 6062 | **9** |
| `pilotnov` | 874 → 867 | **7** |
| `pilot-ja` | 835 → 828 | **7** |
| `perold` | 600 → 594 | **6** |
| `seba` | 477 → 476 | **1** |
| `scrs8` | 450 → 449 | **1** |
| `d2q06c` | 2098 → 2097 | **1** |

Nine for nine, 55 rows, no other instance moved a row. A prediction made from
a read-only counter and confirmed by the solver itself.

## The verdict: REJECT, and not on cost

`make netlib` exits 1 with **4 regressions, all on `pilotnov`**.

```
parent    obj=-4497.2761882188706  ref=-4497.2761882188715  objective=ok  checker=ok
          iters=2374   work=86587427    dual=0     rsub=8.16e-13
candidate obj=-3169.5271937202242  ref=-4497.2761882188715  objective=OUT-OF-TOLERANCE
          iters=87432  work=2616239810  dual=0.89  rsub=117   checker=REJECTED
```

**The published objective is wrong by 29%**, and the solver still reports
`optimal`. The point is primal-feasible — `col=1.11e-17`, `row=6.8e-08`,
`rowrel=2.67e-12` — so this is not the containment class `numerics-reviewer`
warned to watch. It is a feasible but **suboptimal** point published as
optimal, with a dual violation of 0.89 and the checker's own suboptimality
bound at 117.

The checker caught it. No digest comparison and no work bar would have: the
answer is feasible and deterministic.

`netlib-infeas` and `netlib-kennington` both read `0 regressed, 0 improved, 0
new`, and Kennington moves no digest at all.

## What it would have bought, stated because a refusal owes both sides

`campaign/geomean-netlib.txt`. Four instances get cheaper and three get dearer
by more than the billing noise:

| | work ratio |
|---|---|
| `ganges` | **0.8429x** |
| `dfl001` | **0.8951x** |
| `czprob` | **0.9227x** |
| `scrs8` | 0.9837x |
| `d2q06c` | 1.037x |
| `perold` | 1.0675x |
| `pilot-ja` | 1.1669x |
| `pilotnov` | **30.2150x** |

Geometric mean over all 94: **1.0358x**. `dfl001` is one of the two instances
that are 74% of the set's total work (D46), so its 0.8951x is a real prize and
the refusal is not free.

**The band between 1.0000x and 1.0015x on about fifty instances is billing, not
cost.** D106's block now runs on every cost-0 bounded singleton column and
bills its range charge there. The four Kennington instances that moved
(0.9977x to 0.9999x) moved for the same reason and carry identical digests.
`numerics-reviewer` named this before the campaign ran.

## What is now open, and it is about the shipped code

**D106 has never in its life been handed a cost-0 bounded singleton column.**
D95's branch sits above it and takes all of them, on every instance of all four
sets. This candidate is the first thing that ever gave it that population: 55
columns, of which **48 came back with a correct answer and 7 did not** — all
seven on `pilotnov`.

So D106's four stated restrictions are not sufficient for that population, and
nothing in the repository says which fifth one is missing. The shipped code is
not affected, because the order it ships with never asks the question. That is
a safety margin nobody chose, and it is now written down.

Why `pilotnov`'s 7 and not `pilot-ja`'s 7 is not settled here. Settling it
needs `jaos-debug`'s procedure on those seven columns, and `pilotnov` is
already the instance D116 found sensitive to a different presolve change
(27.5x there, with a correct answer).

## What survived

One test, from `numerics-reviewer`'s first finding:
`test_the_basis_count_promise_breaks_on_a_declined_column`. It pins
`TODO.md`'s own named minimum case — 2 basic against `num_row = 1`, and 1 in
the reference build — on a column the implied free family declines by margin
rather than by order. The reviewer built the probe that proved the old pin
would have stopped detecting anything under this candidate.

## Reopen conditions

- A mechanism that explains `pilotnov`'s seven columns, and a fifth
  restriction on D106 that declines them. Then the 55 are worth re-asking:
  `ganges`, `dfl001` and `czprob` are a real gain.
- Or a design that gives D106 first refusal only on the columns it is proved
  sound for, which is the same question stated as a predicate.

Handed to `TODO.md` §4b.
