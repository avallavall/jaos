# No loan is outstanding when the duals are published, on any of 128 instances

Taken 2026-08-18, following `TODO.md` §5a item 1. One probe, one file. The
suspicion `numerics-reviewer` raised while reviewing D122 is **removed**, and
the assert that removed it stays in the tree. Closed as D123.

## The question

`refresh` re-runs `shift_to_feasible` over every variable when
`repair_singular_basis` fired (`src/simplex.c:1335`). Both
`take_best_if_better` and `restore_settled` call `refresh` **after** their own
`repay_shifts`. On that path `reenter_after_settling` returns with loans back
in the costs, and nothing settles them before `classify_optimum` and
`publish`.

The published objective is safe whatever the answer is — `publish` builds it
from `m->col_cost`. The duals are not: `sol_dual` is a BTRAN of `s->cost` and
`sol_redcost` is `s->d`, and both would carry the loan.

So: **is that path ever taken?**

## The answer: no, on every instance that answers

`assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v])` for every variable,
on the OPTIMAL branch of `publish` and only there. A solve that ends anywhere
else never calls `settle_shifts`, is entitled to carry loans, and publishes
four arrays of zeros instead.

The cost is compared as well as the record, for D122's reason: a column whose
cost moved while its record cancelled back to zero is the case D121 measured
186 of on `pilotnov`, and the record alone would not see it.

| set | answered | aborted | `publish` assert fired |
|---|---|---|---|
| netlib | 83 | 11 | **0** |
| netlib-infeas | 29 | 0 | **0** |
| netlib-kennington | 16 | 0 | **0** |
| | **128** | **11** | **0** |

Every one of the eleven aborts is the same pre-existing assert, and it is the
standing debt `TODO.md` already carries:

```
   11 presolve.c:2127: ps_replay_one: Assertion `want_lo <= want_hi'
```

It is reached before the solve publishes anything, so those eleven instances
say nothing either way. 128 is what `TODO.md` predicted would answer.

## The negative control, because a clean result is the suspicious kind

An instrument that finds nothing is worth nothing until it is shown able to
find something. A **copy** of the tree gets one line, on the OPTIMAL path,
immediately before `publish` is called:

```c
s.cost[0] += 1.0;
s.shift[0] += 1.0;
```

and the first instance aborts:

```
run: .../02-32/src/simplex.c:3828: publish: Assertion
     `s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]' failed.
```

The assert is reached on the branch it is meant to guard, and it fires on
exactly the state it exists to catch. `src/` is read and never written; the
control lives only in the copy, which the script deletes.

## What this does not say

**It is not a proof that the path cannot be taken.** It is a proof that no
instance in the three sets takes it. The assert stays precisely because of
that: a harder model reaching it will now stop instead of publishing duals
nobody can defend.

**And nothing here is a repair**, so nothing is costed. With `NDEBUG` back
on, the assert compiles to nothing and the release gate reads 94, 29 and 16
instances bit-identical to the committed record on the three sets.

## Reproducing it

`run-outstanding-loan.sh`, beside this file. It cleans first, because `make`
cannot see a `CFLAGS` change (D82), and it opens with a canary that fails if
the asserts were not compiled in — without it, an empty result and a build
with the asserts switched off look exactly alike.

**Re-run against commit `5b92ead`**, the tree this evidence was taken on. The script anchors on source that later commits rewrote; `make record-check` knows it is pinned.
